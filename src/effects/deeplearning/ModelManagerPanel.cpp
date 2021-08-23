/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelManagerPanel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "EffectDeepLearning.h"
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


#include "AllThemeResources.h"
#include "Theme.h"

#define MANAGERPANEL_WIDTH 1000
#define MODELCARDPANEL_WIDTH 600
#define MODELCARDPANEL_HEIGHT 150
#define DETAILEDMODELCARDPANEL_WIDTH 400
#define DETAILEDMODELCARDPANEL_HEIGHT 400

// ModelManagerPanel
// TODO: need to get rid of the unique ptrs to UI elements
ModelManagerPanel::ModelManagerPanel(wxWindow *parent, EffectDeepLearning *effect)
                                    : wxPanelWrapper(parent)
{
   mEffect = effect;
   mTools = nullptr;

   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
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
         mScroller = S.Style(wxVSCROLL | wxTAB_TRAVERSAL)
         .StartScroller();
         {
         }
         S.EndScroller();
         // TODO: this is a temporary hack. The scroller should
         // dynamicallyu adjust its size to fit the contents.
         wxSize size(MODELCARDPANEL_WIDTH+10, DETAILEDMODELCARDPANEL_HEIGHT);
         mScroller->SetVirtualSize(size);
         mScroller->SetSize(size); 
         mScroller->SetMinSize(size); 
         mScroller->SetMaxSize(size);

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
      pair.second->SetModelStatus(ModelCardPanel::ModelStatus::disabled);

      if (card)
      {
         if (pair.first == card->GetRepoID())
            pair.second->SetModelStatus(ModelCardPanel::ModelStatus::enabled);
      }
   }

   // configure the detailed panel
   if (card)
   {
      mDetailedPanel->PopulateWithNewCard(card);
      mDetailedPanel->SetModelStatus(ModelCardPanel::ModelStatus::enabled);
   }

}

// ManagerToolsPanel

ManagerToolsPanel::ManagerToolsPanel(wxWindow *parent, ModelManagerPanel *panel)
   : wxPanelWrapper((wxWindow *)parent, wxID_ANY, wxDefaultPosition, wxSize(MANAGERPANEL_WIDTH, 30))
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
      mFetchStatus->SetLabel(XO("Manager ready.").Translation());
   }
}

void ManagerToolsPanel::OnExplore(wxCommandEvent & WXUNUSED(event))
{
   ExploreDialog dialog = ExploreDialog(mManagerPanel->GetParent(), mManagerPanel);
   dialog.ShowModal();
}

// ExploreDialog

ExploreDialog::ExploreDialog(wxWindow *parent, ModelManagerPanel *panel)
                           : wxDialogWrapper(parent, wxID_ANY, XO("Explore Models"))
{
   ShuttleGui S(this, eIsCreating);
   S.StartStatic(XO(""), true);
   {
      S.AddFixedText(
         XO(
            "Deep learning models for Audacity are contributed by the open-source \n"
            "community and are hosted in HuggingFace. You can explore models for Audacity\n"
            "by clicking the following link: "
         )
      );

      S.AddWindow(
         safenew wxHyperlinkCtrl(
            S.GetParent(), wxID_ANY, 
            "https://huggingface.co/models?filter=audacity",
            "https://huggingface.co/models?filter=audacity"
         ) 
      );

      S.AddFixedText(
         XO(
            "To add a new model to your local collection, use the \n"
            "\"Add From HuggingFace\" button."
         )
      );
   }

   Fit();
   Layout();
   Center();
   SetMinSize(GetSize());
   Refresh();
}

// DomainTagPanel

class DomainTagPanel : public wxPanelWrapper
{
public:
   DomainTagPanel(wxWindow *parent, const wxString &tag, const wxColour &color)
                  :wxPanelWrapper(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
   {
      TranslatableString name = XO("%s").Format(tag);
      SetLabel(name);

      SetMaxSize(wxSize(90, 25));
      // SetWindowStyle(wxBORDER_SIMPLE);
      SetBackgroundColour(color);

      ShuttleGui S(this, eIsCreating);
      wxStaticText *txt = S.AddVariableText(name, true);
      SetVirtualSize(txt->GetSize());

      txt->SetBackgroundColour(color);
      wxFont font = txt->GetFont();
      font.SetPointSize(11);
      txt->SetFont(font);

      Refresh();
      
      Fit();
      Layout();
   }
};

// ModelCardPanel

ModelCardPanel::ModelCardPanel(wxWindow *parent, wxWindowID winid, ModelCardHolder card, 
                              EffectDeepLearning *effect, ModelManagerPanel *managerPanel, const wxSize& size)
    : wxPanelWrapper(parent, winid, wxDefaultPosition, size, wxBORDER_SIMPLE), 
      mModelName(nullptr),
      mModelSize(nullptr),
      mModelAuthor(nullptr), 
      mShortDescription(nullptr),  
      mLongDescription(nullptr),  
      mInstallButton(nullptr), 
      mInstallStatusText(nullptr),
      mInstallProgressGauge(nullptr),
      mSelectButton(nullptr), 
      mMoreInfoButton(nullptr)
{
   SetLabel(XO("Model Card"));
   SetName(XO("Model Card"));

   mParent = parent;
   mCard = card;
   mEffect = effect;
   mManagerPanel = managerPanel;
   // TransferDataToWindow();
}

void ModelCardPanel::Populate()
{
   SetAutoLayout(true);
   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
   Fit();
   Center();
   Layout();
}

void ModelCardPanel::PopulateWithNewCard(ModelCardHolder card)
{
   DestroyChildren();
   SetSizer(nullptr);

   mCard = card;
   Populate();
   Refresh();
   mParent->Fit();
   mParent->Refresh();
   mParent->Layout();
   mParent->GetParent()->Fit();
   mParent->GetParent()->Refresh();
   mParent->GetParent()->Layout();

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

void ModelCardPanel::PopulateDomainTags(ShuttleGui &S)
{
   S.StartHorizontalLay(wxALIGN_LEFT | wxALIGN_TOP, true);
   {
      for (auto &tag : mCard->domain_tags())
      {
         S.AddWindow(
            safenew DomainTagPanel(this, tag, mTagColors[tag])
         );
      }
   }
   S.EndHorizontalLay();
}

void ModelCardPanel::PopulateShortDescription(ShuttleGui &S)
{
   // model description
   // S.StartStatic(XO("Description"));
   S.SetBorder(10);
   mShortDescription = S.AddVariableText(
                                       XO("%s").Format(wxString(mCard->short_description())),
                                       false, wxLEFT);
   S.SetBorder(10);
   // S.EndStatic();
}

void ModelCardPanel::PopulateLongDescription(ShuttleGui &S)
{
   // model description
   S.StartStatic(XO(""));
   mLongDescription = S.AddVariableText(
                                       XO("%s").Format(wxString(mCard->long_description())),
                                       false, wxLEFT, 
                                       GetSize().GetWidth() - 30);
   S.EndStatic();
}

void ModelCardPanel::PopulateMetadata(ShuttleGui &S)
{
   S.StartMultiColumn(2, wxALIGN_LEFT);
   {
      S.AddVariableText(XO("Effect: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%s")
                            .Format(mCard->effect_type()));

      S.AddVariableText(XO("Sample Rate: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%d")
                            .Format(mCard->sample_rate()));

      std::string tagString;
      for (auto &tag : mCard->tags())
      {
         if (!tagString.empty())
            tagString = tagString +  ", " + tag;
         else
            tagString = tag;
      }
      
      S.AddVariableText(XO("Tags: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(XO("%s")
                            .Format(tagString));
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

      bool installed = manager.IsInstalled(mCard);
      TranslatableString status = installed ? XO("installed") : XO("uninstalled");
      mInstallStatusText = S.AddVariableText(status, true);

      InstallStatus iStatus = installed ? InstallStatus::installed : InstallStatus::uninstalled;
      wxColour statusColor = mInstallStatusColors[iStatus];
      mInstallStatusText->SetForegroundColour(statusColor);

      // TODO: do translatable strings here from the begginign
      TranslatableString cmd = installed ? XO("Uninstall") : XO("Install");
      mInstallButton = S.AddButton(cmd);

      SetInstallStatus(installed ? InstallStatus::installed : InstallStatus::uninstalled);

      mSelectButton = S.AddButton(XO("Select"));
      mSelectButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                        wxCommandEventHandler(ModelCardPanel::OnSelect), NULL, this);
   }
   S.EndVerticalLay();
}

void ModelCardPanel::PopulateMoreInfo(ShuttleGui &S)
{
   S.StartHorizontalLay(wxCENTER, true);
   {
      mMoreInfoButton = S.AddButton(XO("More Info"));
      mMoreInfoButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                           wxCommandEventHandler(ModelCardPanel::OnMoreInfo), NULL, this);
   }
}

void ModelCardPanel::FetchModelSize()
{
   DeepModelManager &manager = DeepModelManager::Get();

   ModelSizeCallback onGetModelSize = [this](size_t size)
   {
      float sizeMB = (float)size / (float)(1024 * 1024);
      mModelSize->SetLabel(XO("[%.1f MB]").Format(sizeMB).Translation());
   }; 

   manager.FetchModelSize(mCard, onGetModelSize);
}

void ModelCardPanel::SetInstallStatus(InstallStatus status)
{
   if (status == InstallStatus::installed)
   {
      this->mInstallButton->SetLabel("Uninstall");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnUninstall), NULL, this);
      this->mInstallProgressGauge->Hide();
      this->mInstallStatusText->SetLabel("installed");
   }
   else if (status == InstallStatus::installing)
   {
      this->mInstallButton->SetLabel("Cancel");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnCancelInstall), NULL, this);
      this->mInstallProgressGauge->Show();

      this->mInstallStatusText->SetLabel("installing...");
   }
   else
   {
      this->mInstallButton->SetLabel("Install");
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnInstall), NULL, this);
      this->mInstallProgressGauge->Hide();

      this->mInstallStatusText->SetLabel("uninstalled");
   }
   
   wxColour statusColor = mInstallStatusColors[status];
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

   manager.CancelInstall(mCard);
   manager.Uninstall(mCard);
   SetInstallStatus(InstallStatus::uninstalled);
}

// TODO: this is good, but still hangs on "installing"
// even when you turn off the connection
void ModelCardPanel::OnInstall(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();

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

void ModelCardPanel::OnSelect(wxCommandEvent &event)
{  
   mEffect->SetModel(mCard);
}

void ModelCardPanel::OnEnable(wxCommandEvent &event)
{
   mEffect->SetModel(mCard);
}

void ModelCardPanel::OnMoreInfo(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();
   wxString url = wxString(manager.GetMoreInfoURL(mCard));
   wxLaunchDefaultBrowser(url);
}

void ModelCardPanel::SetModelStatus(ModelStatus status)
{
   if (status == ModelStatus::enabled)
   {
      SetBackgroundColour(
         theTheme.Colour(clrMediumSelected)
      );
   }
   else
   {
      SetBackgroundColour(
         theTheme.Colour(clrMedium)
      );
   }

   // Fit();
   Refresh();
}

void ModelCardPanel::OnClick(wxMouseEvent &event)
{
   mEffect->SetModel(mCard);
}

// SimpleModelCardPanel

SimpleModelCardPanel::SimpleModelCardPanel(wxWindow *parent, wxWindowID id,
                           ModelCardHolder card, EffectDeepLearning *effect, ModelManagerPanel *managerPanel)
      : ModelCardPanel(parent, id, card, effect, managerPanel, wxSize(MODELCARDPANEL_WIDTH, MODELCARDPANEL_HEIGHT))
{
   Populate();
}


void SimpleModelCardPanel::PopulateOrExchange(ShuttleGui &S)
{
   // the layout is actually 2 columns,
   // but we add a small space in the middle, which takes up a column
   S.StartMultiColumn(3, wxEXPAND);
   {
      // left column:
      // repo name, repo author, model tags, and model description
      S.SetStretchyCol(0);
      S.StartVerticalLay(wxALIGN_LEFT, true);
      {
         PopulateNameAndAuthor(S);
         PopulateDomainTags(S);
         PopulateShortDescription(S);
      }
      S.EndVerticalLay();

      // dead space (center column)
      S.AddSpace(5, 0);

      S.StartMultiColumn(1);
      {
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
}

// DetailedModelCardPanel

DetailedModelCardPanel::DetailedModelCardPanel(wxWindow *parent, wxWindowID id,
                                               ModelCardHolder card, EffectDeepLearning *effect, 
                                                ModelManagerPanel *managerPanel)
      : ModelCardPanel(parent, id, card, effect, managerPanel,
                       wxSize(DETAILEDMODELCARDPANEL_WIDTH, 
                              DETAILEDMODELCARDPANEL_HEIGHT))
{
   if (card)
      Populate();
}

void DetailedModelCardPanel::PopulateOrExchange(ShuttleGui &S)
{
   S.StartVerticalLay(wxALIGN_LEFT, true);
   {
      PopulateNameAndAuthor(S);
      PopulateDomainTags(S);
      PopulateLongDescription(S);
      PopulateMetadata(S);
      PopulateMoreInfo(S);
   }
}
