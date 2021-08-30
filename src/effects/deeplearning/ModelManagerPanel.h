/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.

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

static const int cardPanel_w = 600;
static const int cardPanel_h = 150;
static const int cardPanel_x_offset = 20;

static const int detailedCardPanel_w = 400;
static const int detailedCardPanel_h = 400;

static const int managerPanel_w = cardPanel_w + detailedCardPanel_h + 
                                   cardPanel_x_offset + 20;

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
