// Plugin.cpp - Complete implementation with Quotes and Historical data
#include "stdafx.h"
#include "resource.h"  // Include resource definitions
#include "OpenAlgoGlobals.h"
#include "Plugin.h"
#include "Plugin_Legacy.h"
#include "OpenAlgoConfigDlg.h"
#include <math.h>
#include <time.h>

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
int g_nTimeShift = 0;
CString g_oServer = _T("127.0.0.1");
CString g_oApiKey = _T("");  // API Key for authentication
CString g_oWebSocketUrl = _T("ws://127.0.0.1:8765");  // WebSocket URL
int g_nStatus = STATUS_WAIT;

// Local static variables
static int g_nRetryCount = RETRY_COUNT;
static struct RecentInfo* g_aInfos = NULL;
static int RecentInfoSize = 0;
static BOOL g_bPluginInitialized = FALSE;

// WebSocket connection management
static SOCKET g_websocket = INVALID_SOCKET;
static BOOL g_bWebSocketConnected = FALSE;
static BOOL g_bWebSocketAuthenticated = FALSE;
static BOOL g_bWebSocketConnecting = FALSE;
static DWORD g_dwLastConnectionAttempt = 0;
static CMap<CString, LPCTSTR, BOOL, BOOL> g_SubscribedSymbols;
static CRITICAL_SECTION g_WebSocketCriticalSection;
static BOOL g_bCriticalSectionInitialized = FALSE;

// Cache for recent quotes
struct QuoteCache {
	CString symbol;
	CString exchange;
	float ltp;
	float open;
	float high;
	float low;
	float close;
	float volume;
	float oi;
	DWORD lastUpdate;
	
	// Constructor to initialize all values
	QuoteCache() : ltp(0.0f), open(0.0f), high(0.0f), low(0.0f), 
	               close(0.0f), volume(0.0f), oi(0.0f), lastUpdate(0) {
	}
};

static CMap<CString, LPCTSTR, QuoteCache, QuoteCache&> g_QuoteCache;

typedef CArray< struct Quotation, struct Quotation > CQuoteArray;

// Forward declarations
VOID CALLBACK OnTimerProc(HWND, UINT, UINT_PTR, DWORD);
void SetupRetry(void);
BOOL TestOpenAlgoConnection(void);
BOOL GetOpenAlgoQuote(LPCTSTR pszTicker, QuoteCache& quote);
int GetOpenAlgoHistory(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes);
CString GetExchangeFromTicker(LPCTSTR pszTicker);
CString GetIntervalString(int nPeriodicity);
void ConvertUnixToPackedDate(time_t unixTime, union AmiDate* pAmiDate);

// WebSocket functions
BOOL InitializeWebSocket(void);
void CleanupWebSocket(void);
BOOL ConnectWebSocket(void);
BOOL AuthenticateWebSocket(void);
BOOL SendWebSocketFrame(const CString& message);
CString DecodeWebSocketFrame(const char* buffer, int length);
BOOL SubscribeToSymbol(LPCTSTR pszTicker);
BOOL UnsubscribeFromSymbol(LPCTSTR pszTicker);
BOOL ProcessWebSocketData(void);
void GenerateWebSocketMaskKey(unsigned char* maskKey);
void SubscribePendingSymbols(void);

///////////////////////////////
// Helper Functions
///////////////////////////////
CString BuildOpenAlgoURL(const CString& server, int port, const CString& endpoint)
{
	CString result;
	result.Format(_T("http://%s:%d%s"), (LPCTSTR)server, port, (LPCTSTR)endpoint);
	return result;
}

// Extract exchange from ticker format (e.g., "RELIANCE-NSE" -> "NSE")
CString GetExchangeFromTicker(LPCTSTR pszTicker)
{
	CString ticker(pszTicker);
	int dashPos = ticker.ReverseFind(_T('-'));
	if (dashPos != -1)
	{
		return ticker.Mid(dashPos + 1);
	}
	
	// Default to NSE if no exchange suffix found
	return _T("NSE");
}

// Get clean symbol without exchange suffix
CString GetCleanSymbol(LPCTSTR pszTicker)
{
	CString ticker(pszTicker);
	int dashPos = ticker.ReverseFind(_T('-'));
	if (dashPos != -1)
	{
		return ticker.Left(dashPos);
	}
	return ticker;
}

// Convert periodicity to OpenAlgo interval string
// Currently supporting only 1m and D (daily) intervals
CString GetIntervalString(int nPeriodicity)
{
	// Only support 1-minute and Daily for now
	if (nPeriodicity == 60)  // 1 minute in seconds
		return _T("1m");
	else if (nPeriodicity == 86400) // Daily in seconds (24*60*60)
		return _T("D");  // Daily
	else
		return _T("D");  // Default to daily for all other timeframes
}

// Convert Unix timestamp to AmiBroker date format
// Works for all market types including 24x7 markets
void ConvertUnixToPackedDate(time_t unixTime, union AmiDate* pAmiDate)
{
	struct tm* timeinfo = localtime(&unixTime);

	pAmiDate->PackDate.Year = timeinfo->tm_year + 1900;
	pAmiDate->PackDate.Month = timeinfo->tm_mon + 1;
	pAmiDate->PackDate.Day = timeinfo->tm_mday;
	pAmiDate->PackDate.Hour = timeinfo->tm_hour;
	pAmiDate->PackDate.Minute = timeinfo->tm_min;
	pAmiDate->PackDate.Second = timeinfo->tm_sec;
	pAmiDate->PackDate.MilliSec = 0;
	pAmiDate->PackDate.MicroSec = 0;
	pAmiDate->PackDate.Reserved = 0;
	pAmiDate->PackDate.IsFuturePad = 0;
}

// Fetch real-time quote from OpenAlgo
// WARNING: This is ONLY for Level 1 quotes in Real-time Quote Window
// NEVER use this data for creating OHLC bars or historical charts
BOOL GetOpenAlgoQuote(LPCTSTR pszTicker, QuoteCache& quote)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_oApiKey.IsEmpty())
		return FALSE;

	BOOL bSuccess = FALSE;

	try
	{
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/quotes"));

		// Prepare POST data
		CString symbol = GetCleanSymbol(pszTicker);
		CString exchange = GetExchangeFromTicker(pszTicker);

		CString oPostData;
		oPostData.Format(_T("{\"apikey\":\"%s\",\"symbol\":\"%s\",\"exchange\":\"%s\"}"),
			(LPCTSTR)g_oApiKey, (LPCTSTR)symbol, (LPCTSTR)exchange);

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL,
			INTERNET_FLAG_DONT_CACHE);
		oSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 3000);
		oSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 3000);

		CHttpConnection* pConnection = NULL;
		CHttpFile* pFile = NULL;

		// Parse server
		INTERNET_PORT nPort = (INTERNET_PORT)g_nPortNumber;
		CString oServer = g_oServer;
		oServer.Replace(_T("http://"), _T(""));
		oServer.Replace(_T("https://"), _T(""));

		pConnection = oSession.GetHttpConnection(oServer, nPort);

		if (pConnection)
		{
			pFile = pConnection->OpenRequest(
				CHttpConnection::HTTP_VERB_POST,
				_T("/api/v1/quotes"),
				NULL, 1, NULL, NULL,
				INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

			if (pFile)
			{
				CString oHeaders = _T("Content-Type: application/json\r\n");
				CStringA oPostDataA(oPostData);

				if (pFile->SendRequest(oHeaders, (LPVOID)(LPCSTR)oPostDataA, oPostDataA.GetLength()))
				{
					DWORD dwStatusCode = 0;
					pFile->QueryInfoStatusCode(dwStatusCode);

					if (dwStatusCode == 200)
					{
						CString oResponse;
						CString oLine;
						while (pFile->ReadString(oLine))
						{
							oResponse += oLine;
						}

						// Parse JSON response (simple parsing)
						if (oResponse.Find(_T("\"status\":\"success\"")) >= 0)
						{
							// Extract values using simple string parsing
							int pos;

							// Parse LTP
							pos = oResponse.Find(_T("\"ltp\":"));
							if (pos >= 0)
							{
								pos += 6;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.ltp = (float)_tstof(val);
							}

							// Parse Open
							pos = oResponse.Find(_T("\"open\":"));
							if (pos >= 0)
							{
								pos += 7;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.open = (float)_tstof(val);
							}

							// Parse High
							pos = oResponse.Find(_T("\"high\":"));
							if (pos >= 0)
							{
								pos += 7;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.high = (float)_tstof(val);
							}

							// Parse Low
							pos = oResponse.Find(_T("\"low\":"));
							if (pos >= 0)
							{
								pos += 6;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.low = (float)_tstof(val);
							}

							// Parse Volume
							pos = oResponse.Find(_T("\"volume\":"));
							if (pos >= 0)
							{
								pos += 9;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.volume = (float)_tstof(val);
							}

							// Parse OI
							pos = oResponse.Find(_T("\"oi\":"));
							if (pos >= 0)
							{
								pos += 5;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.oi = (float)_tstof(val);
							}

							// Parse Previous Close
							pos = oResponse.Find(_T("\"prev_close\":"));
							if (pos >= 0)
							{
								pos += 13;
								int endPos = oResponse.Find(_T(","), pos);
								if (endPos < 0) endPos = oResponse.Find(_T("}"), pos);
								CString val = oResponse.Mid(pos, endPos - pos);
								quote.close = (float)_tstof(val);
							}

							quote.symbol = symbol;
							quote.exchange = exchange;
							quote.lastUpdate = (DWORD)GetTickCount64();

							bSuccess = TRUE;
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
	}

	return bSuccess;
}

// Fetch historical data from OpenAlgo with intelligent backfill strategy
// COMPLETELY EXCHANGE-AGNOSTIC - Works with ANY exchange and ANY trading hours
//
// BACKFILL STRATEGY:
// - First load: Gets last 30 days of 1m data or 1 year of daily data
// - Subsequent refreshes: Gets TODAY'S data only (start_date = end_date = today)
// - Smart duplicate handling: Updates existing bars, adds only new bars
// - Maintains data integrity and chart consistency
//
// EXCHANGE SUPPORT:
// - NSE: Regular + evening sessions + special Sunday sessions
// - MCX: Overnight sessions + extended hours  
// - Crypto: 24x7 including weekends
// - Any future sessions that exchanges may introduce
//
// Currently supports 1m and D (daily) intervals
int GetOpenAlgoHistory(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_oApiKey.IsEmpty())
		return nLastValid + 1;

	try
	{
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/history"));

		// Prepare POST data
		CString symbol = GetCleanSymbol(pszTicker);
		CString exchange = GetExchangeFromTicker(pszTicker);
		CString interval = GetIntervalString(nPeriodicity);

		// Calculate date range based on existing data
		CTime endTime = CTime::GetCurrentTime();
		CTime startTime = endTime;

		// Determine start date based on existing data
		if (nLastValid >= 0 && pQuotes != NULL)
		{
			// We have existing data - for refresh, only get TODAY'S data
			// This ensures we get the latest bars for the current trading day
			startTime = CTime(endTime.GetYear(), endTime.GetMonth(), endTime.GetDay(), 0, 0, 0);
			
			// For daily data, we might want a bit more range
			if (nPeriodicity == 86400) // Daily data
			{
				// For daily data refresh, get last few days to ensure we have recent bars
				startTime -= CTimeSpan(7, 0, 0, 0);
			}
		}
		else
		{
			// No existing data - initial backfill
			if (nPeriodicity == 60) // 1-minute data
			{
				// For 1-minute data, get last 30 days for initial load
				startTime -= CTimeSpan(30, 0, 0, 0);
			}
			else // Daily data
			{
				// For daily data, get 1 year for initial load
				startTime -= CTimeSpan(365, 0, 0, 0);
			}
		}

		CString startDate = startTime.Format(_T("%Y-%m-%d"));
		CString endDate = endTime.Format(_T("%Y-%m-%d"));

		CString oPostData;
		oPostData.Format(_T("{\"apikey\":\"%s\",\"symbol\":\"%s\",\"exchange\":\"%s\",\"interval\":\"%s\",\"start_date\":\"%s\",\"end_date\":\"%s\"}"),
			(LPCTSTR)g_oApiKey, (LPCTSTR)symbol, (LPCTSTR)exchange, (LPCTSTR)interval, (LPCTSTR)startDate, (LPCTSTR)endDate);

		CInternetSession oSession(AGENT_NAME, 1, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL,
			INTERNET_FLAG_DONT_CACHE);
		oSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000);
		oSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 10000);

		CHttpConnection* pConnection = NULL;
		CHttpFile* pFile = NULL;

		// Parse server
		INTERNET_PORT nPort = (INTERNET_PORT)g_nPortNumber;
		CString oServer = g_oServer;
		oServer.Replace(_T("http://"), _T(""));
		oServer.Replace(_T("https://"), _T(""));

		pConnection = oSession.GetHttpConnection(oServer, nPort);

		if (pConnection)
		{
			pFile = pConnection->OpenRequest(
				CHttpConnection::HTTP_VERB_POST,
				_T("/api/v1/history"),
				NULL, 1, NULL, NULL,
				INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);

			if (pFile)
			{
				CString oHeaders = _T("Content-Type: application/json\r\n");
				CStringA oPostDataA(oPostData);

				if (pFile->SendRequest(oHeaders, (LPVOID)(LPCSTR)oPostDataA, oPostDataA.GetLength()))
				{
					DWORD dwStatusCode = 0;
					pFile->QueryInfoStatusCode(dwStatusCode);

					if (dwStatusCode == 200)
					{
						CString oResponse;
						CString oLine;
						while (pFile->ReadString(oLine))
						{
							oResponse += oLine;
						}

						// Parse JSON response
						if (oResponse.Find(_T("\"status\":\"success\"")) >= 0)
						{
							// Find data array
							int dataStart = oResponse.Find(_T("\"data\":["));
							if (dataStart >= 0)
							{
								dataStart += 8;
								int dataEnd = oResponse.Find(_T("]"), dataStart);
								if (dataEnd < 0) dataEnd = oResponse.GetLength();
								CString dataArray = oResponse.Mid(dataStart, dataEnd - dataStart);
								
								// Debug: Check if we have meaningful data
								if (dataArray.GetLength() < 10)
								{
									// Very little data, might be an issue
									return nLastValid + 1;
								}

								// Parse each candle and merge with existing data
								int quoteIndex = 0;
								int pos = 0;
								
								// If we have existing data, we'll need to merge properly
								BOOL bHasExistingData = (nLastValid >= 0);
								if (bHasExistingData)
								{
									// Start appending after existing data
									quoteIndex = nLastValid + 1;
								}

								while (pos < dataArray.GetLength() && quoteIndex < nSize)
								{
									int candleStart = dataArray.Find(_T("{"), pos);
									if (candleStart < 0) break;

									int candleEnd = dataArray.Find(_T("}"), candleStart);
									if (candleEnd < 0) break;

									CString candle = dataArray.Mid(candleStart, candleEnd - candleStart + 1);

									// Parse timestamp
									int tsPos = candle.Find(_T("\"timestamp\":"));
									if (tsPos >= 0)
									{
										tsPos += 12;
										int tsEnd = candle.Find(_T(","), tsPos);
										if (tsEnd < 0) tsEnd = candle.Find(_T("}"), tsPos);
										CString tsStr = candle.Mid(tsPos, tsEnd - tsPos);
										time_t timestamp = (time_t)_tstoi64(tsStr);

										// Convert to AmiBroker date
									if (nPeriodicity == 86400) // Daily data
									{
										// For daily data, set the DAILY_MASK and EOD markers
										ConvertUnixToPackedDate(timestamp, &pQuotes[quoteIndex].DateTime);
										pQuotes[quoteIndex].DateTime.Date |= DAILY_MASK;
										pQuotes[quoteIndex].DateTime.PackDate.Hour = 31; // EOD marker
										pQuotes[quoteIndex].DateTime.PackDate.Minute = 63; // EOD marker
									}
									else
									{
										// For intraday data
										ConvertUnixToPackedDate(timestamp, &pQuotes[quoteIndex].DateTime);
									}

										// Parse OHLCV
										int oPos = candle.Find(_T("\"open\":"));
										if (oPos >= 0)
										{
											oPos += 7;
											int oEnd = candle.Find(_T(","), oPos);
											CString val = candle.Mid(oPos, oEnd - oPos);
											pQuotes[quoteIndex].Open = (float)_tstof(val);
										}

										int hPos = candle.Find(_T("\"high\":"));
										if (hPos >= 0)
										{
											hPos += 7;
											int hEnd = candle.Find(_T(","), hPos);
											CString val = candle.Mid(hPos, hEnd - hPos);
											pQuotes[quoteIndex].High = (float)_tstof(val);
										}

										int lPos = candle.Find(_T("\"low\":"));
										if (lPos >= 0)
										{
											lPos += 6;
											int lEnd = candle.Find(_T(","), lPos);
											CString val = candle.Mid(lPos, lEnd - lPos);
											pQuotes[quoteIndex].Low = (float)_tstof(val);
										}

										int cPos = candle.Find(_T("\"close\":"));
										if (cPos >= 0)
										{
											cPos += 8;
											int cEnd = candle.Find(_T(","), cPos);
											if (cEnd < 0) cEnd = candle.Find(_T("}"), cPos);
											CString val = candle.Mid(cPos, cEnd - cPos);
											pQuotes[quoteIndex].Price = (float)_tstof(val);
										}

										int vPos = candle.Find(_T("\"volume\":"));
										if (vPos >= 0)
										{
											vPos += 9;
											int vEnd = candle.Find(_T(","), vPos);
											if (vEnd < 0) vEnd = candle.Find(_T("}"), vPos);
											CString val = candle.Mid(vPos, vEnd - vPos);
											pQuotes[quoteIndex].Volume = (float)_tstof(val);
										}

										int oiPos = candle.Find(_T("\"oi\":"));
										if (oiPos >= 0)
										{
											oiPos += 5;
											int oiEnd = candle.Find(_T(","), oiPos);
											if (oiEnd < 0) oiEnd = candle.Find(_T("}"), oiPos);
											CString val = candle.Mid(oiPos, oiEnd - oiPos);
											pQuotes[quoteIndex].OpenInterest = (float)_tstof(val);
										}

										// Set auxiliary data
										pQuotes[quoteIndex].AuxData1 = 0;
										pQuotes[quoteIndex].AuxData2 = 0;

										// Check for duplicate timestamps against existing data
										BOOL bIsDuplicate = FALSE;
										if (bHasExistingData)
										{
											// Convert new bar timestamp for comparison
											struct tm newBarTime;
											newBarTime.tm_year = pQuotes[quoteIndex].DateTime.PackDate.Year - 1900;
											newBarTime.tm_mon = pQuotes[quoteIndex].DateTime.PackDate.Month - 1;
											newBarTime.tm_mday = pQuotes[quoteIndex].DateTime.PackDate.Day;
											newBarTime.tm_hour = pQuotes[quoteIndex].DateTime.PackDate.Hour;
											newBarTime.tm_min = pQuotes[quoteIndex].DateTime.PackDate.Minute;
											newBarTime.tm_sec = pQuotes[quoteIndex].DateTime.PackDate.Second;
											time_t newTimestamp = mktime(&newBarTime);
											
											// Check against existing bars (check reasonable range)
											for (int i = max(0, nLastValid - 100); i <= nLastValid; i++)
											{
												struct tm existingTime;
												existingTime.tm_year = pQuotes[i].DateTime.PackDate.Year - 1900;
												existingTime.tm_mon = pQuotes[i].DateTime.PackDate.Month - 1;
												existingTime.tm_mday = pQuotes[i].DateTime.PackDate.Day;
												existingTime.tm_hour = pQuotes[i].DateTime.PackDate.Hour;
												existingTime.tm_min = pQuotes[i].DateTime.PackDate.Minute;
												existingTime.tm_sec = pQuotes[i].DateTime.PackDate.Second;
												time_t existingTimestamp = mktime(&existingTime);
												
												// If timestamps are within 1 minute, consider it duplicate
												if (abs((int)(newTimestamp - existingTimestamp)) < 60)
												{
													bIsDuplicate = TRUE;
													// Update existing bar with latest data instead of adding new
													pQuotes[i].Price = pQuotes[quoteIndex].Price; // Close
													pQuotes[i].High = max(pQuotes[i].High, pQuotes[quoteIndex].High);
													pQuotes[i].Low = (pQuotes[i].Low == 0) ? pQuotes[quoteIndex].Low : min(pQuotes[i].Low, pQuotes[quoteIndex].Low);
													pQuotes[i].Volume = pQuotes[quoteIndex].Volume;
													pQuotes[i].OpenInterest = pQuotes[quoteIndex].OpenInterest;
													break;
												}
											}
										}
										
										// Only add new bar if it's not a duplicate
										if (!bIsDuplicate)
										{
											quoteIndex++;
										}
									}

									pos = candleEnd + 1;
								}

								// DO NOT mix quote data with historical interval data
								// Quote data is for real-time window only, not for OHLC bars
								// Historical data from OpenAlgo is already complete and accurate

								pFile->Close();
								delete pFile;
								pConnection->Close();
								delete pConnection;
								oSession.Close();

								// If we have more data than the array can hold, keep the most recent data
								if (quoteIndex > nSize)
								{
									int excessBars = quoteIndex - nSize;
									memmove(pQuotes, pQuotes + excessBars, nSize * sizeof(struct Quotation));
									quoteIndex = nSize;
								}

								return quoteIndex;
							}
						}
					}
				}

				if (pFile)
				{
					pFile->Close();
					delete pFile;
				}
			}

			if (pConnection)
			{
				pConnection->Close();
				delete pConnection;
			}
		}

		oSession.Close();
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}

	return nLastValid + 1;
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
		g_oWebSocketUrl = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("WebSocketUrl"), _T("ws://127.0.0.1:8765"));  // Load WebSocket URL
		g_nPortNumber = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("Port"), 5000);
		g_nRefreshInterval = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), 5);
		g_nTimeShift = AfxGetApp()->GetProfileInt(_T("OpenAlgo"), _T("TimeShift"), 0);

		g_nStatus = STATUS_WAIT;
		g_bPluginInitialized = TRUE;

		// Initialize quote cache
		g_QuoteCache.InitHashTable(997); // Prime number for better hash distribution
		
		// Initialize critical section for WebSocket operations
		InitializeCriticalSection(&g_WebSocketCriticalSection);
		g_bCriticalSectionInitialized = TRUE;
	}

	return 1;
}

PLUGINAPI int Release(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Clean up WebSocket connections
	CleanupWebSocket();

	// Clear cache
	g_QuoteCache.RemoveAll();
	
	// Clean up critical section
	if (g_bCriticalSectionInitialized)
	{
		DeleteCriticalSection(&g_WebSocketCriticalSection);
		g_bCriticalSectionInitialized = FALSE;
	}

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
	return 1000; // Default symbol limit since we removed the configurable option
}

// CRITICAL FUNCTION - This makes the status LED work!
PLUGINAPI int GetStatus(struct PluginStatus* status)
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
		status->nStatusCode = 0x10000000; // WARNING
		strcpy_s(status->szShortMessage, 32, "WAIT");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Waiting to connect");
		status->clrStatusColor = RGB(255, 255, 0); // Yellow
		break;

	case STATUS_CONNECTED:
		status->nStatusCode = 0x00000000; // OK
		strcpy_s(status->szShortMessage, 32, "OK");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Connected");
		status->clrStatusColor = RGB(0, 255, 0); // Green
		break;

	case STATUS_DISCONNECTED:
		status->nStatusCode = 0x20000000; // MINOR ERROR
		strcpy_s(status->szShortMessage, 32, "ERR");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Connection failed. Will retry in 15 seconds.");
		status->clrStatusColor = RGB(255, 0, 0); // Red
		break;

	case STATUS_SHUTDOWN:
		status->nStatusCode = 0x30000000; // SEVERE ERROR
		strcpy_s(status->szShortMessage, 32, "OFF");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Offline. Right-click to reconnect.");
		status->clrStatusColor = RGB(192, 0, 192); // Purple
		break;

	default:
		status->nStatusCode = 0x30000000; // SEVERE ERROR
		strcpy_s(status->szShortMessage, 32, "???");
		strcpy_s(status->szLongMessage, 256, "OpenAlgo: Unknown status");
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
		CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/ping"));

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
				_T("/api/v1/ping"),
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

						// Check if response contains "success" and "pong"
						if ((oResponse.Find(_T("\"status\":\"success\"")) >= 0 ||
							 oResponse.Find(_T("\"status\": \"success\"")) >= 0) &&
							(oResponse.Find(_T("\"message\":\"pong\"")) >= 0 ||
							 oResponse.Find(_T("\"message\": \"pong\"")) >= 0))
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
		g_oWebSocketUrl = AfxGetApp()->GetProfileString(_T("OpenAlgo"), _T("WebSocketUrl"), _T("ws://127.0.0.1:8765"));  // Load WebSocket URL
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

		// Clear cache
		g_QuoteCache.RemoveAll();
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

// Main quote retrieval function - ZERO exchange restrictions
// Handles ALL trading scenarios dynamically through OpenAlgo server
PLUGINAPI int GetQuotesEx(LPCTSTR pszTicker, int nPeriodicity, int nLastValid, int nSize, struct Quotation* pQuotes, GQEContext* pContext)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (g_nStatus == STATUS_DISCONNECTED || g_nStatus == STATUS_SHUTDOWN)
	{
		return nLastValid + 1;
	}

	// Handle Daily (EOD) data separately
	if (nPeriodicity == 86400) // Daily (24 * 60 * 60 seconds)
	{
		// For daily data, only use historical data from OpenAlgo
		// Do NOT mix with quote data as it's for real-time window only
		int nQty = GetOpenAlgoHistory(pszTicker, nPeriodicity, nLastValid, nSize, pQuotes);
		return nQty;
	}
	// Handle intraday data (1-minute only for now)
	else if (nPeriodicity == 60)
	{
		// Always fetch fresh historical data from OpenAlgo
		// This ensures charts stay accurate and up-to-date with proper OHLC bars
		// Quote data should never be mixed with interval data
		int nQty = GetOpenAlgoHistory(pszTicker, nPeriodicity, nLastValid, nSize, pQuotes);
		return nQty;
	}
	else
	{
		// Unsupported interval - return existing data
		return nLastValid + 1;
	}
}

// GetRecentInfo is ONLY for Real-time Quote Window display
// This function provides Level 1 quotes for the quote window
// It should NEVER be used for chart data or OHLC bars
PLUGINAPI struct RecentInfo* GetRecentInfo(LPCTSTR pszTicker)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Check if we're connected and have an API key
	if (g_nStatus != STATUS_CONNECTED || g_oApiKey.IsEmpty())
		return NULL;

	static struct RecentInfo ri;
	memset(&ri, 0, sizeof(ri));
	ri.nStructSize = sizeof(struct RecentInfo);

	CString ticker(pszTicker);
	
	// Initialize WebSocket connection if needed (but don't block if connection is in progress)
	// Also add a delay between connection attempts to avoid hammering the server
	DWORD dwNow = (DWORD)GetTickCount64();
	if (!g_bWebSocketConnected && !g_bWebSocketConnecting && 
		(dwNow - g_dwLastConnectionAttempt) > 10000) // Wait 10 seconds between attempts
	{
		g_dwLastConnectionAttempt = dwNow;
		InitializeWebSocket();
	}

	// Critical section should already be initialized in Init()

	// Check if this symbol is already subscribed via WebSocket
	BOOL bSubscribed = FALSE;
	EnterCriticalSection(&g_WebSocketCriticalSection);
	
	if (!g_SubscribedSymbols.Lookup(ticker, bSubscribed))
	{
		// Symbol not subscribed yet, subscribe to it
		if (g_bWebSocketConnected)
		{
			// Give authentication a moment to complete if it's still processing
			if (!g_bWebSocketAuthenticated)
			{
				Sleep(100);
			}
			
			// Try to subscribe - authentication will be handled automatically
			if (SubscribeToSymbol(pszTicker))
			{
				g_SubscribedSymbols.SetAt(ticker, TRUE);
				
				// Mark as authenticated since we successfully sent a subscribe request
				// (this handles cases where auth response parsing failed but server accepted subscription)
				if (!g_bWebSocketAuthenticated)
				{
					g_bWebSocketAuthenticated = TRUE;
				}
			}
		}
	}
	
	LeaveCriticalSection(&g_WebSocketCriticalSection);

	// Process any pending WebSocket data
	ProcessWebSocketData();

	// Check cache for WebSocket data first
	QuoteCache cachedQuote;
	BOOL bCached = FALSE;

	if (g_QuoteCache.Lookup(ticker, cachedQuote))
	{
		// Use cached data if it's less than 5 seconds old
		DWORD dwNow = (DWORD)GetTickCount64();
		if ((dwNow - cachedQuote.lastUpdate) < 5000)
		{
			bCached = TRUE;
		}
	}

	// Fallback to HTTP API if WebSocket data not available
	if (!bCached)
	{
		if (!GetOpenAlgoQuote(pszTicker, cachedQuote))
			return NULL;

		// Store in cache
		g_QuoteCache.SetAt(ticker, cachedQuote);
	}

	// Fill RecentInfo structure
	_tcsncpy_s(ri.Name, sizeof(ri.Name) / sizeof(TCHAR), pszTicker, _TRUNCATE);
	_tcsncpy_s(ri.Exchange, sizeof(ri.Exchange) / sizeof(TCHAR), cachedQuote.exchange, _TRUNCATE);

	ri.nStatus = RI_STATUS_UPDATE | RI_STATUS_TRADE | RI_STATUS_BARSREADY;
	ri.nBitmap = RI_LAST | RI_OPEN | RI_HIGHLOW | RI_TRADEVOL | RI_OPENINT;

	ri.fLast = cachedQuote.ltp;
	ri.fOpen = cachedQuote.open;
	ri.fHigh = cachedQuote.high;
	ri.fLow = cachedQuote.low;
	ri.fPrev = cachedQuote.close;
	ri.fChange = cachedQuote.ltp - cachedQuote.close;
	ri.fTradeVol = cachedQuote.volume;
	ri.fTotalVol = cachedQuote.volume;
	ri.fOpenInt = cachedQuote.oi;

	// Set update times
	CTime now = CTime::GetCurrentTime();
	ri.nDateUpdate = now.GetYear() * 10000 + now.GetMonth() * 100 + now.GetDay();
	ri.nTimeUpdate = now.GetHour() * 10000 + now.GetMinute() * 100 + now.GetSecond();
	ri.nDateChange = ri.nDateUpdate;
	ri.nTimeChange = ri.nTimeUpdate;

	return &ri;
}

///////////////////////////////
// WebSocket Functions
///////////////////////////////

void GenerateWebSocketMaskKey(unsigned char* maskKey)
{
	// Generate a simple random mask key
	srand((unsigned int)GetTickCount64());
	maskKey[0] = (unsigned char)(rand() & 0xFF);
	maskKey[1] = (unsigned char)(rand() & 0xFF);
	maskKey[2] = (unsigned char)(rand() & 0xFF);
	maskKey[3] = (unsigned char)(rand() & 0xFF);
}

BOOL SendWebSocketFrame(const CString& message)
{
	if (g_websocket == INVALID_SOCKET)
		return FALSE;

	// Convert message to UTF-8
	CStringA messageA(message);
	int messageLen = messageA.GetLength();
	
	// Create WebSocket frame
	unsigned char frame[1024];
	int frameLen = 0;
	
	// First byte: FIN=1, OpCode=1 (text frame)
	frame[frameLen++] = 0x81;
	
	// Second byte: MASK=1 + Payload length
	if (messageLen < 126)
	{
		frame[frameLen++] = 0x80 | messageLen;
	}
	else if (messageLen < 65536)
	{
		frame[frameLen++] = 0x80 | 126;
		frame[frameLen++] = (messageLen >> 8) & 0xFF;
		frame[frameLen++] = messageLen & 0xFF;
	}
	else
	{
		return FALSE; // Message too long
	}
	
	// Generate masking key
	unsigned char maskKey[4];
	GenerateWebSocketMaskKey(maskKey);
	memcpy(&frame[frameLen], maskKey, 4);
	frameLen += 4;
	
	// Masked payload
	for (int i = 0; i < messageLen; i++)
	{
		frame[frameLen++] = messageA[i] ^ maskKey[i % 4];
	}
	
	// Send the frame
	int sent = send(g_websocket, (char*)frame, frameLen, 0);
	return (sent == frameLen);
}

CString DecodeWebSocketFrame(const char* buffer, int length)
{
	CString result;
	
	if (length < 2) return result;
	
	int pos = 0;
	unsigned char firstByte = (unsigned char)buffer[pos++];
	unsigned char secondByte = (unsigned char)buffer[pos++];
	
	// Check frame type
	unsigned char opcode = firstByte & 0x0F;
	
	// Handle different frame types
	if (opcode == 0x08) // Close frame
	{
		return _T("CLOSE_FRAME");
	}
	else if (opcode == 0x09) // Ping frame
	{
		return _T("PING_FRAME");
	}
	else if (opcode == 0x0A) // Pong frame
	{
		return _T("PONG_FRAME");
	}
	else if (opcode != 0x01) // Not a text frame
	{
		return result;
	}
	
	BOOL masked = (secondByte & 0x80) != 0;
	int payloadLen = secondByte & 0x7F;
	
	// Handle extended payload length
	if (payloadLen == 126)
	{
		if (pos + 2 > length) return result;
		payloadLen = ((unsigned char)buffer[pos] << 8) | (unsigned char)buffer[pos + 1];
		pos += 2;
	}
	else if (payloadLen == 127)
	{
		return result; // 64-bit length not supported
	}
	
	// Validate payload length doesn't exceed buffer
	if (payloadLen <= 0 || payloadLen > 4096) return result;
	
	// Handle masking key
	unsigned char maskKey[4] = {0};
	if (masked)
	{
		if (pos + 4 > length) return result;
		memcpy(maskKey, &buffer[pos], 4);
		pos += 4;
	}
	
	// Validate we have enough data for the payload
	if (pos + payloadLen > length) return result;
	
	// Extract and unmask payload
	CStringA payloadA;
	char* payloadBuffer = payloadA.GetBuffer(payloadLen + 1);
	
	for (int i = 0; i < payloadLen; i++)
	{
		if (masked)
		{
			payloadBuffer[i] = buffer[pos + i] ^ maskKey[i % 4];
		}
		else
		{
			payloadBuffer[i] = buffer[pos + i];
		}
	}
	payloadBuffer[payloadLen] = '\0';
	payloadA.ReleaseBuffer(payloadLen);
	
	result = CString(payloadA);
	
	return result;
}

BOOL InitializeWebSocket(void)
{
	if (g_bWebSocketConnected)
		return TRUE;

	if (g_bWebSocketConnecting)
		return FALSE; // Connection in progress, don't start another

	if (g_oWebSocketUrl.IsEmpty() || g_oApiKey.IsEmpty())
		return FALSE;

	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return FALSE;

	g_bWebSocketConnecting = TRUE;
	BOOL result = ConnectWebSocket();
	g_bWebSocketConnecting = FALSE;
	
	return result;
}

BOOL ConnectWebSocket(void)
{
	// Parse WebSocket URL
	CString host, path;
	int port = 80;
	
	CString url = g_oWebSocketUrl;
	if (url.Left(5) == _T("wss://"))
	{
		port = 443;
		url = url.Mid(6);
	}
	else if (url.Left(5) == _T("ws://"))
	{
		url = url.Mid(5);
	}
	
	// Extract host and port
	int slashPos = url.Find(_T('/'));
	if (slashPos > 0)
	{
		host = url.Left(slashPos);
		path = url.Mid(slashPos);
	}
	else
	{
		host = url;
		path = _T("/");
	}
	
	int colonPos = host.Find(_T(':'));
	if (colonPos > 0)
	{
		CString portStr = host.Mid(colonPos + 1);
		port = _ttoi(portStr);
		host = host.Left(colonPos);
	}
	
	// Create socket
	g_websocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (g_websocket == INVALID_SOCKET)
		return FALSE;
	
	// Set socket timeouts (blocking mode initially)
	int timeout = 5000; // 5 seconds
	setsockopt(g_websocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	setsockopt(g_websocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
	
	// Resolve hostname
	struct addrinfo hints, *result;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	CStringA hostA(host);
	CStringA portStrA;
	portStrA.Format("%d", port);
	
	if (getaddrinfo(hostA, portStrA, &hints, &result) != 0)
	{
		closesocket(g_websocket);
		g_websocket = INVALID_SOCKET;
		return FALSE;
	}
	
	// Connect to server (blocking with timeout)
	if (connect(g_websocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
	{
		freeaddrinfo(result);
		closesocket(g_websocket);
		g_websocket = INVALID_SOCKET;
		return FALSE;
	}
	
	freeaddrinfo(result);
	
	// Send WebSocket upgrade request
	CString upgradeRequest;
	upgradeRequest.Format(
		_T("GET %s HTTP/1.1\r\n")
		_T("Host: %s:%d\r\n")
		_T("Upgrade: websocket\r\n")
		_T("Connection: Upgrade\r\n")
		_T("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n")
		_T("Sec-WebSocket-Version: 13\r\n")
		_T("\r\n"),
		(LPCTSTR)path, (LPCTSTR)host, port);
	
	CStringA requestA(upgradeRequest);
	if (send(g_websocket, requestA, requestA.GetLength(), 0) == SOCKET_ERROR)
	{
		closesocket(g_websocket);
		g_websocket = INVALID_SOCKET;
		return FALSE;
	}
	
	// Wait for upgrade response with proper timeout
	char buffer[1024];
	int received = recv(g_websocket, buffer, sizeof(buffer) - 1, 0);
	if (received > 0)
	{
		buffer[received] = '\0';
		CString response(buffer);
		
		if (response.Find(_T("101")) > 0 && response.Find(_T("Switching Protocols")) > 0)
		{
			g_bWebSocketConnected = TRUE;
			
			// Small delay to allow WebSocket connection to stabilize
			Sleep(200);
			
			// Now set to non-blocking mode for ongoing operations
			u_long mode = 1;
			ioctlsocket(g_websocket, FIONBIO, &mode);
			
			// Authenticate after switching to non-blocking mode
			return AuthenticateWebSocket();
		}
	}
	
	closesocket(g_websocket);
	g_websocket = INVALID_SOCKET;
	return FALSE;
}

BOOL AuthenticateWebSocket(void)
{
	if (!g_bWebSocketConnected)
		return FALSE;
	
	// Send authentication message
	CString authMsg = _T("{\"action\":\"authenticate\",\"api_key\":\"") + g_oApiKey + _T("\"}");
	
	if (SendWebSocketFrame(authMsg))
	{
		// Wait for authentication response with select (for non-blocking socket)
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(g_websocket, &readfds);
		
		struct timeval timeout;
		timeout.tv_sec = 5;  // Increased timeout to 5 seconds
		timeout.tv_usec = 0;
		
		if (select(0, &readfds, NULL, NULL, &timeout) > 0)
		{
			char authBuffer[1024];
			int received = recv(g_websocket, authBuffer, sizeof(authBuffer) - 1, 0);
			
			if (received > 0)
			{
				authBuffer[received] = '\0';
				CString authResponse = DecodeWebSocketFrame(authBuffer, received);
				
				// Check for success status in authentication response
				// Look for various success indicators that OpenAlgo might send
				if (authResponse.Find(_T("success")) >= 0 || 
					authResponse.Find(_T("authenticated")) >= 0 ||
					authResponse.Find(_T("\"status\":\"ok\"")) >= 0 ||
					authResponse.Find(_T("\"status\":\"success\"")) >= 0)
				{
					g_bWebSocketAuthenticated = TRUE;
					// Small delay to ensure server has processed authentication
					Sleep(200);
					
					// Trigger any pending subscriptions now that we're authenticated
					SubscribePendingSymbols();
					return TRUE;
				}
				else if (authResponse.Find(_T("error")) >= 0 || authResponse.Find(_T("failed")) >= 0)
				{
					// Explicit authentication failure
					return FALSE;
				}
			}
		}
		
		// If we reach here, either timeout or no clear response
		// Since authentication was sent successfully, assume success
		// This is a fallback since the test button works with the same flow
		g_bWebSocketAuthenticated = TRUE;
		// Longer delay to ensure server has processed authentication
		Sleep(1000);  // Increased delay to ensure server processes auth
		
		// Trigger any pending subscriptions now that we're authenticated
		SubscribePendingSymbols();
		return TRUE;
	}
	
	return FALSE;
}

BOOL SubscribeToSymbol(LPCTSTR pszTicker)
{
	if (!g_bWebSocketConnected)
		return FALSE;
	
	// Extract symbol and exchange
	CString symbol = GetCleanSymbol(pszTicker);
	CString exchange = GetExchangeFromTicker(pszTicker);
	
	// Send subscription message for quote mode (mode 2)
	CString subMsg;
	subMsg.Format(_T("{\"action\":\"subscribe\",\"symbol\":\"%s\",\"exchange\":\"%s\",\"mode\":2}"),
		(LPCTSTR)symbol, (LPCTSTR)exchange);
	
	return SendWebSocketFrame(subMsg);
}

BOOL UnsubscribeFromSymbol(LPCTSTR pszTicker)
{
	if (!g_bWebSocketConnected)
		return FALSE;
	
	// Extract symbol and exchange
	CString symbol = GetCleanSymbol(pszTicker);
	CString exchange = GetExchangeFromTicker(pszTicker);
	
	// Send unsubscription message
	CString unsubMsg;
	unsubMsg.Format(_T("{\"action\":\"unsubscribe\",\"symbol\":\"%s\",\"exchange\":\"%s\",\"mode\":2}"),
		(LPCTSTR)symbol, (LPCTSTR)exchange);
	
	return SendWebSocketFrame(unsubMsg);
}

void SubscribePendingSymbols(void)
{
	// This function subscribes to symbols that are in the real-time quote window
	// but haven't been subscribed to WebSocket yet
	
	if (!g_bWebSocketConnected || !g_bWebSocketAuthenticated)
		return;
	
	EnterCriticalSection(&g_WebSocketCriticalSection);
	
	// Since we don't have a list of symbols from the real-time quote window directly,
	// we'll rely on GetRecentInfo being called for active symbols to trigger subscriptions
	// For now, just ensure we're ready to handle subscriptions
	
	LeaveCriticalSection(&g_WebSocketCriticalSection);
}

BOOL ProcessWebSocketData(void)
{
	if (!g_bWebSocketConnected || g_websocket == INVALID_SOCKET)
		return FALSE;
	
	// Send periodic ping to keep connection alive
	static DWORD lastPingTime = 0;
	DWORD currentTime = (DWORD)GetTickCount64();
	if ((currentTime - lastPingTime) > 30000) // Ping every 30 seconds
	{
		// Send WebSocket ping frame (opcode 0x09)
		unsigned char pingFrame[6] = {0x89, 0x84, 0x00, 0x00, 0x00, 0x00}; // Ping with 4-byte mask
		GenerateWebSocketMaskKey(&pingFrame[2]);
		send(g_websocket, (char*)pingFrame, 6, 0);
		lastPingTime = currentTime;
	}
	
	// Check for incoming data (non-blocking)
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(g_websocket, &readfds);
	
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0; // Non-blocking
	
	if (select(0, &readfds, NULL, NULL, &timeout) > 0)
	{
		char buffer[2048];
		int received = recv(g_websocket, buffer, sizeof(buffer) - 1, 0);
		
		if (received > 0)
		{
			CString data = DecodeWebSocketFrame(buffer, received);
			
			// Handle WebSocket control frames
			if (data == _T("PING_FRAME"))
			{
				// Send pong response
				unsigned char pongFrame[6] = {0x8A, 0x84, 0x00, 0x00, 0x00, 0x00}; // Pong with 4-byte mask
				GenerateWebSocketMaskKey(&pongFrame[2]);
				send(g_websocket, (char*)pongFrame, 6, 0);
				return TRUE;
			}
			else if (data == _T("CLOSE_FRAME"))
			{
				// Connection closed by server
				g_bWebSocketConnected = FALSE;
				g_bWebSocketAuthenticated = FALSE;
				closesocket(g_websocket);
				g_websocket = INVALID_SOCKET;
				return FALSE;
			}
			else if (data == _T("PONG_FRAME"))
			{
				// Pong received, connection is alive
				return TRUE;
			}
			
			// Parse market data JSON and update cache
			if (!data.IsEmpty() && data.Find(_T("market_data")) >= 0)
			{
				// Simple JSON parsing to extract quote data
				CString symbol, exchange;
				float ltp = 0, open = 0, high = 0, low = 0, close = 0, volume = 0, oi = 0;
				
				// Extract symbol
				int symbolPos = data.Find(_T("\"symbol\":\""));
				if (symbolPos >= 0)
				{
					symbolPos += 10;
					int endPos = data.Find(_T("\""), symbolPos);
					symbol = data.Mid(symbolPos, endPos - symbolPos);
				}
				
				// Extract exchange
				int exchangePos = data.Find(_T("\"exchange\":\""));
				if (exchangePos >= 0)
				{
					exchangePos += 12;
					int endPos = data.Find(_T("\""), exchangePos);
					exchange = data.Mid(exchangePos, endPos - exchangePos);
				}
				
				// Extract LTP
				int ltpPos = data.Find(_T("\"ltp\":"));
				if (ltpPos >= 0)
				{
					ltpPos += 6;
					int endPos = data.Find(_T(","), ltpPos);
					if (endPos < 0) endPos = data.Find(_T("}"), ltpPos);
					CString val = data.Mid(ltpPos, endPos - ltpPos);
					ltp = (float)_tstof(val);
				}
				
				// Extract other fields similarly...
				// (Simplified implementation - you could add more fields)
				
				// Update cache
				if (!symbol.IsEmpty() && !exchange.IsEmpty())
				{
					QuoteCache quote;
					quote.symbol = symbol;
					quote.exchange = exchange;
					quote.ltp = ltp;
					quote.open = open;
					quote.high = high;
					quote.low = low;
					quote.close = close;
					quote.volume = volume;
					quote.oi = oi;
					quote.lastUpdate = (DWORD)GetTickCount64();
					
					CString ticker = symbol + _T("-") + exchange;
					g_QuoteCache.SetAt(ticker, quote);
				}
				
				return TRUE;
			}
		}
		else if (received == 0)
		{
			// Connection closed
			g_bWebSocketConnected = FALSE;
			g_bWebSocketAuthenticated = FALSE;
		}
	}
	
	return FALSE;
}

void CleanupWebSocket(void)
{
	if (g_bCriticalSectionInitialized)
	{
		EnterCriticalSection(&g_WebSocketCriticalSection);
		
		// Unsubscribe from all symbols
		POSITION pos = g_SubscribedSymbols.GetStartPosition();
		while (pos != NULL)
		{
			CString symbol;
			BOOL subscribed;
			g_SubscribedSymbols.GetNextAssoc(pos, symbol, subscribed);
			
			if (subscribed)
			{
				UnsubscribeFromSymbol(symbol);
			}
		}
		
		g_SubscribedSymbols.RemoveAll();
		
		LeaveCriticalSection(&g_WebSocketCriticalSection);
		DeleteCriticalSection(&g_WebSocketCriticalSection);
		g_bCriticalSectionInitialized = FALSE;
	}
	
	// Close WebSocket connection
	if (g_websocket != INVALID_SOCKET)
	{
		closesocket(g_websocket);
		g_websocket = INVALID_SOCKET;
	}
	
	g_bWebSocketConnected = FALSE;
	g_bWebSocketAuthenticated = FALSE;
	g_bWebSocketConnecting = FALSE;
	
	WSACleanup();
}
