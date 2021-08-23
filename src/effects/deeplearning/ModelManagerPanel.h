
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
class SimpleModelCardPanel;
class DetailedModelCardPanel;
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

   void SetSelectedCard(ModelCardHolder card);

   void FetchCards();

private:
   wxScrolledWindow *mScroller;

   ManagerToolsPanel *mTools;
   std::map<std::string, std::unique_ptr<SimpleModelCardPanel>> mPanels;
   DetailedModelCardPanel *mDetailedPanel;
   EffectDeepLearning *mEffect;
   
   friend class ManagerToolsPanel;
   friend class EffectDeepLearning;
};

class ModelCardPanel /* not final */: public wxPanelWrapper
{
public:
   ModelCardPanel(wxWindow *parent, wxWindowID winid, 
                  ModelCardHolder card, EffectDeepLearning *effect,
                  ModelManagerPanel *panel, const wxSize& size);

   virtual void PopulateOrExchange(ShuttleGui &S) = 0;
   bool TransferDataToWindow() override;
   bool TransferDataFromWindow() override;

   // calbacks
   void OnInstall(wxCommandEvent &event);
   void OnCancelInstall(wxCommandEvent &event);
   void OnUninstall(wxCommandEvent &event);

   void OnEnable(wxCommandEvent &event);
   void OnSelect(wxCommandEvent &event);
   void OnApply(wxCommandEvent &event);
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
   void PopulateWithNewCard(ModelCardHolder card);

protected:
   std::map<InstallStatus, wxColour> mInstallStatusColors = {
      { InstallStatus::uninstalled, wxColour("#CF6377") },
      { InstallStatus::installing,  wxColour(233, 196, 106) },
      { InstallStatus::installed,   wxColour(42, 157, 143) }
   };

   using DomainTag = std::string;
   std::map<DomainTag, wxColour> mTagColors = {
      { "music",           wxColour("#CF6377") },
      { "speech",          wxColour(233, 196, 106) },
      { "environmental",   wxColour(42, 157, 143) },
      { "other",           wxColour(168, 218, 220) },
   };

protected:

   void Populate();
   void PopulateNameAndAuthor(ShuttleGui &S);
   void PopulateDomainTags(ShuttleGui &S);
   void PopulateShortDescription(ShuttleGui &S);
   void PopulateLongDescription(ShuttleGui &S);
   void PopulateMoreInfo(ShuttleGui &S);
   void PopulateMetadata(ShuttleGui &S);
   void PopulateInstallCtrls(ShuttleGui &S);

   void FetchModelSize();

private:
   wxWindow *mParent;

   wxStaticText *mModelName;
   wxStaticText *mModelSize;
   wxStaticText *mModelAuthor;
   wxStaticText *mShortDescription;
   wxStaticText *mLongDescription;

   wxButton *mInstallButton;
   wxStaticText *mInstallStatusText;
   wxGauge *mInstallProgressGauge;

   wxButton *mSelectButton;
   wxButton *mApplyButton;
   wxButton *mMoreInfoButton;

   ModelCardHolder mCard;

   EffectDeepLearning *mEffect;
   ModelManagerPanel *mManagerPanel;
};

class SimpleModelCardPanel final : public ModelCardPanel
{
public:
   SimpleModelCardPanel(wxWindow *parent, wxWindowID id, 
                           ModelCardHolder card, EffectDeepLearning *effect, 
                           ModelManagerPanel *managerPanel);
   void PopulateOrExchange(ShuttleGui &S);
};

class DetailedModelCardPanel final : public ModelCardPanel
{
public:
   DetailedModelCardPanel(wxWindow *parent, wxWindowID id, 
                        ModelCardHolder card, EffectDeepLearning *effect, 
                        ModelManagerPanel *managerPanel);
   void PopulateOrExchange(ShuttleGui &S);
};
