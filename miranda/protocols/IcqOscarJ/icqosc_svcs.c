// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
//
// Copyright � 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright � 2001,2002 Jon Keating, Richard Hughes
// Copyright � 2002,2003,2004 Martin �berg, Sam Kothari, Robert Rainwater
// Copyright � 2004,2005 Joe Kucera
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// -----------------------------------------------------------------------------
//
// File name      : $Source$
// Revision       : $Revision$
// Last change on : $Date$
// Last change by : $Author$
//
// DESCRIPTION:
//
//  High-level code for exported API services
//
// -----------------------------------------------------------------------------

#include "icqoscar.h"



int gbIdleAllow;
int icqGoingOnlineStatus;

//extern icq_mode_messages modeMsgs;
extern CRITICAL_SECTION modeMsgsMutex;
extern WORD wListenPort;


int IcqGetCaps(WPARAM wParam, LPARAM lParam)
{
	int nReturn = 0;


	switch (wParam)
	{

	case PFLAGNUM_1:
		nReturn = PF1_IM | PF1_URL | PF1_AUTHREQ | PF1_BASICSEARCH | PF1_ADDSEARCHRES |
			PF1_VISLIST | PF1_INVISLIST | PF1_MODEMSG | PF1_FILE | PF1_EXTSEARCH |
			PF1_EXTSEARCHUI | PF1_SEARCHBYEMAIL | PF1_SEARCHBYNAME |
      PF1_INDIVMODEMSG | PF1_ADDED | PF1_CONTACT;
    if (!gbAimEnabled)
      nReturn |= PF1_NUMERICUSERID;
		if (gbSsiEnabled && ICQGetContactSettingByte(NULL, "ServerAddRemove", DEFAULT_SS_ADDSERVER))
			nReturn |= PF1_SERVERCLIST;
		break;

	case PFLAGNUM_2:
		nReturn = PF2_ONLINE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND |
			PF2_FREECHAT | PF2_INVISIBLE;
		break;

	case PFLAGNUM_3:
		nReturn = PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND | PF2_HEAVYDND |
			PF2_FREECHAT;
		break;

  case PFLAGNUM_4:
    nReturn = PF4_SUPPORTIDLE;
    if (gbAvatarsEnabled)
      nReturn |= PF4_AVATARS;
#ifdef DBG_CAPMTN
		nReturn |= PF4_SUPPORTTYPING;
#endif
		break;

	case PFLAG_UNIQUEIDTEXT:
		nReturn = (int)Translate("User ID");
		break;

	case PFLAG_UNIQUEIDSETTING:
		nReturn = (int)UNIQUEIDSETTING;
		break;

	case PFLAG_MAXCONTACTSPERPACKET:
		nReturn = MAX_CONTACTSSEND;
		break;

	case PFLAG_MAXLENOFMESSAGE:
		nReturn = MAX_MESSAGESNACSIZE-102;
	}

	return nReturn;
}



int IcqGetName(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		strncpy((char *)lParam, Translate(gpszICQProtoName), wParam);

		return 0; // Success
	}

	return 1; // Failure
}



int IcqLoadIcon(WPARAM wParam, LPARAM lParam)
{
	UINT id;


	switch (wParam & 0xFFFF)
	{
		case PLI_PROTOCOL:
			id = IDI_ICQ;
			break;

		default:
			return 0; // Failure
	}

	return (int)LoadImage(hInst, MAKEINTRESOURCE(id), IMAGE_ICON, GetSystemMetrics(wParam&PLIF_SMALL?SM_CXSMICON:SM_CXICON), GetSystemMetrics(wParam&PLIF_SMALL?SM_CYSMICON:SM_CYICON), 0);
}



int IcqIdleChanged(WPARAM wParam, LPARAM lParam)
{
	int bIdle = (lParam&IDF_ISIDLE);
	int bPrivacy = (lParam&IDF_PRIVACY);

	if (bPrivacy) return 0;

  ICQWriteContactSettingDword(NULL, "IdleTS", bIdle ? time(0) : 0);

  if (gbTempVisListEnabled) // remove temporary visible users
    clearTemporaryVisibleList();

  icq_setidle(bIdle ? 1 : 0);

	return 0;
}



int IcqGetAvatarInfo(WPARAM wParam, LPARAM lParam)
{
  PROTO_AVATAR_INFORMATION* pai = (PROTO_AVATAR_INFORMATION*)lParam;
  DWORD dwUIN;
  uid_str szUID;
  DBVARIANT dbv, dbvSaved;
  int dwPaFormat;

  if (!gbAvatarsEnabled) return GAIR_NOAVATAR;

  if (ICQGetContactSetting(pai->hContact, "AvatarHash", &dbv))
    return GAIR_NOAVATAR; // we did not found avatar hash - no avatar available

  if (ICQGetContactSettingUID(pai->hContact, &dwUIN, &szUID))
  {
    DBFreeVariant(&dbv);

    return GAIR_NOAVATAR; // we do not support avatars for invalid contacts
  }
  dwPaFormat = ICQGetContactSettingByte(pai->hContact, "AvatarType", PA_FORMAT_UNKNOWN);
  if (dwPaFormat != PA_FORMAT_UNKNOWN)
  { // we know the format, test file
    GetFullAvatarFileName(dwUIN, szUID, dwPaFormat, pai->filename, MAX_PATH);

    pai->format = dwPaFormat; 

    if (!ICQGetContactSetting(pai->hContact, "AvatarSaved", &dbvSaved) && !memcmp(dbv.pbVal, dbvSaved.pbVal, 0x14))
    { // hashes are the same
      if (access(pai->filename, 0) == 0)
      {
        DBFreeVariant(&dbv);

        return GAIR_SUCCESS; // we have found the avatar file, whoala
      }
      DBFreeVariant(&dbvSaved);
    }
  }

  if ((wParam & GAIF_FORCE) != 0 && pai->hContact != 0)
  { // request avatar data
    GetAvatarFileName(dwUIN, szUID, pai->filename, MAX_PATH);
    GetAvatarData(pai->hContact, dwUIN, szUID, dbv.pbVal, dbv.cpbVal, pai->filename);
    DBFreeVariant(&dbv);

    return GAIR_WAITFOR;
  }

  DBFreeVariant(&dbv);

  return GAIR_NOAVATAR;
}



int IcqSetStatus(WPARAM wParam, LPARAM lParam)
{
	int nNewStatus = MirandaStatusToSupported(wParam);

  if (gbTempVisListEnabled) // remove temporary visible users
    clearTemporaryVisibleList();

	if (nNewStatus != gnCurrentStatus)
	{
		// New status is OFFLINE
		if (nNewStatus == ID_STATUS_OFFLINE)
		{
			icq_packet packet;

      // for quick logoff
      icqGoingOnlineStatus = nNewStatus;

			// Send disconnect packet
			packet.wLen = 0;
			write_flap(&packet, ICQ_CLOSE_CHAN);
			sendServPacket(&packet);

			icq_serverDisconnect(FALSE);

			SetCurrentStatus(ID_STATUS_OFFLINE);

			NetLog_Server("Logged off.");
		}
		else
		{
			switch (gnCurrentStatus)
			{

			// We are offline and need to connect
			case ID_STATUS_OFFLINE:
				{
					char *pszPwd;

					// Update user connection settings
					UpdateGlobalSettings();

					// Read UIN from database
					dwLocalUIN = ICQGetContactSettingDword(NULL, UNIQUEIDSETTING, 0);
					if (dwLocalUIN == 0)
					{
						SetCurrentStatus(ID_STATUS_OFFLINE);

						icq_LogMessage(LOG_FATAL, Translate("You have not entered a ICQ number.\nConfigure this in Options->Network->ICQ and try again."));
						return 0;
					}

					// Set status to 'Connecting'
					icqGoingOnlineStatus = nNewStatus;
					SetCurrentStatus(ID_STATUS_CONNECTING);

					// Read password from database
          pszPwd = GetUserPassword(FALSE);

          if (pszPwd)
						icq_login(pszPwd);
					else 
						RequestPassword();

					break;
				}

			// We are connecting... We only need to change the going online status
			case ID_STATUS_CONNECTING:
				{
					icqGoingOnlineStatus = nNewStatus;
					break;
				}

			// We are already connected so we should just change status
			default:
				{
					SetCurrentStatus(nNewStatus);

					if (gnCurrentStatus == ID_STATUS_INVISIBLE)
					{
						// Tell whos on our visible list
						icq_sendEntireVisInvisList(0);
						if (gbSsiEnabled)
							updateServVisibilityCode(3);
						icq_setstatus(MirandaStatusToIcq(gnCurrentStatus));
            if (gbAimEnabled)
            {
              char** szMsg = MirandaStatusToAwayMsg(gnCurrentStatus);

              EnterCriticalSection(&modeMsgsMutex);
              if (szMsg)
                icq_sendSetAimAwayMsgServ(*szMsg);
              LeaveCriticalSection(&modeMsgsMutex);
            }
					}
					else
					{
						// Tell whos on our invisible list
						icq_sendEntireVisInvisList(1);
						icq_setstatus(MirandaStatusToIcq(gnCurrentStatus));
						if (gbSsiEnabled)
							updateServVisibilityCode(4);
            if (gbAimEnabled)
            {
              char** szMsg = MirandaStatusToAwayMsg(gnCurrentStatus);

              EnterCriticalSection(&modeMsgsMutex);
              if (szMsg)
                icq_sendSetAimAwayMsgServ(*szMsg);
              LeaveCriticalSection(&modeMsgsMutex);
            }
					}
				}
			}
		}
	}

	return 0;
}



int IcqGetStatus(WPARAM wParam, LPARAM lParam)
{
	return gnCurrentStatus;
}



int IcqSetAwayMsg(WPARAM wParam, LPARAM lParam)
{
	char** ppszMsg = NULL;


  EnterCriticalSection(&modeMsgsMutex);

  ppszMsg = MirandaStatusToAwayMsg(wParam);
  if (!ppszMsg)
  {
		LeaveCriticalSection(&modeMsgsMutex);
		return 1; // Failure
  }

	// Free old message
	if (ppszMsg)
		SAFE_FREE(&(*ppszMsg));

	// Set new message
	if (lParam)
		*ppszMsg = _strdup((char*)lParam);
	else
		*ppszMsg = NULL;

  if (gbAimEnabled && (gnCurrentStatus == (int)wParam))
    icq_sendSetAimAwayMsgServ(*ppszMsg);

	LeaveCriticalSection(&modeMsgsMutex);

	return 0; // Success
}



int IcqAuthAllow(WPARAM wParam, LPARAM lParam)
{
	if (icqOnline && wParam)
	{
		DBEVENTINFO dbei;
    DWORD body[2];
		DWORD uin;
    uid_str uid;
    HANDLE hContact;


		ZeroMemory(&dbei, sizeof(dbei));
		dbei.cbSize = sizeof(dbei);
		dbei.cbBlob = sizeof(DWORD)*2;
		dbei.pBlob = (PBYTE)&body;

		if (CallService(MS_DB_EVENT_GET, wParam, (LPARAM)&dbei))
			return 1;

		if (dbei.eventType != EVENTTYPE_AUTHREQUEST)
			return 1;

		if (strcmp(dbei.szModule, gpszICQProtoName))
			return 1;

    hContact = (HANDLE)body[1]; // this is bad - needs new auth system

    if (ICQGetContactSettingUID(hContact, &uin, &uid))
      return 1;

		icq_sendAuthResponseServ(uin, uid, 1, "");

		return 0; // Success
	}

	return 1; // Failure
}



int IcqAuthDeny(WPARAM wParam, LPARAM lParam)
{
	if (icqOnline && wParam)
	{
		DBEVENTINFO dbei;
    DWORD body[2];
    DWORD uin;
    uid_str uid;
    HANDLE hContact;


		ZeroMemory(&dbei, sizeof(dbei));
		dbei.cbSize = sizeof(dbei);
		dbei.cbBlob = sizeof(DWORD)*2;
		dbei.pBlob = (PBYTE)&body;

		if (CallService(MS_DB_EVENT_GET, wParam, (LPARAM)&dbei))
			return 1;

		if (dbei.eventType != EVENTTYPE_AUTHREQUEST)
			return 1;

		if (strcmp(dbei.szModule, gpszICQProtoName))
			return 1;

    hContact = (HANDLE)body[1]; // this is bad - needs new auth system

    if (ICQGetContactSettingUID(hContact, &uin, &uid))
      return 1;

		icq_sendAuthResponseServ(uin, uid, 0, (char *)lParam);

		return 0; // Success
	}

	return 1; // Failure
}



static int cheekySearchId = -1;
static DWORD cheekySearchUin;
static char* cheekySearchUid;
static VOID CALLBACK CheekySearchTimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	ICQSEARCHRESULT isr = {0};

	KillTimer(hwnd, idEvent);

	isr.hdr.cbSize = sizeof(isr);
  if (cheekySearchUin)
  	isr.hdr.nick = "";
  else
    isr.hdr.nick = cheekySearchUid;
	isr.hdr.firstName = "";
	isr.hdr.lastName = "";
	isr.hdr.email = "";
	isr.uin = cheekySearchUin;

	ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)cheekySearchId, (LPARAM)&isr);
	ICQBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)cheekySearchId, 0);

	cheekySearchId = -1;
}


int IcqBasicSearch(WPARAM wParam, LPARAM lParam)
{
  if (lParam)
  {
    char* pszSearch = (char*)lParam;
    DWORD dwUin;

    if (strlen(pszSearch))
    {
      char pszUIN[255];
      int nHandle = 0;
      unsigned int i, j;

      if (!gbAimEnabled)
      {
        for (i=j=0; (i<strlen(pszSearch)) && (j<255); i++)
        { // we take only numbers
          if ((pszSearch[i]>=0x30) && (pszSearch[i]<=0x39))
          {
            pszUIN[j] = pszSearch[i];
            j++;
          }
        }
      }
      else
      {
        for (i=j=0; (i<strlen(pszSearch)) && (j<255); i++)
        { // we remove spaces and slashes
          if ((pszSearch[i]!=0x20) && (pszSearch[i]!='-'))
          {
            pszUIN[j] = pszSearch[i];
            j++;
          }
        }
      }
      pszUIN[j] = 0;
      
      if (strlen(pszUIN))
      {
        if (IsStringUIN(pszUIN))
          dwUin = atoi(pszUIN);
        else
          dwUin = 0;

        // Cheeky instant UIN search
        if (!dwUin || GetKeyState(VK_CONTROL)&0x8000)
        {
          cheekySearchId = GenerateCookie(0);
          cheekySearchUin = dwUin;
          cheekySearchUid = strdup(pszUIN);
          SetTimer(NULL, 0, 10, CheekySearchTimerProc); // The caller needs to get this return value before the results
          nHandle = cheekySearchId;
        }
        else if (icqOnline)
        {
          nHandle = SearchByUin(dwUin);
        }

        // Success
        return nHandle;
      }
    }
  }

  // Failure
  return 0;
}


int IcqSearchByEmail(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline && (strlen((char*)lParam) > 0))
  {
    DWORD dwSearchId, dwSecId;

		// Success
		dwSearchId = SearchByEmail((char *)lParam);
    dwSecId = icq_searchAimByEmail((char *)lParam, dwSearchId);
    if (dwSearchId) 
      return dwSearchId;
    else
      return dwSecId;
	}

	return 0; // Failure
}


int IcqSearchByDetails(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
	{
		PROTOSEARCHBYNAME *psbn=(PROTOSEARCHBYNAME*)lParam;


    if (psbn->pszNick || psbn->pszFirstName || psbn->pszLastName)
		{
			// Success
			return SearchByNames(psbn->pszNick, psbn->pszFirstName, psbn->pszLastName);
		}
	}

	return 0; // Failure
}


int IcqCreateAdvSearchUI(WPARAM wParam, LPARAM lParam)
{
	if (lParam && hInst)
	{
		// Success
		return (int)CreateDialog(hInst, MAKEINTRESOURCE(IDD_ICQADVANCEDSEARCH), (HWND)lParam, AdvancedSearchDlgProc);
	}

	return 0; // Failure
}


int IcqSearchByAdvanced(WPARAM wParam, LPARAM lParam)
{
	if (icqOnline && IsWindow((HWND)lParam))
	{
		int nDataLen;
		BYTE* bySearchData;

		if (bySearchData = createAdvancedSearchStructure((HWND)lParam, &nDataLen))
		{
			int result;

			result = icq_sendAdvancedSearchServ(bySearchData, nDataLen);
			SAFE_FREE(&bySearchData);

			return result; // Success
		}
	}

	return 0; // Failure
}



// TODO: Adding needs some more work in general
static HANDLE AddToListByUIN(DWORD dwUin, DWORD dwFlags)
{
	HANDLE hContact;
  int bAdded;

	hContact = HContactFromUIN(dwUin, &bAdded);

	if (hContact)
	{
		if ((!dwFlags & PALF_TEMPORARY) && DBGetContactSettingByte(hContact, "CList", "NotOnList", 1)) 
    {
			DBDeleteContactSetting(hContact, "CList", "NotOnList");
			DBDeleteContactSetting(hContact, "CList", "Hidden");
		}

		return hContact; // Success
	}

	return NULL; // Failure
}


static HANDLE AddToListByUID(char *szUID, DWORD dwFlags)
{
	HANDLE hContact;
  int bAdded;

	hContact = HContactFromUID(szUID, &bAdded);

	if (hContact)
	{
		if ((!dwFlags & PALF_TEMPORARY) && DBGetContactSettingByte(hContact, "CList", "NotOnList", 1)) 
    {
			DBDeleteContactSetting(hContact, "CList", "NotOnList");
			DBDeleteContactSetting(hContact, "CList", "Hidden");
		}

		return hContact; // Success
	}

	return NULL; // Failure
}


int IcqAddToList(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		ICQSEARCHRESULT *isr = (ICQSEARCHRESULT*)lParam;

		if (isr->hdr.cbSize == sizeof(ICQSEARCHRESULT))
    {
      if (!isr->uin)
        return (int)AddToListByUID(isr->hdr.nick, wParam);
      else
			  return (int)AddToListByUIN(isr->uin, wParam);
    }
	}

	return 0; // Failure
}



// Todo: This function needs to be rewritten
int IcqAddToListByEvent(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei = {0};
	DWORD uin;


	dbei.cbSize = sizeof(dbei);
	dbei.cbBlob = sizeof(DWORD);
	dbei.pBlob = (PBYTE)&uin;

	if (CallService(MS_DB_EVENT_GET, lParam, (LPARAM)&dbei))
		return 0;

	if (strcmp(dbei.szModule, gpszICQProtoName))
		return 0;


	if (dbei.eventType == EVENTTYPE_CONTACTS)
	{
		int i;
		char* pbOffset;


		dbei.pBlob = (PBYTE)malloc(dbei.cbBlob+1);
		dbei.pBlob[dbei.cbBlob] = '\0';
		if (CallService(MS_DB_EVENT_GET, lParam, (LPARAM)&dbei))
		{
			SAFE_FREE(&dbei.pBlob);
			return 0;
		}

		uin = 0;

		for (i = 0, pbOffset = (char*)dbei.pBlob; i <= HIWORD(wParam); i++)
		{
			pbOffset += lstrlen((char*)pbOffset) + 1; // Nick
			if (pbOffset-dbei.pBlob >= (int)dbei.cbBlob)
				break;
			uin = atoi((char*)pbOffset);
			pbOffset += lstrlen((char*)pbOffset)+1; // Uin
			if (pbOffset-dbei.pBlob >= (int)dbei.cbBlob)
				break;
		}
		SAFE_FREE(&dbei.pBlob);
	}
	else if (dbei.eventType != EVENTTYPE_AUTHREQUEST && dbei.eventType != EVENTTYPE_ADDED)
	{
		return 0;
	}


	if (uin != 0)
	{
		return (int)AddToListByUIN(uin, LOWORD(wParam)); // Success
	}


	return 0; // Failure
}



int IcqSetNickName(WPARAM wParam, LPARAM lParam)
{
	PBYTE buf = NULL;
  int buflen = 0;

  ICQWriteContactSettingString(NULL, "Nick", (char*)lParam);

 	ppackLEWord(&buf, &buflen, 0);  // data length
	ppackTLVLNTS(&buf, &buflen, (char*)lParam, TLV_NICKNAME, 1);
	*(PWORD)buf = buflen - 2;
  IcqChangeInfo(META_SET_FULLINFO_REQ, (LPARAM)buf);

  return 0; // Not defined // TODO: change when definition is ready
}



int IcqChangeInfo(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
  {
		return icq_changeUserDetailsServ((WORD)wParam, (PBYTE)(lParam+2), *(PWORD)lParam); // Success
	}

	return 0; // Failure
}



static int messageRate = 0;
static DWORD lastMessageTick = 0;
int IcqGetInfo(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
  { // TODO: add checking for SGIF_ONOPEN, otherwise max one per 10sec
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD dwUin;

    if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
    {
      return 0; // Invalid contact
    }

		messageRate -= (GetTickCount() - lastMessageTick)/10;
		if (messageRate<0) // TODO: this is bad, needs centralising
			messageRate = 0;
		lastMessageTick = GetTickCount();
		messageRate += 67; // max 1.5 msgs/sec when rate is high

		// server kicks if 100 msgs sent instantly, so send max 50 instantly
		if (messageRate < 67*50)
		{
			icq_sendGetInfoServ(dwUin, (ccs->wParam & SGIF_MINIMAL) != 0);

			return 0; // Success
		}
	}

	return 1; // Failure
}



int IcqFileAllow(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD dwUin;

    if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
      return 0; // Invalid contact

		if (dwUin && icqOnline && ccs->hContact && ccs->lParam && ccs->wParam)
		{
			filetransfer* ft = ((filetransfer *)ccs->wParam);

			ft->szSavePath = _strdup((char *)ccs->lParam);
			AddExpectedFileRecv(ft);

			// Was request received thru DC and have we a open DC, send through that
			if (ft->bDC && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
				icq_sendFileAcceptDirect(ccs->hContact, ft);
			else
				icq_sendFileAcceptServ(dwUin, ft, 0);

			return ccs->wParam; // Success
		}
	}

	return 0; // Failure
}



int IcqFileDeny(WPARAM wParam, LPARAM lParam)
{
	int nReturnValue = 1;

	if (lParam)
	{
		CCSDATA *ccs = (CCSDATA *)lParam;
		filetransfer *ft = (filetransfer*)ccs->wParam;
    DWORD dwUin;

    if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
      return 1; // Invalid contact

		if (icqOnline && dwUin && ccs->wParam && ccs->hContact) 
    {
			// Was request received thru DC and have we a open DC, send through that
			if (ft->bDC && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
				icq_sendFileDenyDirect(ccs->hContact, ft, (char*)ccs->lParam);
			else
			  icq_sendFileDenyServ(dwUin, ft, (char*)ccs->lParam, 0);

			nReturnValue = 0; // Success
		}
		/* FIXME: ft leaks (but can get double freed?) */
	}

	return nReturnValue;
}



int IcqFileCancel(WPARAM wParam, LPARAM lParam)
{
	if (lParam /*&& icqOnline*/)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD dwUin;

    if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
      return 1; // Invalid contact

		if (ccs->hContact && dwUin && ccs->wParam)
		{
			filetransfer * ft = (filetransfer * ) ccs->wParam;

			icq_CancelFileTransfer(ccs->hContact, ft);

			return 0; // Success
		}
	}

	return 1; // Failure
}



int IcqFileResume(WPARAM wParam, LPARAM lParam)
{
	if (icqOnline && wParam)
	{
		PROTOFILERESUME *pfr = (PROTOFILERESUME*)lParam;

		icq_sendFileResume((filetransfer *)wParam, pfr->action, pfr->szFilename);

		return 0; // Success
	}

	return 1; // Failure
}



int IcqSendSms(WPARAM wParam, LPARAM lParam)
{
	if (icqOnline && wParam && lParam)
		return icq_sendSMSServ((const char *)wParam, (const char *)lParam);

	return 0; // Failure
}



// Maybe we should be saving these up for batch changing, but I can't be bothered yet
int IcqSetApparentMode(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD uin;
    uid_str uid;

    if (ICQGetContactSettingUID(ccs->hContact, &uin, &uid))
      return 1; // Invalid contact

		if (ccs->hContact)
		{
			// Only 3 modes are supported
			if (ccs->wParam == 0 || ccs->wParam == ID_STATUS_ONLINE || ccs->wParam == ID_STATUS_OFFLINE)
			{
				int oldMode = ICQGetContactSettingWord(ccs->hContact, "ApparentMode", 0);

				// Don't send redundant updates
				if ((int)ccs->wParam != oldMode)
				{
					ICQWriteContactSettingWord(ccs->hContact, "ApparentMode", (WORD)ccs->wParam);

					// Not being online is only an error when in SS mode. This is not handled
					// yet so we just ignore this for now.
					if (icqOnline)
					{
						if (oldMode != 0) // Remove from old list
							icq_sendChangeVisInvis(ccs->hContact, uin, uid, oldMode==ID_STATUS_OFFLINE, 0);
						if (ccs->wParam != 0) // Add to new list
							icq_sendChangeVisInvis(ccs->hContact, uin, uid, ccs->wParam==ID_STATUS_OFFLINE, 1);
					}

          return 0; // Success
				}
			}
		}
	}

	return 1; // Failure
}



int IcqGetAwayMsg(WPARAM wParam,LPARAM lParam)
{
	if (lParam && icqOnline)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD dwUin;
    uid_str szUID;
		WORD wStatus;

    if (ICQGetContactSettingUID(ccs->hContact, &dwUin, &szUID))
      return 0; // Invalid contact

		wStatus = ICQGetContactSettingWord(ccs->hContact, "Status", ID_STATUS_OFFLINE);

		if (dwUin)
		{
			int wMessageType = 0;

			switch(wStatus)
			{

			case ID_STATUS_AWAY:
				wMessageType = MTYPE_AUTOAWAY;
				break;

			case ID_STATUS_NA:
				wMessageType = MTYPE_AUTONA;
				break;

			case ID_STATUS_OCCUPIED:
				wMessageType = MTYPE_AUTOBUSY;
				break;

			case ID_STATUS_DND:
				wMessageType = MTYPE_AUTODND;
				break;

			case ID_STATUS_FREECHAT:
				wMessageType = MTYPE_AUTOFFC;
				break;

			default:
				break;
			}

			if (wMessageType)
      {
        if (gbDCMsgEnabled && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
        {
          int iRes = icq_sendGetAwayMsgDirect(ccs->hContact, wMessageType);
          if (iRes) return iRes; // we succeded, return
        }
				return icq_sendGetAwayMsgServ(dwUin, wMessageType); // Success
      }
		}
    else
    {
      if (wStatus == ID_STATUS_AWAY)
      {
        return icq_sendGetAimAwayMsgServ(szUID, MTYPE_AUTOAWAY);
      }
    }
	}

	return 0; // Failure
}



int IcqSendMessage(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact && ccs->lParam)
		{
			WORD wRecipientStatus;
			DWORD dwCookie;
			DWORD dwUin;
      uid_str szUID;
			char* pszText;

      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, &szUID))
      { // Invalid contact
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_MESSAGE, Translate("The receiver has an invalid user ID."));
        return dwCookie;
			}

      if (dwUin && gbTempVisListEnabled && gnCurrentStatus == ID_STATUS_INVISIBLE)
        makeContactTemporaryVisible(ccs->hContact);  // make us temporarily visible to contact

      pszText = (char*)ccs->lParam;

			wRecipientStatus = ICQGetContactSettingWord(ccs->hContact, "Status", ID_STATUS_OFFLINE);

			// Failure scenarios
			if (!icqOnline)
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_MESSAGE, Translate("You cannot send messages when you are offline."));
			}
			else if ((wRecipientStatus == ID_STATUS_OFFLINE) && (strlen(pszText) > 450))
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_MESSAGE, Translate("Messages to offline contacts must be shorter than 450 characters."));
			}
			// Looks OK
			else
			{
				message_cookie_data* pCookieData;

        if (wRecipientStatus != ID_STATUS_OFFLINE && gbUtfEnabled==2 && !IsUSASCII(pszText, strlen(pszText)) 
          && CheckContactCapabilities(ccs->hContact, CAPF_UTF) && ICQGetContactSettingByte(ccs->hContact, "UnicodeSend", 1))
        { // text contains national chars and we should send all this as Unicode, so do it
          char* pszUtf = NULL;
          int nStrSize = MultiByteToWideChar(CP_ACP, 0, pszText, strlen(pszText), (wchar_t*)pszUtf, 0);
          int nRes;

          pszUtf = calloc(nStrSize + 2, sizeof(wchar_t));
          pszUtf[0] = '\0'; // we omit ansi string - not used...
          MultiByteToWideChar(CP_ACP, 0, pszText, strlen(pszText), (wchar_t*)(pszUtf+1), nStrSize);
          *(WORD*)(pszUtf + 1 + nStrSize*sizeof(wchar_t)) = '\0'; // trailing zeros

          ccs->lParam = (LPARAM)pszUtf; // yeah, this is quite a hack, BE AWARE OF THAT !!!
          ccs->wParam |= PREF_UNICODE;

          nRes = IcqSendMessageW(wParam, lParam);
          ccs->lParam = (LPARAM)pszText;

          SAFE_FREE(&pszUtf); // release memory

          return nRes;
        }

        if (!dwUin)
        { // prepare AIM Html message
          char *mng = MangleXml(pszText, strlennull(pszText));
          char *tmp = (char*)malloc(strlennull(mng) + 28);

          strcpy(tmp, "<HTML><BODY>");
          strcat(tmp, mng);
          SAFE_FREE(&mng);
          strcat(tmp, "</BODY></HTML>");
          pszText = tmp;
        }

				// Set up the ack type
				pCookieData = malloc(sizeof(message_cookie_data));
				pCookieData->bMessageType = MTYPE_PLAIN;
				if (!ICQGetContactSettingByte(NULL, "SlowSend", 1))
					pCookieData->nAckType = ACKTYPE_NONE;
				else if ((!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY)) ||
					(wRecipientStatus == ID_STATUS_OFFLINE) || !dwUin)
					pCookieData->nAckType = ACKTYPE_SERVER;
				else
					pCookieData->nAckType = ACKTYPE_CLIENT;

#ifdef _DEBUG
    		NetLog_Server("Send message - Message cap is %u", CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY));
		    NetLog_Server("Send message - Contact status is %u", wRecipientStatus);
#endif
        if (dwUin && gbDCMsgEnabled && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
        {
          int iRes = icq_SendDirectMessage(dwUin, ccs->hContact, pszText, strlen(pszText), 1, pCookieData, NULL);
          if (iRes) return iRes; // we succeded, return
        }

        if ((!dwUin || !CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY)) || (wRecipientStatus == ID_STATUS_OFFLINE))
				{
					dwCookie = icq_SendChannel1Message(dwUin, szUID, ccs->hContact, pszText, pCookieData);
				}
				else
				{
					WORD wPriority;

					if (wRecipientStatus == ID_STATUS_ONLINE || wRecipientStatus == ID_STATUS_FREECHAT)
						wPriority = 0x0001;
					else
						wPriority = 0x0021;

					dwCookie = icq_SendChannel2Message(dwUin, pszText, strlen(pszText), wPriority, pCookieData, NULL);
				}

				// This will stop the message dialog from waiting for the real message delivery ack
				if (pCookieData->nAckType == ACKTYPE_NONE)
				{
					icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_SUCCESS, ACKTYPE_MESSAGE, NULL);
					// We need to free this here since we will never see the real ack
					// The actual cookie value will still have to be returned to the message dialog though
					SAFE_FREE(&pCookieData);
					FreeCookie(dwCookie);
				}
        if (!dwUin) SAFE_FREE(&pszText);
			}

			return dwCookie; // Success
		}
	}

	return 0; // Failure
}



int IcqSendMessageW(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact && ccs->lParam)
		{
			WORD wRecipientStatus;
			DWORD dwCookie;
			DWORD dwUin;
      uid_str szUID;
			wchar_t* pszText;

			if (!gbUtfEnabled || (ccs->wParam & PREF_UNICODE == PREF_UNICODE) || (!CheckContactCapabilities(ccs->hContact, CAPF_UTF)))
			{	// send as unicode only if marked as unicode & unicode enabled
				return IcqSendMessage(wParam, lParam);
			}

      if (!ICQGetContactSettingByte(ccs->hContact, "UnicodeSend", 1))
      { // has the user blocked sending unicode to that user ?
        return IcqSendMessage(wParam, lParam);
      }

      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, &szUID))
      { // Invalid contact
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_MESSAGE, Translate("The receiver has an invalid user ID."));
        return dwCookie;
			}

      if (dwUin && gbTempVisListEnabled && gnCurrentStatus == ID_STATUS_INVISIBLE)
        makeContactTemporaryVisible(ccs->hContact); // make us temporarily visible to contact

			pszText = (wchar_t*)((char*)ccs->lParam+strlen((char*)ccs->lParam)+1); // get the UTF-16 part

			wRecipientStatus = ICQGetContactSettingWord(ccs->hContact, "Status", ID_STATUS_OFFLINE);

			// Failure scenarios
			if (!icqOnline)
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_MESSAGE, Translate("You cannot send messages when you are offline."));
			}
			// Looks OK
			else
			{
				message_cookie_data* pCookieData;

				if ((wRecipientStatus == ID_STATUS_OFFLINE) || IsUnicodeAscii(pszText, wcslen(pszText)))
				{ // send as plain if no special char or user offline
					return IcqSendMessage(wParam, lParam);
				};

				// Set up the ack type
				pCookieData = malloc(sizeof(message_cookie_data));
				pCookieData->bMessageType = MTYPE_PLAIN;
				if (!ICQGetContactSettingByte(NULL, "SlowSend", 1))
					pCookieData->nAckType = ACKTYPE_NONE;
				else if ((!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY)) || !dwUin)
					pCookieData->nAckType = ACKTYPE_SERVER;
				else
					pCookieData->nAckType = ACKTYPE_CLIENT;

#ifdef _DEBUG
    		NetLog_Server("Send unicode message - Message cap is %u", CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY));
		    NetLog_Server("Send unicode message - Contact status is %u", wRecipientStatus);
#endif
        if (dwUin && gbDCMsgEnabled && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
        {
					char* utf8msg = make_utf8_string(pszText);
          int iRes;

          iRes = icq_SendDirectMessage(dwUin, ccs->hContact, utf8msg, strlen(utf8msg), 1, pCookieData, CAP_UTF8MSGS);
					SAFE_FREE(&utf8msg);

          if (iRes) return iRes; // we succeded, return
        }

				if (!dwUin || !CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY))
				{
          char* utmp;
          char* mng;
          char* tmp;

          if (!dwUin)
          {
            utmp = make_utf8_string(pszText);
            mng = MangleXml(utmp, strlennull(utmp));
            SAFE_FREE(&utmp);
            tmp = (char*)malloc(strlennull(mng) + 28);
            strcpy(tmp, "<HTML><BODY>");
            strcat(tmp, mng);
            SAFE_FREE(&mng);
            strcat(tmp, "</BODY></HTML>");
            pszText = make_unicode_string(tmp);
            SAFE_FREE(&tmp);
          }

					dwCookie = icq_SendChannel1MessageW(dwUin, szUID, ccs->hContact, pszText, pCookieData);

          if (!dwUin) SAFE_FREE(&pszText);
				}
				else
				{
					WORD wPriority;
					char* utf8msg;

					if (wRecipientStatus == ID_STATUS_ONLINE || wRecipientStatus == ID_STATUS_FREECHAT)
						wPriority = 0x0001;
					else
						wPriority = 0x0021;

					utf8msg = make_utf8_string(pszText);
					dwCookie = icq_SendChannel2Message(dwUin, utf8msg, strlen(utf8msg), wPriority, pCookieData, CAP_UTF8MSGS);

					SAFE_FREE(&utf8msg);
				}

				// This will stop the message dialog from waiting for the real message delivery ack
				if (pCookieData->nAckType == ACKTYPE_NONE)
				{
					icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_SUCCESS, ACKTYPE_MESSAGE, NULL);
					// We need to free this here since we will never see the real ack
					// The actual cookie value will still have to be returned to the message dialog though
					SAFE_FREE(&pCookieData);
					FreeCookie(dwCookie);
				}

			}
			return dwCookie; // Success
		}
	}
	return 0; // Failure
}



int IcqSendUrl(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact && ccs->lParam)
		{
			DWORD dwCookie;
			WORD wRecipientStatus;
			DWORD dwUin;

      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
      { // Invalid contact
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_URL, Translate("The receiver has an invalid user ID."));
        return dwCookie;
			}

			wRecipientStatus = ICQGetContactSettingWord(ccs->hContact, "Status", ID_STATUS_OFFLINE);

			// Failure
			if (!icqOnline)
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_URL, Translate("You cannot send messages when you are offline."));
			}
			// Looks OK
			else
			{
				message_cookie_data* pCookieData;
				char* szDesc;
				char* szUrl;
				char* szBody;
				int nBodyLen;
				int nDescLen;
				int nUrlLen;


				// Set up the ack type
				pCookieData = malloc(sizeof(message_cookie_data));
				pCookieData->bMessageType = MTYPE_URL;
				if (!ICQGetContactSettingByte(NULL, "SlowSend", 1))
				{
					pCookieData->nAckType = ACKTYPE_NONE;
				}
				else if ((!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY)) ||
					(wRecipientStatus == ID_STATUS_OFFLINE))
				{
					pCookieData->nAckType = ACKTYPE_SERVER;
				}
				else
				{
					pCookieData->nAckType = ACKTYPE_CLIENT;
				}

				// Format the body
				szUrl = (char*)ccs->lParam;
				nUrlLen = strlen(szUrl);
				szDesc = (char *)ccs->lParam + nUrlLen + 1;
				nDescLen = strlen(szDesc);
				nBodyLen = nUrlLen + nDescLen + 2;
				szBody = (char *)malloc(nBodyLen);
				strcpy(szBody, szDesc);
				szBody[nDescLen] = (char)0xFE; // Separator
				strcpy(szBody + nDescLen + 1, szUrl);


        if (gbDCMsgEnabled && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
        {
          int iRes = icq_SendDirectMessage(dwUin, ccs->hContact, szBody, nBodyLen, 1, pCookieData, NULL);
          if (iRes) return iRes; // we succeded, return
        }

				// Select channel and send
				if (!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY) ||
					wRecipientStatus == ID_STATUS_OFFLINE)
				{
					dwCookie = icq_SendChannel4Message(dwUin, MTYPE_URL,
						(WORD)nBodyLen, szBody, pCookieData);
				}
				else
				{
					WORD wPriority;

					if (wRecipientStatus == ID_STATUS_ONLINE || wRecipientStatus == ID_STATUS_FREECHAT)
						wPriority = 0x0001;
					else
						wPriority = 0x0021;

					dwCookie = icq_SendChannel2Message(dwUin, szBody, nBodyLen, wPriority, pCookieData, NULL);
				}

				// Free memory used for body
				SAFE_FREE(&szBody);

				// This will stop the message dialog from waiting for the real message delivery ack
				if (pCookieData->nAckType == ACKTYPE_NONE)
				{
					icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_SUCCESS, ACKTYPE_URL, NULL);
					// We need to free this here since we will never see the real ack
					// The actual cookie value will still have to be returned to the message dialog though
					SAFE_FREE(&pCookieData);
					FreeCookie(dwCookie);
				}
			}

			return dwCookie; // Success
		}
	}

	return 0; // Failure
}



int IcqSendContacts(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact && ccs->lParam)
		{
			int nContacts;
			int i;
			HANDLE* hContactsList = (HANDLE*)ccs->lParam;
			DWORD dwUin;
			char* szProto;
			WORD wRecipientStatus;
			DWORD dwCookie;

      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
			{ // Invalid contact
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_CONTACTS, Translate("The receiver has an invalid user ID."));
        return dwCookie;
			}

			wRecipientStatus = ICQGetContactSettingWord(ccs->hContact, "Status", ID_STATUS_OFFLINE);
			nContacts = HIWORD(ccs->wParam);

			// Failures
			if (!icqOnline)
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_CONTACTS, Translate("You cannot send messages when you are offline."));
			}
			else if (!hContactsList || (nContacts < 1) || (nContacts > MAX_CONTACTSSEND))
			{
				dwCookie = GenerateCookie(0);
				icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_CONTACTS, Translate("Bad data (internal error #1)"));
			}
			// OK
			else
			{
				int nBodyLength;
				char szUin[17];
				char szCount[17];
				struct icq_contactsend_s* contacts = NULL;


				// Format the body
				// This is kinda messy, but there is no simple way to do it. First
				// we need to calculate the length of the packet.
				if (contacts = (struct icq_contactsend_s*)calloc(sizeof(struct icq_contactsend_s), nContacts))
				{
					nBodyLength = 0;
					for(i = 0; i < nContacts; i++)
					{
						szProto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContactsList[i], 0);
						if (szProto == NULL || strcmp(szProto, gpszICQProtoName))
							break; // Abort if a non icq contact is found
						contacts[i].uin = ICQGetContactSettingDword(hContactsList[i], UNIQUEIDSETTING, 0);
						// Todo: update this
						contacts[i].szNick = _strdup((char*)CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContactsList[i], 0));
						// Compute this contact's length
						_itoa(contacts[i].uin, szUin, 10);
						nBodyLength += strlen(szUin) + 1;
						nBodyLength += strlen(contacts[i].szNick) + 1;
					}

					if (i == nContacts)
					{
						message_cookie_data* pCookieData;
						char* pBody;
						char* pBuffer;

#ifdef _DEBUG
						NetLog_Server("Sending contacts to %d.", dwUin);
#endif

						// Compute count record's length
						_itoa(nContacts, szCount, 10);
						nBodyLength += strlen(szCount) + 1;

						// Finally we need to copy the contact data into the packet body
						pBuffer = pBody = (char *)calloc(1, nBodyLength);
						strncpy(pBuffer, szCount, nBodyLength);
						pBuffer += strlen(pBuffer);
						*pBuffer++ = (char)0xFE;
						for (i = 0; i < nContacts; i++)
						{
							_itoa(contacts[i].uin, szUin, 10);
							strcpy(pBuffer, szUin);
							pBuffer += strlen(pBuffer);
							*pBuffer++ = (char)0xFE;
							strcpy(pBuffer, contacts[i].szNick);
							pBuffer += strlen(pBuffer);
							*pBuffer++ = (char)0xFE;
						}

						// Set up the ack type
						pCookieData = malloc(sizeof(message_cookie_data));
						pCookieData->bMessageType = MTYPE_CONTACTS;
						if (!ICQGetContactSettingByte(NULL, "SlowSend", 1))
							pCookieData->nAckType = ACKTYPE_NONE;
						else if ((!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY)) ||
							(wRecipientStatus == ID_STATUS_OFFLINE))
						{
							pCookieData->nAckType = ACKTYPE_SERVER;
						}
						else
						{
							pCookieData->nAckType = ACKTYPE_CLIENT;
						}

            if (gbDCMsgEnabled && IsDirectConnectionOpen(ccs->hContact, DIRECTCONN_STANDARD))
            {
              int iRes = icq_SendDirectMessage(dwUin, ccs->hContact, pBody, nBodyLength, 1, pCookieData, NULL);
              if (iRes) return iRes; // we succeded, return
            }

						// Select channel and send
						if (!CheckContactCapabilities(ccs->hContact, CAPF_SRV_RELAY) ||
							wRecipientStatus == ID_STATUS_OFFLINE)
						{
							dwCookie = icq_SendChannel4Message(dwUin, MTYPE_CONTACTS,
								(WORD)nBodyLength, pBody, pCookieData);
						}
						else
						{
							WORD wPriority;

							if (wRecipientStatus == ID_STATUS_ONLINE || wRecipientStatus == ID_STATUS_FREECHAT)
								wPriority = 0x0001;
							else
								wPriority = 0x0021;

							dwCookie = icq_SendChannel2Message(dwUin, pBody, nBodyLength, wPriority, pCookieData, NULL);
						}


						// This will stop the message dialog from waiting for the real message delivery ack
						if (pCookieData->nAckType == ACKTYPE_NONE)
					 	{
							icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_SUCCESS, ACKTYPE_CONTACTS, NULL);
							// We need to free this here since we will never see the real ack
							// The actual cookie value will still have to be returned to the message dialog though
							SAFE_FREE(&pCookieData);
							FreeCookie(dwCookie);
						}
					}
					else
					{
						dwCookie = GenerateCookie(0);
						icq_SendProtoAck(ccs->hContact, dwCookie, ACKRESULT_FAILED, ACKTYPE_CONTACTS, Translate("Bad data (internal error #2)"));
					}

					for(i = 0; i < nContacts; i++)
					{
						SAFE_FREE(&contacts[i].szNick);
					}

					SAFE_FREE(&contacts);
				}
				else
				{
				  dwCookie = 0;
				}
			}

			return dwCookie;
		}
	}

	// Exit with Failure
	return 0;
}



int IcqSendFile(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact && ccs->lParam && ccs->wParam)
		{
			HANDLE hContact = ccs->hContact;
			char** files = (char**)ccs->lParam;
			char* pszDesc = (char*)ccs->wParam;
      DWORD dwUin;

      if (ICQGetContactSettingUID(hContact, &dwUin, NULL))
        return 0; // Invalid contact

			if (dwUin)
			{
				if (ICQGetContactSettingWord(hContact, "Status", ID_STATUS_OFFLINE) != ID_STATUS_OFFLINE)
				{
					WORD wClientVersion;

					wClientVersion = ICQGetContactSettingWord(ccs->hContact, "Version", 7);
					if (wClientVersion < 7)
					{
						NetLog_Server("IcqSendFile() can't send to version %u", wClientVersion);
					}
					else
					{
						int i;
						filetransfer* ft;
						struct _stat statbuf;

						// Initialize filetransfer struct
						ft = calloc(1,sizeof(filetransfer));

						ft->bMessageType = MTYPE_FILEREQ;

						// Select DC protocol
						ft->nVersion = (wClientVersion == 7) ? 7: 8;

						for (ft->dwFileCount = 0; files[ft->dwFileCount]; ft->dwFileCount++);
						ft->status = 0;
						ft->files = (char **)malloc(sizeof(char *) * ft->dwFileCount);
						ft->dwTotalSize = 0;
						for (i = 0; i < (int)ft->dwFileCount; i++)
						{
							ft->files[i] = _strdup(files[i]);

							if (_stat(files[i], &statbuf))
								NetLog_Server("IcqSendFile() was passed invalid filename(s)");
							else
								ft->dwTotalSize += statbuf.st_size;
						}
						ft->szDescription = _strdup(pszDesc);
						ft->dwUin = dwUin;
						ft->hContact = hContact;
						ft->dwTransferSpeed = 100;
						ft->sending = 1;
						ft->fileId = -1;
						ft->iCurrentFile = 0;
						ft->dwCookie = AllocateCookie(0, dwUin, ft);
						ft->hConnection = NULL;

						// Send file transfer request
						{
							char szFiles[64];
							char* pszFiles;


							NetLog_Server("Init file send");

							if (ft->dwFileCount == 1)
							{
								pszFiles = strrchr(ft->files[0], '\\');
								if (pszFiles)
									pszFiles++;
								else
									pszFiles = ft->files[0];
							}
							else
							{
								null_snprintf(szFiles, 64, "%d Files", ft->dwFileCount);
								pszFiles = szFiles;
							}

							// Send packet
							{
								if (ft->nVersion == 7)
								{
                  if (gbDCMsgEnabled && IsDirectConnectionOpen(hContact, DIRECTCONN_STANDARD))
                  {
                    int iRes = icq_sendFileSendDirectv7(dwUin, hContact, (WORD)ft->dwCookie, pszFiles, ft->szDescription, ft->dwTotalSize); 
                    if (iRes) return (int)(HANDLE)ft; // Success
                  }
									NetLog_Server("Sending v%u file transfer request through server", 8);
                  icq_sendFileSendServv7(dwUin, ft->dwCookie, pszFiles, ft->szDescription, ft->dwTotalSize);
								}
								else
								{
                  if (gbDCMsgEnabled && IsDirectConnectionOpen(hContact, DIRECTCONN_STANDARD))
                  {
                    int iRes = icq_sendFileSendDirectv8(dwUin, hContact, (WORD)ft->dwCookie, pszFiles, ft->szDescription, ft->dwTotalSize); 
                    if (iRes) return (int)(HANDLE)ft; // Success
                  }
									NetLog_Server("Sending v%u file transfer request through server", 8);
									icq_sendFileSendServv8(dwUin, ft->dwCookie, pszFiles, ft->szDescription, ft->dwTotalSize, 0);
								}
							}
						}

						return (int)(HANDLE)ft; // Success
					}
				}
			}
		}
	}

	return 0; // Failure
}


int IcqSendAuthRequest(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;
    DWORD dwUin;

		if (ccs->hContact)
		{
      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
        return 1; // Invalid contact

			if (dwUin && ccs->lParam)
			{
				icq_sendAuthReqServ(dwUin, (char *)ccs->lParam);

				return 0; // Success
			}
		}
	}

	return 1; // Failure
}



int IcqSendYouWereAdded(WPARAM wParam, LPARAM lParam)
{
	if (lParam && icqOnline)
	{
		CCSDATA* ccs = (CCSDATA*)lParam;

		if (ccs->hContact)
		{
			DWORD dwUin, dwMyUin;

      if (ICQGetContactSettingUID(ccs->hContact, &dwUin, NULL))
        return 1; // Invalid contact

			dwMyUin = ICQGetContactSettingDword(NULL, UNIQUEIDSETTING, 0);

			if (dwUin)
			{
				icq_sendYouWereAddedServ(dwUin, dwMyUin);

				return 0; // Success
			}
		}
	}

	return 1; // Failure
}



int IcqGrantAuthorization(WPARAM wParam, LPARAM lParam)
{
  if (gnCurrentStatus != ID_STATUS_OFFLINE && gnCurrentStatus != ID_STATUS_CONNECTING && wParam != 0)
  {
    DWORD dwUin;
    uid_str szUid;

    if (ICQGetContactSettingUID((HANDLE)wParam, &dwUin, &szUid))
      return 0; // Invalid contact

    // send without reason, do we need any ?
    icq_sendGrantAuthServ(dwUin, szUid, NULL);
  }

  return 0;
}



int IcqSendUserIsTyping(WPARAM wParam, LPARAM lParam)
{
	int nResult = 1;
	HANDLE hContact = (HANDLE)wParam;


	if (hContact && icqOnline)
	{
		if (CheckContactCapabilities(hContact, CAPF_TYPING))
		{
  		switch (lParam)
			{
			case PROTOTYPE_SELFTYPING_ON:
				sendTypingNotification(hContact, MTN_BEGUN);
				nResult = 0;
				break;

			case PROTOTYPE_SELFTYPING_OFF:
				sendTypingNotification(hContact, MTN_FINISHED);
				nResult = 0;
				break;

			default:
				break;
			}
		}
	}

	return nResult;
}



/*
   ---------------------------------
   |           Receiving           |
   ---------------------------------
*/

int IcqRecvAwayMsg(WPARAM wParam,LPARAM lParam)
{
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;


  ICQBroadcastAck(ccs->hContact, ACKTYPE_AWAYMSG, ACKRESULT_SUCCESS,
		(HANDLE)pre->lParam, (LPARAM)pre->szMessage);

	return 0;
}



int IcqRecvMessage(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;


	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");

	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = gpszICQProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = (pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_MESSAGE;
	dbei.cbBlob = strlen(pre->szMessage) + 1;
	if ( pre->flags & PREF_UNICODE )
		dbei.cbBlob *= ( sizeof( wchar_t )+1 );
	dbei.pBlob = (PBYTE)pre->szMessage;

	CallService(MS_DB_EVENT_ADD, (WPARAM)ccs->hContact, (LPARAM)&dbei);

  // stop contact from typing - some clients do not sent stop notify
  if (CheckContactCapabilities(ccs->hContact, CAPF_TYPING))
    CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)ccs->hContact, PROTOTYPE_CONTACTTYPING_OFF);

  return 0;
}



int IcqRecvUrl(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;
	char* szDesc;


	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");

	szDesc = pre->szMessage + strlen(pre->szMessage) + 1;

	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = gpszICQProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = (pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_URL;
	dbei.cbBlob = strlen(pre->szMessage) + strlen(szDesc) + 2;
	dbei.pBlob = (PBYTE)pre->szMessage;

	CallService(MS_DB_EVENT_ADD, (WPARAM)ccs->hContact, (LPARAM)&dbei);

	return 0;
}



int IcqRecvContacts(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei = {0};
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;
	ICQSEARCHRESULT** isrList = (ICQSEARCHRESULT**)pre->szMessage;
	int i;
	char szUin[17];
	PBYTE pBlob;


	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");

	for (i = 0; i < pre->lParam; i++)
	{
		dbei.cbBlob += strlen(isrList[i]->hdr.nick) + 1;
		_itoa(isrList[i]->uin, szUin, 10);
		dbei.cbBlob += strlen(szUin) + 1;
	}
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = gpszICQProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = (pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_CONTACTS;
	dbei.pBlob = (PBYTE)malloc(dbei.cbBlob);
	for (i = 0, pBlob = dbei.pBlob; i < pre->lParam; i++)
	{
		strcpy(pBlob, isrList[i]->hdr.nick); pBlob += strlen(pBlob) + 1;
		_itoa(isrList[i]->uin, szUin, 10);
		strcpy(pBlob, szUin); pBlob += strlen(pBlob) + 1;
	}

	CallService(MS_DB_EVENT_ADD, (WPARAM)ccs->hContact, (LPARAM)&dbei);

	SAFE_FREE(&dbei.pBlob);

	return 0;
}



int IcqRecvFile(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;
	char* szDesc;
	char* szFile;


	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");

	szFile = pre->szMessage + sizeof(DWORD);
	szDesc = szFile + strlen(szFile) + 1;

	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = gpszICQProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags = (pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_FILE;
	dbei.cbBlob = sizeof(DWORD) + strlen(szFile) + strlen(szDesc) + 2;
	dbei.pBlob = (PBYTE)pre->szMessage;

	CallService(MS_DB_EVENT_ADD, (WPARAM)ccs->hContact, (LPARAM)&dbei);

	return 0;
}



int IcqRecvAuth(WPARAM wParam, LPARAM lParam)
{
	DBEVENTINFO dbei;
	CCSDATA* ccs = (CCSDATA*)lParam;
	PROTORECVEVENT* pre = (PROTORECVEVENT*)ccs->lParam;


	DBDeleteContactSetting(ccs->hContact, "CList", "Hidden");

	ZeroMemory(&dbei, sizeof(dbei));
	dbei.cbSize = sizeof(dbei);
	dbei.szModule = gpszICQProtoName;
	dbei.timestamp = pre->timestamp;
	dbei.flags=(pre->flags & PREF_CREATEREAD) ? DBEF_READ : 0;
	dbei.eventType = EVENTTYPE_AUTHREQUEST;
	dbei.cbBlob = pre->lParam;
	dbei.pBlob = (PBYTE)pre->szMessage;

	CallService(MS_DB_EVENT_ADD, (WPARAM)NULL, (LPARAM)&dbei);

	return 0;
}
