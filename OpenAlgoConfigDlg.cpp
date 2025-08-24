// OpenAlgoConfigDlg.cpp : implementation file
#include "stdafx.h"
#include "resource.h"  // Include resource definitions
#include "Plugin.h"  // Include Plugin.h before OpenAlgoGlobals.h
#include "OpenAlgoGlobals.h"  // Include global variables
#include "OpenAlgoPlugin.h"
#include "OpenAlgoConfigDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Global variables are now properly declared in OpenAlgoGlobals.h
// No need for extern declarations here

/////////////////////////////////////////////////////////////////////////////
// COpenAlgoConfigDlg dialog

COpenAlgoConfigDlg::COpenAlgoConfigDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COpenAlgoConfigDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(COpenAlgoConfigDlg)
	//}}AFX_DATA_INIT

	// Initialize m_pSite member variable
	m_pSite = NULL;
}

void COpenAlgoConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COpenAlgoConfigDlg)
	//}}AFX_DATA_MAP

	// Map dialog controls to global variables
	DDX_Text(pDX, IDC_SERVER_EDIT, g_oServer);
	DDV_MaxChars(pDX, g_oServer, 255);

	DDX_Text(pDX, IDC_APIKEY_EDIT, g_oApiKey);
	DDV_MaxChars(pDX, g_oApiKey, 255);

	DDX_Check(pDX, IDC_AUTOSYMBOLS_CHECK, g_bAutoAddSymbols);

	DDX_Text(pDX, IDC_PORT_EDIT, g_nPortNumber);
	DDV_MinMaxInt(pDX, g_nPortNumber, 1, 65535);

	DDX_Text(pDX, IDC_INTERVAL_EDIT, g_nRefreshInterval);
	DDV_MinMaxInt(pDX, g_nRefreshInterval, 1, 3600); // 1 second to 1 hour

	DDX_Text(pDX, IDC_MAXSYMBOL_EDIT, g_nSymbolLimit);
	DDV_MinMaxInt(pDX, g_nSymbolLimit, 1, 1000);

	DDX_Check(pDX, IDC_OPTIMIZED_INTRADAY_CHECK, g_bOptimizedIntraday);

	DDX_Text(pDX, IDC_TIMESHIFT_EDIT, g_nTimeShift);
	DDV_MinMaxInt(pDX, g_nTimeShift, -48, 48);
}

BEGIN_MESSAGE_MAP(COpenAlgoConfigDlg, CDialog)
	//{{AFX_MSG_MAP(COpenAlgoConfigDlg)
	ON_BN_CLICKED(IDC_RETRIEVE_BUTTON, OnRetrieveButton)
	ON_BN_CLICKED(IDC_TEST_CONNECTION_BUTTON, OnTestConnectionButton)
	ON_BN_CLICKED(IDC_AUTOSYMBOLS_CHECK, OnAutoSymbolsCheck)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COpenAlgoConfigDlg message handlers

BOOL COpenAlgoConfigDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set dialog title
	SetWindowText(_T("OpenAlgo Plugin Configuration"));

	// Update control states based on current settings
	UpdateControlStates();

	// Set focus to server edit control
	CWnd* pServerEdit = GetDlgItem(IDC_SERVER_EDIT);
	if (pServerEdit)
	{
		pServerEdit->SetFocus();
	}

	return FALSE;  // We set the focus manually
}

void COpenAlgoConfigDlg::OnOK()
{
	// Update data from controls
	if (!UpdateData(TRUE))
	{
		return; // Validation failed
	}

	// Validate server address
	if (g_oServer.IsEmpty())
	{
		AfxMessageBox(_T("Please enter a server address."), MB_OK | MB_ICONWARNING);
		GetDlgItem(IDC_SERVER_EDIT)->SetFocus();
		return;
	}

	// Validate API Key
	if (g_oApiKey.IsEmpty())
	{
		AfxMessageBox(_T("Please enter your OpenAlgo API Key."), MB_OK | MB_ICONWARNING);
		GetDlgItem(IDC_APIKEY_EDIT)->SetFocus();
		return;
	}

	// Save settings to registry under "OpenAlgo" key
	AfxGetApp()->WriteProfileString(_T("OpenAlgo"), _T("Server"), g_oServer);
	AfxGetApp()->WriteProfileString(_T("OpenAlgo"), _T("ApiKey"), g_oApiKey);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("Port"), g_nPortNumber);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), g_nRefreshInterval);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("AutoAddSymbols"), g_bAutoAddSymbols);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("SymbolLimit"), g_nSymbolLimit);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("OptimizedIntraday"), g_bOptimizedIntraday);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("TimeShift"), g_nTimeShift);

	CDialog::OnOK();
}

void COpenAlgoConfigDlg::OnRetrieveButton()
{
	// Update data from controls first
	if (!UpdateData(TRUE))
	{
		return;
	}

	// Check if m_pSite is valid before using it
	if (m_pSite == NULL)
	{
		SetDlgItemText(IDC_STATUS_STATIC, _T("Error: AmiBroker interface not available"));
		return;
	}

	// Show progress
	SetDlgItemText(IDC_STATUS_STATIC, _T("Retrieving symbols from OpenAlgo server..."));

	// Change cursor to wait cursor
	CWaitCursor wait;

	// Get available symbols from OpenAlgo server
	CString oSymbolList = GetAvailableSymbols();

	if (oSymbolList.IsEmpty())
	{
		SetDlgItemText(IDC_STATUS_STATIC, _T("Failed to retrieve symbols. Check server connection."));
		return;
	}

	CString oSymbol;
	int iCount = 0;
	int iAdded = 0;

	// Parse the comma-separated symbol list
	int iPos = 0;
	while (AfxExtractSubString(oSymbol, oSymbolList, iPos, _T(',')))
	{
		iPos++;
		iCount++;

		// Trim whitespace
		oSymbol.TrimLeft();
		oSymbol.TrimRight();

		if (!oSymbol.IsEmpty())
		{
			// Skip test symbols
			if (oSymbol.CompareNoCase(_T("OPENALGO_TEST")) != 0 &&
				oSymbol.CompareNoCase(_T("TEST")) != 0)
			{
				// Add symbol to AmiBroker database
				// Check if AddStockNew is available (newer AmiBroker versions)
				if (m_pSite->nStructSize >= sizeof(struct InfoSite))
				{
					if (m_pSite->AddStockNew)
					{
						m_pSite->AddStockNew(oSymbol);
						iAdded++;
					}
					else if (m_pSite->AddStock)
					{
						m_pSite->AddStock(oSymbol);
						iAdded++;
					}
				}
				else if (m_pSite->AddStock)
				{
					// Fallback for older versions
					m_pSite->AddStock(oSymbol);
					iAdded++;
				}
			}
		}

		// Limit the number of symbols if needed
		if (iAdded >= g_nSymbolLimit)
		{
			break;
		}
	}

	// Update status
	CString oStatus;
	oStatus.Format(_T("Retrieved %d symbols, added %d to database"), iCount, iAdded);
	SetDlgItemText(IDC_STATUS_STATIC, oStatus);
}

void COpenAlgoConfigDlg::OnTestConnectionButton()
{
	// Update data from controls
	if (!UpdateData(TRUE))
	{
		return;
	}

	// Validate API Key
	if (g_oApiKey.IsEmpty())
	{
		AfxMessageBox(_T("Please enter your OpenAlgo API Key before testing the connection."), MB_OK | MB_ICONWARNING);
		GetDlgItem(IDC_APIKEY_EDIT)->SetFocus();
		return;
	}

	SetDlgItemText(IDC_STATUS_STATIC, _T("Testing connection..."));

	// Change cursor to wait cursor
	CWaitCursor wait;

	// Test connection to OpenAlgo server using ping endpoint
	BOOL bConnected = FALSE;
	CString oURL = BuildOpenAlgoURL(g_oServer, g_nPortNumber, _T("/api/v1/ping"));

	try
	{
		CInternetSession oSession(_T("OpenAlgo Plugin Test"), 1,
			INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_DONT_CACHE);
		oSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
		oSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 5000);

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
						// Try to read response
						CString oResponse;
						CString oLine;
						while (pFile->ReadString(oLine))
						{
							oResponse += oLine;
							if (oResponse.GetLength() > 1000) break; // Limit response size
						}

						// Check if response contains "success" and "pong" (simple JSON parsing)
						if ((oResponse.Find(_T("\"status\":\"success\"")) >= 0 ||
							 oResponse.Find(_T("\"status\": \"success\"")) >= 0) &&
							(oResponse.Find(_T("\"message\":\"pong\"")) >= 0 ||
							 oResponse.Find(_T("\"message\": \"pong\"")) >= 0))
						{
							bConnected = TRUE;

							// Try to extract broker from response
							int iBrokerPos = oResponse.Find(_T("\"broker\":"));
							if (iBrokerPos >= 0)
							{
								iBrokerPos += 10; // Move past "broker":"
								int iEndPos = oResponse.Find(_T("\""), iBrokerPos);
								if (iEndPos > iBrokerPos)
								{
									CString oBroker = oResponse.Mid(iBrokerPos, iEndPos - iBrokerPos);
									CString oStatus;
									oStatus.Format(_T("Connection successful! Broker: %s"), (LPCTSTR)oBroker);
									SetDlgItemText(IDC_STATUS_STATIC, oStatus);
								}
								else
								{
									SetDlgItemText(IDC_STATUS_STATIC,
										_T("Connection successful! OpenAlgo server is running."));
								}
							}
							else
							{
								SetDlgItemText(IDC_STATUS_STATIC,
									_T("Connection successful! OpenAlgo server is running."));
							}
						}
						else if (oResponse.Find(_T("\"status\":\"error\"")) >= 0)
						{
							SetDlgItemText(IDC_STATUS_STATIC,
								_T("Connection failed: Invalid API Key or server error."));
						}
						else
						{
							SetDlgItemText(IDC_STATUS_STATIC,
								_T("Connection failed: Unexpected response from server."));
						}
					}
					else
					{
						CString oStatus;
						oStatus.Format(_T("Server returned HTTP status code: %lu"), dwStatusCode);
						SetDlgItemText(IDC_STATUS_STATIC, oStatus);
					}
				}
				else
				{
					SetDlgItemText(IDC_STATUS_STATIC, _T("Failed to send request to server."));
				}

				pFile->Close();
				delete pFile;
			}

			pConnection->Close();
			delete pConnection;
		}
		else
		{
			SetDlgItemText(IDC_STATUS_STATIC, _T("Failed to connect to server."));
		}

		oSession.Close();
	}
	catch (CInternetException* e)
	{
		TCHAR szError[256];
		e->GetErrorMessage(szError, 256);

		CString oStatus;
		oStatus.Format(_T("Connection failed: %s"), szError);
		SetDlgItemText(IDC_STATUS_STATIC, oStatus);

		e->Delete();
	}
}

void COpenAlgoConfigDlg::OnAutoSymbolsCheck()
{
	UpdateData(TRUE);
	UpdateControlStates();
}

void COpenAlgoConfigDlg::UpdateControlStates()
{
	// Enable/disable controls based on auto-add symbols setting
	BOOL bEnableSymbolLimit = (g_bAutoAddSymbols == TRUE);

	CWnd* pSymbolLimitEdit = GetDlgItem(IDC_MAXSYMBOL_EDIT);
	if (pSymbolLimitEdit)
	{
		pSymbolLimitEdit->EnableWindow(bEnableSymbolLimit);
	}

	CWnd* pSymbolLimitLabel = GetDlgItem(IDC_STATIC_MAXSYMBOLS);
	if (pSymbolLimitLabel)
	{
		pSymbolLimitLabel->EnableWindow(bEnableSymbolLimit);
	}
}