
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
#include "wx/colour.h"

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
   void OnExplore(wxCommandEvent & WXUNUSED(event));

private:
   wxStaticText *mFetchStatus;
   wxButton *mAddRepoButton;
   wxButton *mExploreButton;
   ModelManagerPanel *mManagerPanel;

};

class ExploreDialog : public wxDialogWrapper
{
public:
   ExploreDialog(wxWindow *parent, ModelManagerPanel *panel);
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
   friend class EffectDeepLearning;
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
   void OnMoreInfo(wxCommandEvent &event);

   void OnClick(wxMouseEvent &event);

   ModelCardHolder GetCard() const { return mCard; }


   enum class InstallStatus 
   {
      uninstalled, 
      installing, 
      installed
   };

   enum class ModelStatus
   {
      enabled,
      disabled
   };

   void SetInstallStatus(InstallStatus status);
   void SetModelStatus(ModelStatus status);

private:
   using DomainTag = std::string;
   std::map<DomainTag, wxColour> mTagColors = {
      { "music",           wxColour("#CF6377") },
      { "speech",          wxColour(233, 196, 106) },
      { "environmental",   wxColour(42, 157, 143) },
      { "other",           wxColour(168, 218, 220) },
   };

private:
   // handlers
   void PopulateNameAndAuthor(ShuttleGui &S);
   void PopulateDomainTags(ShuttleGui &S);
   void PopulateDescription(ShuttleGui &S);
   void PopulateMetadata(ShuttleGui &S);
   void PopulateInstallCtrls(ShuttleGui &S);

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
   wxButton *mMoreInfoButton;

   ModelCardHolder mCard;

   EffectDeepLearning *mEffect;
};

