/*
IRC plugin for Miranda IM

Copyright (C) 2003-05 Jurgen Persson
Copyright (C) 2007-09 George Hazan

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

#ifndef _IRCWIN_H_
#define _IRCWIN_H_

#define MIRANDA_VER  0x0A00
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501

#include "m_stdhdr.h"

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <math.h>
#include <winsock.h>
#include <commctrl.h>
#include <time.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "newpluginapi.h"
#include "m_system.h"
#include "m_system_cpp.h"
#include "m_protocols.h"
#include "m_protomod.h"
#include "m_protosvc.h"
#include "m_protoint.h"
#include "m_clist.h"
#include "m_options.h"
#include "m_database.h"
#include "m_utils.h"
#include "m_skin.h"
#include "m_netlib.h"
#include "m_clui.h"
#include "m_langpack.h"
#include "m_message.h"
#include "m_userinfo.h"
#include "m_addcontact.h"
#include "m_button.h"
#include "m_genmenu.h"
#include "m_file.h"
#include "m_ignore.h"
#include "m_chat.h"
#include "m_icolib.h"
#include "m_ircscript.h"
#include "win2k.h"

#include "resource.h"

#define IRC_QUICKCONNECT      "/QuickConnectMenu"
#define IRC_JOINCHANNEL       "/JoinChannelMenu"
#define IRC_CHANGENICK        "/ChangeNickMenu"
#define IRC_SHOWLIST          "/ShowListMenu"
#define IRC_SHOWSERVER        "/ShowServerMenu"
#define IRC_UM_CHANSETTINGS   "/UMenuChanSettings"
#define IRC_UM_WHOIS          "/UMenuWhois"
#define IRC_UM_DISCONNECT     "/UMenuDisconnect"
#define IRC_UM_IGNORE         "/UMenuIgnore"

#define STR_QUITMESSAGE  "\002Miranda IM!\002 Smaller, Faster, Easier. http://miranda-im.org"
#define STR_USERINFO     "I'm a happy Miranda IM user! Get it here: http://miranda-im.org"
#define STR_AWAYMESSAGE  "I'm away from the computer." // Default away
#define DCCSTRING        " (DCC)"
#define SERVERSMODULE    "IRC Servers"
#define SERVERWINDOW	 _T("Network log")

#define DCC_CHAT		1
#define DCC_SEND		2

#define FILERESUME_CANCEL	11

struct CIrcProto;

#include "mstring.h"
typedef CMStringA String;

class CCallocBase
{
public:
	__inline void* operator new( size_t size )
	{	return ::calloc( 1, size );
	}
	__inline void operator delete( void* p )
	{	::free( p );
	}
};

struct _A2T
{
	_A2T( const char* s ) :
		buf( mir_a2t( s ))
		{}

	_A2T( const char* s, int cp ) :
		buf( mir_a2t_cp( s, cp ))
		{}

	_A2T( const String& s ) :
		buf( mir_a2t( s ))
		{}

	~_A2T()
	{	mir_free(buf);
	}

	__inline operator TCHAR*() const
	{	return buf;
	}

	__inline operator CMString() const
	{	return CMString(buf);
	}

	TCHAR* buf;
};

struct _T2A
{
	_T2A( const TCHAR* s ) :
		buf( mir_t2a(s) )
		{}

	_T2A( const TCHAR* s, int cp ) :
		buf( mir_t2a_cp( s, cp ))
		{}

	_T2A( const CMString& s ) :
		buf( mir_t2a( s ))
		{}

	~_T2A()
	{	mir_free(buf);
	}

	__inline operator char*() const
	{	return buf;
	}

	__inline operator String() const
	{	return String(buf);
	}

	char* buf;
};

// special service for tweaking performance, implemented in chat.dll
#define MS_GC_GETEVENTPTR  "GChat/GetNewEventPtr"
typedef int (*GETEVENTFUNC)(WPARAM wParam, LPARAM lParam);
typedef struct  {
	GETEVENTFUNC pfnAddEvent;
}
	GCPTRS;

#define IP_AUTO       1
#define IP_MANUAL     2

struct IPRESOLVE      // Contains info about the channels
{
	IPRESOLVE( const char* _addr, int _type ) :
		sAddr( _addr ),
		iType( _type )
	{}

	~IPRESOLVE()
	{}

	String     sAddr;
	int        iType;
};

struct CHANNELINFO   // Contains info about the channels
{
	TCHAR* pszTopic;
	TCHAR* pszMode;
	TCHAR* pszPassword;
	TCHAR* pszLimit;
	BYTE   OwnMode;	/* own mode on the channel. Bitmask:
												0: voice
												1: halfop
												2: op
												3: admin
												4: owner		*/
	int    codepage;
};

struct SERVER_INFO  // Contains info about different servers
{
	~SERVER_INFO();

	char *m_name, *m_address, *m_group;
	int  m_portStart, m_portEnd, m_iSSL;
};

struct PERFORM_INFO  // Contains 'm_perform buffer' for different networks
{
	PERFORM_INFO( const char* szSetting, const TCHAR* value ) :
		mSetting( szSetting ),
		mText( value )
	{}

	~PERFORM_INFO()
	{}

	String mSetting;
	CMString mText;
};

struct CONTACT // Contains info about users
{
	TCHAR* name;
	TCHAR* user;
	TCHAR* host;
	bool ExactOnly;
	bool ExactWCOnly;
	bool ExactNick;
};

struct TDbSetting
{
	int    offset;
	char*  name;
	int    type;
	size_t size;
	union
	{
		int    defValue;
		TCHAR* defStr;
	};
};

#include "irclib.h"
using namespace irc;

#include "irc_dlg.h"

/////////////////////////////////////////////////////////////////////////////////////////

struct CIrcProto;
typedef void    ( __cdecl CIrcProto::*IrcThreadFunc )( void* param );
typedef int     ( __cdecl CIrcProto::*IrcEventFunc )( WPARAM, LPARAM );
typedef INT_PTR ( __cdecl CIrcProto::*IrcServiceFunc )( WPARAM, LPARAM );
typedef INT_PTR ( __cdecl CIrcProto::*IrcServiceFuncParam )( WPARAM, LPARAM, LPARAM );

typedef bool (CIrcProto::*PfnIrcMessageHandler)(const CIrcMessage* pmsg);

struct CIrcHandler
{
	CIrcHandler( const TCHAR* _name, PfnIrcMessageHandler _handler ) :
		m_name( _name ),
		m_handler( _handler )
	{}

	const TCHAR* m_name;
	PfnIrcMessageHandler m_handler;
};

struct CIrcProto : public PROTO_INTERFACE, public CCallocBase
{
				CIrcProto( const char*, const TCHAR* );
			   ~CIrcProto();

				// Protocol interface

	virtual	HANDLE __cdecl AddToList( int flags, PROTOSEARCHRESULT* psr );
	virtual	HANDLE __cdecl AddToListByEvent( int flags, int iContact, HANDLE hDbEvent );

	virtual	int    __cdecl Authorize( HANDLE hContact );
	virtual	int    __cdecl AuthDeny( HANDLE hContact, const TCHAR* szReason );
	virtual	int    __cdecl AuthRecv( HANDLE hContact, PROTORECVEVENT* );
	virtual	int    __cdecl AuthRequest( HANDLE hContact, const TCHAR* szMessage );

	virtual	HANDLE __cdecl ChangeInfo( int iInfoType, void* pInfoData );

	virtual	HANDLE __cdecl FileAllow( HANDLE hContact, HANDLE hTransfer, const TCHAR* szPath );
	virtual	int    __cdecl FileCancel( HANDLE hContact, HANDLE hTransfer );
	virtual	int    __cdecl FileDeny( HANDLE hContact, HANDLE hTransfer, const TCHAR* szReason );
	virtual	int    __cdecl FileResume( HANDLE hTransfer, int* action, const TCHAR** szFilename );

	virtual	DWORD_PTR __cdecl GetCaps( int type, HANDLE hContact = NULL );
	virtual	HICON  __cdecl GetIcon( int iconIndex );
	virtual	int    __cdecl GetInfo( HANDLE hContact, int infoType );

	virtual	HANDLE __cdecl SearchBasic( const PROTOCHAR* id );
	virtual	HANDLE __cdecl SearchByEmail( const PROTOCHAR* email );
	virtual	HANDLE __cdecl SearchByName( const PROTOCHAR* nick, const PROTOCHAR* firstName, const PROTOCHAR* lastName );
	virtual	HWND   __cdecl SearchAdvanced( HWND owner );
	virtual	HWND   __cdecl CreateExtendedSearchUI( HWND owner );

	virtual	int    __cdecl RecvContacts( HANDLE hContact, PROTORECVEVENT* );
	virtual	int    __cdecl RecvFile( HANDLE hContact, PROTORECVFILET* );
	virtual	int    __cdecl RecvMsg( HANDLE hContact, PROTORECVEVENT* );
	virtual	int    __cdecl RecvUrl( HANDLE hContact, PROTORECVEVENT* );

	virtual	int    __cdecl SendContacts( HANDLE hContact, int flags, int nContacts, HANDLE* hContactsList );
	virtual	HANDLE __cdecl SendFile( HANDLE hContact, const TCHAR* szDescription, TCHAR** ppszFiles );
	virtual	int    __cdecl SendMsg( HANDLE hContact, int flags, const char* msg );
	virtual	int    __cdecl SendUrl( HANDLE hContact, int flags, const char* url );

	virtual	int    __cdecl SetApparentMode( HANDLE hContact, int mode );
	virtual	int    __cdecl SetStatus( int iNewStatus );

	virtual	HANDLE __cdecl GetAwayMsg( HANDLE hContact );
	virtual	int    __cdecl RecvAwayMsg( HANDLE hContact, int mode, PROTORECVEVENT* evt );
	virtual	int    __cdecl SendAwayMsg( HANDLE hContact, HANDLE hProcess, const char* msg );
	virtual	int    __cdecl SetAwayMsg( int m_iStatus, const TCHAR* msg );

	virtual	int    __cdecl UserIsTyping( HANDLE hContact, int type );

	virtual	int    __cdecl OnEvent( PROTOEVENTTYPE eventType, WPARAM wParam, LPARAM lParam );

	// Services
	INT_PTR __cdecl SvcCreateAccMgrUI( WPARAM, LPARAM );
	INT_PTR __cdecl GetMyAwayMsg( WPARAM, LPARAM );

	INT_PTR __cdecl OnChangeNickMenuCommand( WPARAM, LPARAM );
	INT_PTR __cdecl OnDoubleclicked( WPARAM, LPARAM );
	INT_PTR __cdecl OnJoinChat( WPARAM, LPARAM );
	INT_PTR __cdecl OnJoinMenuCommand( WPARAM, LPARAM );
	INT_PTR __cdecl OnLeaveChat( WPARAM, LPARAM );
	INT_PTR __cdecl OnMenuChanSettings( WPARAM, LPARAM );
	INT_PTR __cdecl OnMenuDisconnect( WPARAM , LPARAM );
	INT_PTR __cdecl OnMenuIgnore( WPARAM, LPARAM );
	INT_PTR __cdecl OnMenuWhois( WPARAM, LPARAM );
	INT_PTR __cdecl OnQuickConnectMenuCommand(WPARAM, LPARAM );
	INT_PTR __cdecl OnShowListMenuCommand( WPARAM, LPARAM );
	INT_PTR __cdecl OnShowServerMenuCommand( WPARAM, LPARAM );

	// Events
	int __cdecl OnContactDeleted( WPARAM, LPARAM );
	int __cdecl OnInitOptionsPages( WPARAM, LPARAM );
	int __cdecl OnInitUserInfo( WPARAM, LPARAM );
	int __cdecl OnModulesLoaded( WPARAM, LPARAM );
	int __cdecl OnMenuPreBuild( WPARAM, LPARAM );
	int __cdecl OnPreShutdown( WPARAM, LPARAM );
	int __cdecl OnDbSettingChanged( WPARAM, LPARAM );

	int __cdecl GCEventHook( WPARAM, LPARAM );
	int __cdecl GCMenuHook( WPARAM, LPARAM );

	// Data

	char     m_serverName[100];
	char     m_password [500];
	TCHAR    m_identSystem[10];
	char     m_network[30];
	char     m_portStart[10];
	char     m_portEnd[10];
	int      m_iSSL;
	TCHAR    m_identPort[10];
	TCHAR    m_retryWait[10];
	TCHAR    m_retryCount[10];
	TCHAR    m_nick[30], m_pNick[30];
	TCHAR    m_alternativeNick[30];
	TCHAR    m_name[200];
	TCHAR    m_userID[200];
	TCHAR    m_quitMessage[400];
	TCHAR    m_userInfo[500];
	char     m_myHost[50];
	char     m_mySpecifiedHost[500];
	char     m_mySpecifiedHostIP[50];
	char     m_myLocalHost[50];
	WORD     m_myLocalPort;
	TCHAR*   m_alias;
	int      m_serverComboSelection;
	int      m_quickComboSelection;
	int      m_onlineNotificationTime;
	int      m_onlineNotificationLimit;
	BYTE     m_scriptingEnabled;
	BYTE     m_IPFromServer;
	BYTE     m_showAddresses;
	BYTE     m_disconnectDCCChats;
	BYTE     m_disableErrorPopups;
	BYTE     m_rejoinChannels;
	BYTE     m_rejoinIfKicked;
	BYTE     m_hideServerWindow;
	BYTE     m_ident;
	BYTE     m_identTimer;
	BYTE     m_retry;
	BYTE     m_disableDefaultServer;
	BYTE     m_autoOnlineNotification;
	BYTE     m_sendKeepAlive;
	BYTE     m_joinOnInvite;
	BYTE     m_perform;
	BYTE     m_forceVisible;
	BYTE     m_ignore;
	BYTE     m_ignoreChannelDefault;
	BYTE     m_useServer;
	BYTE     m_DCCFileEnabled;
	BYTE     m_DCCChatEnabled;
	BYTE     m_DCCChatAccept;
	BYTE     m_DCCChatIgnore;
	BYTE     m_DCCPassive;
	BYTE     m_DCCMode;
	WORD     m_DCCPacketSize;
	BYTE     m_manualHost;
	BYTE     m_oldStyleModes;
	BYTE     m_channelAwayNotification;
	BYTE     m_sendNotice;
	BYTE     m_utfAutodetect;
	int      m_codepage;
	COLORREF colors[16];
	HICON    hIcon[13];

	OBJLIST<CMString> vUserhostReasons;
	OBJLIST<CMString> vWhoInProgress;

	CRITICAL_SECTION cs;
	CRITICAL_SECTION m_gchook;
	CRITICAL_SECTION m_resolve;
	HANDLE           m_evWndCreate;

	CMString m_statusMessage;
	bool    m_bMbotInstalled;
	int     m_iTempCheckTime;

	CIrcSessionInfo si;

	int       m_iRetryCount;
	int       m_portCount;
	DWORD     m_bConnectRequested;
	DWORD     m_bConnectThreadRunning;

	HGENMENU hMenuRoot, hMenuQuick, hMenuServer, hMenuJoin, hMenuNick, hMenuList;
	HANDLE   hNetlib, hNetlibDCC;

	bool  bTempDisableCheck, bTempForceCheck, bEcho;
	bool	nickflag;

	bool     bPerformDone;

	CJoinDlg*    m_joinDlg;
	CListDlg*    m_listDlg;
	CManagerDlg* m_managerDlg;
	CNickDlg*    m_nickDlg;
	CWhoisDlg*   m_whoisDlg;
	CQuickDlg*   m_quickDlg;
	CIgnorePrefsDlg* m_ignoreDlg;
	
	int      m_noOfChannels, m_manualWhoisCount;
	String   sChannelModes, sUserModes;
	CMString sChannelPrefixes, sUserModePrefixes, WhoisAwayReply;

	CDlgBase::CreateParam OptCreateAccount, OptCreateConn, OptCreateIgnore, OptCreateOther;

	//clist.cpp
	HANDLE CList_AddContact(struct CONTACT * user, bool InList, bool SetOnline);
	bool   CList_SetAllOffline(BYTE ChatsToo);
	HANDLE CList_SetOffline(struct CONTACT * user);

	bool   CList_AddEvent(struct CONTACT * user, HICON Icon, HANDLE event, const char * tooltip, int type ) ;
	HANDLE CList_FindContact (struct CONTACT * user);
	BOOL   CList_AddDCCChat(const CMString& name, const CMString& hostmask, unsigned long adr, int port) ;

	//commandmonitor.cpp
	UINT_PTR IdentTimer, InitTimer, KeepAliveTimer, OnlineNotifTimer, OnlineNotifTimer3;	

	int  AddOutgoingMessageToDB(HANDLE hContact, TCHAR* msg);
	bool DoOnConnect(const CIrcMessage *pmsg);
	int  DoPerform(const char* event);
	void __cdecl ResolveIPThread( void* di );

	bool AddIgnore(const TCHAR* mask, const TCHAR* mode, const TCHAR* network) ;
	int  IsIgnored(const CMString& nick, const CMString& address, const CMString& host, char type) ;
	int  IsIgnored(CMString user, char type);
	bool RemoveIgnore(const TCHAR* mask) ;

	//input.cpp
	CMString DoAlias( const TCHAR *text, TCHAR *window);
	BOOL     DoHardcodedCommand( CMString text, TCHAR* window, HANDLE hContact );
	CMString DoIdentifiers( CMString text, const TCHAR* window );
	void     FormatMsg(CMString& text);
	bool     PostIrcMessageWnd(TCHAR* pszWindow, HANDLE hContact,const TCHAR* szBuf);
	bool     PostIrcMessage( const TCHAR* fmt, ...);

	// irclib.cpp
	UINT_PTR	DCCTimer;
	void     SendIrcMessage( const TCHAR*, bool bNotify = true, int codepage = -1 );

	// ircproto.cpp
	void __cdecl AckBasicSearch( void* param );
	void __cdecl AckMessageFail( void* info );
	void __cdecl AckMessageFailDcc( void* info );
	void __cdecl AckMessageSuccess( void* info );

	int  SetStatusInternal( int iNewStatus, bool bIsInternal );

	//options.cpp
	HWND m_hwndConnect;

	OBJLIST<CIrcIgnoreItem> m_ignoreItems;

	int      m_channelNumber;
	CMString m_whoReply;
	CMString sNamesList;
	CMString sTopic;
	CMString sTopicName;
	CMString	sTopicTime;
	CMString m_namesToWho;
	CMString m_channelsToWho;
	CMString m_namesToUserhost;

	void    InitPrefs(void);
	void    InitIgnore(void);

	void    ReadSettings( TDbSetting* sets, int count );
	void    RewriteIgnoreSettings( void );
	void    WriteSettings( TDbSetting* sets, int count );

	//output
	BOOL   ShowMessage (const CIrcMessage* pmsg);

	//scripting.cpp
	INT_PTR  __cdecl Scripting_InsertRawIn(WPARAM wParam,LPARAM lParam);
	INT_PTR  __cdecl Scripting_InsertRawOut(WPARAM wParam,LPARAM lParam);
	INT_PTR  __cdecl Scripting_InsertGuiIn(WPARAM wParam,LPARAM lParam);
	INT_PTR  __cdecl Scripting_InsertGuiOut(WPARAM wParam,LPARAM lParam);
	INT_PTR  __cdecl Scripting_GetIrcData(WPARAM wparam, LPARAM lparam);
	BOOL Scripting_TriggerMSPRawIn(char ** pszRaw);
	BOOL Scripting_TriggerMSPRawOut(char ** pszRaw);
	BOOL Scripting_TriggerMSPGuiIn(WPARAM * wparam, GCEVENT * gce);
	BOOL Scripting_TriggerMSPGuiOut(GCHOOK * gch);

	// services.cpp
	void   ConnectToServer(void);
	void   DisconnectFromServer(void);
	void   DoNetlibLog( const char* fmt, ... );
	void   IrcHookEvent( const char*, IrcEventFunc );
	void   InitMainMenus(void);

	void   ircFork( IrcThreadFunc, void* arg );
	HANDLE ircForkEx( IrcThreadFunc, void* arg );

	UINT_PTR  RetryTimer;

	void __cdecl ConnectServerThread( void* );
	void __cdecl DisconnectServerThread( void* );

	//tools.cpp
	void     AddToJTemp(TCHAR op, CMString& sCommand);
	bool     AddWindowItemData(CMString window, const TCHAR* pszLimit, const TCHAR* pszMode, const TCHAR* pszPassword, const TCHAR* pszTopic);
	INT_PTR  CallChatEvent(WPARAM wParam, LPARAM lParam);
	void     CreateProtoService( const char* serviceName, IrcServiceFunc pFunc );
	INT_PTR  DoEvent(int iEvent, const TCHAR* pszWindow, const TCHAR* pszNick, const TCHAR* pszText, const TCHAR* pszStatus, const TCHAR* pszUserInfo, DWORD_PTR dwItemData, bool bAddToLog, bool bIsMe,time_t timestamp = 1);
	void     FindLocalIP(HANDLE con);
	bool     FreeWindowItemData(CMString window, CHANNELINFO* wis);
	#if defined( _UNICODE )
		bool  IsChannel(const char* sName);
	#endif
	bool     IsChannel(const TCHAR* sName);
	void     KillChatTimer(UINT_PTR &nIDEvent);
	CMString MakeWndID(const TCHAR* sWindow);
	CMString ModeToStatus(int sMode);
	CMString PrefixToStatus(int cPrefix);
	int      SetChannelSBText(CMString sWindow, CHANNELINFO * wi);
	void     SetChatTimer(UINT_PTR &nIDEvent,UINT uElapse, TIMERPROC lpTimerFunc);

	void     ClearUserhostReasons(int type);
	void     DoUserhostWithReason(int type, CMString reason, bool bSendCommand, CMString userhostparams, ...);
	CMString GetNextUserhostReason(int type);
	CMString PeekAtReasons(int type);

	int      getByte( const char* name, BYTE defaultValue );
	int      getByte( HANDLE hContact, const char* name, BYTE defaultValue );
	int      getDword( const char* name, DWORD defaultValue );
	int      getDword( HANDLE hContact, const char* name, DWORD defaultValue );
	int      getString( const char* name, DBVARIANT* );
	int      getString( HANDLE hContact, const char* name, DBVARIANT* );
	int      getTString( const char* name, DBVARIANT* );
	int      getTString( HANDLE hContact, const char* name, DBVARIANT* );
	int      getWord( const char* name, WORD defaultValue );
	int      getWord( HANDLE hContact, const char* name, WORD defaultValue );

	void     setByte( const char* name, BYTE value );
	void     setByte( HANDLE hContact, const char* name, BYTE value );
	void     setDword( const char* name, DWORD value );
	void     setDword( HANDLE hContact, const char* name, DWORD value );
	void     setString( const char* name, const char* value );
	void     setString( HANDLE hContact, const char* name, const char* value );
	void     setTString( const char* name, const TCHAR* value );
	void     setTString( HANDLE hContact, const char* name, const TCHAR* value );
	void     setWord( const char* name, int value );
	void     setWord( HANDLE hContact, const char* name, int value );

	// userinfo.cpp
	void __cdecl AckUserInfoSearch( void* hContact );

	////////////////////////////////////////////////////////////////////////////////////////
	// former CIrcSession class

	void AddDCCSession(HANDLE hContact, CDccSession* dcc);
	void AddDCCSession(DCCINFO*  pdci, CDccSession* dcc);
	void RemoveDCCSession(HANDLE hContact);
	void RemoveDCCSession(DCCINFO*  pdci);
	
	CDccSession* FindDCCSession(HANDLE hContact);
	CDccSession* FindDCCSession(DCCINFO* pdci);
	CDccSession* FindDCCSendByPort(int iPort);
	CDccSession* FindDCCRecvByPortAndName(int iPort, const TCHAR* szName);
	CDccSession* FindPassiveDCCSend(int iToken);
	CDccSession* FindPassiveDCCRecv(CMString sName, CMString sToken);
	
	void DisconnectAllDCCSessions(bool Shutdown);
	void CheckDCCTimeout(void);

	bool Connect(const CIrcSessionInfo& info);
	void Disconnect(void);
	void KillIdent(void);

	#if defined( _UNICODE )
		int NLSend(const TCHAR* fmt, ...);
	#endif
	int NLSend(const char* fmt, ...);
	int NLSend(const unsigned char* buf, int cbBuf);
	int NLSendNoScript( const unsigned char* buf, int cbBuf);
	int NLReceive(unsigned char* buf, int cbBuf);
	void InsertIncomingEvent(TCHAR* pszRaw);

	__inline bool IsConnected() const { return con != NULL; }

	// send-to-stream operators
	int getCodepage() const;
	__inline void setCodepage( int aPage ) { codepage = aPage; }

	CIrcSessionInfo m_info;

protected :
	int codepage;
	HANDLE con;
	HANDLE hBindPort;
	void DoReceive();
	LIST<CDccSession> m_dcc_chats;
	LIST<CDccSession> m_dcc_xfers;

private :
	CRITICAL_SECTION    m_dcc;      // protect the dcc objects

	void createMessageFromPchar( const char* p );
	void Notify(const CIrcMessage* pmsg);
	void __cdecl ThreadProc( void *pparam );

	////////////////////////////////////////////////////////////////////////////////////////
	// former CIrcMonitor class

	bool OnIrc_PING(const CIrcMessage* pmsg);
	bool OnIrc_WELCOME(const CIrcMessage* pmsg);
	bool OnIrc_YOURHOST(const CIrcMessage* pmsg);
	bool OnIrc_NICK(const CIrcMessage* pmsg);
	bool OnIrc_PRIVMSG(const CIrcMessage* pmsg);
	bool OnIrc_JOIN(const CIrcMessage* pmsg);
	bool OnIrc_QUIT(const CIrcMessage* pmsg);
	bool OnIrc_PART(const CIrcMessage* pmsg);
	bool OnIrc_KICK(const CIrcMessage* pmsg);
	bool OnIrc_MODE(const CIrcMessage* pmsg);
	bool OnIrc_USERHOST_REPLY(const CIrcMessage* pmsg);
	bool OnIrc_MODEQUERY(const CIrcMessage* pmsg);
	bool OnIrc_NAMES(const CIrcMessage* pmsg);
	bool OnIrc_ENDNAMES(const CIrcMessage* pmsg);
	bool OnIrc_INITIALTOPIC(const CIrcMessage* pmsg);
	bool OnIrc_INITIALTOPICNAME(const CIrcMessage* pmsg);
	bool OnIrc_TOPIC(const CIrcMessage* pmsg);
	bool OnIrc_TRYAGAIN(const CIrcMessage* pmsg);
	bool OnIrc_NOTICE(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_NAME(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_CHANNELS(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_SERVER(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_AWAY(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_IDLE(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_END(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_OTHER(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_AUTH(const CIrcMessage* pmsg);
	bool OnIrc_WHOIS_NO_USER(const CIrcMessage* pmsg);
	bool OnIrc_NICK_ERR(const CIrcMessage* pmsg);
	bool OnIrc_ENDMOTD(const CIrcMessage* pmsg);
	bool OnIrc_LISTSTART(const CIrcMessage* pmsg);
	bool OnIrc_LIST(const CIrcMessage* pmsg);
	bool OnIrc_LISTEND(const CIrcMessage* pmsg);
	bool OnIrc_BANLIST(const CIrcMessage* pmsg);
	bool OnIrc_BANLISTEND(const CIrcMessage* pmsg);
	bool OnIrc_SUPPORT(const CIrcMessage* pmsg);
	bool OnIrc_BACKFROMAWAY(const CIrcMessage* pmsg);
	bool OnIrc_SETAWAY(const CIrcMessage* pmsg);
	bool OnIrc_JOINERROR(const CIrcMessage* pmsg);
	bool OnIrc_UNKNOWN(const CIrcMessage* pmsg);
	bool OnIrc_ERROR(const CIrcMessage* pmsg);
	bool OnIrc_NOOFCHANNELS(const CIrcMessage* pmsg);
	bool OnIrc_PINGPONG(const CIrcMessage* pmsg);
	bool OnIrc_INVITE(const CIrcMessage* pmsg);
	bool OnIrc_WHO_END(const CIrcMessage* pmsg);
	bool OnIrc_WHO_REPLY(const CIrcMessage* pmsg);
	bool OnIrc_WHOTOOLONG(const CIrcMessage* pmsg);

	bool IsCTCP(const CIrcMessage* pmsg);

	void OnIrcDefault(const CIrcMessage* pmsg);
	void OnIrcDisconnected();

	static OBJLIST<CIrcHandler> m_handlers;

	PfnIrcMessageHandler FindMethod(const TCHAR* lpszName);

	void OnIrcMessage(const CIrcMessage* pmsg);
	CMString sNick4Perform;
};

// map actual member functions to their associated IRC command.
// put any number of this macro in the class's constructor.
#define	IRC_MAP_ENTRY(name, member)	\
	m_handlers.insert( new CIrcHandler( _T(name), &CIrcProto::OnIrc_##member ));

/////////////////////////////////////////////////////////////////////////////////////////
// Functions

//main.cpp
extern HINSTANCE hInst;

extern LIST<CIrcProto> g_Instances;

extern int mirVersion;
extern OBJLIST<SERVER_INFO> g_servers;

void   UpgradeCheck(void);

CIrcProto* GetTimerOwner( UINT_PTR eventId );

VOID CALLBACK IdentTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK TimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK KeepAliveTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK OnlineNotifTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK OnlineNotifTimerProc3( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK DCCTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );
VOID CALLBACK RetryTimerProc( HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime );

// options.cpp

void    InitServers(void);
void    RereadServers(void);

void    InitContactMenus(void);
void	UninitContactMenus(void);

void    WindowSetIcon(HWND hWnd, int iconId);
void    WindowFreeIcon(HWND hWnd);

void    AddIcons(void);
void    UninitIcons(void);
HICON   LoadIconEx(int iIndex, bool big = false);
HANDLE  GetIconHandle(int iconId);
void    ReleaseIconEx(HICON hIcon);

// services.cpp

extern BOOL bChatInstalled, m_bMbotInstalled;

//tools.cpp
int          __stdcall WCCmp(const TCHAR* wild, const TCHAR* string);
char*        __stdcall IrcLoadFile(TCHAR * szPath);
CMString     __stdcall GetWord(const TCHAR* text, int index);
CMString&    __stdcall ReplaceString (CMString& text, const TCHAR* replaceme, const TCHAR* newword);
const TCHAR* __stdcall GetWordAddress(const TCHAR* text, int index);
void         __stdcall RemoveLinebreaks( CMString& Message );
TCHAR*       __stdcall my_strstri(const TCHAR *s1, const TCHAR *s2) ;
TCHAR*       __stdcall DoColorCodes (const TCHAR* text, bool bStrip, bool bReplacePercent);
char*        __stdcall rtrim( char *string );

#if defined( _UNICODE )
	String&  __stdcall ReplaceString (String& text, const char* replaceme, const char* newword);
	String   __stdcall GetWord(const char* text, int index);
#endif

#define NEWSTR_ALLOCA(A) (A==NULL)?NULL:strcpy((char*)alloca(strlen(A)+1),A)
#define NEWTSTR_ALLOCA(A) (A==NULL)?NULL:_tcscpy((TCHAR*)alloca(sizeof(TCHAR)*(_tcslen(A)+1)),A)

#pragma comment(lib,"comctl32.lib")

#endif
