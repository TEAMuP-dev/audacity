/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   EffectDeepLearning.cpp
   Hugo Flores Garcia

******************************************************************/

#include "EffectDeepLearning.h"
#include "DeepModelManager.h"
#include "ModelManagerPanel.h"

#include "FileNames.h"
#include "Shuttle.h"
#include "ShuttleGui.h"
#include <wx/range.h>

#include <torch/script.h>

#include <wx/log.h>
#include <wx/stattext.h>

#include <WaveClip.h>

// ModelCardPanel

EffectDeepLearning::EffectDeepLearning()
{
   mManagerPanel = NULL;
   mCard = NULL;
}

bool EffectDeepLearning::Init()
{
   DeepModelManager &manager = DeepModelManager::Get();

   // try loading the model (if available)
   mModel = std::make_shared<DeepModel>();
   if (mCard)
   {
      if (manager.IsInstalled(mCard))
      {
         mModel = manager.GetModel(mCard);
      }
   }

   return true;
}

void EffectDeepLearning::End()
{
   // release model (may still be active in thread)
   mModel.reset();

   // TODO:  how to clean up card panels?
   if (mManagerPanel)
      mManagerPanel->Clear();
}

bool EffectDeepLearning::Process()
{
   // throw an error if there isn't a model loaded
   if (!mModel->IsLoaded())
   {
      Effect::MessageBox(
          XO("Please load a model before applying the effect."),
          wxICON_ERROR);
      return false;
   }

   // Iterate over each track.
   // All needed because this effect needs to introduce
   // silence in the sync-lock group tracks to keep sync
   CopyInputTracks(true); // Set up mOutputTracks.
   bool bGoodResult = true;

   mCurrentTrackNum = 0;

   // NOTE: because we will append the separated tracks to mOutputTracks in ProcessOne(),
   // we need to collect the track pointers before calling ProcessOne()
   std::vector<WaveTrack *> pOutLeaders;
   for (WaveTrack *track : mOutputTracks->SelectedLeaders<WaveTrack>())
      pOutLeaders.emplace_back(track);

   // now that we have all the tracks we want to process,
   // go ahead and process each!
   for (WaveTrack *pOutLeader : pOutLeaders)
   {

      //Get start and end times from track
      double tStart = pOutLeader->GetStartTime();
      double tEnd = pOutLeader->GetEndTime();

      //Set the current bounds to whichever left marker is
      //greater and whichever right marker is less:
      tStart = wxMax(mT0, tStart);
      tEnd = wxMin(mT1, tEnd);

      // Process only if the right marker is to the right of the left marker
      if (tEnd > tStart)
      {
         //ProcessOne() (implemented below) processes a single track
         if (!ProcessOne(pOutLeader, tStart, tEnd))
            bGoodResult = false;
      }
      // increment current track
      mCurrentTrackNum++;
   }

   ReplaceProcessedTracks(bGoodResult);

   return bGoodResult;
}

size_t EffectDeepLearning::GetNumChannels(WaveTrack *leader)
{
   return TrackList::Channels(leader).size();
}

// gets a list of starting samples and block lengths
// dictated by the track, so we can process the audio
// audio in blocks
std::vector<BlockIndex> EffectDeepLearning::GetBlockIndices(WaveTrack *track, double tStart, double tEnd)
{
   std::vector<BlockIndex> blockIndices;

   const WaveClipHolders &clips = track->GetClips();

   for (const auto &clip : clips)
   {
      sampleCount clipStart = clip->GetStartSample();
      sampleCount clipEnd = clip->GetEndSample();

      sampleCount samplePos = clipStart;

      while (samplePos < clipEnd)
      {
         //Get a blockSize of samples (smaller than the size of the buffer)
         size_t blockSize = limitSampleBufferSize(
            track->GetBestBlockSize(samplePos),
            clipEnd - samplePos);
         
         blockIndices.emplace_back(BlockIndex(samplePos, blockSize));

         samplePos += blockSize;
      }
   }

   return blockIndices;
}

// TODO: get rid of the Floats entirely and simply pass the data_ptr
// to empty torch contiguous zeros
torch::Tensor EffectDeepLearning::BuildMonoTensor(WaveTrack *track, float *buffer,
                                                  sampleCount start, size_t len)
{
   //Get the samples from the track and put them in the buffer
   if (!track->GetFloats(buffer, start, len))
      throw std::runtime_error("An error occurred while copying samples to tensor buffer.");

   // get tensor input from buffer
   torch::Tensor audio = torch::from_blob(buffer, len,
                                          torch::TensorOptions().dtype(torch::kFloat));
   audio = audio.unsqueeze(0); // add channel dimension

   return audio;
}

torch::Tensor EffectDeepLearning::BuildMultichannelTensor(WaveTrack *leader, float *buffer,
                                                          sampleCount start, size_t len)
{
   auto channels = TrackList::Channels(leader);
   std::vector<torch::Tensor> channelStack;

   // because we're reusing the same buffer, it's important that
   // we clone the tensor.
   for (WaveTrack *channel : channels)
      channelStack.emplace_back(
          BuildMonoTensor(channel, buffer, start, len).clone());

   return torch::stack(channelStack);
}

torch::Tensor EffectDeepLearning::ForwardPassInThread(torch::Tensor input)
{
   torch::Tensor output;

   std::atomic<bool> done = {false};
   std::atomic<bool> success = {true};

   // make a copy of the model (in case we need to abort)
   DeepModelHolder model = this->mModel;

   auto thread = std::thread(
      [model, &input, &output, &done, &success]()
      {
         try
         {
            // TODO this won't work because the model 
            torch::Tensor tempOut = model->Forward(input);

            // only write to output tensor if abort was not requested
            if (success)
               output = tempOut;
         }
         catch (const std::exception &e)
         {
            wxLogError(e.what());
            wxLogDebug(e.what());
            success = false;
            output = torch::zeros_like(input);
         }
         done = true;
      }
   );

   // wait for the thread to finish
   while (!done)
   {
      if (TrackProgress(mCurrentTrackNum, mCurrentProgress))
      {
         // abort if requested
         success = false;

         // tensor output will be destroyed once the thread is destroyed
         thread.detach();

         output = torch::zeros_like(input);
         return output;
      }

      wxMilliSleep(50);
   }
   thread.join();

   if (!success)
   {
      Effect::MessageBox(XO("An error occurred during the forward pass"
                        "This model may be broken."),
                        wxOK | wxICON_ERROR);
   }

   return output;
}

torch::Tensor EffectDeepLearning::Resample(torch::Tensor input, int SampleRateIn, int SampleRateOut)
{
   try
   {
      input = mModel->Resample(input, SampleRateIn, SampleRateOut);
   }
   catch(const ModelException& e)
   {
      Effect::MessageBox(XO("An error occurred while resampling the audio data."),
                         wxOK | wxICON_ERROR);
   }

   return input;
}

torch::Tensor EffectDeepLearning::ForwardPass(torch::Tensor input)
{
   torch::Tensor output;
   try
   {
      output = mModel->Forward(input);
   }
   catch (const std::exception &e)
   {
      wxLogError(e.what());
      wxLogDebug(e.what());
      Effect::MessageBox(XO("An error occurred during the forward pass"
                            "This model may be broken."),
                         wxOK | wxICON_ERROR);

      output = torch::zeros_like(input);
   }
   return output;
}

void EffectDeepLearning::TensorToTrack(torch::Tensor waveform, WaveTrack::Holder track,
                                       double tStart, double tEnd)
{
   if (!(waveform.size(0) == 1))
      throw std::runtime_error("Input waveform tensor should be shape (1, samples)");

   // get the data pointer
   float *data = waveform.contiguous().data_ptr<float>();
   size_t outputLen = waveform.size(-1);

   // add the data to a temporary track, then
   // paste on our output track
   WaveTrack::Holder tmp = track->EmptyCopy();
   tmp->Append(reinterpret_cast<samplePtr>(data), floatSample, outputLen);
   tmp->Flush();

   try
   {
      track->ClearAndPaste(tStart, tEnd, tmp.get());
   }
   catch (const std::exception &e)
   {
      Effect::MessageBox(XO("Error copying tensor data to output track"),
                         wxOK | wxICON_ERROR);
   }
}

// UI stuff
void EffectDeepLearning::PopulateOrExchange(ShuttleGui &S)
{
   DeepModelManager &manager = DeepModelManager::Get();

   S.StartVerticalLay(wxCENTER, true);
   {

      if (!mManagerPanel)
         mManagerPanel.reset(safenew ModelManagerPanel(S.GetParent(), this));
      
      mManagerPanel->PopulateOrExchange(S);

      S.StartHorizontalLay(wxCENTER, false);
      {
         std::string modelDesc;
         if (mModel->IsLoaded())
            mModelDesc = S.AddVariableText(XO("%s is Ready").Format(mCard->GetRepoID()));
         else
            mModelDesc = S.AddVariableText(XO("Not Ready"));
      }
      S.EndHorizontalLay();
   }
   S.EndVerticalLay();
}

void EffectDeepLearning::SetModel(ModelCardHolder card)
{
   // if card is empty, reset the model
   if (!card)
   {
      mModel.reset(safenew DeepModel());
      mCard = nullptr;

      mModelDesc->SetLabel(XO("Not Ready").Translation());
      return;
   }


   auto &manager = DeepModelManager::Get();

   if (!manager.IsInstalled(card))
   {
      Effect::MessageBox(
          XO("Please install the model before selecting it."),
          wxICON_ERROR);
   }
   else
   {
      if (!(mModel->IsLoaded() && ((*mModel->GetCard()) == (*card))))
      {
         mModel = manager.GetModel(card);
         mCard = card;
      }
      mModelDesc->SetLabel(XO("%s is Ready").Format(mCard->GetRepoID()).Translation());
   }  
}
