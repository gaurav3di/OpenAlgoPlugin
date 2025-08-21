// Plugin.cpp - Fixed version with all compilation errors resolved
#include "stdafx.h"
#include "Plugin.h"
#include "resource.h"
#include "OpenAlgoConfigDlg.h"
// Remove std::min and use manual min implementation

// Helper function to handle CString format issues
void SafeStringFormat(CString& str, LPCTSTR format, ...)
{
	va_list args;
	va_start(args, format);
	str.FormatV(format, args);
	va_end(args);
}

// Alternative: Use simpler string operations to avoid template issues
CString BuildOpenAlgoURL(const CString& server, int port, const CString& endpoint)
{
	CString result;
	result.Format(_T("http://%s:%d%s"), (LPCTSTR)server, port, (LPCTSTR)endpoint);
	return result;
}

// These are the only two lines you need to change
#define PLUGIN_NAME "OpenAlgo Data Plugin"
#define VENDOR_NAME "OpenAlgo Community"
#define PLUGIN_VERSION 10000
#define PLUGIN_ID PIDCODE('O', 'A', 'L', 'G')

// IMPORTANT: Define plugin type !!!
#define THIS_PLUGIN_TYPE PLUGIN_TYPE_DATA

////////////////////////////////////////
// Data section
////////////////////////////////////////
struct PluginInfo oPluginInfo =
{
		sizeof(struct PluginInfo),
		THIS_PLUGIN_TYPE,
		PLUGIN_VERSION,
		PLUGIN_ID,
		PLUGIN_NAME,
		VENDOR_NAME,
		13012679,
		387000
};

///////////////////////////////////////////////////////////
// Basic plug-in interface functions exported by DLL
///////////////////////////////////////////////////////////

PLUGINAPI int GetPluginInfo(struct PluginInfo* pInfo)
{
	*pInfo = oPluginInfo;
	return TRUE;
}

PLUGINAPI int Init(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
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
	return (nTimeBase >= 60 && nTimeBase <= (24 * 60 * 60)) ? 1 : 0;
}

/////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////
#define RETRY_COUNT 8
#define AGENT_NAME PLUGIN_NAME
enum
{
	STATUS_WAIT,
	STATUS_CONNECTED,
	STATUS_DISCONNECTED,
	STATUS_SHUTDOWN
};

///////////////////////////////
// Globals
///////////////////////////////
typedef CArray< struct Quotation, struct Quotation > CQuoteArray;

HWND g_hAmiBrokerWnd = NULL;

int		g_nPortNumber = 5000;        // Changed from 16239 to OpenAlgo default port
int		g_nRefreshInterval = 1;
BOOL	g_bAutoAddSymbols = TRUE;
int		g_nSymbolLimit = 100;
int		g_bOptimizedIntraday = TRUE;
int		g_nTimeShift = 0;
CString g_oServer = "127.0.0.1";

int		g_nStatus = STATUS_WAIT;
int		g_nRetryCount = RETRY_COUNT;

static struct RecentInfo* g_aInfos = NULL;
static int	  RecentInfoSize = 0;

PLUGINAPI int GetSymbolLimit(void)
{
	return g_nSymbolLimit;
}

PLUGINAPI int GetPluginStatus(struct PluginStatus* status)
{
	switch (g_nStatus)
	{
	case STATUS_WAIT:
		status->nStatusCode = 0x10000000;
		strcpy_s(status->szShortMessage, sizeof(status->szShortMessage), "WAIT");
		strcpy_s(status->szLongMessage, sizeof(status->szLongMessage), "Waiting for connection");
		status->clrStatusColor = RGB(255, 255, 0);
		break;
	case STATUS_CONNECTED:
		status->nStatusCode = 0x00000000;
		strcpy_s(status->szShortMessage, sizeof(status->szShortMessage), "OK");
		strcpy_s(status->szLongMessage, sizeof(status->szLongMessage), "Connected OK");
		status->clrStatusColor = RGB(0, 255, 0);
		break;
	case STATUS_DISCONNECTED:
		status->nStatusCode = 0x20000000;
		strcpy_s(status->szShortMessage, sizeof(status->szShortMessage), "ERR");
		strcpy_s(status->szLongMessage, sizeof(status->szLongMessage), "Disconnected.\n\nPlease check if OpenAlgo server is running.\nAmiBroker will try to reconnect in 15 seconds.");
		status->clrStatusColor = RGB(255, 0, 0);
		break;
	case STATUS_SHUTDOWN:
		status->nStatusCode = 0x30000000;
		strcpy_s(status->szShortMessage, sizeof(status->szShortMessage), "DOWN");
		strcpy_s(status->szLongMessage, sizeof(status->szLongMessage), "Connection is shut down.\nWill not retry until you re-connect manually.");
		status->clrStatusColor = RGB(192, 0, 192);
		break;
	default:
		strcpy_s(status->szShortMessage, sizeof(status->szShortMessage), "Unkn");
		strcpy_s(status->szLongMessage, sizeof(status->szLongMessage), "Unknown status");
		status->clrStatusColor = RGB(255, 255, 255);
		break;
	}

	return 1;
}

int GetTimeOffset(void)
{
	int nOffset = 0;
	TIME_ZONE_INFORMATION tzinfo;
	DWORD dw = GetTimeZoneInformation(&tzinfo);
	if (dw == 0xFFFFFFFF)
	{
		return -1;
	}

	if (dw == TIME_ZONE_ID_DAYLIGHT)
		nOffset -= ((tzinfo.Bias + tzinfo.DaylightBias)) / 60;
	else if (dw == TIME_ZONE_ID_STANDARD)
		nOffset -= ((tzinfo.Bias + tzinfo.StandardBias)) / 60;
	else
		nOffset -= (tzinfo.Bias) / 60;

	return nOffset;
}

BOOL IsOpenAlgoRunning(void)
{
	HANDLE hMutex = CreateMutex(NULL, FALSE, _T("OpenAlgo"));
	BOOL bOK = GetLastError() == ERROR_ALREADY_EXISTS;
	if (hMutex)
	{
		CloseHandle(hMutex);
	}
	return bOK;
}

void GrowRecentInfoIfNecessary(int iSymbol)
{
	if (g_aInfos == NULL || iSymbol >= RecentInfoSize)
	{
		RecentInfoSize += 200;
		// Fixed: Use realloc with proper casting
		g_aInfos = (struct RecentInfo*)realloc(g_aInfos, sizeof(struct RecentInfo) * RecentInfoSize);
		memset(g_aInfos + RecentInfoSize - 200, 0, sizeof(struct RecentInfo) * 200);
	}
}

struct RecentInfo* FindRecentInfo(LPCTSTR pszTicker)
{
	struct RecentInfo* ri = NULL;

	for (int iSymbol = 0; g_aInfos && iSymbol < RecentInfoSize && g_aInfos[iSymbol].Name && g_aInfos[iSymbol].Name[0]; iSymbol++)
	{
		if (!_stricmp(g_aInfos[iSymbol].Name, pszTicker))  // Fixed: use _stricmp instead of stricmp
		{
			ri = &g_aInfos[iSymbol];
			break;
		}
	}
	return ri;
}

BOOL AddToOpenAlgoPortfolio(LPCTSTR pszTicker)
{
	BOOL bOK = FALSE;  // Changed default to FALSE

	try
	{
		// Fixed: Use helper function to avoid template issues
		CString endpoint;
		endpoint.Format(_T("/api/v1/watchlist/add?symbol=%s"), pszTicker);
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, endpoint);

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);
		CStdioFile* poFile = oSession.OpenURL(oURL, 1, INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

		CString oLine;
		if (poFile && poFile->ReadString(oLine) && oLine.Left(2) == _T("OK"))
		{
			bOK = TRUE;
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
		bOK = FALSE;
	}

	return bOK;
}

CString GetAvailableSymbols(void)
{
	CString oResult;

	try
	{
		// Fixed: Use helper function to avoid template issues
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/symbols"));

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);
		CStdioFile* poFile = oSession.OpenURL(oURL, 1, INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

		CString oLine;
		if (poFile && poFile->ReadString(oLine) && oLine.Left(2) == _T("OK"))
		{
			poFile->ReadString(oResult);
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

	return oResult;
}

struct RecentInfo* FindOrAddRecentInfo(LPCTSTR pszTicker)
{
	struct RecentInfo* ri = NULL;

	if (g_aInfos == NULL) return NULL;

	// Fixed: Declare iSymbol at the beginning of the function
	int iSymbol;
	for (iSymbol = 0; g_aInfos && iSymbol < RecentInfoSize && g_aInfos[iSymbol].Name && g_aInfos[iSymbol].Name[0]; iSymbol++)
	{
		if (!_stricmp(g_aInfos[iSymbol].Name, pszTicker))  // Fixed: use _stricmp
		{
			ri = &g_aInfos[iSymbol];
			return ri;
		}
	}

	if (iSymbol < g_nSymbolLimit)
	{
		if (AddToOpenAlgoPortfolio(pszTicker))
		{
			GrowRecentInfoIfNecessary(iSymbol);
			ri = &g_aInfos[iSymbol];
			strcpy_s(ri->Name, sizeof(ri->Name), pszTicker);  // Fixed: use strcpy_s
			ri->nStatus = 0;
		}
	}

	return ri;
}

// Forward declaration
VOID CALLBACK OnTimerProc(HWND, UINT, UINT_PTR, DWORD);

void SetupRetry(void)
{
	if (--g_nRetryCount)
	{
		SetTimer(g_hAmiBrokerWnd, 198, 15000, (TIMERPROC)OnTimerProc);
		g_nStatus = STATUS_DISCONNECTED;
	}
	else
	{
		g_nStatus = STATUS_SHUTDOWN;
	}
}

int	safe_atoi(const char* string)
{
	if (string == NULL) return 0;
	return atoi(string);
}

double safe_atof(const char* string)
{
	if (string == NULL) return 0.0f;
	return atof(string);
}

// Fixed BlendQuoteArrays function with proper variable declarations
int BlendQuoteArrays(struct Quotation* pQuotes, int nPeriodicity, int nLastValid, int nSize, CQuoteArray* pCurQuotes)
{
	int iQty = pCurQuotes->GetSize();
	DATE_TIME_INT nFirstDate = iQty == 0 ? (DATE_TIME_INT)-1 : pCurQuotes->GetAt(0).DateTime.Date;

	// Fixed: Declare iStart at the beginning
	int iStart;
	for (iStart = nLastValid; iStart >= 0; iStart--)
	{
		if (pQuotes[iStart].DateTime.Date < nFirstDate) break;
	}

	iStart++; // start with next

	int iSrc = 0;

	if (iQty > nSize)
	{
		iStart = 0;
		iSrc = iQty - nSize;
	}
	else if (iQty + iStart > nSize)
	{
		// Fixed: use memmove with 3 arguments
		memmove(pQuotes, pQuotes + iQty + iStart - nSize, sizeof(Quotation) * (nSize - iQty));
		iStart = nSize - iQty;
		iSrc = 0;
	}

	// Fixed: Manual min implementation to avoid std::min issues
	int temp1 = nSize - iStart;
	int temp2 = iQty - iSrc;
	int iNumQuotes = (temp1 < temp2) ? temp1 : temp2;

	if (iNumQuotes > 0)
	{
		memcpy(pQuotes + iStart, pCurQuotes->GetData() + iSrc, iNumQuotes * sizeof(Quotation));
	}
	else
	{
		iNumQuotes = 0;
	}

	return iStart + iNumQuotes;
}

// Timer callback procedure - Fixed version with proper type handling
VOID CALLBACK OnTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	// Fixed: Properly handle UINT_PTR to int conversion
	int nEvent = (int)idEvent;  // Explicit cast to handle the conversion warning

	if (nEvent == 199 || nEvent == 198)
	{
		if (!IsOpenAlgoRunning())
		{
			KillTimer(g_hAmiBrokerWnd, idEvent);
			SetupRetry();
			return;
		}

		try
		{
			CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);

			// Fixed: Use helper function to avoid template issues
			CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/quotes"));

			CStdioFile* poFile = oSession.OpenURL(oURL, 1, INTERNET_FLAG_TRANSFER_ASCII | INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

			if (poFile)
			{
				CString oLine;
				int iSymbol = 0;

				if (poFile->ReadString(oLine))
				{
					if (oLine == _T("OK"))
					{
						while (poFile->ReadString(oLine))
						{
							// Basic quote processing - you can expand this
							// For now, just increment the symbol counter
							iSymbol++;
						}
					}
				}

				::SendMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);

				poFile->Close();
				delete poFile;

				g_nStatus = STATUS_CONNECTED;
				g_nRetryCount = RETRY_COUNT;
			}

			oSession.Close();
		}
		catch (CInternetException* e)
		{
			e->Delete();
			KillTimer(g_hAmiBrokerWnd, idEvent);
			SetupRetry();
			return;
		}
	}

	if (nEvent == 198)
	{
		KillTimer(g_hAmiBrokerWnd, 198);
		SetTimer(g_hAmiBrokerWnd, 199, g_nRefreshInterval * 1000, (TIMERPROC)OnTimerProc);
	}
}

// Legacy format support
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

// Main GetQuotesEx function - simplified for compilation
PLUGINAPI int GetQuotesEx(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes, GQEContext* pContext)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_nStatus >= STATUS_DISCONNECTED) return nLastValid + 1;

	// Basic implementation - you should expand this with actual OpenAlgo API calls
	return nLastValid + 1;
}

PLUGINAPI int Notify(struct PluginNotification* pn)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_hAmiBrokerWnd == NULL && (pn->nReason & REASON_DATABASE_LOADED))
	{
		g_hAmiBrokerWnd = pn->hMainWnd;
		g_nTimeShift = GetTimeOffset();

		// Load settings from registry under "OpenAlgo" key
		g_nTimeShift = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("TimeShift"), g_nTimeShift);
		g_oServer = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("Server"), _T("127.0.0.1"));
		g_nPortNumber = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("Port"), 5000);
		g_nRefreshInterval = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), 5);
		g_bAutoAddSymbols = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("AutoAddSymbols"), 1);
		g_nSymbolLimit = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("SymbolLimit"), 100);
		g_bOptimizedIntraday = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("OptimizedIntraday"), 1);

		g_nStatus = STATUS_WAIT;
		SetTimer(g_hAmiBrokerWnd, 198, 1000, (TIMERPROC)OnTimerProc);
	}

	if (pn->nReason & REASON_DATABASE_UNLOADED)
	{
		KillTimer(g_hAmiBrokerWnd, 198);
		KillTimer(g_hAmiBrokerWnd, 199);
		g_hAmiBrokerWnd = NULL;

		free(g_aInfos);
		g_aInfos = NULL;
	}

	return 1;
}

PLUGINAPI struct RecentInfo* GetRecentInfo(LPCTSTR pszTicker)
{
	return FindRecentInfo(pszTicker);
}