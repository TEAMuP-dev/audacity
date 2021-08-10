/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   DeepModelManager.cpp
   Hugo Flores Garcia

******************************************************************/

#include "DeepModelManager.h"

#include "FileNames.h"

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
   std::string schemaPath = wxFileName(BuiltInModulesDir(), wxT("modelcard-schema.json"))
                           .GetFullPath().ToStdString();
   mModelCardSchema = parsers::ParseFile(schemaPath);
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
   static const std::string sep = "_";

   FilePath repoDir = FileNames::MkDir( 
      wxFileName(DLModelsDir(), 
                 card->author() + sep + card->name() 
      ).GetFullPath()
   );

   return repoDir;
}

DeepModelHolder DeepModelManager::GetModel(ModelCardHolder card)
{
   if (!IsInstalled(card))
      throw ModelManagerException("model is not loaded.");

   DeepModelHolder model = std::make_shared<DeepModel>();
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
   FilePath repoDir = wxFileName(card->local_path()).GetFullPath();

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
      if (!this->IsInstalling(card))
         return;

      return handler(current, expected);
   }
   );

   // set up the install handler
   CompletionHandler installHandler(
   [this, card, handler {std::move(onCompleted)} ](int httpCode, std::string body)
   {
      if (!this->IsInstalling(card))
      {
         Uninstall(card);
         return;
      }

      if (!(httpCode == 200) || 
          !(httpCode == 302)  ||
          !(body.size() > 0))
         Uninstall(card);
          
      
      // let the caller handle this
      handler(httpCode, body);

      // get rid of the cached response
      this->mResponseMap.erase(card->GetRepoID());
   });

   // download the model
   try 
   {
      // save the metadata
      wxLogDebug(wxString("saving modelcard for %s \n").Format(wxString(card->GetRepoID())));
      card->SerializeToFile(wxFileName(GetRepoDir(card), "metadata.json").GetFullPath().ToStdString());

      wxLogDebug(wxString("downloading model for %s \n").Format(wxString(card->GetRepoID())));

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
            wxLogDebug(
               wxString("Failed to validate metadata.json for repo %s ;\n %s")
                        .Format(
                           wxString(card->GetRepoID()),
                           wxString(e.what())
                        )
            );
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
   std::string url = "https://huggingface.co/"+repoID+"/resolve/main/";
   return url;
}

void DeepModelManager::FetchLocalCards(CardFetchedCallback onCardFetched)
{
   FilePaths pathList;
   FilePaths modelFiles; 

   // NOTE: maybe we should support multiple search paths? 
   pathList.push_back(DLModelsDir());

   FileNames::FindFilesInPathList(wxT("model.pt"), pathList, 
                                 modelFiles, wxDIR_FILES | wxDIR_DIRS);

   for (const auto &modelFile : modelFiles)
   {
      wxFileName modelPath(modelFile);
      wxFileName cardPath(modelPath.GetFullPath());

      cardPath.SetFullName("metadata.json");

      if (cardPath.FileExists() && modelPath.FileExists())
      {
         ModelCardHolder card(safenew ModelCard());
         bool success = NewCardFromLocal(card, cardPath.GetFullPath().ToStdString());
         onCardFetched(success, card);
      }
   }
}

void DeepModelManager::FetchRepos(RepoListFetchedCallback onReposFetched)
{
   // NOTE: the url below asks for all repos in huggingface
   // that contain the tag "audacity". 
   // however, it might be better for us to keep a curated list of 
   // models which we show to the user, and allow the user to explore huggingface
   // on their own for more repos
   // std::string query = mAPIEndpoint + "models?filter=audacity";
   std::string query = GetRootURL("hugggof/audacity-deepmodels") + "models.json";

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
         bool success = true;

         DocHolder reposDoc;
         try
         {
            reposDoc = parsers::ParseString(body);
         }
         catch (const InvalidModelCardDocument &e)
         {
            wxLogError("error parsing JSON reponse for fetching repos");
            wxLogError(e.what());
            success = false;
         }

         if (success && reposDoc->IsArray())
         {
            for (auto itr = reposDoc->Begin(); itr != reposDoc->End(); ++itr)
            {
               wxLogDebug(
                  wxString("Found repo with name %s")
                     .Format(wxString(itr->GetString()))
               );
               repos.emplace_back(itr->GetString());
            }
         }
         else 
            success = false;

         onReposFetched(success, repos);
      }
   };

   doGet(query, handler);
}

void DeepModelManager::FetchCard(const std::string &repoID, CardFetchedCallback onCardFetched)
{ 
   std::string modelCardUrl = GetRootURL(repoID) + "metadata.json";
   // TODO: how do you handle an exception inside a thread, like this one? 
   CompletionHandler completionHandler = 
   [this, modelCardUrl, repoID, onCardFetched = std::move(onCardFetched)]
   (int httpCode, std::string body)
   { 
      if (!(httpCode == 200))
      {
         wxLogError(
            wxString("GET request failed for url %s. Error code: %d")
                  .Format(wxString(modelCardUrl), httpCode)
         );
      }
      else
      {
         ModelCardHolder card(safenew ModelCard());
         bool success = NewCardFromHuggingFace(card, body, repoID);
         onCardFetched(success, card);
      }
   };

   network_manager::ResponsePtr response = doGet(modelCardUrl, completionHandler);
}

void DeepModelManager::FetchModelSize(ModelCardHolder card, ModelSizeCallback onModelSizeRetrieved)
{
   if (card->is_local())
   {
      FilePath repoDir = GetRepoDir(card);
      wxFileName modelPath = wxFileName(repoDir, "model.pt");

      if (modelPath.FileExists())
      {
         size_t model_size = (size_t)wxFile(modelPath.GetFullPath(), wxFile::read).Length();

         card->model_size(model_size);
         onModelSizeRetrieved(model_size);
      }
   }
   else
   {
      using namespace network_manager;

      std::string modelUrl = GetRootURL(card->GetRepoID()) + "model.pt";

      Request request(modelUrl);

      NetworkManager &manager = NetworkManager::GetInstance();
      ResponsePtr response = manager.doHead(request);

      response->setRequestFinishedCallback(
         [card, response, handler = std::move(onModelSizeRetrieved)](IResponse*)
         {
            if (!((response->getHTTPCode() == 200) || (response->getHTTPCode() == 302)))
               return;

            if (response->hasHeader("x-linked-size"))
            {
               std::string length = response->getHeader("x-linked-size");
               std::cerr << length << std::endl;

               size_t modelSize = std::stoi(length, nullptr, 10);
               handler(modelSize);

               card->model_size(modelSize);
            }

         }
      );
   }
}

bool DeepModelManager::NewCardFromHuggingFace(ModelCardHolder card, const std::string &jsonBody, const std::string &repoID)
{
   bool success = true;

   wxStringTokenizer st(wxString(repoID), wxT("/"));
   std::string sAuthor = st.GetNextToken().ToStdString();
   std::string sName = st.GetNextToken().ToStdString();
   
   try
   {
      DocHolder doc = parsers::ParseString(jsonBody);
      card->Deserialize(doc, mModelCardSchema);
      card->name(sName);
      card->author(sAuthor);
      card->set_local(false);
      card->local_path(GetRepoDir(card).ToStdString());

   }
   catch (const InvalidModelCardDocument &e)
   {
      wxLogError(wxString(e.what()));
      success = false;
   }
   catch (const char *msg)
   { 
      success = false;
      wxLogError(wxString(msg));
      wxASSERT(false);
   }
   
   return success;
}

bool DeepModelManager::NewCardFromLocal(ModelCardHolder card, const std::string &filePath)
{
   bool success = true; 

   try
   {
      std::string localPath = wxFileName(wxString(filePath)).GetPath().ToStdString();
      card->DeserializeFromFile(filePath, mModelCardSchema);
      card->set_local(true);
      card->local_path(localPath);
   }
   catch (const InvalidModelCardDocument &e)
   {
      wxLogError(wxString(e.what()));
      success = false;
   }

   return success;
}

network_manager::ResponsePtr DeepModelManager::DownloadModel
(ModelCardHolder card, ProgressCallback onProgress, CompletionHandler onCompleted)
{
   using namespace network_manager;

   // TODO: this is not raising an error for an invalid URL 
   // try adding a double slash anyuwhere to break it. 
   // its because huggingface returns 200s saying "Not Found"
   std::string url = GetRootURL(card->GetRepoID())  + "model.pt";
   
   wxLogDebug(
      wxString("downloading from %s").Format(wxString(url))
   );

   Request request(url);

   // send request
   NetworkManager &manager = NetworkManager::GetInstance();
   ResponsePtr response = manager.doGet(request);

   // open a file to write the model to 
   std::string repoId = card->GetRepoID(); 
   wxString path = wxFileName(this->GetRepoDir(card), "model.pt").GetFullPath();
   std::shared_ptr<wxFile> file = std::make_shared<wxFile>(path, wxFile::write);

   // set callback for download progress
   if (onProgress)
      response->setDownloadProgressCallback(onProgress);

   // completion handler
   response->setRequestFinishedCallback(
      [response, handler = std::move(onCompleted)](IResponse*) 
      {
         const std::string responseData = response->readAll<std::string>();

         if (handler)
            handler(response->getHTTPCode(), responseData);
      }
   );

   // write to file here
   response->setOnDataReceivedCallback(
      [this, response, card, file](IResponse*) 
      {
         // abort
         if (!this->IsInstalling(card))
         {
            Uninstall(card);
            return;
         }

         // only attempt save if request succeeded
         int httpCode = response->getHTTPCode();
         if ((httpCode == 200) || (httpCode == 302))
         {
            try
            {
               file->SeekEnd();

               const std::string responseData = response->readAll<std::string>();
               file->Write(responseData.c_str(), responseData.size());
            }
            // TODO: what to catch here? if anything
            catch (const char *msg)
            {
               // clean up 
               Uninstall(card);
               wxLogError(msg);
            }
         }
      }
   );

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