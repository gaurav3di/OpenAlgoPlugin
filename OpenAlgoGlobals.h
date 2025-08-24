// OpenAlgoGlobals.h - Global variables and functions shared across the plugin
#ifndef OPENALGO_GLOBALS_H
#define OPENALGO_GLOBALS_H

#include "stdafx.h"
#include "resource.h"  // Include resource definitions

// Status enum - MUST be defined before any usage
enum OpenAlgoStatus
{
	STATUS_WAIT = 0,
	STATUS_CONNECTED = 1,
	STATUS_DISCONNECTED = 2,
	STATUS_SHUTDOWN = 3
};

// Global variables - declare as extern here
extern HWND g_hAmiBrokerWnd;
extern int g_nPortNumber;
extern int g_nRefreshInterval;
extern int g_nTimeShift;
extern CString g_oServer;
extern CString g_oApiKey;  // API Key for authentication
extern CString g_oWebSocketUrl;  // WebSocket URL for real-time data
extern int g_nStatus;

// Global function declarations
CString GetAvailableSymbols(void);
CString BuildOpenAlgoURL(const CString& server, int port, const CString& endpoint);
// AddToOpenAlgoPortfolio is internal to Plugin.cpp, not needed here

#endif // OPENALGO_GLOBALS_H