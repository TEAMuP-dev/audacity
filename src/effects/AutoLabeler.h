/**********************************************************************
   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   Labeler.h
   Hugo Flores Garcia
   Aldo Aguilar
******************************************************************/
/**
\file AutoLabeler
\brief AutoLabeler is an effect for labeling audio components in a track. 
******************************************************************/
/**
\class EffectLabeler
\breif A labeler which uses a deep learning model to output probits and
time sereies data to add labels to an audio track.
*******************************************************************/

#pragma once

#include "deeplearning/DeepModel.h"
#include "deeplearning/EffectDeepLearning.h"

class EffectLabeler final: public EffectDeepLearning
{
public:
   static const ComponentInterfaceSymbol Symbol;

   EffectLabeler();
   virtual ~EffectLabeler();

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
