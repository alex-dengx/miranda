/*
 * $Id$
 *
 * myYahoo Miranda Plugin 
 *
 * Authors: Gennady Feldman (aka Gena01) 
 *          Laurent Marechal (aka Peorth)
 *
 * This code is under GPL and is based on AIM, MSN and Miranda source code.
 * I want to thank Robert Rainwater and George Hazan for their code and support
 * and for answering some of my questions during development of this plugin.
 */
#include <time.h>
#include <malloc.h>
#include <sys/stat.h>

#include "yahoo.h"
#include "http_gateway.h"
#include "version.h"
#include "resource.h"

#include <m_system.h>
#include <m_langpack.h>
#include <m_options.h>
#include <m_skin.h>
#include <m_message.h>
#include <m_idle.h>
#include <m_userinfo.h>
#include <m_png.h>

static BOOL CALLBACK AvatarDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int YAHOO_SaveBitmapAsAvatar( HBITMAP hBitmap, const char* szFileName );
HBITMAP YAHOO_StretchBitmap( HBITMAP hBitmap );

extern yahoo_local_account *ylad;
/*
 * The Following PNG related stuff copied from MSN. Thanks George!
 */
BOOL YAHOO_LoadPngModule()
{
	if ( !ServiceExists(MS_DIB2PNG) || !ServiceExists(MS_PNG2DIB)) {
		//DialogBox( hInst, MAKEINTRESOURCE( IDD_GET_PNG2DIB ), NULL, LoadPng2dibProc );
		MessageBox( NULL, Translate( "Your png2dib.dll is either obsolete or damaged. " ),
				Translate( "Error" ), MB_OK | MB_ICONSTOP );
		return FALSE;
	}

	return TRUE;		
}

int OnDetailsInit(WPARAM wParam, LPARAM lParam)
{
  char* szProto;
  OPTIONSDIALOGPAGE odp;
  char szAvtCaption[MAX_PATH+8];

  //MessageBox(NULL, "HALLO!!", "AA", MB_OK);
  
  szProto = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO, lParam, 0);
  if ((lstrcmp(szProto, yahooProtocolName)) && lParam)
    return 0;

  //MessageBox(NULL, "HALLO 123!!", "AA", MB_OK);
  
  if ((lParam == 0) && YAHOO_GetByte( "ShowAvatars", 0 ))
  {
	 // Avatar page only for valid contacts
	  odp.cbSize = sizeof(odp);
	  odp.hIcon = NULL;
	  odp.hInstance = hinstance;
      odp.pfnDlgProc = AvatarDlgProc;
      odp.position = -1899999997;
      odp.pszTemplate = MAKEINTRESOURCE(IDD_INFO_AVATAR);
      snprintf(szAvtCaption, sizeof(szAvtCaption), "%s %s", Translate(yahooProtocolName), Translate("Avatar"));
      odp.pszTitle = szAvtCaption;

	  CallService(MS_USERINFO_ADDPAGE, wParam, (LPARAM)&odp);
  }

  return 0;
}

static char* ChooseAvatarFileName()
{
  char* szDest = (char*)malloc(MAX_PATH+0x10);
  char str[MAX_PATH];
  char szFilter[512];
  OPENFILENAME ofn = {0};
  str[0] = 0;
  szDest[0]='\0';
  CallService(MS_UTILS_GETBITMAPFILTERSTRINGS,sizeof(szFilter),(LPARAM)szFilter);
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.lpstrFilter = szFilter;
  //ofn.lpstrFilter = "PNG Bitmaps (*.png)\0*.png\0";
  ofn.lpstrFile = szDest;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  ofn.nMaxFile = MAX_PATH;
  ofn.nMaxFileTitle = MAX_PATH;
  ofn.lpstrDefExt = "png";
  if (!GetOpenFileName(&ofn)){
    free(szDest);
    return NULL;
  }

  return szDest;
}

/*
 *31 bit hash function  - this is based on g_string_hash function from glib
 */
int YAHOO_avt_hash(const char *key, long ksize)
{
  const char *p = key;
  int h = *p;
  long l = 0;
  
  if (h)
	for (p += 1; l < ksize; p++, l++)
	  h = (h << 5) - h + *p;

  return h;
}
	
int calcMD5Hash(char* szFile)
{
  if (szFile) {
    HANDLE hFile = NULL, hMap = NULL;
    BYTE* ppMap = NULL;
    long cbFileSize = 0;
    int ck = 0;	

    if ((hFile = CreateFile(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL )) != INVALID_HANDLE_VALUE)
      if ((hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL)) != NULL)
        if ((ppMap = (BYTE*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)) != NULL)
          cbFileSize = GetFileSize( hFile, NULL );

    if (cbFileSize != 0){
      ck = YAHOO_avt_hash((char *)ppMap, cbFileSize);
    }

    if (ppMap != NULL) UnmapViewOfFile(ppMap);
    if (hMap  != NULL) CloseHandle(hMap);
    if (hFile != NULL) CloseHandle(hFile);

    if (ck) return ck;
  }
  
  return 0;
}

void upload_avt(int id, int fd, int error, void *data);

/**************** Send Avatar ********************/
void __cdecl yahoo_send_avt_thread(void *psf) 
{
	y_filetransfer *sf = psf;
	long tFileSize = 0;
	struct _stat statbuf;
	
	if (sf == NULL) {
		YAHOO_DebugLog("SF IS NULL!!!");
		return;
	}
	
	YAHOO_DebugLog("[Uploading avatar] filename: %s ", sf->filename);
	
	if ( _stat( sf->filename, &statbuf ) == 0 ) {
		tFileSize = statbuf.st_size;

		yahoo_send_avatar(ylad->id, sf->filename, tFileSize, &upload_avt, sf);
	}

	free(sf->filename);
	free(sf);
}

HBITMAP YAHOO_SetAvatar(const char *szFile)
{
	char szMyFile[MAX_PATH+1];
    HBITMAP avt;

	avt = (HBITMAP)CallService(MS_UTILS_LOADBITMAP, 0, (WPARAM)szFile);
	
	if (avt == NULL)
		return NULL;
	  
	if (( avt = YAHOO_StretchBitmap( avt )) == NULL )
		return NULL;
	
	GetAvatarFileName(NULL, szMyFile, MAX_PATH, 2);
	  
	if (avt && YAHOO_SaveBitmapAsAvatar( avt, szMyFile) == 0) {
		unsigned int hash;
				
		hash = calcMD5Hash(szMyFile);
		if (hash) {
			YAHOO_DebugLog("[Avatar Info] File: '%s' CK: %d", szMyFile, hash);	
			  
			/* now check and make sure we don't reupload same thing over again */
			if (hash != YAHOO_GetDword("AvatarHash", 0) || time(NULL) > YAHOO_GetDword("AvatarTS",0)) {
				YAHOO_SetString(NULL, "AvatarFile", szMyFile);
				DBWriteContactSettingDword(NULL, yahooProtocolName, "AvatarHash", hash);
			
				YAHOO_SendAvatar(szMyFile);
			} else {
				YAHOO_DebugLog("[Avatar Info] Same checksum and avatar on YahooFT. Not Reuploading.");	  
			}
		}
	}

	return avt;
}

static BOOL CALLBACK AvatarDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_INITDIALOG:
	  //MessageBox(NULL, "HALLO AVATARS!!", "AA", MB_OK);
    TranslateDialogDefault(hwndDlg);
    {
	  DBVARIANT dbv;
      char szAvatar[MAX_PATH];

		ShowWindow(GetDlgItem(hwndDlg, -1), SW_SHOW);
        if (!yahooLoggedIn){
          EnableWindow(GetDlgItem(hwndDlg, IDC_SETAVATAR), FALSE);
          EnableWindow(GetDlgItem(hwndDlg, IDC_DELETEAVATAR), FALSE);
		  EnableWindow(GetDlgItem(hwndDlg, IDC_SHARE_AVATAR), FALSE);
        }

		SetButtonCheck( hwndDlg, IDC_SHARE_AVATAR, YAHOO_GetByte( "ShareAvatar", 0 ) == 2 );
		
		if (!DBGetContactSetting(NULL, yahooProtocolName, "AvatarFile", &dbv)) {
			HBITMAP avt;

			lstrcpy(szAvatar, dbv.pszVal);
			DBFreeVariant(&dbv);
			
			avt = (HBITMAP)CallService(MS_UTILS_LOADBITMAP, 0, (WPARAM)szAvatar);
			if (avt) {
				avt = (HBITMAP)SendDlgItemMessage(hwndDlg, IDC_AVATAR, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)avt);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SHARE_AVATAR), TRUE);
				if (avt) DeleteObject(avt); // we release old avatar if any
			}
		} else {
			EnableWindow(GetDlgItem(hwndDlg, IDC_DELETEAVATAR), FALSE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SHARE_AVATAR), FALSE);
		}
    }
    return TRUE;
  
  case WM_COMMAND:
    switch(LOWORD(wParam))
    {
    case IDC_SETAVATAR:
      {
        char* szFile;
        HBITMAP avt;
		
        if ((szFile = ChooseAvatarFileName()) != NULL) {
		  avt = YAHOO_SetAvatar(szFile);

		  free(szFile);

		  if (avt){
			avt = (HBITMAP)SendDlgItemMessage(hwndDlg, IDC_AVATAR, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)avt);
			EnableWindow(GetDlgItem(hwndDlg, IDC_DELETEAVATAR), TRUE);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SHARE_AVATAR), TRUE);
		  }
		  if (avt) DeleteObject(avt); // we release old avatar if any
		}
		
	  }
      break;
    case IDC_DELETEAVATAR:
      {
        HBITMAP avt;

		YAHOO_DebugLog("[Deleting Avatar Info]");	
		
        avt = (HBITMAP)SendDlgItemMessage(hwndDlg, IDC_AVATAR, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
        if (avt) DeleteObject(avt); // we release old avatar if any
		
		/* remove ALL our Avatar Info Keys */
		DBDeleteContactSetting(NULL, yahooProtocolName, "AvatarFile");	
		DBDeleteContactSetting(NULL, yahooProtocolName, "AvatarHash");
		DBDeleteContactSetting(NULL, yahooProtocolName, "AvatarURL");	
		DBDeleteContactSetting(NULL, yahooProtocolName, "AvatarTS");	
		
		/* Send a Yahoo packet saying we don't got an avatar anymore */
		YAHOO_set_avatar(0);
		
		/* clear the avatar window */
		InvalidateRect( hwndDlg, NULL, TRUE );
		
		YAHOO_SetByte("ShareAvatar",0);
		SetButtonCheck(hwndDlg, IDC_SHARE_AVATAR, 0);
		EnableWindow(GetDlgItem(hwndDlg, IDC_DELETEAVATAR), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_SHARE_AVATAR), FALSE);
      }
      break;
	case IDC_SHARE_AVATAR:
		YAHOO_SetByte("ShareAvatar",IsDlgButtonChecked(hwndDlg, IDC_SHARE_AVATAR) ? 2 : 0);
		/* Send a Yahoo packet saying we don't got an avatar anymore */
		YAHOO_set_avatar(YAHOO_GetByte( "ShareAvatar", 0 )? 2 : 0);
    }
    break;
  }

  return FALSE;
}

// 
// YAHOO_StretchBitmap (copied from MSN, Thanks George)
//
HBITMAP YAHOO_StretchBitmap( HBITMAP hBitmap )
{
	BITMAPINFO bmStretch; 
	BITMAP bmp;
	UINT* ptPixels;
	HDC hDC, hBmpDC;
	HBITMAP hOldBitmap1, hOldBitmap2, hStretchedBitmap;
	int side, dx, dy;
	
	ZeroMemory(&bmStretch, sizeof(bmStretch));
	bmStretch.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmStretch.bmiHeader.biWidth = 96;
	bmStretch.bmiHeader.biHeight = 96;
	bmStretch.bmiHeader.biPlanes = 1;
	bmStretch.bmiHeader.biBitCount = 32;

	hStretchedBitmap = CreateDIBSection( NULL, &bmStretch, DIB_RGB_COLORS, ( void* )&ptPixels, NULL, 0);
	if ( hStretchedBitmap == NULL ) {
		YAHOO_DebugLog( "Bitmap creation failed with error %ld", GetLastError() );
		return NULL;
	}

	hDC = CreateCompatibleDC( NULL );
	hOldBitmap1 = ( HBITMAP )SelectObject( hDC, hBitmap );
	GetObject( hBitmap, sizeof( BITMAP ), &bmp );

	hBmpDC = CreateCompatibleDC( hDC );
	hOldBitmap2 = ( HBITMAP )SelectObject( hBmpDC, hStretchedBitmap );

	if ( bmp.bmWidth > bmp.bmHeight ) {
		side = bmp.bmHeight;
		dx = ( bmp.bmWidth - bmp.bmHeight )/2;
		dy = 0;
	}
	else {
		side = bmp.bmWidth;
		dx = 0;
		dy = ( bmp.bmHeight - bmp.bmWidth )/2;
	}

	SetStretchBltMode( hBmpDC, HALFTONE );
	StretchBlt( hBmpDC, 0, 0, 96, 96, hDC, dx, dy, side, side, SRCCOPY );

	SelectObject( hDC, hOldBitmap1 );
	DeleteObject( hBitmap );
	DeleteDC( hDC );

	SelectObject( hBmpDC, hOldBitmap2 );
	DeleteDC( hBmpDC );
	return hStretchedBitmap;
}

//
// YAHOO_SaveBitmapAsAvatar - updates the avatar database settins and file from a bitmap
//
int YAHOO_SaveBitmapAsAvatar( HBITMAP hBitmap, const char* szFileName ) 
{
	BITMAPINFO* bmi;
	HDC hdc;
	HBITMAP hOldBitmap;
	BITMAPINFOHEADER* pDib;
	BYTE* pDibBits;
	long dwPngSize = 0;
	DIB2PNG convertor;
	
	if ( !YAHOO_LoadPngModule())
		return 1;

	hdc = CreateCompatibleDC( NULL );
	hOldBitmap = ( HBITMAP )SelectObject( hdc, hBitmap );

	bmi = ( BITMAPINFO* )_alloca( sizeof( BITMAPINFO ) + sizeof( RGBQUAD )*256 );
	memset( bmi, 0, sizeof (BITMAPINFO ));
	bmi->bmiHeader.biSize = 0x28;
	if ( GetDIBits( hdc, hBitmap, 0, 96, NULL, bmi, DIB_RGB_COLORS ) == 0 ) {
		/*TWinErrorCode errCode;
		MSN_ShowError( "Unable to get the bitmap: error %d (%s)", errCode.mErrorCode, errCode.getText() );*/
		return 2;
	}

	pDib = ( BITMAPINFOHEADER* )GlobalAlloc( LPTR, sizeof( BITMAPINFO ) + sizeof( RGBQUAD )*256 + bmi->bmiHeader.biSizeImage );
	if ( pDib == NULL )
		return 3;

	memcpy( pDib, bmi, sizeof( BITMAPINFO ) + sizeof( RGBQUAD )*256 );
	pDibBits = (( BYTE* )pDib ) + sizeof( BITMAPINFO ) + sizeof( RGBQUAD )*256;

	GetDIBits( hdc, hBitmap, 0, pDib->biHeight, pDibBits, ( BITMAPINFO* )pDib, DIB_RGB_COLORS );
	SelectObject( hdc, hOldBitmap );
	DeleteDC( hdc );

	convertor.pbmi = ( BITMAPINFO* )pDib;
	convertor.pDiData = pDibBits;
	convertor.pResult = NULL;
	convertor.pResultLen = &dwPngSize;
	if ( !CallService( MS_DIB2PNG, 0, (LPARAM)&convertor )) {

		GlobalFree( pDib );
		return 2;
	}

	convertor.pResult = (char *)malloc(dwPngSize);
	CallService( MS_DIB2PNG, 0, (LPARAM)&convertor );

	GlobalFree( pDib );
	{	
		FILE* out;
		char tFileName[ MAX_PATH ];
		GetAvatarFileName( NULL, tFileName, sizeof tFileName, 2);
		out = fopen( tFileName, "wb" );
		if ( out != NULL ) {
			fwrite( convertor.pResult, dwPngSize, 1, out );
			fclose( out );
		}	
	}
	free(convertor.pResult);

	return ERROR_SUCCESS;
}

void upload_avt(int id, int fd, int error, void *data)
{
    y_filetransfer *sf = (y_filetransfer*) data;
    long size = 0, rsize = 0;
	char buf[1024];
	DWORD dw;
	struct _stat statbuf;
	
	if (fd < 0) {
		LOG(("[get_fd] Connect Failed!"));
		error = 1;
	}

	if (_stat( sf->filename, &statbuf ) != 0 )
		error = 1;
	
    if(!error) {
		 HANDLE myhFile    = CreateFile(sf->filename,
                                   GENERIC_READ,
                                   FILE_SHARE_READ|FILE_SHARE_WRITE,
			           NULL,
			           OPEN_EXISTING,
			           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			           0);


		if(myhFile !=INVALID_HANDLE_VALUE) {
			//LOG(("proto: %s, hContact: %p", yahooProtocolName, sf->hContact));
			NETLIBSELECT nls;
			
			nls.cbSize = sizeof(nls);
			nls.dwTimeout = 10; // 1000 millis = 1 sec 

			nls.hReadConns[0] = (HANDLE)fd;
			nls.hReadConns[1] = NULL;
			nls.hWriteConns[0] = NULL;
			nls.hExceptConns[0] = NULL;

			LOG(("Sending file: %s", sf->filename));
			
			do {
				//LOG(("Peeking..."));
				//dw = Netlib_Recv((HANDLE)fd, buf, 1024, MSG_PEEK/*MSG_NODUMP*/);
				dw = CallService(MS_NETLIB_SELECT, (WPARAM) 0, (LPARAM)&nls);
				
				//LOG(("Peeked: %ld bytes", dw));
				if (dw > 0) {
					dw = Netlib_Recv((HANDLE)fd, buf, 1024, 0);
					LOG(("Got: %ld bytes", dw));
					if (dw > 0 && rsize == 0) {
						//parse the response code we expecting 200 OK here
						if (strnicmp(buf,"HTTP/", 5) == 0) {
							int z=8; // HTTP/1.0
							
							while (z < dw && buf[z] == ' ') z++;
							
							if (strnicmp(&buf[z],"200", 3) != 0) {
								// we got a problem!!!!
								sf->cancel = 1; //cancel things
								LOG(("NOT A 200 OK response!!!"));
							} else {
								LOG(("200 OK response."));
							}
						}
					} else if (dw == 0) {
						//connection alive but all data received??
						
						LOG(("Connection read ready, but no data to read?"));
						LOG(("Bytes read: %ld", rsize));
						break;
					} else if (dw < 0) {
						LOG(("Connection closed?"));
						LOG(("Bytes read: %ld", rsize));
						break;
					}
					rsize +=dw;
				} else if (dw < 0) {
					// something went wrong
					LOG(("Negative result? Connection closed?"));
					
					if (size >= statbuf.st_size) {
						// done sending, just break the loop
						break;
					} else {
						LOG(("Upload Failed. Receive error?"));
						error = 1;
					}
				}

				if (size < statbuf.st_size) {
					ReadFile(myhFile, buf, 1024, &dw, NULL);
				
					if (dw) {
						//dw = send(fd, buf, dw, 0);
						dw = Netlib_Send((HANDLE)fd, buf, dw, MSG_NODUMP);
						
						if (dw < 1) {
							LOG(("Upload Failed. Send error?"));
							error = 1;
							break;
						}
						
						size += dw;
					}
				}
					
				if (sf->cancel) {
					LOG(("Upload Cancelled! "));
					error = 1;
					break;
				}
			} while (dw >= 0 && !error);
			
			CloseHandle(myhFile);
		}
    }	

	//sf->state = FR_STATE_DONE;
    LOG(("File send complete!"));
}

void YAHOO_SendAvatar(const char *szFile)
{
	y_filetransfer *sf;

	sf = (y_filetransfer*) malloc(sizeof(y_filetransfer));
	sf->filename = strdup(szFile);
	sf->cancel = 0;
	pthread_create(yahoo_send_avt_thread, sf);
}

void yahoo_reset_avatar(HANDLE 	hContact, int bFail)
{
    PROTO_AVATAR_INFORMATION AI;
        
	LOG(("[YAHOO_RESET_AVATAR]"));
	//DBDeleteContactSetting(hContact, yahooProtocolName, "PictCK" );
	DBWriteContactSettingDword(hContact, yahooProtocolName, "PictCK", 0);

	AI.cbSize = sizeof AI;
	AI.format = PA_FORMAT_BMP;
	AI.hContact = hContact;

	GetAvatarFileName(AI.hContact, AI.filename, sizeof AI.filename, DBGetContactSettingByte(hContact, yahooProtocolName,"AvatarType", 0));
	DeleteFile(AI.filename);
	
	// STUPID SCRIVER Doesn't listen to ACKTYPE_AVATAR. so remove the file reference!
	DBDeleteContactSetting(AI.hContact, "ContactPhoto", "File");	
	//AI.filename[0]='\0';
	if (bFail)
		ProtoBroadcastAck(yahooProtocolName, hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED,(HANDLE) &AI, 0);
}

void YAHOO_request_avatar(const char* who)
{
	time_t  last_chk, cur_time;
	HANDLE 	hContact = 0;
	char    szFile[MAX_PATH];
	
	if (!YAHOO_GetByte( "ShowAvatars", 0 )) {
		LOG(("Avatars disabled, but available for: %s", who));
		return;
	}
	
	hContact = getbuddyH(who);
	
	if (!hContact)
		return;
	
	
	GetAvatarFileName(hContact, szFile, sizeof szFile, DBGetContactSettingByte(hContact, yahooProtocolName,"AvatarType", 0));
	DeleteFile(szFile);
	
	time(&cur_time);
	last_chk = DBGetContactSettingDword(hContact, yahooProtocolName, "PictLastCheck", 0);
	
	/*
	 * time() - in seconds ( 60*60 = 1 hour)
	 */
	if (DBGetContactSettingDword(hContact, yahooProtocolName,"PictCK", 0) == 0 || 
		last_chk == 0 || (cur_time - last_chk) < 300) {
			
		DBWriteContactSettingDword(hContact, yahooProtocolName, "PictLastCheck", cur_time);

		LOG(("Requesting Avatar for: %s", who));
		yahoo_request_buddy_avatar(ylad->id, who);
	} else {
		LOG(("Avatar Not Available for: %s (Flood Check in Effect)", who));
	}
}

void YAHOO_bcast_picture_update(int buddy_icon)
{
	HANDLE hContact;
	char *szProto;
	
	/* need to get online buddies and then send then picture_update packets (ARGH YAHOO!)*/
	for ( hContact = ( HANDLE )YAHOO_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
		   hContact != NULL;
			hContact = ( HANDLE )YAHOO_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 ))
	{
		szProto = ( char* )YAHOO_CallService( MS_PROTO_GETCONTACTBASEPROTO, ( WPARAM )hContact, 0 );
		if ( szProto != NULL && !lstrcmp( szProto, yahooProtocolName ))
		{
			if (YAHOO_GetWord(hContact, "Status", ID_STATUS_OFFLINE)!=ID_STATUS_OFFLINE) {
							DBVARIANT dbv;

				if ( DBGetContactSetting( hContact, yahooProtocolName, YAHOO_LOGINID, &dbv ))
					continue;

				yahoo_send_picture_update(ylad->id, dbv.pszVal, buddy_icon);
				DBFreeVariant( &dbv );
			}
		}
	}
}

void YAHOO_set_avatar(int buddy_icon)
{
	yahoo_send_avatar_update(ylad->id,buddy_icon);
	
	YAHOO_bcast_picture_update(buddy_icon);
}

void YAHOO_bcast_picture_checksum(int cksum)
{
	HANDLE hContact;
	char *szProto;
	
	yahoo_send_picture_checksum(ylad->id, NULL, cksum);
	
	/* need to get online buddies and then send then picture_update packets (ARGH YAHOO!)*/
	for ( hContact = ( HANDLE )YAHOO_CallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
		   hContact != NULL;
			hContact = ( HANDLE )YAHOO_CallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 ))
	{
		szProto = ( char* )YAHOO_CallService( MS_PROTO_GETCONTACTBASEPROTO, ( WPARAM )hContact, 0 );
		if ( szProto != NULL && !lstrcmp( szProto, yahooProtocolName ))
		{
			if (YAHOO_GetWord(hContact, "Status", ID_STATUS_OFFLINE)!=ID_STATUS_OFFLINE) {
							DBVARIANT dbv;

				if ( DBGetContactSetting( hContact, yahooProtocolName, YAHOO_LOGINID, &dbv ))
					continue;

				yahoo_send_picture_checksum(ylad->id, dbv.pszVal, cksum);
				DBFreeVariant( &dbv );
			}
		}
	}
}

