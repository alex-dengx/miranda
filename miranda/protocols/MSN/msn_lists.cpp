/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol.
Copyright (c) 2006-2008 Boris Krasnovskiy.
Copyright (c) 2003-2005 George Hazan.
Copyright (c) 2002-2003 Richard Hughes (original version).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "msn_global.h"
#include "msn_proto.h"
#include "sdk/m_smileyadd.h"

void CMsnProto::Lists_Init(void)
{
	InitializeCriticalSection( &csLists );
}

void CMsnProto::Lists_Uninit(void)
{
	Lists_Wipe();
	DeleteCriticalSection( &csLists );
}

void  CMsnProto::Lists_Wipe( void )
{
	EnterCriticalSection( &csLists );
	contList.destroy();
	LeaveCriticalSection( &csLists );
}

bool CMsnProto::Lists_IsInList( int list, const char* email )
{
	EnterCriticalSection(&csLists);
	
	MsnContact* p = contList.find((MsnContact*)&email);
	bool res = p != NULL;
	if (res && list != -1)
		res &= ((p->list & list) == list);

	LeaveCriticalSection(&csLists);
	return res;
}

int CMsnProto::Lists_GetMask( const char* email )
{
	EnterCriticalSection( &csLists );

	MsnContact* p = contList.find((MsnContact*)&email);
	int res = p ? p->list : 0;

	LeaveCriticalSection( &csLists );
	return res;
}

int CMsnProto::Lists_GetNetId( const char* email )
{
	if (email[0] == 0) return NETID_UNKNOWN;

	EnterCriticalSection( &csLists );

	MsnContact* p = contList.find((MsnContact*)&email);
	int res = p ? p->netId : NETID_UNKNOWN;

	LeaveCriticalSection( &csLists );
	return res;
}

int CMsnProto::Lists_Add(int list, int netId, const char* email)
{
	EnterCriticalSection(&csLists);

	MsnContact* p = contList.find((MsnContact*)&email);
	if ( p == NULL )
	{
		p = new MsnContact;
		p->list = list;
		p->netId = netId;
		p->email = mir_strdup(email);
		contList.insert(p);
	}
	else
		p->list |= list;
	int result = p->list;

	LeaveCriticalSection( &csLists );
	return result;
}

void  CMsnProto::Lists_Remove( int list, const char* email )
{
	EnterCriticalSection( &csLists );
	int i = contList.getIndex((MsnContact*)&email);
	if ( i != -1 ) 
	{
		MsnContact& p = contList[i];
		p.list &= ~list;
		if (p.list == 0) contList.remove(i);
	}
	LeaveCriticalSection( &csLists );
}


void CMsnProto::MSN_CleanupLists(void)
{
	CallService(MS_CLIST_GROUPCREATE, 0, (LPARAM)TranslateT("Non IM Contacts"));

//	EnterCriticalSection(&csLists);
	for (int i=contList.getCount(); i--; )
	{
		MsnContact& p = contList[i];

		if ((p.list & (LIST_FL | LIST_RL)) == 0 && (p.list & (LIST_AL | LIST_BL)) != 0 && p.netId != NETID_LCS) 
		{
			MSN_SharingAddDelMember(p.email, p.list, p.netId, "DeleteMember");
			p.list &= ~(LIST_AL | LIST_BL);

			if (p.list == 0) 
			{
				contList.remove(i);
				continue;
			}
		}

		HANDLE hContact = MSN_HContactFromEmail(p.email, p.email, true, false);
		MSN_SetContactDb(hContact, p.email);
		if (p.list & LIST_PL)
		{
			if (p.list & (LIST_AL | LIST_BL))
				MSN_AddUser( hContact, p.email, p.netId, LIST_PL + LIST_REMOVE );
			else
				MSN_AddAuthRequest( hContact, p.email, p.email );
		}

		if (p.list == LIST_RL)
			MSN_AddAuthRequest( hContact, p.email, p.email );
	}
//	LeaveCriticalSection(&csLists);

	for (HANDLE hContact = (HANDLE)MSN_CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		 hContact != NULL; 
	     hContact = (HANDLE)MSN_CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM)hContact, 0)) 
	{
		if (!MSN_IsMyContact(hContact)) continue;

		char szEmail[MSN_MAX_EMAIL_LEN];
		if (getStaticString(hContact, "e-mail", szEmail, sizeof(szEmail)) == 0 && Lists_IsInList(-1, szEmail))
		{
			const int mask = Lists_GetMask(szEmail);
			if (mask & LIST_FL)
			{
				char path[MAX_PATH];
				MSN_GetCustomSmileyFileName(hContact, path, sizeof(path), "", 0);
				if (path[0])
				{
					SMADD_CONT cont;
					cont.cbSize = sizeof(SMADD_CONT);
					cont.hContact = hContact;
					cont.type = 0;
					cont.path = mir_a2t(path);

					MSN_CallService(MS_SMILEYADD_LOADCONTACTSMILEYS, 0, (LPARAM)&cont);
					mir_free(cont.path);
				}
			}
			if (mask != 0) continue;
		}

		MSN_CallService(MS_DB_CONTACT_DELETE, (WPARAM)hContact, 0);
	}
}

void CMsnProto::MSN_CreateContList(void)
{
	bool *used = (bool*)mir_calloc(contList.getCount()*sizeof(bool));

	char cxml[8192]; 
	size_t sz;

	sz = mir_snprintf(cxml , sizeof(cxml), "<ml l=\"1\">");
		
	EnterCriticalSection(&csLists);

	for ( int i=0; i < contList.getCount(); i++ )
	{
		if (used[i]) continue;

		const char* lastds = strchr(contList[i].email, '@');
		bool newdom = true;

		for ( int j=0; j < contList.getCount(); j++ )
		{
			if (used[j]) continue;
			
			const MsnContact& C = contList[j];
			
			if (C.list == LIST_RL || C.netId == NETID_EMAIL)
			{
				used[j] = true;
				continue;
			}

			const char* dom = strchr(C.email, '@');
			if (dom == NULL && lastds == NULL)
			{
				if (sz == 0) sz = mir_snprintf(cxml+sz, sizeof(cxml), "<ml l=\"1\">");
				if (newdom)
				{
					sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, "<t>");
					newdom = false;
				}

				sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, "<c n=\"%s\" l=\"%d\"/>", C.email, C.list & ~LIST_RL);
				used[j] = true;
			}
			else if (dom != NULL && lastds != NULL && _stricmp(lastds, dom) == 0)
			{
				if (sz == 0) sz = mir_snprintf(cxml, sizeof(cxml), "<ml l=\"1\">");
				if (newdom)
				{
					sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, "<d n=\"%s\">", lastds+1);
					newdom = false;
				}

				*(char*)dom = 0;
				sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, "<c n=\"%s\" l=\"%d\" t=\"%d\"/>", C.email, C.list & ~LIST_RL, C.netId);
				*(char*)dom = '@';
				used[j] = true;
			}

			if (used[j] && sz > 7400)
			{ 
				sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, "</%c></ml>", lastds ? 'd' : 't' );
				msnNsThread->sendPacket("ADL", "%d\r\n%s", sz, cxml);
				sz = 0;
				newdom = true;
			}
		}
		if (!newdom) sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz, lastds ? "</d>" : "</t>" );
	}
	LeaveCriticalSection(&csLists);

	if (sz) 
	{
		sz += mir_snprintf(cxml+sz, sizeof(cxml)-sz,  "</ml>" );
		msnNsThread->sendPacket("ADL", "%d\r\n%s", sz, cxml);
	}

	mir_free(used);
}

/////////////////////////////////////////////////////////////////////////////////////////
// MSN Server List Manager dialog procedure

static void ResetListOptions(HWND hwndList, CMsnProto* proto)
{
	SendMessage(hwndList,CLM_SETBKBITMAP,0,(LPARAM)(HBITMAP)NULL);
	SendMessage(hwndList,CLM_SETBKCOLOR,GetSysColor(COLOR_WINDOW),0);
	SendMessage(hwndList,CLM_SETGREYOUTFLAGS,0,0);
	SendMessage(hwndList,CLM_SETLEFTMARGIN,2,0);
	SendMessage(hwndList,CLM_SETINDENT,10,0);

	for(int i=0; i<=FONTID_MAX; i++)
		SendMessage(hwndList,CLM_SETTEXTCOLOR,i,GetSysColor(COLOR_WINDOWTEXT));

	SetWindowLong(hwndList,GWL_STYLE,GetWindowLong(hwndList,GWL_STYLE)|CLS_SHOWHIDDEN);
}

static void SetAllContactIcons( HWND hwndList, CMsnProto* proto )
{
	for ( HANDLE hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
		hContact != NULL; 
		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 )) 
	{
		HANDLE hItem = ( HANDLE )SendMessage( hwndList, CLM_FINDCONTACT, ( WPARAM )hContact, 0 );
		if ( hItem == NULL ) continue;

		if ( !proto->MSN_IsMyContact( hContact )) {
			SendMessage( hwndList, CLM_DELETEITEM, ( WPARAM )hItem, 0 );
			continue;
		}

		char szEmail[ MSN_MAX_EMAIL_LEN ];
		if ( proto->getStaticString( hContact, "e-mail", szEmail, sizeof( szEmail ))) {
			SendMessage( hwndList, CLM_DELETEITEM, ( WPARAM )hItem, 0 );
			continue;
		}

		DWORD dwMask = proto->Lists_GetMask( szEmail );
		if ( SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(0,0)) == 0xFF )
			SendMessage( hwndList, CLM_SETEXTRAIMAGE,( WPARAM )hItem, MAKELPARAM(0,( dwMask & LIST_FL )?1:0));
		if ( SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(1,0)) == 0xFF )
			SendMessage( hwndList, CLM_SETEXTRAIMAGE,( WPARAM )hItem, MAKELPARAM(1,( dwMask & LIST_AL )?2:0));
		if ( SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(2,0)) == 0xFF )
			SendMessage( hwndList, CLM_SETEXTRAIMAGE,( WPARAM )hItem, MAKELPARAM(2,( dwMask & LIST_BL )?3:0));
		if ( SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(3,0)) == 0xFF )
			SendMessage( hwndList, CLM_SETEXTRAIMAGE,( WPARAM )hItem, MAKELPARAM(3,( dwMask & LIST_RL )?4:0));
	}
}

static void SaveListItem( HANDLE hContact, const char* szEmail, int list, int iPrevValue, int iNewValue, CMsnProto* proto )
{
	if ( iPrevValue == iNewValue )
		return;

	if ( iNewValue == 0 )
		list += LIST_REMOVE;

	proto->MSN_AddUser( hContact, szEmail, proto->Lists_GetNetId( szEmail ), list );
}

static void SaveSettings( HWND hwndList, CMsnProto* proto )
{
	for ( HANDLE hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
		hContact != NULL; 
		hContact = ( HANDLE )MSN_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 )) 
	{
		HANDLE hItem = ( HANDLE )SendMessage( hwndList, CLM_FINDCONTACT, ( WPARAM )hContact, 0 );
		if ( hItem == NULL ) continue;

		if ( !proto->MSN_IsMyContact( hContact )) continue;

		char szEmail[ MSN_MAX_EMAIL_LEN ];
		if ( proto->getStaticString( hContact, "e-mail", szEmail, sizeof( szEmail ))) continue;

		int dwMask = proto->Lists_GetMask( szEmail );
		SaveListItem( hContact, szEmail, LIST_FL, ( dwMask & LIST_FL )?1:0, SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(0,0)), proto);
		SaveListItem( hContact, szEmail, LIST_AL, ( dwMask & LIST_AL )?2:0, SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(1,0)), proto);
		SaveListItem( hContact, szEmail, LIST_BL, ( dwMask & LIST_BL )?3:0, SendMessage( hwndList, CLM_GETEXTRAIMAGE, ( WPARAM )hItem, MAKELPARAM(2,0)), proto);

		proto->MSN_SetContactDb(hContact, szEmail );
	}
}

INT_PTR CALLBACK DlgProcMsnServLists(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch ( msg ) {
	case WM_INITDIALOG:
		TranslateDialogDefault( hwndDlg );
		{	
			SetWindowLong(hwndDlg, GWL_USERDATA, lParam);
			CMsnProto* proto = (CMsnProto*)lParam;

			HIMAGELIST hIml = ImageList_Create(
				GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
				ILC_MASK | (IsWinVerXPPlus() ? ILC_COLOR32 : ILC_COLOR16 ), 5, 5 );
			
			HICON hIcon = LoadSkinnedIcon(SKINICON_OTHER_SMALLDOT);
			ImageList_AddIcon( hIml, hIcon );
			CallService( MS_SKIN2_RELEASEICON, ( WPARAM )hIcon, 0 );
			
			hIcon =  LoadIconEx( "list_fl" );
			ImageList_AddIcon( hIml, hIcon );
			SendDlgItemMessage( hwndDlg, IDC_ICON_FL, STM_SETICON, ( WPARAM )hIcon, 0 );

			hIcon =  LoadIconEx( "list_al" );
			ImageList_AddIcon( hIml, hIcon );
			SendDlgItemMessage( hwndDlg, IDC_ICON_AL, STM_SETICON, ( WPARAM )hIcon, 0 );
			
			hIcon =  LoadIconEx( "list_bl" );
			ImageList_AddIcon( hIml, hIcon );
			SendDlgItemMessage( hwndDlg, IDC_ICON_BL, STM_SETICON, ( WPARAM )hIcon, 0 );

			hIcon =  LoadIconEx( "list_rl" );
			ImageList_AddIcon( hIml, hIcon );
			SendDlgItemMessage( hwndDlg, IDC_ICON_RL, STM_SETICON, ( WPARAM )hIcon, 0 );

			SendDlgItemMessage( hwndDlg, IDC_LIST, CLM_SETEXTRAIMAGELIST, 0, (LPARAM)hIml );
		}
		SendDlgItemMessage(hwndDlg,IDC_LIST,CLM_SETEXTRACOLUMNS,4,0);

		return TRUE;

//	case WM_SETFOCUS:
//		SetFocus(GetDlgItem(hwndDlg,IDC_LIST));
//		break;

	case WM_NOTIFY:
	{
		CMsnProto* proto = (CMsnProto*)GetWindowLong(hwndDlg, GWL_USERDATA);

		LPNMHDR nmc = (LPNMHDR)lParam;
		if ( nmc->idFrom == 0 && nmc->code == PSN_APPLY ) {
			SaveSettings(GetDlgItem(hwndDlg,IDC_LIST), proto);
			break;
		}

		if ( nmc->idFrom != IDC_LIST )
			break;

		switch ( nmc->code) {
		case CLN_NEWCONTACT:
		case CLN_LISTREBUILT:
			SetAllContactIcons(nmc->hwndFrom, proto);
			//fall through
		case CLN_OPTIONSCHANGED:
			ResetListOptions(nmc->hwndFrom, proto);
			break;

		case NM_CLICK:
			HANDLE hItem;
			NMCLISTCONTROL *nm=(NMCLISTCONTROL*)lParam;
			DWORD hitFlags;
			int iImage;
			int itemType;

			// Make sure we have an extra column, also we can't change RL list
			if ( nm->iColumn == -1 || nm->iColumn == 3 )
				break;

			// Find clicked item
			hItem = (HANDLE)SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_HITTEST, (WPARAM)&hitFlags, MAKELPARAM(nm->pt.x,nm->pt.y));
			// Nothing was clicked
			if ( hItem == NULL )
				break;

			// It was not our extended icon
			if ( !( hitFlags & CLCHT_ONITEMEXTRA ))
				break;

			// Get image in clicked column (0=none, 1=FL, 2=AL, 3=BL, 4=RL)
			iImage = SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nm->iColumn, 0));
			if ( iImage == 0 )
				iImage = nm->iColumn + 1;
			else
				iImage = 0;

			// Get item type (contact, group, etc...)
			itemType = SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_GETITEMTYPE, (WPARAM)hItem, 0);
			if ( itemType == CLCIT_CONTACT ) { // A contact
				SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(nm->iColumn, iImage));
				if (iImage && SendDlgItemMessage(hwndDlg,IDC_LIST,CLM_GETEXTRAIMAGE,(WPARAM)hItem,MAKELPARAM( 3-nm->iColumn, 0)) != 0xFF )
					if ( nm->iColumn == 1 || nm->iColumn == 2 )
						SendDlgItemMessage(hwndDlg, IDC_LIST, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM( 3-nm->iColumn, 0));
			}

			// Activate Apply button
			SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);
			break;
		}
		break;
	}

	case WM_DESTROY:
		HIMAGELIST hIml=(HIMAGELIST)SendDlgItemMessage(hwndDlg,IDC_LIST,CLM_GETEXTRAIMAGELIST,0,0);
		ImageList_Destroy(hIml);
		ReleaseIconEx( "list_fl" );
		ReleaseIconEx( "list_al" );
		ReleaseIconEx( "list_bl" );
		ReleaseIconEx( "list_rl" );
		break;
	}

	return FALSE;
}
