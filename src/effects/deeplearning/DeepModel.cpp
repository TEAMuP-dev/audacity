/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "DeepModel.h"
#include "DeepModelManager.h"

#include <torch/script.h>
#include <torch/torch.h>

#include "../../WaveTrack.h"
#include "../../WaveClip.h"

// DeepModel Implementation

DeepModel::DeepModel() : mLoaded(false)
                        //  mCard(ModelCardHolder())
{}

void DeepModel::LoadResampler()
{
   // load the resampler module
   std::string resamplerPath = wxFileName(DeepModelManager::BuiltInModulesDir(), wxT("resampler.pt"))
                                       .GetFullPath().ToStdString();
   mResampler = std::make_unique<torch::jit::script::Module>
                  (torch::jit::load(resamplerPath, torch::kCPU));
   mResampler->eval();
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
      throw ModelException(e.what());
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
      throw ModelException(e.what());
   }
}

bool DeepModel::IsLoaded()
{
   return mLoaded; 
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

torch::Tensor DeepModel::Resample(const torch::Tensor &waveform, int sampleRateIn, 
                                  int sampleRateOut)
{
   if (!mLoaded) throw ModelException("Attempted resample while is not loaded."
                                       " Please call Load() first."); 

   // set up inputs
   // torchaudio likes that sample rates are cast to float, for some reason.
   std::vector<torch::jit::IValue> inputs = {waveform, 
                                             (float)sampleRateIn, 
                                             (float)sampleRateOut};

   torch::Tensor output;
   try
   {
      output = mResampler->forward(inputs).toTensor();
   }
   catch (const std::exception &e)
   {
      throw ModelException(e.what());
   }

   return output.contiguous();
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