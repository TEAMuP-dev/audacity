/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModel.h
   Hugo Flores Garcia

******************************************************************/
/**

\class DeepModel
\brief base class for handling torchscript models

*/
/*******************************************************************/

#pragma once

#include "ModelCard.h"

#include <torch/script.h>
#include <torch/torch.h>

using ModulePtr = std::unique_ptr<torch::jit::script::Module>;

class ModelException : public std::exception
{
public:
   ModelException(const std::string& msg) : m_msg(msg) {}
   virtual const char* what() const throw () {return m_msg.c_str();}
   const std::string m_msg;
};

class DeepModel
{
   void LoadModel(const std::string &path);
   void LoadResampler();
   void Cleanup();

public:
   DeepModel();
   DeepModel(ModelCard &card);

   // @execsafety: strong 
   // load a torchscript model along with it's metadata, 
   // which is stored in a ModelCard.
   void Load(const std::string &modelPath);
   void Load(std::istream &bytes);
   bool IsLoaded(){ return mLoaded; };

   // @execsafety: strong (will throw if model is not loaded)
   void Save(const std::string &modelPath);

   // use the ModelCard to access metadata attribute's in the 
   // models metadata.json file. 
   void SetCard(ModelCardHolder card);
   ModelCardHolder GetCard();
   int GetSampleRate(){return mSampleRate;}

   // @execsafety: strong (may throw if model is not loaded or 
   // forward pass fails)
   // waveform should be shape (channels, samples)
   torch::Tensor Resample(const torch::Tensor &waveform, int sampleRateIn, int sampleRateOut);

   // @execsafety: strong (may throw if model is not loaded or 
   // forward pass fails)
   // waveform should be shape (channels, samples)
   torch::Tensor Forward(const torch::Tensor &waveform);

private:
   ModulePtr mModel;
   ModulePtr mResampler;

   ModelCardHolder mCard;

   int mSampleRate;
   bool mLoaded;

};

