// Plugin.cpp - Fixed version with working status display and funds endpoint
#include "stdafx.h"
#include "resource.h"  // Include resource definitions
#include "OpenAlgoGlobals.h"
#include "Plugin.h"
#include "Plugin_Legacy.h"
#include "OpenAlgoConfigDlg.h"

// Plugin identification
#define PLUGIN_NAME "OpenAlgo Data Plugin"
#define VENDOR_NAME "OpenAlgo Community"
#define PLUGIN_VERSION 10003
#define PLUGIN_ID PIDCODE('O', 'A', 'L', 'G')
#define THIS_PLUGIN_TYPE PLUGIN_TYPE_DATA
#define AGENT_NAME PLUGIN_NAME

// Timer IDs
#define TIMER_INIT 198
#define TIMER_REFRESH 199
#define RETRY_COUNT 8

////////////////////////////////////////
// Plugin Info Structure
////////////////////////////////////////
static struct PluginInfo oPluginInfo =
{
	sizeof(struct PluginInfo),
	THIS_PLUGIN_TYPE,
	PLUGIN_VERSION,
	PLUGIN_ID,
	PLUGIN_NAME,
	VENDOR_NAME,
	0,
	530000
};

///////////////////////////////
// Global Variables
///////////////////////////////
HWND g_hAmiBrokerWnd = NULL;
int g_nPortNumber = 5000;
int g_nRefreshInterval = 5;
BOOL g_bAutoAddSymbols = TRUE;
int g_nSymbolLimit = 100;
BOOL g_bOptimizedIntraday = TRUE;
int g_nTimeShift = 0;
CString g_oServer = _T("127.0.0.1");
CString g_oApiKey = _T("");  // API Key for authentication
int g_nStatus = STATUS_WAIT;

// Local static variables
static int g_nRetryCount = RETRY_COUNT;
static struct RecentInfo* g_aInfos = NULL;
static int RecentInfoSize = 0;
static BOOL g_bPluginInitialized = FALSE;

typedef CArray< struct Quotation, struct Quotation > CQuoteArray;

// Forward declarations
VOID CALLBACK OnTimerProc(HWND, UINT, UINT_PTR, DWORD);
void SetupRetry(void);
BOOL TestOpenAlgoConnection(void);

///////////////////////////////
// Helper Functions
///////////////////////////////
CString BuildOpenAlgoURL(const CString& server, int port, const CString& endpoint)
{
	CString result;
	result.Format(_T("http://%s:%d%s"), (LPCTSTR)server, port, (LPCTSTR)endpoint);
	return result;
}

BOOL AddToOpenAlgoPortfolio(LPCTSTR pszTicker)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	BOOL bOK = FALSE;
	try
	{
		CString endpoint;
		endpoint.Format(_T("/api/v1/watchlist/add?symbol=%s"), pszTicker);
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, endpoint);

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);

		// For watchlist endpoint, we might need to add API key as header or parameter
		// This depends on OpenAlgo's implementation
		CStdioFile* poFile = oSession.OpenURL(oURL, 1, INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

		CString oLine;
		if (poFile && poFile->ReadString(oLine))
		{
			if (oLine.Find(_T("OK")) >= 0 || oLine.Find(_T("success")) >= 0)
			{
				bOK = TRUE;
			}
		}

		if (poFile)
		{
			poFile->Close();
			delete poFile;
		}
		oSession.Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		g_nStatus = STATUS_DISCONNECTED;
	}
	return bOK;
}

///////////////////////////////////////////////////////////
// Exported Functions
///////////////////////////////////////////////////////////
PLUGINAPI int GetPluginInfo(struct PluginInfo* pInfo)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (pInfo == NULL) return FALSE;

	*pInfo = oPluginInfo;
	return TRUE;
}

PLUGINAPI int Init(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!g_bPluginInitialized)
	{
		// Initialize on first call
		g_oServer = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("Server"), _T("127.0.0.1"));
		g_oApiKey = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("ApiKey"), _T(""));  // Load API Key
		g_nPortNumber = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("Port"), 5000);
		g_nRefreshInterval = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), 5);
		g_bAutoAddSymbols = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("AutoAddSymbols"), 1);
		g_nSymbolLimit = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("SymbolLimit"), 100);
		g_bOptimizedIntraday = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("OptimizedIntraday"), 1);
		g_nTimeShift = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("TimeShift"), 0);

		g_nStatus = STATUS_WAIT;
		g_bPluginInitialized = TRUE;
	}

	return 1;
}

PLUGINAPI int Release(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return 1;
}

PLUGINAPI int Configure(LPCTSTR pszPath, struct InfoSite* pSite)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	COpenAlgoConfigDlg oDlg;
	oDlg.m_pSite = pSite;

	if (oDlg.DoModal() == IDOK)
	{
		// Force status update after config change
		if (g_hAmiBrokerWnd != NULL)
		{
			::PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
		}
	}

	return 1;
}

PLUGINAPI AmiVar GetExtraData(LPCTSTR pszTicker, LPCTSTR pszName, int nArraySize, int nPeriodicity, void* (*pfAlloc)(unsigned int nSize))
{
	AmiVar var;
	var.type = VAR_NONE;
	var.val = 0;
	return var;
}

PLUGINAPI int SetTimeBase(int nTimeBase)
{
	return 1;
}

PLUGINAPI int GetSymbolLimit(void)
{
	return g_nSymbolLimit;
}

// CRITICAL FUNCTION - This makes the status LED work!
PLUGINAPI int GetPluginStatus(struct PluginStatus* status)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (status == NULL) return 0;

	// MUST set structure size
	status->nStructSize = sizeof(struct PluginStatus);

	// Ensure we have valid status
	if (g_nStatus < STATUS_WAIT || g_nStatus > STATUS_SHUTDOWN)
	{
		g_nStatus = STATUS_WAIT;
	}

	switch (g_nStatus)
	{
	case STATUS_WAIT:
		status->nStatusCode = 0x10000000;
		strcpy_s(status->szShortMessage, 32, "WAIT");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Waiting to connect");
		status->clrStatusColor = RGB(255, 255, 0); // Yellow
		break;

	case STATUS_CONNECTED:
		status->nStatusCode = 0x00000000;
		strcpy_s(status->szShortMessage, 32, "OK");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Connected");
		status->clrStatusColor = RGB(0, 255, 0); // Green
		break;

	case STATUS_DISCONNECTED:
		status->nStatusCode = 0x20000000;
		strcpy_s(status->szShortMessage, 32, "ERR");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Connection failed");
		status->clrStatusColor = RGB(255, 0, 0); // Red
		break;

	case STATUS_SHUTDOWN:
		status->nStatusCode = 0x30000000;
		strcpy_s(status->szShortMessage, 32, "OFF");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Offline");
		status->clrStatusColor = RGB(192, 0, 192); // Purple
		break;

	default:
		status->nStatusCode = 0x40000000;
		strcpy_s(status->szShortMessage, 32, "???");
		strcpy_s(status->szLongMessage, 256, "Unknown status");
		status->clrStatusColor = RGB(128, 128, 128); // Gray
		break;
	}

	return 1; // MUST return 1!
}

CString GetAvailableSymbols(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CString oResult;
	try
	{
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/symbols"));

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);
		oSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
		oSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 5000);

		CStdioFile* poFile = oSession.OpenURL(oURL, 1,
			INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

		if (poFile)
		{
			CString oLine;
			if (poFile->ReadString(oLine))
			{
				if (oLine.Left(2) == _T("OK"))
				{
					poFile->ReadString(oResult);
				}
				else
				{
					oResult = oLine;
				}
			}
			poFile->Close();
			delete poFile;
		}
		oSession.Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		g_nStatus = STATUS_DISCONNECTED;
	}
	return oResult;
}

BOOL TestOpenAlgoConnection(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Check if API key is configured
	if (g_oApiKey.IsEmpty())
	{
		return FALSE;
	}

	BOOL bConnected = FALSE;

	try
	{
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/funds"));

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL,
			INTERNET_FLAG_DONT_CACHE);
		oSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 2000);
		oSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 2000);

		// Prepare POST data with API key
		CString oPostData;
		oPostData.Format(_T("{\"apikey\":\"%s\"}"), (LPCTSTR)g_oApiKey);

		CHttpConnection* pConnection = NULL;
		CHttpFile* pFile = NULL;

		// Parse server and port
		INTERNET_PORT nPort = (INTERNET_PORT)g_nPortNumber;
		CString oServer = g_oServer;

		// Remove http:// or https:// if present
		oServer.Replace(_T("http://"), _T(""));
		oServer.Replace(_T("https://"), _T(""));

		pConnection = oSession.GetHttpConnection(oServer, nPort);

		if (pConnection)
		{
			// Create POST request
			pFile = pConnection->OpenRequest(
				CHttpConnection::HTTP_VERB_POST,
				_T("/api/v1/funds"),
				NULL,
				1,
				NULL,
				NULL,
				INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

			if (pFile)
			{
				// Set headers
				CString oHeaders = _T("Content-Type: application/json\r\n");

				// Convert string to UTF-8 for sending
				CStringA oPostDataA(oPostData);

				// Send the request
				BOOL bResult = pFile->SendRequest(oHeaders, (LPVOID)(LPCSTR)oPostDataA, oPostDataA.GetLength());

				if (bResult)
				{
					DWORD dwStatusCode = 0;
					pFile->QueryInfoStatusCode(dwStatusCode);

					if (dwStatusCode == 200)
					{
						// Read response to verify it's valid
						CString oResponse;
						CString oLine;
						while (pFile->ReadString(oLine))
						{
							oResponse += oLine;
							if (oResponse.GetLength() > 500) break; // Limit response size
						}

						// Check if response contains "success"
						if (oResponse.Find(_T("\"status\":\"success\"")) >= 0 ||
							oResponse.Find(_T("\"status\": \"success\"")) >= 0)
						{
							bConnected = TRUE;
						}
					}
				}

				pFile->Close();
				delete pFile;
			}

			pConnection->Close();
			delete pConnection;
		}

		oSession.Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
		bConnected = FALSE;
	}

	return bConnected;
}

void SetupRetry(void)
{
	if (--g_nRetryCount > 0)
	{
		if (g_hAmiBrokerWnd != NULL)
		{
			SetTimer(g_hAmiBrokerWnd, TIMER_INIT, 15000, (TIMERPROC)OnTimerProc);
		}
		g_nStatus = STATUS_DISCONNECTED;
	}
	else
	{
		g_nStatus = STATUS_SHUTDOWN;
	}

	if (g_hAmiBrokerWnd != NULL)
	{
		::PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
	}
}

VOID CALLBACK OnTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (idEvent == TIMER_INIT || idEvent == TIMER_REFRESH)
	{
		if (!TestOpenAlgoConnection())
		{
			if (g_hAmiBrokerWnd != NULL)
			{
				KillTimer(g_hAmiBrokerWnd, idEvent);
			}
			SetupRetry();
			return;
		}

		g_nStatus = STATUS_CONNECTED;
		g_nRetryCount = RETRY_COUNT;

		if (g_hAmiBrokerWnd != NULL)
		{
			::PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);

			if (idEvent == TIMER_INIT)
			{
				KillTimer(g_hAmiBrokerWnd, TIMER_INIT);
				SetTimer(g_hAmiBrokerWnd, TIMER_REFRESH, g_nRefreshInterval * 1000, (TIMERPROC)OnTimerProc);
			}
		}
	}
}

PLUGINAPI int Notify(struct PluginNotification* pn)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (pn == NULL) return 0;

	// Database loaded - start connection
	if ((pn->nReason & REASON_DATABASE_LOADED))
	{
		g_hAmiBrokerWnd = pn->hMainWnd;

		// Reload settings
		g_oServer = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("Server"), _T("127.0.0.1"));
		g_oApiKey = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("ApiKey"), _T(""));  // Load API Key
		g_nPortNumber = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("Port"), 5000);
		g_nRefreshInterval = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), 5);

		g_nStatus = STATUS_WAIT;
		g_nRetryCount = RETRY_COUNT;

		// Start connection timer
		if (g_hAmiBrokerWnd != NULL)
		{
			SetTimer(g_hAmiBrokerWnd, TIMER_INIT, 1000, (TIMERPROC)OnTimerProc);
			// Force immediate status update
			::PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
		}
	}

	// Database unloaded - cleanup
	if (pn->nReason & REASON_DATABASE_UNLOADED)
	{
		if (g_hAmiBrokerWnd != NULL)
		{
			KillTimer(g_hAmiBrokerWnd, TIMER_INIT);
			KillTimer(g_hAmiBrokerWnd, TIMER_REFRESH);
		}
		g_hAmiBrokerWnd = NULL;
		g_nStatus = STATUS_SHUTDOWN;

		free(g_aInfos);
		g_aInfos = NULL;
		RecentInfoSize = 0;
	}

	// Right-click on status area - show menu
	if (pn->nReason & REASON_STATUS_RMBCLICK)
	{
		if (g_hAmiBrokerWnd != NULL)
		{
			HMENU hMenu = CreatePopupMenu();

			if (g_nStatus == STATUS_SHUTDOWN || g_nStatus == STATUS_DISCONNECTED)
			{
				AppendMenu(hMenu, MF_STRING, 1, _T("Connect"));
			}
			else
			{
				AppendMenu(hMenu, MF_STRING, 2, _T("Disconnect"));
			}
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MF_STRING, 3, _T("Configure..."));

			POINT pt;
			GetCursorPos(&pt);

			int nCmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
				pt.x, pt.y, 0, g_hAmiBrokerWnd, NULL);

			DestroyMenu(hMenu);

			switch (nCmd)
			{
			case 1: // Connect
				g_nStatus = STATUS_WAIT;
				g_nRetryCount = RETRY_COUNT;
				SetTimer(g_hAmiBrokerWnd, TIMER_INIT, 1000, (TIMERPROC)OnTimerProc);
				break;

			case 2: // Disconnect
				KillTimer(g_hAmiBrokerWnd, TIMER_INIT);
				KillTimer(g_hAmiBrokerWnd, TIMER_REFRESH);
				g_nStatus = STATUS_SHUTDOWN;
				break;

			case 3: // Configure
				Configure(pn->pszDatabasePath, NULL);
				break;
			}

			// Update status display
			::PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
		}
	}

	return 1;
}

PLUGINAPI int GetQuotes(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct QuotationFormat4* pQuotes)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	Quotation* pQuote5 = (struct Quotation*)malloc(nSize * sizeof(Quotation));
	QuotationFormat4* src = pQuotes;
	Quotation* dst = pQuote5;

	int i;
	for (i = 0; i <= nLastValid; i++, src++, dst++)
	{
		ConvertFormat4Quote(src, dst);
	}

	int nQty = GetQuotesEx(pszTicker, nPeriodicity, nLastValid, nSize, pQuote5, NULL);

	dst = pQuote5;
	src = pQuotes;

	for (i = 0; i < nQty; i++, dst++, src++)
	{
		ConvertFormat5Quote(dst, src);
	}

	free(pQuote5);
	return nQty;
}

PLUGINAPI int GetQuotesEx(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes, GQEContext* pContext)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_nStatus == STATUS_DISCONNECTED || g_nStatus == STATUS_SHUTDOWN)
	{
		return nLastValid + 1;
	}

	// TODO: Implement actual quote fetching using OpenAlgo API with authentication
	// This will need to use the g_oApiKey for authentication
	return nLastValid + 1;
}

PLUGINAPI struct RecentInfo* GetRecentInfo(LPCTSTR pszTicker)
{
	return NULL;
}