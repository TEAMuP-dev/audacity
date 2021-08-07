/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelManagerPanel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "EffectDeepLearning.h"

#include "Shuttle.h"
#include "ShuttleGui.h"

#include <wx/scrolwin.h>
#include <wx/range.h>
#include <wx/gauge.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>
#include <wx/stattext.h>

// ModelManagerPanel
// TODO: need to get rid of the unique ptrs to UI elements
ModelManagerPanel::ModelManagerPanel(wxWindow *parent, EffectDeepLearning *effect)
{
   mEffect = effect;
   mTools = NULL;
}

void ModelManagerPanel::PopulateOrExchange(ShuttleGui & S)
{
   S.StartVerticalLay(true);
   {
      if (!mTools) 
         mTools = safenew ManagerToolsPanel(S.GetParent(), this);
      
      mTools->PopulateOrExchange(S);

      mScroller = S.Style(wxVSCROLL | wxTAB_TRAVERSAL)
         .StartScroller();
      {
      }
      S.EndScroller();
      // TODO: this is a temporary hack. The scroller should
      // dynamicallyu adjust its size to fit the contents.
      mScroller->SetVirtualSize(wxSize(1000, 400));
      mScroller->SetMinSize(wxSize(1000, 400)); 

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
   mScroller->EnableScrolling(true, true);
   std::string repoId = card->GetRepoID();
   mPanels[repoId] = std::make_unique<ModelCardPanel>(mScroller, wxID_ANY, card, mEffect);

   ShuttleGui S(mScroller, eIsCreating);
   mPanels[repoId]->PopulateOrExchange(S);

   wxSizer *sizer = mScroller->GetSizer();
   if (sizer)
   {
      sizer->SetSizeHints(mScroller);
   }
   mScroller->FitInside();
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

   // TODO: this needs a progress gauge
   // Should go on the top panel
   manager.FetchModelCards(onCardFetched, onCardFetchedProgress);
}

// ManagerToolsPanel

ManagerToolsPanel::ManagerToolsPanel(wxWindow *parent, ModelManagerPanel *panel)
   : wxPanelWrapper((wxWindow *)parent, wxID_ANY)
{
   mManagerPanel = panel;
   mFetchStatus = nullptr;
   mAddRepoButton = nullptr;
}

void ManagerToolsPanel::PopulateOrExchange(ShuttleGui &S)
{
   S.StartStatic(XO("Tools"));
   S.StartHorizontalLay(wxLEFT, true);
   {
      mFetchStatus = S.AddVariableText(XO("Fetching models..."), false);

      mAddRepoButton = S.AddButton(XO("Add HuggingFace Repo"));
      mAddRepoButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
                              wxCommandEventHandler(ManagerToolsPanel::OnAddRepo), NULL, this);
   }
   S.EndHorizontalLay();
   S.EndStatic();
}

void ManagerToolsPanel::OnAddRepo(wxCommandEvent & WXUNUSED(event))
{
   DeepModelManager &manager = DeepModelManager::Get();

   CardFetchedCallback onCardFetched = mManagerPanel->GetCardFetchedCallback();

   wxString msg = XO("Enter a HuggingFace Repo ID \n"
                    "For example: \"huggof/ConvTasNet-DAMP-Vocals\"\n").Translation();
   wxString caption = XO("AddRepo").Translation();
   wxTextEntryDialog dialog(this, msg, caption, wxEmptyString);

   if (dialog.ShowModal() == wxID_OK)
   {
      std::string repoId = dialog.GetValue().ToStdString();
      manager.FetchCard(repoId, onCardFetched);
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
      mFetchStatus->SetLabel(XO("manager ready").Translation());
   }
}

// ModelCardPanel

ModelCardPanel::ModelCardPanel(wxWindow *parent, wxWindowID winid, ModelCardHolder card, 
                              EffectDeepLearning *effect)
    : wxPanelWrapper(parent, winid, wxDefaultPosition )
{
   SetLabel(XO("Model Card"));
   SetName(XO("Model Card"));

   mParent = parent;
   mCard = card;
   mEffect = effect;

   // this->SetBackgroundColour(wxColour())
   SetAutoLayout(true);

   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);

   TransferDataToWindow();
}

bool ModelCardPanel::TransferDataToWindow()
{
   return true;
}

bool ModelCardPanel::TransferDataFromWindow()
{
   ShuttleGui S(this, eIsSavingToPrefs);
   PopulateOrExchange(S);

   gPrefs->Flush();

   return true;
}

void ModelCardPanel::PopulateNameAndAuthor(ShuttleGui &S)
{
   // {repo-name} [model size]
   // S.StartHorizontalLay(wxALIGN_LEFT, true);
   S.StartMultiColumn(2, wxALIGN_LEFT);
   {
      mModelName = S.AddVariableText(XO("%s")
                                          .Format(mCard->name()),
                                       false, wxLEFT);
      mModelName->SetFont(wxFont(wxFontInfo().Bold()));

      // model size
      mModelSize = S.AddVariableText(XO("[- MB]"));
      FetchModelSize();

   }
   S.EndMultiColumn();
   // S.EndHorizontalLay();

   // by {author}
   S.StartHorizontalLay(wxALIGN_LEFT, true);
   {
      S.AddVariableText(XO("by"));
      mModelAuthor = S.AddVariableText(XO("%s")
                                              .Format(mCard->author()));
      mModelAuthor->SetFont(wxFont(wxFontInfo().Bold()));
   }
   S.EndHorizontalLay();
}

void ModelCardPanel::PopulateDescription(ShuttleGui &S)
{
   // model description
   S.StartStatic(XO("Description"));
   mModelDescription = S.AddVariableText(
                                       XO("%s").Format(wxString(mCard->short_description())),
                                       false, wxLEFT);
   S.EndStatic();
}

void ModelCardPanel::PopulateMetadata(ShuttleGui &S)
{
   S.StartMultiColumn(2, wxCENTER);
   {
      S.AddVariableText(XO("Effect: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%s")
                            .Format(mCard->effect_type()));

      S.AddVariableText(XO("Domain: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%s")
                            .Format(" NONE ")); // FIXME

      S.AddVariableText(XO("Sample Rate: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%d")
                            .Format(mCard->sample_rate()));
   }
   S.EndMultiColumn();
}

void ModelCardPanel::PopulateInstallCtrls(ShuttleGui &S)
{
   DeepModelManager &manager = DeepModelManager::Get();

   S.StartVerticalLay(wxCENTER, true);
   {
      // add a progress gauge for downloads, but hide it
      mInstallProgressGauge = safenew wxGauge(S.GetParent(), wxID_ANY, 100); // TODO:  sizing
      mInstallProgressGauge->SetSize(wxSize(80, 20));
      S.AddWindow(mInstallProgressGauge);
      mInstallProgressGauge->Show();

      S.StartHorizontalLay(wxCENTER, true);
      {
         bool installed = manager.IsInstalled(mCard);
         TranslatableString status = installed ? XO("installed") : XO("uninstalled");
         mInstallStatusText = S.AddVariableText(status);

         wxColour statusColor = installed ? *wxGREEN : *wxRED;
         mInstallStatusText->SetForegroundColour(statusColor);

         // TODO: do translatable strings here from the begginign
         TranslatableString cmd = installed ? XO("Uninstall") : XO("Install");
         mInstallButton = S.AddButton(cmd);

         SetInstallStatus(installed ? InstallStatus::installed : InstallStatus::uninstalled);
      }
      S.EndHorizontalLay();

      mEnableButton = S.AddButton(XO("Enable"));
      mEnableButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                             wxCommandEventHandler(ModelCardPanel::OnEnable), NULL, this);

      // mInstallProgressGauge->Hide();
   }
   S.EndVerticalLay();
}

void ModelCardPanel::PopulateOrExchange(ShuttleGui &S)
{
   DeepModelManager &manager = DeepModelManager::Get();

   // the layout is actually 2 columns,
   // but we add a small space in the middle, which takes up a column
   S.StartStatic(XO(""), 1);
   S.StartMultiColumn(3, wxEXPAND);
   {
      // left column:
      // repo name, repo author, and model description
      S.SetStretchyCol(0);
      S.StartVerticalLay(wxALIGN_LEFT, true);
      {
         PopulateNameAndAuthor(S);

         PopulateDescription(S);
      }
      S.EndVerticalLay();

      // dead space (center column)
      S.AddSpace(5, 0);

      S.StartMultiColumn(1);
      {
         // top: other model metadata
         S.StartVerticalLay(wxALIGN_TOP, false);
         {
            PopulateMetadata(S);
         }
         S.EndVerticalLay();

         // bottom: install and uninstall controls
         S.StartVerticalLay(wxALIGN_BOTTOM, false);
         {
            S.StartHorizontalLay(wxALIGN_RIGHT);
            {
               PopulateInstallCtrls(S);
            }
            S.EndHorizontalLay();
         }
         S.EndVerticalLay();
      }
      S.EndVerticalLay();
   }
   S.EndMultiColumn();
   S.EndStatic();
}

void ModelCardPanel::FetchModelSize()
{
   DeepModelManager &manager = DeepModelManager::Get();

   ModelSizeCallback onGetModelSize = [this](size_t size)
   {
      float sizeMB = (float)size / (float)(1024 * 1024);
      mModelSize->SetLabel(XO("[%.1f MB]").Format(sizeMB).Translation());
   }; 

   manager.FetchModelSize(mCard->GetRepoID(), onGetModelSize);
}

void ModelCardPanel::SetInstallStatus(InstallStatus status)
{
   wxColour statusColor;
   if (status == InstallStatus::installed)
   {
      this->mInstallButton->SetLabel("Uninstall");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnUninstall), NULL, this);
      this->mInstallProgressGauge->Hide();
      this->mInstallStatusText->SetLabel("installed");

      statusColor = *wxGREEN;
   }
   else if (status == InstallStatus::installing)
   {
      this->mInstallButton->SetLabel("Cancel");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnCancelInstall), NULL, this);
      this->mInstallProgressGauge->Show();

      this->mInstallStatusText->SetLabel("installing...");
      statusColor = *wxBLACK; 
   }
   else
   {
      this->mInstallButton->SetLabel("Install");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnInstall), NULL, this);
      this->mInstallProgressGauge->Hide();

      this->mInstallStatusText->SetLabel("uninstalled");
      statusColor = *wxRED;
   }

   mInstallStatusText->SetForegroundColour(statusColor);
   this->Layout();
   this->GetParent()->Layout();
}

void ModelCardPanel::OnUninstall(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();

   // TODO: show a prompt to confirm?
   manager.Uninstall(mCard);
   this->SetInstallStatus(InstallStatus::uninstalled);

   mEffect->SetModel(nullptr);      
}

void ModelCardPanel::OnCancelInstall(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();

   // TODO: cleanup!!
   manager.CancelInstall(mCard);
   manager.Uninstall(mCard);
   SetInstallStatus(InstallStatus::uninstalled);
}

// TODO: this is good, but still hangs on "installing"
// even when you turn off the connection
void ModelCardPanel::OnInstall(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();

   // TODO: what if the user closes the window while this is downloading?
   // should the destructor of something make sure that no installation was left halfway thru?
   // what does the network manager do in that case? 
   ProgressCallback onProgress([this, &manager](int64_t current, int64_t expected)
   {
      this->CallAfter(
         [current, expected, this, &manager]()
         {
            if (expected > 0)
            {
               // update the progress gauge
               this->mInstallProgressGauge->SetRange(expected);
               this->mInstallProgressGauge->SetValue(current);
            }
            else
               this->mInstallProgressGauge->Pulse();
         });
   });

   CompletionHandler onInstallDone([this, &manager](int httpCode, std::string responseBody)
   {  
      this->CallAfter(
         [this, httpCode, &manager]()
         {
            if (httpCode == 200 || httpCode == 302)
            {
               // check if install succeeded
               if (manager.IsInstalled(this->mCard))
                  this->SetInstallStatus(InstallStatus::installed);
               else
               {
                  this->mEffect->Effect::MessageBox(
                     XO("An error ocurred while installing the model with Repo ID %s. ")
                        .Format(this->mCard->GetRepoID())
                  );
                  this->SetInstallStatus(InstallStatus::uninstalled);
               }
            }
            else
            {
               this->SetInstallStatus(InstallStatus::uninstalled);
               this->mEffect->Effect::MessageBox(
                  XO("An error ocurred while downloading the model with Repo ID %s. \n"
                     "HTTP Code: %d").Format(this->mCard->GetRepoID(), httpCode)
               );
            }
         }
      ); 
   });

   if (!manager.IsInstalled(mCard))
   {
      this->SetInstallStatus(InstallStatus::installing);

      // TODO: since this is done in another thread, how do I catch an error, like
      // losing the connection in the middle of a download? 
      manager.Install(mCard, std::move(onProgress), std::move(onInstallDone));
   }
}

void ModelCardPanel::OnEnable(wxCommandEvent &event)
{
   auto &manager = DeepModelManager::Get();
   mEffect->SetModel(mCard);
}