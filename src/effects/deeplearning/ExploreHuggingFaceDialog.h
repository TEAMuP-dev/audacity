/**********************************************************************

   Audacity: A Digital Audio Editor
   Audacity(R) is copyright (c) 1999-2021 Audacity Team.
   License: GPL v2.  See License.txt.

   ModelManagerPanel.h
   Hugo Flores Garcia

******************************************************************/
/**

\class ExploreHuggingFaceDialog
\brief ExploreHuggingFaceDialog TODO

*/
/*******************************************************************/

#include "ModelManagerPanel.h"

class ExploreHuggingFaceDialog : public wxDialogWrapper
{
public:
   ExploreHuggingFaceDialog(wxWindow *parent, ModelManagerPanel *panel);
};