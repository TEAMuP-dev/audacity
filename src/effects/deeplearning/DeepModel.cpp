/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.

   DeepModel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "DeepModel.h"
#include "DeepModelManager.h"

#include <torch/script.h>
#include <torch/torch.h>

#include "../../WaveTrack.h"
#include "../../WaveClip.h"

#include "CodeConversions.h"

// DeepModel Implementation

void DeepModel::LoadResampler()
{
   // load the resampler module
   std::string resamplerPath = audacity::ToUTF8(wxFileName(DeepModelManager::BuiltInModulesDir(), wxT("resampler.pt"))
                                       .GetFullPath());
   try
   {
      mResampler = std::make_unique<torch::jit::script::Module>
                     (torch::jit::load(resamplerPath, torch::kCPU));
      mResampler->eval();
   }
   catch (const std::exception &e)
   {
      Cleanup();
      throw ModelException(XO("Error an error occurred while loading the resampler"), e.what());
   }
}

void DeepModel::Load(const std::string &modelPath)
{
   try
   { 
      // set mResampler
      LoadResampler();

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
      throw ModelException(XO("Error while loading model"), e.what());
   }
}

void DeepModel::Load(std::istream &bytes)
{
   try
   {
      // load the resampler module
      LoadResampler();

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
      throw ModelException(XO("Error while loading model"), e.what());
   }
}

bool DeepModel::IsLoaded() const
{
   return mLoaded; 
}

void DeepModel::Save(const std::string &modelPath) const
{
   if (!mLoaded)
      throw ModelException(XO("attempted save when no module was loaded."), "");

   mModel->save(modelPath);
}

void DeepModel::SetCard(ModelCardHolder card)
{
   // set the card
   mCard = card;

   // set sample rate
   mSampleRate = mCard->sample_rate();
}

ModelCardHolder DeepModel::GetCard() const
{
   return ModelCardHolder(mCard);
}

void DeepModel::Cleanup()
{
   // cleanup
   mModel.reset();
   mLoaded = false;
}

torch::Tensor DeepModel::Resample(const torch::Tensor &waveform, int sampleRateIn, 
                                  int sampleRateOut) const
{
   if (!mLoaded) 
      throw ModelException(XO("Attempted resample while is not loaded."
                                       " Please call Load() first."), ""); 

   // set up inputs
   // torchaudio likes that sample rates are cast to float, for some reason.
   std::vector<torch::jit::IValue> inputs = {waveform, 
                                             static_cast<float>(sampleRateIn), 
                                             static_cast<float>(sampleRateOut)};

   try
   {
      return mResampler->forward(inputs).toTensor();
   }
   catch (const std::exception &e)
   {
      throw ModelException(XO("A libtorch error occured while resampling."), e.what());
   }
}

// forward pass through the model!
torch::Tensor DeepModel::Forward(const torch::Tensor &waveform) const
{
   // NoGradGuard prevets the model from storing gradients, which should
   // make computation faster and memory usage lower. 
   torch::NoGradGuard NoGrad;

   if (!mLoaded) 
      throw ModelException(XO("Attempted forward pass while model is not loaded."
                                       " Please call Load() first."), ""); 

   // set up for jit model
   std::vector<torch::jit::IValue> inputs = {waveform};

   // forward pass!
   try
   {
      return  mModel->forward(inputs).toTensor().contiguous();
   }
   catch (const std::exception &e)
   {
      throw ModelException(XO("A libtorch error occurred during the forward pass"), e.what());
   }
}
