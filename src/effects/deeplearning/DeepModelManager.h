/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModelManager.h
   Hugo Flores Garcia

******************************************************************/
/**

\class DeepModelManager
\brief tools for downloading and managing Audacity models hosted in HuggingFace

*/
/*******************************************************************/

#pragma once

#include "DeepModel.h"
#include "ModelCard.h"

#include "Request.h"
#include "NetworkManager.h"
#include "widgets/ProgressDialog.h"
#include "AudacityException.h"

// this exception should be caught internally, but we 
// derive from MessageBoxException just in case it needs to 
// get handled by Audacity
class ModelManagerException : public MessageBoxException
{
public:
   ModelManagerException(const TranslatableString msg, std::string trace) : 
                        m_msg(msg),
                        m_trace(trace),
                        MessageBoxException{
                           ExceptionType::Internal,
                           XO("Model Manager Error")
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
      { return XO("Model Manager Error: \n %s").Format(m_msg);}

   const TranslatableString m_msg;
   const std::string m_trace;
};

using RepoIDList = std::vector<std::string>;

// callbacks
using CompletionHandler = std::function<void (int httpCode, std::string responseBody)>;
using ProgressCallback = std::function<void(int64_t current, int64_t expected)>;

using ModelSizeCallback = std::function<void(size_t size)>;
using CardFetchProgressCallback = std::function<void(int64_t current, int64_t expected)>;
using RepoListFetchedCallback = std::function<void(bool success, RepoIDList repos)>;
using CardFetchedCallback = std::function<void(bool succcess, ModelCardHolder card)>;

class DeepModelManager
{
   // private! Use Get()
   DeepModelManager(); 
   ~DeepModelManager();

   FilePath GetRepoDir(ModelCardHolder card);

   std::string GetRootURL(const std::string &repoID);
   audacity::network_manager::ResponsePtr doGet(std::string url, CompletionHandler completionHandler, 
                                                ProgressCallback onProgress=NULL);

   void FetchRepos(RepoListFetchedCallback onReposFetched);

   // download a model
   audacity::network_manager::ResponsePtr DownloadModel(ModelCardHolder card, 
                                                        ProgressCallback onProgress, 
                                                        CompletionHandler onCompleted);

public:

   static DeepModelManager& Get();
   static void Shutdown();

   // base directory for deep learning models
   // TODO: maybe we want to support a couple of search paths?
   static FilePath DLModelsDir();
   static FilePath BuiltInModulesDir();

   // loads the deep model and passes ownership to the caller
   DeepModelHolder GetModel(ModelCardHolder card);

   // returns a URL to the HF's repo's readme
   std::string GetMoreInfoURL(ModelCardHolder card);

   // download and install a deep learning model
   bool IsInstalled(ModelCardHolder card);
   bool IsInstalling(ModelCardHolder card);

   // may fail silently, check with IsInstalled()
   void Install(ModelCardHolder card, ProgressCallback onProgress, 
                                 CompletionHandler onCompleted);
   void Uninstall(ModelCardHolder card);
   void CancelInstall(ModelCardHolder card);

   void FetchLocalCards(CardFetchedCallback onCardFetched);
   void FetchModelCards(CardFetchedCallback onCardFetched, CardFetchProgressCallback onProgress);
   void FetchCard(const std::string &repoID, CardFetchedCallback onCardFetched);

   // if the card is local, checks the model.pt file
   // else, it sends a HEAD request for the HF repo's model file
   // if this fails, the callback is not called. 
   void FetchModelSize(ModelCardHolder card, ModelSizeCallback onModelSizeRetrieved);

   ModelCardHolder GetEmptyCard();
   ModelCardCollection GetCards() { return mCards; }
   ModelCardCollection GetCards(std::string effect_type);

private:
   // factory functions for model cards
   bool NewCardFromHuggingFace(ModelCardHolder card, const std::string &jsonBody, const std::string &repoID);
   bool NewCardFromLocal(ModelCardHolder card, const std::string &filePath);

   std::mutex mCardMutex;
      ModelCardCollection mCards;
   
   std::map<std::string, audacity::network_manager::ResponsePtr> mResponseMap;

   const std::string mAPIEndpoint;
   DocHolder mModelCardSchema;
};

