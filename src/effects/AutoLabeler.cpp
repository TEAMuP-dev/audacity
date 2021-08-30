/**********************************************************************
   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.

   AutoLabeler.cpp
   Aldo Aguilar
   Hugo Flores Garcia
******************************************************************/
/*
\class AutoLabeler
\brief AutoLabeler is an effect for labeling audio components in a track
      using deep learning models.

*/
/*******************************************************************/

#include "AutoLabeler.h"

#include <cstddef>
#include <string>
#include <vector>
#include <wx/stattext.h>

#include "FileNames.h"
#include "Shuttle.h"
#include "ShuttleGui.h"
#include "WaveTrack.h"
#include "LabelTrack.h"

#include "LoadEffects.h"

#include <torch/script.h>

// EffectDeepLearning implementation

std::string EffectLabeler::GetDeepEffectID()
{ return "labeler";}

const ComponentInterfaceSymbol EffectLabeler::Symbol
{ XO("Auto Labeler") };

// register audio Labeler
namespace { BuiltinEffectsModule::Registration<EffectLabeler> reg; }

EffectLabeler::EffectLabeler() 
{ 
   SetLinearEffectFlag(false); 
}

EffectLabeler::~EffectLabeler() 
{
}


// ComponentInterface implementation

ComponentInterfaceSymbol EffectLabeler::GetSymbol() { return Symbol; }

TranslatableString EffectLabeler::GetDescription() 
{
  return XO("The auto labeler uses deep learning models to "
         "annotate audio tracks based on their contents automatically."); 
}

ManualPageID EffectLabeler::ManualPage() 
{
  return L"Audio_Labeler"; 
}

// EffectDefinitionInterface implementation

EffectType EffectLabeler::GetType() { return EffectTypeAnalyze; }

// Effect implementation

// ProcessOne() takes a track, transforms it to bunch of buffer-blocks,
// performs a forward pass through the deep model, and writes
// the output to new tracks.
bool EffectLabeler::ProcessOne(WaveTrack *leader, double tStart, double tEnd) 
{
   // get current models labels
   std::vector<std::string> classList = mModel->GetCard()->labels();
   for (size_t i = 0; i < classList.size(); i++) 
   {
      std::cout << classList[i];
      mClasses.emplace_back(wxString(classList[i]));
   }

   if (leader == NULL)
      return false;

   wxString labelTrackName(leader->GetName() + " Labels");
   std::shared_ptr<AddedAnalysisTrack> labelTrack =
      AddAnalysisTrack(labelTrackName);

   sampleFormat origFmt = leader->GetSampleFormat();
   int origRate = leader->GetRate();

   // Initiate processing buffer, most likely shorter than
   // the length of the selection being processed.
   Floats buffer{leader->GetMaxBlockSize()};

   // get each of the blocks we will process
   for (BlockIndex block : GetBlockIndices(leader, tStart, tEnd)) 
   {
      // Get a blockSize of samples (smaller than the size of the buffer)
      sampleCount samplePos = block.first;
      size_t blockSize = block.second;

      // get a torch tensor from the leader track
      torch::Tensor input =
         BuildMultichannelTensor(leader, buffer.get(), samplePos, blockSize);

      // resample!
      input = mModel->Resample(input, origRate, mModel->GetSampleRate());

      // if we're not doing a multichannel forward pass, downmix
      if (!mModel->GetCard()->multichannel())
         input = input.sum(0, true, torch::kFloat);

      // forward pass!
      torch::jit::IValue output = ForwardPassInThread(input);

      // split forward pass output into output tensor and timestamps
      auto [modelOutput, timestamps] = mModel->ToTimestamps(output);

      // write the block's label to the label track
      double blockStart = leader->LongSamplesToTime(samplePos);
      sampleCount blockEndSamples = samplePos + (sampleCount)blockSize;
      double blockEnd = leader->LongSamplesToTime(blockEndSamples);

      TensorToLabelTrack(modelOutput, labelTrack, blockStart, blockEnd, timestamps);

      // Update the Progress meter
      double tPos = leader->LongSamplesToTime(samplePos);
      if (TrackProgress(mCurrentTrackNum, (tPos - tStart) / (tEnd - tStart)))
      return false;
   }
   labelTrack->Commit();
   return true;
}

// TODO: coalesce labels
void EffectLabeler::TensorToLabelTrack
(torch::Tensor output, std::shared_ptr<AddedAnalysisTrack> labelTrack,
   double tStart, double tEnd, torch::Tensor timestamps) 
{
   // TODO: add an internal check to make sure dim-1 of output nad timestamps
   // match
   // TODO: initalize predicted labels to the size of ouput.size(0)
   timestamps += tStart;
   wxString coalesceLabel;
   double tStartCurrLabel = tStart;
   double tEndCurrLabel = tEnd;
   std::vector<wxString> predictedLabels;

   for (int i = 0; i < output.size(0); i++) 
   {
      // finding the corresponding class for times
      torch::Tensor currentProbits = output[i];
      wxString classLabel =
         mClasses[torch::argmax(currentProbits).item().to<int>()];
      predictedLabels.emplace_back(classLabel);
   }

   for (int i = 0; i < output.size(0); i++) 
   {
      double tTempStart = timestamps[i][0].item().to<double>();
      tEndCurrLabel = (timestamps[i][1].item().to<double>() > tEnd)
                           ? tEnd
                           : timestamps[i][1].item().to<double>();

      if (i < output.size(0) && (predictedLabels[i] == predictedLabels[i+1])) {
      if (!(coalesceLabel == predictedLabels[i])) {
         coalesceLabel = predictedLabels[i];
         tStartCurrLabel = tTempStart;
      }
      }
      else {
      SelectedRegion blockRegion = SelectedRegion(tStartCurrLabel, tEndCurrLabel);
      labelTrack->get()->AddLabel(blockRegion, predictedLabels[i]);
      coalesceLabel = predictedLabels[i];
      tStartCurrLabel = tEndCurrLabel;
      }
   }
} 
