/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModelManager.cpp
   Hugo Flores Garcia

******************************************************************/

#include "DeepModelManager.h"

#include "NetworkManager.h"
#include "IResponse.h"
#include "Request.h"

#include <wx/tokenzr.h>
#include <wx/log.h>

using namespace audacity;

// TODO: the manager is not thread safe? 
DeepModelManager::DeepModelManager() : mCards(ModelCardCollection()),
                                       mAPIEndpoint("https://huggingface.co/api/")
{
}

DeepModelManager::~DeepModelManager()
{

}

DeepModelManager& DeepModelManager::Get()
{
   static DeepModelManager manager;

   return manager;
}

FilePath DeepModelManager::DLModelsDir()
{
   return FileNames::MkDir( wxFileName( FileNames::DataDir(), wxT("deeplearning-models") ).GetFullPath() );
}

FilePath DeepModelManager::BuiltInModulesDir()
{
   return FileNames::MkDir( wxFileName( FileNames::BaseDir(), wxT("deeplearning-models") ).GetFullPath() );
}

FilePath DeepModelManager::GetRepoDir(ModelCardHolder card)
{
   // TODO: do we really want these fields in the JSON file?
   // or should they be members of the ModelCard class? 

   FilePath authorDir = FileNames::MkDir( 
         wxFileName( DLModelsDir(), card->author() ).GetFullPath() 
   );

   FilePath repoDir = FileNames::MkDir(
         wxFileName( authorDir, card->name() ).GetFullPath()
   );

   return repoDir;
}

std::unique_ptr<DeepModel> DeepModelManager::GetModel(ModelCardHolder card)
{
   if (!IsInstalled(card))
   {
      wxASSERT(IsInstalled(card)); // TODO: 
      throw ModelManagerException("model is not loaded.");
   }

   std::unique_ptr<DeepModel> model = std::make_unique<DeepModel>();
   model->SetCard(card);

   // GetRepoDir won't work if the card is empty
   wxFileName path = wxFileName(GetRepoDir(card), "model.pt");

   // finally, load
   model->Load(path.GetFullPath().ToStdString());

   return model;
}

//TODO fetch installed cards

bool DeepModelManager::IsInstalled(ModelCardHolder card)
{
   FilePath repoDir = GetRepoDir(card);

   wxFileName modelPath = wxFileName(repoDir, "model.pt");
   wxFileName metadataPath = wxFileName(repoDir, "metadata.json");

   return (modelPath.FileExists() && metadataPath.FileExists());
}

void DeepModelManager::Install(ModelCardHolder card, ProgressCallback onProgress, CompletionHandler onCompleted)
{ 
   if (IsInstalled(card))
      return ;

   ProgressCallback progressHandler(
   [this, card, handler {std::move(onProgress)}](int64_t current, int64_t expected)
   {
      // if the install has been cancelled, bail
      // if we don't bail early, then calling the handler will 
      // segfault.
      ModelCardHolder cardCopy = card;
      if (!this->IsInstalling(cardCopy))
         return;

      return handler(current, expected);
   }
   );

   // set up the install handler
   CompletionHandler installHandler(
   [this, card, handler {std::move(onCompleted)} ](int httpCode, std::string body)
   {
      // if the install has been cancelled, bail
      // if we don't bail early, then calling the handler will 
      // segfault.
      ModelCardHolder cardCopy = ModelCardHolder(card);
      if (!this->IsInstalling(cardCopy))
         return;

      std::string repoId = card->GetRepoID(); 
      std::string path = wxFileName(this->GetRepoDir(card), "model.pt").GetFullPath().ToStdString();

      // only attempt save if request succeeded
      if ((httpCode == 200) || (httpCode == 302))
      {
         std::istringstream modulestr(body);

         DeepModel tmpModel = DeepModel();
         tmpModel.SetCard(cardCopy);
         try
         {
            tmpModel.Load(modulestr);
            tmpModel.Save(path);
         }
         catch (const ModelException &e)
         {
            // clean up 
            Uninstall(cardCopy);
            wxLogError(e.what());
         }
      }

      // let the caller handle this
      handler(httpCode, body);

      // get rid of the cached response
      this->mResponseMap.erase(repoId);
   });

   // download the model
   try 
   {
      // save the metadata
      std::stringstream msg;
      msg<<"saving model card for "<<card->GetRepoID()<<std::endl;
      wxLogDebug(wxString(msg.str()));
      card->SerializeToFile(wxFileName(GetRepoDir(card), "metadata.json").GetFullPath().ToStdString());

      msg = std::stringstream();
      msg<<"downloading model for "<<card->GetRepoID()<<std::endl;
      wxLogDebug(wxString(msg.str()));

      network_manager::ResponsePtr response = DownloadModel(card, progressHandler, installHandler);

      // add response to a temporary map (in case of cancellation)
      mResponseMap[card->GetRepoID()] = response;
   }
   catch (const char *msg)
   {
      wxLogError(msg);
      return;
   }
}

void DeepModelManager::Uninstall(ModelCardHolder card)
{

   wxRemoveFile(wxFileName(GetRepoDir(card), "model.pt").GetFullPath());
   wxRemoveFile(wxFileName(GetRepoDir(card), "metadata.json").GetFullPath());
   
   wxRmDir(wxFileName(GetRepoDir(card)).GetFullPath());
   
}

bool DeepModelManager::IsInstalling(ModelCardHolder card)
{
   return !(mResponseMap.find(card->GetRepoID()) == mResponseMap.end());
}

void DeepModelManager::CancelInstall(ModelCardHolder card)
{
   if (!IsInstalling(card))
    {
      // should never really reach here (can't cancel an install that's not ongoing)
      wxASSERT(false);
      return;
    }   
   else
   {
      std::string repoid = card->GetRepoID();
      network_manager::ResponsePtr response = mResponseMap[repoid];
      response->abort();
      mResponseMap.erase(repoid);
   }
}

ModelCardCollection DeepModelManager::GetCards(std::string effect_type)
{
   ModelCardFilter filterId([=](ModelCardHolder card)
   {
      return card->effect_type() == effect_type;
   });
   return mCards.Filter(filterId);
}

void DeepModelManager::FetchModelCards(CardFetchedCallback onCardFetched, CardFetchProgressCallback onProgress)
{
   // add the card to our collection before passing to the callback
   CardFetchedCallback onCardFetchedWrapper = [this, onCardFetched = std::move(onCardFetched)]
   (bool success, ModelCardHolder card)
   {
      if (success)
      {
         // validate it and insert it into collection
         try
         {
            // TODO: this since we're fetching multiple cards at once,
            // this results in a race condition
            std::lock_guard<std::mutex> guard(mCardMutex);
               this->mCards.Insert(card);

         }
         catch (const std::exception& e)
         {
            // TODO: GetRepoID should be a no-throw if we're going to use it here
            std::stringstream msg;
            msg<<"Failed to validate metadata.json for repo "<<card->GetRepoID()<<std::endl;
            msg<<e.what()<<std::endl;
            wxLogDebug(wxString(msg.str()));
         }
      }
      // pass it on
      onCardFetched(success, card);
   };

   // get the repos 
   RepoListFetchedCallback onRepoListFetched = [this, onCardFetchedWrapper = std::move(onCardFetchedWrapper),
                                                onProgress = std::move(onProgress)]
   (bool success, RepoIDList ids)
   {
      if (success)
      {
         int64_t total = ids.size();
         for (int64_t idx = 0;  idx < total ; idx++)
         {
            onProgress(idx+1, total);
            std::string repoId = ids[idx];
            this->FetchCard(repoId, onCardFetchedWrapper);
         }
      }
   };

   FetchRepos(onRepoListFetched);
}

std::string DeepModelManager::GetRootURL(const std::string &repoID)
{
   std::stringstream url;
   url<<"https://huggingface.co/"<<repoID<<"/resolve/main/";
   return url.str();
}

void DeepModelManager::FetchRepos(RepoListFetchedCallback onReposFetched)
{
   std::string query = mAPIEndpoint + "models?filter=audacity";

   // TODO: handle exception in main thread
   CompletionHandler handler = [onReposFetched = std::move(onReposFetched)]
   (int httpCode, std::string body)
   {
      RepoIDList repos;
      if (!(httpCode == 200))
      {
         onReposFetched(false, repos); 
      }
      else
      {
         // TODO: needs rewrite since 
         DocHolder reposDoc;
         try
         {
            reposDoc = parsers::ParseString(body);
         }
         catch (const InvalidModelCardDocument &e)
         {
            wxLogError("error parsing JSON reponse for fetching repos");
            wxLogError(e.what());
         }

         for (auto itr = reposDoc->Begin(); itr != reposDoc->End(); ++itr)
            // TODO: need to raise an exception if we can't get the object 
            // or modelId
            repos.emplace_back(itr->GetObject()["modelId"].GetString());

         onReposFetched(true, repos);
      }
   };

   doGet(query, handler);
}

void DeepModelManager::FetchCard(const std::string &repoID, CardFetchedCallback onCardFetched)
{ 
   std::string modelCardUrl = GetRootURL(repoID) + "metadata.json";
   ModelCardHolder card = std::make_shared<ModelCard>();
   // TODO: how do you handle an exception inside a thread, like this one? 
   CompletionHandler completionHandler = [modelCardUrl, repoID, card, onCardFetched = std::move(onCardFetched)](int httpCode, std::string body)
   { 
      if (!(httpCode == 200))
      {
         std::stringstream msg;
         msg << "GET request failed for url " << modelCardUrl
                 << ". Error code: " << httpCode << std::endl;
         wxLogError(wxString(msg.str()));
      }
      else
      {
         wxStringTokenizer st(wxString(repoID), wxT("/"));
         std::string sAuthor = st.GetNextToken().ToStdString();
         std::string sName = st.GetNextToken().ToStdString();
         
         // TODO: initalize card
         // card = ModelCard(body, sName, sAuthor));
         bool success = false;
         try
         {
            DocHolder doc = parsers::ParseString(body);
            card->Deserialize(doc);
            card->name(sName);
            card->author(sAuthor);

            success = true;
         }
         catch (const InvalidModelCardDocument &e)
         {
            wxLogError(wxString(e.what()));
         }
         catch (const char *msg)
         { 
            wxLogError(wxString(msg));
            wxASSERT(false);
         }
         
         onCardFetched(success, card);
      }
   };

   network_manager::ResponsePtr response = doGet(modelCardUrl, completionHandler);
}

void DeepModelManager::FetchModelSize(const std::string &repoID, ModelSizeCallback onModelSizeRetrieved)
{
   using namespace network_manager;

   std::string modelUrl = GetRootURL(repoID) + "model.pt";

   Request request(modelUrl);

   NetworkManager &manager = NetworkManager::GetInstance();
   ResponsePtr response = manager.doHead(request);

   response->setRequestFinishedCallback(
      [response, handler = std::move(onModelSizeRetrieved)](IResponse*)
      {
         if (!((response->getHTTPCode() == 200) || (response->getHTTPCode() == 302)))
            return;

         if (response->hasHeader("x-linked-size"))
         {
            std::string length = response->getHeader("x-linked-size");
            std::cerr << length << std::endl;

            size_t modelSize = std::stoi(length, nullptr, 10);
            handler(modelSize);
         }

      }
   );
   
}

network_manager::ResponsePtr DeepModelManager::DownloadModel
(ModelCardHolder card, ProgressCallback onProgress, CompletionHandler onCompleted)
{
   // TODO: this is not raising an error for an invalid URL 
   // try adding a double slash anyuwhere to break it. 
   // its because huggingface returns 200s saying "Not Found"
   std::string modelUrl = GetRootURL(card->GetRepoID())  + "model.pt";
   
   std::stringstream msg;
   msg<<"downloading from "<<modelUrl<<std::endl;
   wxLogDebug(wxString(msg.str()));

   auto response = doGet(modelUrl, onCompleted, onProgress);
   return response;
}

network_manager::ResponsePtr DeepModelManager::doGet
(std::string url, CompletionHandler completionHandler, ProgressCallback onProgress)
{
   using namespace network_manager;

   Request request(url);

   NetworkManager &manager = NetworkManager::GetInstance();
   ResponsePtr response = manager.doGet(request);

   // set callback for download progress
   if (onProgress)
      response->setDownloadProgressCallback(onProgress);

   response->setRequestFinishedCallback(
      [response, handler = std::move(completionHandler)](IResponse*) {
         const std::string responseData = response->readAll<std::string>();

         if (handler)
            handler(response->getHTTPCode(), responseData);
      });

   return response;
}