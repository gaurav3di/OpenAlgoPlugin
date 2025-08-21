// OpenAlgoConfigDlg.cpp : implementation file - Fixed version
#include "stdafx.h"
#include "Plugin.h"
#include "OpenAlgoPlugin.h"
#include "OpenAlgoConfigDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int		g_nPortNumber;
extern int		g_nRefreshInterval;
extern BOOL		g_bAutoAddSymbols;
extern int		g_nSymbolLimit;
extern BOOL		g_bOptimizedIntraday;
extern int		g_nTimeShift;
extern CString  g_oServer;

extern CString GetAvailableSymbols(void);

/////////////////////////////////////////////////////////////////////////////
// COpenAlgoConfigDlg dialog

COpenAlgoConfigDlg::COpenAlgoConfigDlg(CWnd* pParent /*=NULL*/)
	: CDialog(COpenAlgoConfigDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(COpenAlgoConfigDlg)
	//}}AFX_DATA_INIT

	// Fixed: Initialize m_pSite member variable
	m_pSite = NULL;
}

void COpenAlgoConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COpenAlgoConfigDlg)
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_SERVER_EDIT, g_oServer);

	DDX_Check(pDX, IDC_AUTOSYMBOLS_CHECK, g_bAutoAddSymbols);
	DDX_Text(pDX, IDC_PORT_EDIT, g_nPortNumber);
	DDV_MinMaxInt(pDX, g_nPortNumber, 1, 65535);

	DDX_Text(pDX, IDC_INTERVAL_EDIT, g_nRefreshInterval);
	DDV_MinMaxInt(pDX, g_nRefreshInterval, 1, 60);

	DDX_Text(pDX, IDC_MAXSYMBOL_EDIT, g_nSymbolLimit);
	DDV_MinMaxInt(pDX, g_nSymbolLimit, 10, 500);

	DDX_Check(pDX, IDC_OPTIMIZED_INTRADAY_CHECK, g_bOptimizedIntraday);

	DDX_Text(pDX, IDC_TIMESHIFT_EDIT, g_nTimeShift);
	DDV_MinMaxInt(pDX, g_nTimeShift, -48, 48);
}

BEGIN_MESSAGE_MAP(COpenAlgoConfigDlg, CDialog)
	//{{AFX_MSG_MAP(COpenAlgoConfigDlg)
	ON_BN_CLICKED(IDC_RETRIEVE_BUTTON, OnRetrieveButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COpenAlgoConfigDlg message handlers

BOOL COpenAlgoConfigDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	return TRUE;  // return TRUE unless you set the focus to a control
}

void COpenAlgoConfigDlg::OnOK()
{
	CDialog::OnOK();

	// Save settings to registry under "OpenAlgo" key instead of "QuoteTracker"
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("TimeShift"), g_nTimeShift);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("Port"), g_nPortNumber);
	AfxGetApp()->WriteProfileString(_T("OpenAlgo"), _T("Server"), g_oServer);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("RefreshInterval"), g_nRefreshInterval);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("AutoAddSymbols"), g_bAutoAddSymbols);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("SymbolLimit"), g_nSymbolLimit);
	AfxGetApp()->WriteProfileInt(_T("OpenAlgo"), _T("OptimizedIntraday"), g_bOptimizedIntraday);
}

void COpenAlgoConfigDlg::OnRetrieveButton()
{
	// Fixed: Check if m_pSite is valid before using it
	if (m_pSite == NULL)
	{
		SetDlgItemText(IDC_STATUS_STATIC, _T("Error: Site interface not available"));
		return;
	}

	CString oSymbolList = GetAvailableSymbols();
	CString oSymbol;
	int iCnt = 0;  // Fixed: Initialize the variable

	// Fixed: Use proper loop with _T() macro for Unicode support
	for (iCnt = 0; AfxExtractSubString(oSymbol, oSymbolList, iCnt, _T(',')); iCnt++)
	{
		// Skip internal OpenAlgo test symbols instead of "MEDVED"
		if (oSymbol != _T("OPENALGO_TEST"))
		{
			// Fixed: Check if AddStock method is available
			if (m_pSite->nStructSize >= sizeof(struct InfoSite))
			{
				m_pSite->AddStockNew(oSymbol);
			}
			else
			{
				// Fallback to older method if available
				m_pSite->AddStock(oSymbol);
			}
		}
	}

	oSymbol.Format(_T("Retrieved %d symbols"), iCnt);
	SetDlgItemText(IDC_STATUS_STATIC, oSymbol);
}