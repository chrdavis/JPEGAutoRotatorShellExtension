#include "stdafx.h"
#include "resource.h"

#include <windowsx.h>
#include <RotationInterfaces.h>
#include <RotationUI.h>
#include <RotationManager.h>

#define MAX_LOADSTRING 100


#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

HINSTANCE g_hInst;

PCWSTR g_rotationLabel[] =
{
    L"None",
    L"None",
    L"RotateNoneFlipX",
    L"Rotate180FlipNone",
    L"RotateNoneFlipY",
    L"Rotate270FlipY",
    L"Rotate90FlipNone",
    L"Rotate90FlipY",
    L"Rotate270FlipNone"
};

// TODO: We should just implement IRotationUI here
class CJPEGAutoRotatorTestUI: 
    public IDropTarget,
    public IRotationManagerEvents
{
public:
    CJPEGAutoRotatorTestUI()
    {
        OleInitialize(0);
    }

    HRESULT DoModal(__in_opt HWND hwnd)
    {
        HRESULT hr = S_OK;
        INT_PTR ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_MAIN), hwnd, s_DlgProc, (LPARAM)this);
        if (ret < 0)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        return hr;
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(__in REFIID riid, __deref_out void **ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CJPEGAutoRotatorTestUI, IDropTarget),
            QITABENT(CJPEGAutoRotatorTestUI, IRotationManagerEvents),
            {},
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long cRef = InterlockedDecrement(&m_cRef);
        if (!cRef)
            delete this;
        return cRef;
    }

    // IDropTarget
    IFACEMETHODIMP DragEnter(IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
    IFACEMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
    IFACEMETHODIMP DragLeave();
    IFACEMETHODIMP Drop(IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

    // IRotationManagerEvents
    IFACEMETHODIMP OnItemAdded(__in UINT uIndex);
    IFACEMETHODIMP OnItemProcessed(__in UINT uIndex);
    IFACEMETHODIMP OnProgress(__in UINT uCompleted, __in UINT uTotal);
    IFACEMETHODIMP OnCanceled();
    IFACEMETHODIMP OnCompleted();

    static INT_PTR CALLBACK s_DlgProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CJPEGAutoRotatorTestUI* pJPEGAutoRotatorTestUI = reinterpret_cast<CJPEGAutoRotatorTestUI *>(GetWindowLongPtr(hdlg, DWLP_USER));
        if (uMsg == WM_INITDIALOG)
        {
            pJPEGAutoRotatorTestUI = reinterpret_cast<CJPEGAutoRotatorTestUI *>(lParam);
            pJPEGAutoRotatorTestUI->m_hdlg = hdlg;
            SetWindowLongPtr(hdlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pJPEGAutoRotatorTestUI));
        }
        return pJPEGAutoRotatorTestUI ? pJPEGAutoRotatorTestUI->_DlgProc(uMsg, wParam, lParam) : FALSE;
    }

private:
    ~CJPEGAutoRotatorTestUI()
    {
        OleUninitialize();
    }

    INT_PTR _DlgProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void _OnInit();
    void _OnDestroy();
    void _OnCommand(__in const int commandId);
    bool _OnNotify(__in WPARAM wParam, __in LPARAM lParam);

    void _OnStart();
    void _OnStop();
    void _OnClear();

    void _AddItemToListView(__in IRotationItem* pri);
    void _UpdateItemInListView(__in int index, __in IRotationItem* pri);
    void _UpdateStatusPostOperation(__in bool wasCanceled);

    HRESULT _InitRotationManager();
    void _CleanupRotationManager();

    long m_cRef = 1;
    HWND m_hdlg = 0;
    HWND m_hwndLV = 0;
    HWND m_hwndProgress = 0;

    ULONGLONG m_operationStartTime = 0;
    ULONGLONG m_operationEndTime = 0;

    DWORD m_adviseCookie = 0;

    CComPtr<IDropTargetHelper> m_spdth;
    CComPtr<IRotationManager> m_sprm;
    CComPtr<IRotationUI> m_sprui;
};

// IDropTarget
IFACEMETHODIMP CJPEGAutoRotatorTestUI::DragEnter(IDataObject *pdtobj, DWORD /* grfKeyState */, POINTL pt, DWORD *pdwEffect)
{
    if (m_spdth)
    {
        POINT ptT = { pt.x, pt.y };
        m_spdth->DragEnter(m_hdlg, pdtobj, &ptT, *pdwEffect);
    }
    
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::DragOver(DWORD /* grfKeyState */, POINTL pt, DWORD *pdwEffect)
{
    // leave *pdwEffect unchanged, we support all operations
    if (m_spdth)
    {
        POINT ptT = { pt.x, pt.y };
        m_spdth->DragOver(&ptT, *pdwEffect);
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::DragLeave()
{
    if (m_spdth)
    {
        m_spdth->DragLeave();
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::Drop(IDataObject *pdtobj, DWORD, POINTL pt, DWORD *pdwEffect)
{
    if (m_spdth)
    {
        POINT ptT = { pt.x, pt.y };
        m_spdth->Drop(pdtobj, &ptT, *pdwEffect);
    }

    _OnClear();

    // Cache the data object and enable the start button
    EnableWindow(GetDlgItem(m_hdlg, IDC_START), TRUE);
    EnableWindow(m_hwndLV, TRUE);

    _InitRotationManager();

    if (m_sprui)
    {
        m_sprui->Initialize(pdtobj);
    }

    return S_OK;
}

// IRotationManagerEvents
IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnItemAdded(__in UINT uIndex)
{
    // Get the IRotationItem and add it to our list view
    if (m_sprm)
    {
        CComPtr<IRotationItem> spri;
        if (SUCCEEDED(m_sprm->GetItem(uIndex, &spri)))
        {
            _AddItemToListView(spri);
        }
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnItemProcessed(__in UINT uIndex)
{
    // Update the item in our list view
    if (m_sprm)
    {
        CComPtr<IRotationItem> spri;
        if (SUCCEEDED(m_sprm->GetItem(uIndex, &spri)))
        {
            _UpdateItemInListView(uIndex, spri);
        }
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnProgress(__in UINT uCompleted, __in UINT uTotal)
{
    // Update progress control
    SendMessage(m_hwndProgress, PBM_SETPOS, uCompleted, 0);
    wchar_t message[100];
    StringCchPrintf(message, ARRAYSIZE(message), L"Completed %u of %u items", uCompleted, uTotal);
    SendMessage(GetDlgItem(m_hdlg, IDC_PROGRESSTEXT), WM_SETTEXT, 0, (LPARAM)message);
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnCanceled()
{
    m_operationEndTime = GetTickCount64();
    EnableWindow(GetDlgItem(m_hdlg, IDC_START), TRUE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_CLEAR), TRUE);
    _UpdateStatusPostOperation(true);
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnCompleted()
{
    m_operationEndTime = GetTickCount64();
    EnableWindow(GetDlgItem(m_hdlg, IDC_START), TRUE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_CLEAR), TRUE);
    _UpdateStatusPostOperation(false);
    return S_OK;
}

INT_PTR CJPEGAutoRotatorTestUI::_DlgProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    INT_PTR ret = TRUE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        _OnInit();
        break;

    case WM_COMMAND:
        _OnCommand(GET_WM_COMMAND_ID(wParam, lParam));
        break;
    
    case WM_NOTIFY:
        ret = _OnNotify(wParam, lParam);
        break;

    case WM_DESTROY:
        _OnDestroy();
        ret = FALSE;
        break;

    default:
        ret = FALSE;
    }
    return ret;
}

void CJPEGAutoRotatorTestUI::_OnInit()
{
    CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&m_spdth));

    // Initialize list view
    m_hwndLV = GetDlgItem(m_hdlg, IDC_RENAMEITEMLIST);

    // Initialize the progress control
    m_hwndProgress = GetDlgItem(m_hdlg, IDC_PROGRESS);

    // Initialize columns
    wchar_t szColumnName[100];

    LV_COLUMN lvc = {};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 100;
    lvc.pszText = szColumnName;

    PCWSTR columnNames[] = {
        L"Path",
        L"IsValidJPEG",
        L"IsRotationLossless",
        L"OriginalOrientation",
        L"WasRotated",
        L"Result",
    };

    for (int i = 0; i < ARRAYSIZE(columnNames); i++)
    {
        StringCchCopy(szColumnName, ARRAYSIZE(szColumnName), columnNames[i]);
        ListView_InsertColumn(m_hwndLV, i, &lvc);
    }

    RegisterDragDrop(m_hdlg, this);

    // Disable until items added
    EnableWindow(m_hwndLV, FALSE);
}

bool CJPEGAutoRotatorTestUI::_OnNotify(__in WPARAM, __in LPARAM lParam)
{
    bool ret = false;
    NMLISTVIEW * pnm = reinterpret_cast<NMLISTVIEW*>(lParam);
    switch (pnm->hdr.code)
    {
    case LVN_GETEMPTYMARKUP:
        {
            NMLVEMPTYMARKUP *pem = (NMLVEMPTYMARKUP *)lParam;
            pem->dwFlags = EMF_CENTERED;
            StringCchCopy(pem->szMarkup, ARRAYSIZE(pem->szMarkup), L"Drag and Drop Files for the JPEGAutoRotator to use.  Then click Start.");
            ret = true;
            break;
        }
    }
    return ret;
}

void CJPEGAutoRotatorTestUI::_OnCommand(const int commandId)
{
    switch (commandId)
    {
    case IDC_START:
        _OnStart();
        break;
    case IDC_STOP:
        _OnStop();
        break;
    case IDC_CLEAR:
        _OnClear();
        break;
    case IDCANCEL:
        EndDialog(m_hdlg, commandId);
        break;
    }
}

void CJPEGAutoRotatorTestUI::_OnDestroy()
{
    _OnStop();

    RevokeDragDrop(m_hdlg);

    SetWindowLongPtr(m_hdlg, GWLP_USERDATA, NULL);
    m_hdlg = NULL;
}

void CJPEGAutoRotatorTestUI::_OnStart()
{
    EnableWindow(GetDlgItem(m_hdlg, IDC_START), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_CLEAR), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_STOP), TRUE);
    EnableWindow(m_hwndProgress, TRUE);

    UINT itemCount = 0;
    if (m_sprm)
    {
        m_sprm->GetItemCount(&itemCount);
    }

    // Initialize the progress control range
    SendMessage(m_hwndProgress, PBM_SETRANGE32, 0, itemCount);

    // Set to marque mode
    SendMessage(m_hwndProgress, PBM_SETRANGE32, TRUE, 0);

    // Init status text
    SendMessage(GetDlgItem(m_hdlg, IDC_STATUS), WM_SETTEXT, 0, (LPARAM)L"Status: Running...");
    SendMessage(GetDlgItem(m_hdlg, IDC_TIME), WM_SETTEXT, 0, (LPARAM)L"");

    m_operationStartTime = GetTickCount64();
    HRESULT hr = m_sprui->Start();

    if (FAILED(hr))
    {
        MessageBox(m_hdlg, L"Failed to start rotation operation.", L"Rotation Failed", MB_OK | MB_ICONERROR);
        EnableWindow(GetDlgItem(m_hdlg, IDC_START), TRUE);
    }
}

void CJPEGAutoRotatorTestUI::_OnStop()
{
    EnableWindow(GetDlgItem(m_hdlg, IDC_STOP), FALSE);

    // Cancel via the rotation manager
    if (m_sprm)
    {
        m_sprm->Cancel();
    }

    if (m_sprui)
    {
        m_sprui->Close();
        m_sprui = nullptr;
    }
}

void CJPEGAutoRotatorTestUI::_OnClear()
{
    EnableWindow(GetDlgItem(m_hdlg, IDC_CLEAR), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_hdlg, IDC_START), FALSE);

    // Clear the list view
    if (m_hwndLV)
    {
        ListView_DeleteAllItems(m_hwndLV);
    }

    EnableWindow(m_hwndLV, FALSE);
    
    SendMessage(m_hwndProgress, PBM_SETPOS, 0, 0);
    EnableWindow(m_hwndProgress, FALSE);

    SendMessage(m_hwndProgress, PBM_SETRANGE32, 0, 0);
    SendMessage(GetDlgItem(m_hdlg, IDC_PROGRESSTEXT), WM_SETTEXT, 0, (LPARAM)L"");
    SendMessage(GetDlgItem(m_hdlg, IDC_STATUS), WM_SETTEXT, 0, (LPARAM)L"");
    SendMessage(GetDlgItem(m_hdlg, IDC_TIME), WM_SETTEXT, 0, (LPARAM)L"");

    _CleanupRotationManager();
}

HRESULT CJPEGAutoRotatorTestUI::_InitRotationManager()
{
    _CleanupRotationManager();

    HRESULT hr = CRotationManager::s_CreateInstance(&m_sprm);
    if (SUCCEEDED(hr))
    {
        hr = m_sprm->Advise(this, &m_adviseCookie);
        if (SUCCEEDED(hr))
        {
            hr = CRotationUI::s_CreateInstance(m_sprm, &m_sprui);
        }
    }

    return hr;
}

void CJPEGAutoRotatorTestUI::_CleanupRotationManager()
{
    if (m_sprui)
    {
        m_sprui->Close();
        m_sprui = nullptr;
    }

    if (m_sprm)
    {
        m_sprm->Shutdown();
        m_sprm = nullptr;
    }
}

void CJPEGAutoRotatorTestUI::_AddItemToListView(__in IRotationItem* pri)
{
    LV_ITEM lvi = { 0 };
    lvi.iItem = ListView_GetItemCount(m_hwndLV);
    lvi.iSubItem = 0;

    PWSTR itemPath = nullptr;
    pri->get_Path(&itemPath);
    lvi.pszText = itemPath;

    ListView_InsertItem(m_hwndLV, &lvi);

    // Update the sub items
    _UpdateItemInListView(lvi.iItem, pri);
}

void CJPEGAutoRotatorTestUI::_UpdateItemInListView(__in int index, __in IRotationItem* pri)
{
    PWSTR itemPath = nullptr;
    pri->get_Path(&itemPath);

    LV_ITEM lvi = { 0 };

    lvi.iItem = index;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = itemPath;
    lvi.iSubItem = 0;
    lvi.pszText = itemPath;
    ListView_SetItem(m_hwndLV, &lvi);

    CoTaskMemFree(itemPath);

    HRESULT hrResult;
    pri->get_Result(&hrResult);

    // Check if this item has been run through the manager
    // S_FALSE means it has not
    if (hrResult != S_FALSE)
    {
        wchar_t szColumnValue[100] = { 0 };

        BOOL fResult;
        pri->get_IsValidJPEG(&fResult);
        StringCchCopy(szColumnValue, ARRAYSIZE(szColumnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 1;
        lvi.pszText = szColumnValue;
        ListView_SetItem(m_hwndLV, &lvi);

        szColumnValue[0] = L'\0';
        pri->get_IsRotationLossless(&fResult);
        StringCchCopy(szColumnValue, ARRAYSIZE(szColumnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 2;
        lvi.pszText = szColumnValue;
        ListView_SetItem(m_hwndLV, &lvi);

        szColumnValue[0] = L'\0';
        UINT uOriginalOrientation = 1;
        pri->get_OriginalOrientation(&uOriginalOrientation);
        lvi.iSubItem = 3;
        PCWSTR pszLabel = (uOriginalOrientation < ARRAYSIZE(g_rotationLabel)) ? g_rotationLabel[uOriginalOrientation] : L"Invalid!";
        StringCchCopy(szColumnValue, ARRAYSIZE(szColumnValue), pszLabel);
        lvi.pszText = szColumnValue;
        ListView_SetItem(m_hwndLV, &lvi);

        szColumnValue[0] = L'\0';
        pri->get_WasRotated(&fResult);
        StringCchCopy(szColumnValue, ARRAYSIZE(szColumnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 4;
        lvi.pszText = szColumnValue;
        ListView_SetItem(m_hwndLV, &lvi);

        szColumnValue[0] = L'\0';
        StringCchCopy(szColumnValue, ARRAYSIZE(szColumnValue), SUCCEEDED(hrResult) ? L"SUCCEEDED" : L"FAILED");
        lvi.iSubItem = 5;
        lvi.pszText = szColumnValue;
        ListView_SetItem(m_hwndLV, &lvi);
    }

    ListView_SetColumnWidth(m_hwndLV, 0, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndLV, 1, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndLV, 2, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndLV, 3, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndLV, 4, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_hwndLV, 6, LVSCW_AUTOSIZE);
}

void CJPEGAutoRotatorTestUI::_UpdateStatusPostOperation(__in bool wasCanceled)
{
    SendMessage(GetDlgItem(m_hdlg, IDC_STATUS), WM_SETTEXT, 0, wasCanceled ? (LPARAM)L"Status: Canceled" : (LPARAM)L"Status: Completed");
    ULONGLONG ullDiff = m_operationEndTime - m_operationStartTime;
    wchar_t message[100] = { 0 };
    StringCchPrintf(message, ARRAYSIZE(message), L"Duration: %u ms", ullDiff);
    SendMessage(GetDlgItem(m_hdlg, IDC_TIME), WM_SETTEXT, 0, (LPARAM)message);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    g_hInst = hInstance;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        INITCOMMONCONTROLSEX iccex;
        ZeroMemory(&iccex, sizeof(iccex));
        iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        iccex.dwICC = ICC_LINK_CLASS | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&iccex);

        CJPEGAutoRotatorTestUI *pdlg = new CJPEGAutoRotatorTestUI();
        if (pdlg)
        {
            pdlg->DoModal(NULL);
            pdlg->Release();
        }
        CoUninitialize();
    }
    return 0;
}