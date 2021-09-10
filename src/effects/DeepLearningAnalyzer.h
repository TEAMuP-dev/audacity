/**********************************************************************
   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   DeepLearningAnalyzer.h
   Hugo Flores Garcia
   Aldo Aguilar
******************************************************************/
/**
\class DeepLearningAnalyzer
\breif A labeler which uses a deep learning model to output probits and
time sereies data to add labels to an audio track.
*******************************************************************/

#pragma once

#include "deeplearning/DeepModel.h"
#include "deeplearning/DeepLearningEffectBase.h"

class DeepLearningAnalyzer final: public DeepLearningEffectBase
{
public:
   static const ComponentInterfaceSymbol Symbol;

   DeepLearningAnalyzer();
   virtual ~DeepLearningAnalyzer();

   std::string GetDeepEffectID() override;

   // ComponentInterface implementation

   ComponentInterfaceSymbol GetSymbol() override;
   TranslatableString GetDescription() override;
   ManualPageID ManualPage() override;

   // EffectDefinitionInterface implementation

   EffectType GetType() override;

   // Effect implementation
   bool ProcessOne(WaveTrack *track, double tStart, double tEnd) override;

private:

   std::vector<wxString> mClasses;
   void TensorToLabelTrack(torch::Tensor output, std::shared_ptr<AddedAnalysisTrack> labelTrack, 
                                   double tStart, double tEnd, torch::Tensor timestamps);
};
