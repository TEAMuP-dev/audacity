/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelManagerPanel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "EffectDeepLearning.h"
#include "ModelManagerPanel.h"
#include "ExploreHuggingFaceDialog.h"
#include "../EffectUI.h"

#include "Shuttle.h"
#include "ShuttleGui.h"

#include <wx/scrolwin.h>
#include <wx/range.h>
#include <wx/gauge.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>
#include <wx/stattext.h>
#include <wx/hyperlink.h>

#include "Internat.h"
#include "AllThemeResources.h"
#include "Theme.h"

// ModelManagerPanel
// TODO: need to get rid of the unique ptrs to UI elements
ModelManagerPanel::ModelManagerPanel(wxWindow *parent, EffectDeepLearning *effect)
                                    : wxPanelWrapper(parent)
{
   mEffect = effect;
   mTools = nullptr;

   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
   Layout();
   Fit();
   Center();
}

void ModelManagerPanel::PopulateOrExchange(ShuttleGui & S)
{
   DeepModelManager &manager = DeepModelManager::Get();

   S.StartVerticalLay(true);
   {
      mTools = safenew ManagerToolsPanel(S.GetParent(), this);
      // mTools->PopulateOrExchange(S);
      S.AddWindow(mTools);

      S.StartMultiColumn(2, wxEXPAND);
      {
         mScroller = S.StartScroller(wxVSCROLL | wxTAB_TRAVERSAL);
         {
         }
         S.EndScroller();
         wxSize size(cardPanel_w+50, detailedCardPanel_h);
         wxSize vsize(cardPanel_w+cardPanel_x_offset, 
                     detailedCardPanel_h);
         mScroller->SetVirtualSize(vsize);
         mScroller->SetSize(size); 
         mScroller->SetMinSize(size); 
         mScroller->SetMaxSize(size);
         mScroller->SetWindowStyle(wxBORDER_SIMPLE);
         mScroller->SetScrollRate(0, 10);

         // S.SetStretchyCol(1);
         mDetailedPanel = safenew DetailedModelCardPanel(
               S.GetParent(), wxID_ANY, manager.GetEmptyCard(), mEffect, this);

         S.AddWindow(mDetailedPanel);

      }
   }
   S.EndVerticalLay();

   FetchCards();
}

// TODO
void ModelManagerPanel::Clear()
{
   DeepModelManager &manager = DeepModelManager::Get();

   for (auto const& pair : mPanels)
   {
      ModelCardHolder card = pair.second->GetCard();
      if (manager.IsInstalling(card))
         manager.CancelInstall(card);
   } 

   // clean up panels
   mPanels.clear();
}

void ModelManagerPanel::AddCard(ModelCardHolder card)
{
   DeepModelManager &manager = DeepModelManager::Get();

   mScroller->EnableScrolling(true, true);
   std::string repoId = card->GetRepoID();
   mPanels[repoId] = std::make_unique<SimpleModelCardPanel>(mScroller, wxID_ANY,
                                                             card, mEffect, this);

   // if this is the first card we're adding, go ahead and select it
   if (mPanels.size() == 1)
   {
      mEffect->SetModel(card);
   }

   ShuttleGui S(mScroller, eIsCreating);
   S.AddWindow(mPanels[repoId].get(), wxEXPAND);
   // mPanels[repoId]->PopulateOrExchange(S);

   wxSizer *sizer = mScroller->GetSizer();
   if (sizer)
   {
      sizer->SetSizeHints(mScroller);
   }
   mScroller->FitInside();
   mScroller->Layout();
   mScroller->GetParent()->Layout();
}

CardFetchedCallback ModelManagerPanel::GetCardFetchedCallback()
{
   CardFetchedCallback onCardFetched = [this](bool success, ModelCardHolder card)
   {
      this->CallAfter(
         [this, success, card]()
         {
            if (success)
            {
               bool found = mPanels.find(card->GetRepoID()) != mPanels.end();
               bool effectTypeMatches = card->effect_type() == mEffect->GetDeepEffectID();
               if (!found && effectTypeMatches)
                  this->AddCard(card);
            }
         }
      );
   };

   return onCardFetched;
}

void ModelManagerPanel::FetchCards()
{
   DeepModelManager &manager = DeepModelManager::Get();

   CardFetchedCallback onCardFetched = GetCardFetchedCallback();

   CardFetchProgressCallback onCardFetchedProgress = [this](int64_t current, int64_t total)
   {
      this->CallAfter(
         [this, current, total]()
         {
            if (mTools)
               this->mTools->SetFetchProgress(current, total);
         }
      );
   };

   manager.FetchModelCards(onCardFetched, onCardFetchedProgress);
   manager.FetchLocalCards(onCardFetched);
}

void ModelManagerPanel::SetSelectedCard(ModelCardHolder card)
{
   // set all other card panels to disabled
   for (auto& pair : mPanels)
   {
      pair.second->SetModelStatus(ModelCardPanel::ModelStatus::Disabled);

      if (card)
      {
         if (pair.first == card->GetRepoID())
            pair.second->SetModelStatus(ModelCardPanel::ModelStatus::Enabled);
      }
   }

   // configure the detailed panel
   if (card)
   {
      mDetailedPanel->PopulateWithNewCard(card);
      mDetailedPanel->SetModelStatus(ModelCardPanel::ModelStatus::Enabled);
   }

}

// ManagerToolsPanel

ManagerToolsPanel::ManagerToolsPanel(wxWindow *parent, ModelManagerPanel *panel)
   : wxPanelWrapper((wxWindow *)parent, wxID_ANY, wxDefaultPosition, wxSize(managerPanel_w, 30))
{
   mManagerPanel = panel;
   mFetchStatus = nullptr;
   mAddRepoButton = nullptr;
   mExploreButton = nullptr;

   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);

   SetWindowStyle(wxBORDER_SIMPLE);
   // Fit();
   Layout();
   // Center();
   // SetMinSize(GetSize());
   Refresh();
}

void ManagerToolsPanel::PopulateOrExchange(ShuttleGui &S)
{
   S.StartHorizontalLay(wxLEFT, true);
   {
      mAddRepoButton = S.AddButton(XO("Add From HuggingFace"));
      mExploreButton = S.AddButton(XO("Explore Models"));
      mFetchStatus = S.AddVariableText(XO("Fetching models..."), 
                                 true, wxALIGN_CENTER_VERTICAL);
   }
   S.EndHorizontalLay();

   mAddRepoButton->Bind(wxEVT_BUTTON, &ManagerToolsPanel::OnAddRepo, this);
   mExploreButton->Bind(wxEVT_BUTTON, &ManagerToolsPanel::OnExplore, this);
}

void ManagerToolsPanel::OnAddRepo(wxCommandEvent & WXUNUSED(event))
{
   DeepModelManager &manager = DeepModelManager::Get();

   wxString msg = XO("Enter a HuggingFace Repo ID \n"
                    "For example: \"huggof/ConvTasNet-DAMP-Vocals\"\n").Translation();
   wxString caption = XO("AddRepo").Translation();
   wxTextEntryDialog dialog(this, msg, caption, wxEmptyString);

   if (dialog.ShowModal() == wxID_OK)
   {
      wxString repoId = dialog.GetValue();
      // wrap the card fetched callback 
      CardFetchedCallback onCardFetched(
         [this, repoId](bool success, ModelCardHolder card)
         {
            this->CallAfter(
               [this, success, card, repoId]()
               {
                  this->mManagerPanel->GetCardFetchedCallback()(success, card);
                  if (!success)
                  {
                     this->mManagerPanel->mEffect->Effect::MessageBox(
                        XO("An error occurred while fetching  %s from HuggingFace. "
                        "This model may be broken. If you are the model developer, "
                         "check the error log for more details.")
                           .Format(repoId)
                     );
                  }
               }
            );
         }
      );

      manager.FetchCard(repoId.ToStdString(), onCardFetched);
   }
}

void ManagerToolsPanel::SetFetchProgress(int64_t current, int64_t total)
{
   if (!mFetchStatus)
      return;

   if (total == 0)
   {
      wxString translated = XO("Error fetching models.").Translation();
      mFetchStatus->SetLabel(translated);
   }

   if (current < total)
   {
      // TODO: this should be a translatable string
      wxString translated = XO("Fetching %d out of %d")
            .Format((int)current, (int)total).Translation();
      mFetchStatus->SetLabel(translated);
   }

   if (current == total)
   {
      mFetchStatus->SetLabel(XO("Manager ready.").Translation());
   }
}

void ManagerToolsPanel::OnExplore(wxCommandEvent & WXUNUSED(event))
{
   ExploreHuggingFaceDialog dialog(mManagerPanel->GetParent(), mManagerPanel);
   dialog.ShowModal();
}
