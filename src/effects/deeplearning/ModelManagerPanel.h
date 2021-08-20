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
#include "ModelCardPanel.h"

class EffectDeepLearning;
class ShuttleGui;
class ModelManagerPanel;

#define MODELCARDPANEL_WIDTH 600
#define MODELCARDPANEL_HEIGHT 150
#define DETAILEDMODELCARDPANEL_WIDTH 400
#define DETAILEDMODELCARDPANEL_HEIGHT 400
#define MODELCARDPANEL_X_OFFSET 20
#define MANAGERPANEL_WIDTH (MODELCARDPANEL_WIDTH + DETAILEDMODELCARDPANEL_WIDTH + MODELCARDPANEL_X_OFFSET)

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
