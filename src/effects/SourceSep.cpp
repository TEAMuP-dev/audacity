/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   SourceSep.cpp
   Hugo Flores Garcia

******************************************************************/
/**

*/
/*******************************************************************/

#include "SourceSep.h"

#include <wx/stattext.h>

#include "FileNames.h"
#include "../Shuttle.h"
#include "../ShuttleGui.h"
#include "../WaveTrack.h"

#include "LoadEffects.h"

#include <torch/script.h>

// EffectDeepLearning implementation

std::string EffectSourceSep::GetDeepEffectID()
{ return "source-separation";}

const ComponentInterfaceSymbol EffectSourceSep::Symbol
{ XO("Source Separation") };

// register source separation
namespace{ BuiltinEffectsModule::Registration< EffectSourceSep > reg; }

EffectSourceSep::EffectSourceSep()
{
   SetLinearEffectFlag(false);
}

EffectSourceSep::~EffectSourceSep()
{
}

// ComponentInterface implementation

ComponentInterfaceSymbol EffectSourceSep::GetSymbol()
{
   return Symbol;
}

TranslatableString EffectSourceSep::GetDescription()
{
   return XO("The goal of audio source separation is to isolate \
             the sound sources in a given mixture of sounds.");
}

ManualPageID EffectSourceSep::ManualPage()
{
   return L"Source_Separation"; 
}

// EffectDefinitionInterface implementation

EffectType EffectSourceSep::GetType()
{
   return EffectTypeProcess;
}

// Effect implementation

// ProcessOne() takes a track, transforms it to bunch of buffer-blocks,
// performs a forward pass through the deep model, and writes 
// the output to new tracks. 
bool EffectSourceSep::ProcessOne(WaveTrack *leader,
                           double tStart, double tEnd)
{
   if (leader == NULL)
      return false;

   // keep track of the sample format and rate,
   // we want to convert all output tracks to this
   sampleFormat origFmt = leader->GetSampleFormat();
   int origRate = leader->GetRate();

   // initialize source tracks, one for each source that we will separate
   std::vector<WaveTrack::Holder> sourceTracks;
   std::vector<std::string> sourceLabels = mModel->GetCard()->labels();
   sourceTracks = CreateSourceTracks(leader, sourceLabels);
   
   // Initiate processing buffer, most likely shorter than
   // the length of the selection being processed.
   Floats buffer{ leader->GetMaxBlockSize() };

   // get each of the blocks we will process
   for (BlockIndex block : GetBlockIndices(leader, tStart, tEnd))
   {
      //Get a blockSize of samples (smaller than the size of the buffer)
      sampleCount samplePos = block.first;
      size_t blockSize = block.second;
      double tPos = leader->LongSamplesToTime(samplePos); 
   
      // get a torch tensor from the leader track
      torch::Tensor input = BuildMultichannelTensor(leader, buffer.get(), 
                                            samplePos, blockSize).sum(0, true, torch::kFloat); 

      // resample!
      input = mModel->Resample(input, origRate, mModel->GetSampleRate());

      // forward pass!
      torch::Tensor output = ForwardPassInThread(input);

      // resample back
      output = mModel->Resample(output, mModel->GetSampleRate(), origRate);

      // write each source output to the source tracks
      for (size_t idx = 0; idx < output.size(0) ; idx++)
         TensorToTrack(output[idx].unsqueeze(0), sourceTracks[idx], 
                       tPos, tEnd); 

      // Update the Progress meter
      mCurrentProgress = (tPos - tStart) / (tEnd - tStart);
      if (TrackProgress(mCurrentTrackNum, mCurrentProgress)) 
         return false;
   }

   // postprocess the source tracks to the user's sample rate and format
   PostProcessSources(sourceTracks, origFmt, origRate);

   return true;
}

std::vector<WaveTrack::Holder> EffectSourceSep::CreateSourceTracks
(WaveTrack *leader, std::vector<std::string> &labels)
{
   std::vector<WaveTrack::Holder> sources;
   for (auto &label : labels)
   {
      WaveTrack::Holder srcTrack = leader->EmptyCopy();

      // append the source name to the track's name
      srcTrack->SetName(srcTrack->GetName() + wxString("-" + label));
      sources.emplace_back(srcTrack);
   }
   return sources;
}

void EffectSourceSep::PostProcessSources
(std::vector<WaveTrack::Holder> &sourceTracks, sampleFormat fmt, int sampleRate)
{
   // flush all output track buffers
   // convert to the original rate and format
   for (std::shared_ptr<WaveTrack> track : sourceTracks)
   {
      track->Flush();
      track->ConvertToSampleFormat(fmt);
      track->Resample(sampleRate);
      AddToOutputTracks(track);

      // if the parent track used to be stereo,
      // make the source mono anyway
      mOutputTracks->GroupChannels(*track, 1);
   }
}
