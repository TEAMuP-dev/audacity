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
#include "wx/log.h"

#include <torch/script.h>
#include <torch/torch.h>
#include "AudacityException.h"

class DeepModel;

using ModulePtr = std::unique_ptr<torch::jit::script::Module>;
using DeepModelHolder = std::shared_ptr<DeepModel>;

class ModelException final : public MessageBoxException
{
public:
   ModelException(const TranslatableString msg, std::string trace) :
                  m_msg(msg),
                  m_trace(trace),
                  MessageBoxException{
                     ExceptionType::Internal,
                     XO("Deep Model Error")
                  }
   { 
      if (!m_trace.empty()) 
         wxLogError(wxString(m_trace)); 
   }

   // internal message
   virtual const char* what() const throw () 
      { return m_msg.Translation().c_str(); }

   // user facing message
   virtual TranslatableString ErrorMessage() const
      { return XO("Deep Model Error: %s").Format(m_msg);}
   
   const TranslatableString m_msg;
   const std::string m_trace;
};

class DeepModel final
{
public:
   DeepModel() = default;
   DeepModel(ModelCard &card);

   // @execsafety: strong 
   // load a torchscript model along with it's metadata, 
   // which is stored in a ModelCard.
   void Load(const std::string &modelPath);
   void Load(std::istream &bytes);
   bool IsLoaded() const;

   // @execsafety: strong (will throw if model is not loaded)
   void Save(const std::string &modelPath) const;

   // use the ModelCard to access metadata attribute's in the 
   // models metadata.json file. 
   void SetCard(ModelCardHolder card);
   ModelCardHolder GetCard() const;
   int GetSampleRate() const {return mSampleRate;}

   // @execsafety: strong (may throw if model is not loaded or 
   // forward pass fails)
   // waveform should be shape (channels, samples)
   torch::Tensor Resample(const torch::Tensor &waveform, int sampleRateIn, int sampleRateOut) const;

   // @execsafety: strong (may throw if model is not loaded or 
   // forward pass fails)
   // waveform should be shape (channels, samples)
   torch::Tensor Forward(const torch::Tensor &waveform) const;

private:
   ModulePtr mModel;
   ModulePtr mResampler;

   ModelCardHolder mCard;

   int mSampleRate {0};
   bool mLoaded {false};

private:
   void LoadModel(const std::string &path);
   void LoadResampler();
   void Cleanup();
};

