/*
Miranda Database Tool
Copyright (C) 2001  Richard Hughes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "dbtool.h"

BOOL CALLBACK FileAccessDlgProc(HWND hdlg,UINT message,WPARAM wParam,LPARAM lParam);
BOOL CALLBACK ProgressDlgProc(HWND hdlg,UINT message,WPARAM wParam,LPARAM lParam);

BOOL CALLBACK CleaningDlgProc(HWND hdlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	BOOL bReturn;

	if(DoMyControlProcessing(hdlg,message,wParam,lParam,&bReturn)) return bReturn;
	switch(message) {
		case WM_INITDIALOG:
			CheckDlgButton(hdlg,IDC_ERASEHISTORY,opts.bEraseHistory);
			CheckDlgButton(hdlg,IDC_MARKREAD,opts.bMarkRead);
			return TRUE;
		case WZN_PAGECHANGING:
			opts.bEraseHistory=IsDlgButtonChecked(hdlg,IDC_ERASEHISTORY);
			opts.bMarkRead=IsDlgButtonChecked(hdlg,IDC_MARKREAD);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_BACK:
					SendMessage(GetParent(hdlg),WZM_GOTOPAGE,IDD_FILEACCESS,(LPARAM)FileAccessDlgProc);
					break;
				case IDOK:
					SendMessage(GetParent(hdlg),WZM_GOTOPAGE,IDD_PROGRESS,(LPARAM)ProgressDlgProc);
					break;
			}
			break;
	}
	return FALSE;
}