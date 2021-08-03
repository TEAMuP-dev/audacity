
/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelManagerPanel.h
   Hugo Flores Garcia

******************************************************************/
/**

\class ModelManagerPanel
\brief ModelManagerPanel TODO

*/
/*******************************************************************/

#pragma once

#include "DeepModelManager.h"
#include "ModelCard.h"

class EffectDeepLearning;
class ShuttleGui;
class ModelCardPanel;
class ModelManagerPanel;

class ManagerToolsPanel : public wxPanelWrapper
{
public:
   
   ManagerToolsPanel(wxWindow *parent, ModelManagerPanel *panel);

   void PopulateOrExchange(ShuttleGui & S);

   void SetFetchProgress(int64_t current, int64_t total);
   void OnAddRepo(wxCommandEvent & WXUNUSED(event));

private:
   wxStaticText *mFetchStatus;
   wxButton *mAddRepoButton;
   ModelManagerPanel *mManagerPanel;


};

class ModelManagerPanel final : public wxPanelWrapper
{
   CardFetchedCallback GetCardFetchedCallback();

public:
   ModelManagerPanel(wxWindow *parent, EffectDeepLearning *effect);

   void PopulateOrExchange(ShuttleGui & S);
   void Clear();

   void AddCard(ModelCardHolder card);
   void RemoveCard(ModelCardHolder card); // TODO

   void FetchCards();

private:
   wxScrolledWindow *mScroller;

   ManagerToolsPanel *mTools;
   std::map<std::string, std::unique_ptr<ModelCardPanel>> mPanels;
   EffectDeepLearning *mEffect;
   
   friend class ManagerToolsPanel;
};

class ModelCardPanel final : public wxPanelWrapper
{
public:
   ModelCardPanel(wxWindow *parent, wxWindowID winid, 
                  ModelCardHolder card, EffectDeepLearning *effect);

   void PopulateOrExchange(ShuttleGui &S);
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

   // calbacks
   void OnInstall(wxCommandEvent &event);
   void OnCancelInstall(wxCommandEvent &event);
   void OnUninstall(wxCommandEvent &event);

   void OnEnable(wxCommandEvent &event);

   ModelCardHolder GetCard() const { return mCard; }

private:

   enum class InstallStatus 
   {
      uninstalled, 
      installing, 
      installed
   };

   // handlers
   void PopulateNameAndAuthor(ShuttleGui &S);
   void PopulateDescription(ShuttleGui &S);
   void PopulateMetadata(ShuttleGui &S);
   void PopulateInstallCtrls(ShuttleGui &S);

   void SetInstallStatus(InstallStatus status);

   void FetchModelSize();

private:
   wxWindow *mParent;

   wxStaticText *mModelName;
   wxStaticText *mModelSize;
   wxStaticText *mModelAuthor;
   wxStaticText *mModelDescription;

   wxButton *mInstallButton;
   wxStaticText *mInstallStatusText;
   wxGauge *mInstallProgressGauge;

   wxButton *mEnableButton;

   ModelCardHolder mCard;

   EffectDeepLearning *mEffect;
};


