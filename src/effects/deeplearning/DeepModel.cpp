/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "DeepModel.h"

#include <torch/script.h>
#include <torch/torch.h>

#include "../../WaveTrack.h"
#include "../../WaveClip.h"

// DeepModel Implementation

DeepModel::DeepModel() : mLoaded(false)
                        //  mCard(ModelCardHolder())
{}


void DeepModel::Load(const std::string &modelPath)
{
   try
   { 

      // set mModel
      mModel = std::make_unique<torch::jit::script::Module>
                  (torch::jit::load(modelPath, torch::kCPU));
      mModel->eval();
      
      // finally, mark as loaded
      mLoaded = true;
   }
   catch (const std::exception &e)
   {
      Cleanup();
      throw ModelException(e.what());
   }
}

void DeepModel::Load(std::istream &bytes)
{
   try
   {

      // load the model to CPU, as well as the metadata
      mModel = std::make_unique<torch::jit::script::Module>
                     (torch::jit::load(bytes, torch::kCPU));
      mModel->eval();
      
      // finally, mark as loaded
      mLoaded = true;
   }
   catch (const std::exception &e)
   {
      Cleanup();
      throw ModelException(e.what());
   }
}

void DeepModel::Save(const std::string &modelPath)
{
   if (!mLoaded)
      throw ModelException("attempted save when no module was loaded.");

   mModel->save(modelPath);
}

void DeepModel::SetCard(ModelCardHolder card)
{
   // set the card
   mCard = card;

   // set sample rate
   mSampleRate = mCard->sample_rate();
}

ModelCardHolder DeepModel::GetCard()
{
   return ModelCardHolder(mCard);
}

void DeepModel::Cleanup()
{
   // cleanup
   mModel.reset();
   mLoaded = false;
}


// forward pass through the model!
torch::Tensor DeepModel::Forward(const torch::Tensor &waveform)
{
   torch::NoGradGuard no_grad;
   if (!mLoaded) throw ModelException("Attempted forward pass while model is not loaded."
                                       " Please call Load() first."); 

   // set up for jit model
   std::vector<torch::jit::IValue> inputs = {waveform};

   // forward pass!
   torch::Tensor output;
   try
   {
      output = mModel->forward(inputs).toTensor();
   }
   catch (const std::exception &e)
   {
      throw ModelException(e.what());
   }
   // make tensor contiguous to return to track
   output = output.contiguous();

   return output;
}