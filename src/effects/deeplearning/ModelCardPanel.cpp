/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.

   ModelCardPanel.cpp
   Hugo Flores Garcia

******************************************************************/

#include "EffectDeepLearning.h"
#include "ModelManagerPanel.h"
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

// DomainTagPanel

class DomainTagPanel : public wxPanelWrapper
{
public:
   DomainTagPanel(wxWindow *parent, const wxString &tag, const wxColour &color)
                  :wxPanelWrapper(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
   {
      TranslatableString name = Verbatim("%s").Format(tag);
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
      mModelName = S.AddVariableText(Verbatim("%s")
                                          .Format(mCard->name()),
                                       false, wxLEFT);
      mModelName->SetFont(wxFont(wxFontInfo().Bold()));

      // model size
      mModelSize = S.AddVariableText(Verbatim("[- MB]"));
      FetchModelSize();

   }
   S.EndMultiColumn();
   // S.EndHorizontalLay();

   // by {author}
   S.StartHorizontalLay(wxALIGN_LEFT, true);
   {
      S.AddVariableText(XC("by", "author of the model"));
      mModelAuthor = S.AddVariableText(Verbatim("%s")
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
                                       Verbatim("%s").Format(wxString(mCard->short_description())),
                                       false, wxLEFT);
   // S.EndStatic();
}

void ModelCardPanel::PopulateLongDescription(ShuttleGui &S)
{
   // model description
   S.StartStatic(Verbatim(""));
   mLongDescription = S.AddVariableText(
                                       Verbatim("%s").Format(wxString(mCard->long_description())),
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
      S.AddVariableText(Verbatim("%s")
                            .Format(mCard->effect_type()));

      S.AddVariableText(XO("Sample Rate: "))
          ->SetFont(wxFont(wxFontInfo().Bold()));
      S.AddVariableText(Verbatim("%d")
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
      S.AddVariableText(Verbatim("%s")
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

      bool installed = manager.IsInstalled(mCard);
      TranslatableString status = installed ? XO("installed") : XO("uninstalled");
      mInstallStatusText = S.AddVariableText(status, true);

      InstallStatus iStatus = installed ? InstallStatus::Installed : InstallStatus::Uninstalled;
      wxColour statusColor = mInstallStatusColors[iStatus];
      mInstallStatusText->SetForegroundColour(statusColor);

      // TODO: do translatable strings here from the begginign
      TranslatableString cmd = installed ? XO("Uninstall") : XO("Install");
      mInstallButton = S.AddButton(cmd);

      SetInstallStatus(installed ? InstallStatus::Installed : InstallStatus::Uninstalled);

      mSelectButton = S.AddButton(XC("Select", "model"));
      mSelectButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                        wxCommandEventHandler(ModelCardPanel::OnSelect), NULL, this);
   }
   S.EndVerticalLay();
}

void ModelCardPanel::PopulateMoreInfo(ShuttleGui &S)
{
   S.StartHorizontalLay(wxCENTER, true);
   {
      mMoreInfoButton = S.AddButton(XC("More Info", "model"));
      mMoreInfoButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                           wxCommandEventHandler(ModelCardPanel::OnMoreInfo), NULL, this);
   }
}

void ModelCardPanel::FetchModelSize()
{
   DeepModelManager &manager = DeepModelManager::Get();

   ModelSizeCallback onGetModelSize = [this](size_t size)
   {
      float sizeMB = static_cast<float>(size) / static_cast<float>(1024 * 1024);
      mModelSize->SetLabel(Verbatim("[%.1f MB]").Format(sizeMB).Translation());
   }; 

   manager.FetchModelSize(mCard, onGetModelSize);
}

void ModelCardPanel::SetInstallStatus(InstallStatus status)
{
   if (status == InstallStatus::Installed)
   {
      this->mInstallButton->SetLabel(XO("Uninstall").Translation());
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnUninstall), NULL, this);
      this->mInstallProgressGauge->Hide();
      this->mInstallStatusText->SetLabel(XO("installed").Translation());
   }
   else if (status == InstallStatus::Installing)
   {
      this->mInstallButton->SetLabel(XC("Cancel", "install").Translation());
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnCancelInstall), NULL, this);
      this->mInstallProgressGauge->Show();

      this->mInstallStatusText->SetLabel(XO("installing...").Translation());
   }
   else
   {
      this->mInstallButton->SetLabel(XO("Install").Translation());
      this->mInstallButton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, 
                                 wxCommandEventHandler(ModelCardPanel::OnInstall), NULL, this);
      this->mInstallProgressGauge->Hide();

      this->mInstallStatusText->SetLabel(XO("uninstalled").Translation());
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
   this->SetInstallStatus(InstallStatus::Uninstalled);

   mEffect->SetModel(nullptr);      
}

void ModelCardPanel::OnCancelInstall(wxCommandEvent &event)
{
   DeepModelManager &manager = DeepModelManager::Get();

   manager.CancelInstall(mCard);
   manager.Uninstall(mCard);
   SetInstallStatus(InstallStatus::Uninstalled);
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
                  this->SetInstallStatus(InstallStatus::Installed);
               else
               {
                  this->mEffect->Effect::MessageBox(
                     XO("An error ocurred while installing the model with Repo ID %s. ")
                        .Format(this->mCard->GetRepoID())
                  );
                  this->SetInstallStatus(InstallStatus::Uninstalled);
               }
            }
            else
            {
               this->SetInstallStatus(InstallStatus::Uninstalled);
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
      this->SetInstallStatus(InstallStatus::Installing);

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
   if (status == ModelStatus::Enabled)
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
      : ModelCardPanel(parent, id, card, effect, managerPanel, wxSize(cardPanel_w, cardPanel_h))
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
                       wxSize(detailedCardPanel_w, 
                              detailedCardPanel_h))
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
