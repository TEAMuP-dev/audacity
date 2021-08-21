/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   EffectDeepLearning.h
   Hugo Flores Garcia

******************************************************************/
/**

\class EffectDeepLearning
\brief EffectDeepLearning provides methods for using deep learning
                            models in Audacity Effects. 

*/
/*******************************************************************/

#pragma once

#include "ModelManagerPanel.h"
#include "DeepModel.h"
#include "../Effect.h"
#include "WaveTrack.h"

// BlockIndex.first corresponds to the starting sample of a block
// BlockIndex.second corresponds to the length of the block
using BlockIndex = std::pair<sampleCount, size_t>;

class EffectDeepLearning /* not final */ : public Effect
{
public:
   EffectDeepLearning();

   // Effect implementation

   bool Init() override;
   void End() override;
   bool Process() override;
   void PopulateOrExchange(ShuttleGui & S) override;

   // DeepLearningEffect implementation

   // TODO: write desc and instructions
   virtual bool ProcessOne(WaveTrack * track, double tStart, double tEnd) = 0;

   void SetModel(ModelCardHolder card);

   // TODO: write instructions
   virtual std::string GetDeepEffectID() = 0;
   
protected:

   // gets the number of channels in a (possibly multichannel) track
   size_t GetNumChannels(WaveTrack *leader);

   // builds a mono tensor with shape (1, samples)
   // from a track
   torch::Tensor BuildMonoTensor(WaveTrack *track, float *buffer, 
                                 sampleCount start, size_t len);

   // builds a multichannel tensor with shape (channels, samples)
   // given a leader track.
   torch::Tensor BuildMultichannelTensor(WaveTrack *leader, float *buffer, 
                                         sampleCount start, size_t len);

   // performs a forward pass on a helper thread, and sends updates to a progress dialog 
   // to keep the main thread alive. 
   torch::Tensor ForwardPassInThread(torch::Tensor input);

   // writes an output tensor to a track
   // tensor should be shape (1, samples)
   void TensorToTrack(torch::Tensor waveform, WaveTrack::Holder track,
                      double tStart, double tEnd);

   // returns a list of block indices. Use these to 
   // to process the audio in blocks
   std::vector<BlockIndex> GetBlockIndices(WaveTrack *track, 
                                           double tStart, double tEnd);

   // use this to update the progress ba
   int mCurrentTrackNum;
   
   // the deep model itself
   DeepModelHolder mModel;

   // populate this with the current progress in ProcessOne
   double mCurrentProgress {0.0};

   void SetModelDescription();

private:
   ModelCardHolder mCard {nullptr};
   ModelManagerPanel *mManagerPanel {nullptr};
   
   wxStaticText *mModelDesc {nullptr};
};
