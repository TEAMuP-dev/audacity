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

TODO: add a more thorough description

*/
/*******************************************************************/

#pragma once

#include "DeepModel.h"
#include "ModelCard.h"

#include "Request.h"
#include "NetworkManager.h"
#include "widgets/ProgressDialog.h"

class ModelManagerException : public std::exception
{
public:
   ModelManagerException(const std::string& msg) : m_msg(msg) {}
   virtual const char* what() const throw () {return m_msg.c_str();}
   const std::string m_msg;
};

using RepoIDList = std::vector<std::string>;

// callbacks
using CompletionHandler = std::function<void (int httpCode, std::string responseBody)>;
using ProgressCallback = std::function<void(int64_t current, int64_t expected)>;

using ModelSizeCallback = std::function<void(size_t size)>;
using CardFetchProgressCallback = std::function<void(int64_t current, int64_t expected)>;
using RepoListFetchedCallback = std::function<void(bool success, RepoIDList repos)>;
using CardFetchedCallback = std::function<void(bool succcess, ModelCardHolder card)>;

// TODO: what happens when a user is NOT connected to the internet?
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

