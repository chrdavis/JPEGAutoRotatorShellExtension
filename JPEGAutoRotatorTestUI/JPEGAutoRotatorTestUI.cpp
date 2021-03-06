#include "stdafx.h"
#include "resource.h"

#include <windowsx.h>
#include <RotationInterfaces.h>
#include <RotationManager.h>
#include <helpers.h>

#define MAX_LOADSTRING 100

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

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

class CJPEGAutoRotatorTestUI :
    public IDropTarget,
    public IRotationUI,
    public IRotationManagerEvents
{
public:
    CJPEGAutoRotatorTestUI()
    {
        (void)OleInitialize(0);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CJPEGAutoRotatorTestUI, IDropTarget),
            QITABENT(CJPEGAutoRotatorTestUI, IRotationUI),
            QITABENT(CJPEGAutoRotatorTestUI, IRotationManagerEvents),
            {},
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_refCount);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        long refCount = InterlockedDecrement(&m_refCount);
        if (!refCount)
            delete this;
        return refCount;
    }

    // IDropTarget
    IFACEMETHODIMP DragEnter(_In_ IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, _Out_ DWORD *pdwEffect);
    IFACEMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, _Out_ DWORD *pdwEffect);
    IFACEMETHODIMP DragLeave();
    IFACEMETHODIMP Drop(_In_ IDataObject *pdtobj, DWORD grfKeyState, POINTL pt, _Out_ DWORD *pdwEffect);

    // IRotationUI
    IFACEMETHODIMP Start()
    {
        HRESULT hr = S_OK;
        INT_PTR ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_MAIN), nullptr, s_DlgProc, (LPARAM)this);
        if (ret < 0)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        return hr;
    }

    IFACEMETHODIMP Close()
    {
        return S_OK;
    }

    // IRotationManagerEvents
    IFACEMETHODIMP OnItemAdded(_In_ UINT index);
    IFACEMETHODIMP OnItemProcessed(_In_ UINT index);
    IFACEMETHODIMP OnProgress(_In_ UINT completed, _In_ UINT total);
    IFACEMETHODIMP OnCanceled();
    IFACEMETHODIMP OnCompleted();

    static INT_PTR CALLBACK s_DlgProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CJPEGAutoRotatorTestUI* pJPEGAutoRotatorTestUI = reinterpret_cast<CJPEGAutoRotatorTestUI *>(GetWindowLongPtr(hdlg, DWLP_USER));
        if (uMsg == WM_INITDIALOG)
        {
            pJPEGAutoRotatorTestUI = reinterpret_cast<CJPEGAutoRotatorTestUI *>(lParam);
            pJPEGAutoRotatorTestUI->m_dialogWindow = hdlg;
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
    void _OnCommand(_In_ const int commandId);

    void _OnStart();
    void _OnStop();
    void _OnClear();

    void _AddItemToListView(_In_ IRotationItem* pItem);
    void _UpdateItemInListView(_In_ int index, _In_ IRotationItem* pItem);
    void _UpdateStatusPostOperation(_In_ bool wasCanceled);

    HRESULT _InitRotationManager();
    void _CleanupRotationManager();

    long m_refCount = 1;
    HWND m_dialogWindow = 0;
    HWND m_listviewWindow = 0;
    HWND m_progressWindow = 0;

    ULONGLONG m_operationStartTime = 0;
    ULONGLONG m_operationEndTime = 0;

    DWORD m_adviseCookie = 0;

    CComPtr<IDropTargetHelper> m_spdth;
    CComPtr<IRotationManager> m_sprm;
};

// IDropTarget
IFACEMETHODIMP CJPEGAutoRotatorTestUI::DragEnter(_In_ IDataObject *pdtobj, DWORD /* grfKeyState */, POINTL pt, _Out_ DWORD *pdwEffect)
{
    if (m_spdth)
    {
        POINT ptT = { pt.x, pt.y };
        m_spdth->DragEnter(m_dialogWindow, pdtobj, &ptT, *pdwEffect);
    }
    
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::DragOver(DWORD /* grfKeyState */, POINTL pt, _Out_ DWORD *pdwEffect)
{
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

IFACEMETHODIMP CJPEGAutoRotatorTestUI::Drop(_In_ IDataObject *pdtobj, DWORD, POINTL pt, _Out_ DWORD *pdwEffect)
{
    if (m_spdth)
    {
        POINT ptT = { pt.x, pt.y };
        m_spdth->Drop(pdtobj, &ptT, *pdwEffect);
    }

    _OnClear();

    // Cache the data object and enable the start button
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), TRUE);
    EnableWindow(m_listviewWindow, TRUE);

    _InitRotationManager();

    // Enumerate the data object and popuplate the roaming manager
    EnumerateDataObject(pdtobj, m_sprm);

    return S_OK;
}

// IRotationManagerEvents
IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnItemAdded(_In_ UINT index)
{
    // Get the IRotationItem and add it to our list view
    if (m_sprm)
    {
        CComPtr<IRotationItem> spItem;
        if (SUCCEEDED(m_sprm->GetItem(index, &spItem)))
        {
            _AddItemToListView(spItem);
        }
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnItemProcessed(_In_ UINT index)
{
    // Update the item in our list view
    if (m_sprm)
    {
        CComPtr<IRotationItem> spItem;
        if (SUCCEEDED(m_sprm->GetItem(index, &spItem)))
        {
            _UpdateItemInListView(index, spItem);
        }
    }

    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnProgress(_In_ UINT completed, _In_ UINT total)
{
    // Update progress control
    SendMessage(m_progressWindow, PBM_SETPOS, completed, 0);
    wchar_t message[100];
    StringCchPrintf(message, ARRAYSIZE(message), L"Completed %u of %u items", completed, total);
    SendMessage(GetDlgItem(m_dialogWindow, IDC_PROGRESSTEXT), WM_SETTEXT, 0, (LPARAM)message);
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnCanceled()
{
    m_operationEndTime = GetTickCount64();
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), TRUE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_CLEAR), TRUE);
    _UpdateStatusPostOperation(true);
    return S_OK;
}

IFACEMETHODIMP CJPEGAutoRotatorTestUI::OnCompleted()
{
    m_operationEndTime = GetTickCount64();
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), TRUE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_CLEAR), TRUE);
    _UpdateStatusPostOperation(false);
    return S_OK;
}

INT_PTR CJPEGAutoRotatorTestUI::_DlgProc(UINT uMsg, WPARAM wParam, LPARAM)
{
    INT_PTR ret = FALSE;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        _OnInit();
        ret = TRUE;
        break;

    case WM_COMMAND:
        _OnCommand(GET_WM_COMMAND_ID(wParam, lParam));
        break;

    case WM_DESTROY:
        _OnDestroy();
        ret = FALSE;
        break;
    }
    return ret;
}

void CJPEGAutoRotatorTestUI::_OnInit()
{
    (void)CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&m_spdth));

    // Initialize list view
    m_listviewWindow = GetDlgItem(m_dialogWindow, IDC_ROTATEITEMLIST);

    // Initialize the progress control
    m_progressWindow = GetDlgItem(m_dialogWindow, IDC_PROGRESS);

    // Initialize columns
    wchar_t columnName[100];

    LV_COLUMN lvc = {};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 100;
    lvc.pszText = columnName;

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
        StringCchCopy(columnName, ARRAYSIZE(columnName), columnNames[i]);
        ListView_InsertColumn(m_listviewWindow, i, &lvc);
        ListView_SetColumnWidth(m_listviewWindow, i, LVSCW_AUTOSIZE_USEHEADER);
    }

    RegisterDragDrop(m_dialogWindow, this);

    // Disable until items added
    EnableWindow(m_listviewWindow, FALSE);
}

void CJPEGAutoRotatorTestUI::_OnCommand(_In_ const int commandId)
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
        EndDialog(m_dialogWindow, commandId);
        break;
    }
}

void CJPEGAutoRotatorTestUI::_OnDestroy()
{
    _OnStop();

    RevokeDragDrop(m_dialogWindow);

    SetWindowLongPtr(m_dialogWindow, GWLP_USERDATA, NULL);
    m_dialogWindow = NULL;
}

void CJPEGAutoRotatorTestUI::_OnStart()
{
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_CLEAR), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_STOP), TRUE);
    EnableWindow(m_progressWindow, TRUE);

    UINT itemCount = 0;
    if (m_sprm)
    {
        m_sprm->GetItemCount(&itemCount);
    }

    // Initialize the progress control range
    SendMessage(m_progressWindow, PBM_SETRANGE32, 0, itemCount);

    // Set progress control to marque mode
    SendMessage(m_progressWindow, PBM_SETRANGE32, TRUE, 0);

    // Initialize status text
    SendMessage(GetDlgItem(m_dialogWindow, IDC_STATUS), WM_SETTEXT, 0, (LPARAM)L"Status: Running...");
    SendMessage(GetDlgItem(m_dialogWindow, IDC_TIME), WM_SETTEXT, 0, (LPARAM)L"");

    m_operationStartTime = GetTickCount64();
    HRESULT hr = m_sprm->Start();

    if (FAILED(hr))
    {
        MessageBox(m_dialogWindow, L"Failed rotation operation.", L"Rotation Failed", MB_OK | MB_ICONERROR);
        EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), TRUE);
    }
}

void CJPEGAutoRotatorTestUI::_OnStop()
{
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_STOP), FALSE);

    // Cancel via the rotation manager
    if (m_sprm)
    {
        m_sprm->Cancel();
    }
}

void CJPEGAutoRotatorTestUI::_OnClear()
{
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_CLEAR), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_STOP), FALSE);
    EnableWindow(GetDlgItem(m_dialogWindow, IDC_START), FALSE);

    // Clear the list view
    if (m_listviewWindow)
    {
        ListView_DeleteAllItems(m_listviewWindow);
        EnableWindow(m_listviewWindow, FALSE);
    }

    SendMessage(m_progressWindow, PBM_SETPOS, 0, 0);
    EnableWindow(m_progressWindow, FALSE);

    SendMessage(m_progressWindow, PBM_SETRANGE32, 0, 0);
    SendMessage(GetDlgItem(m_dialogWindow, IDC_PROGRESSTEXT), WM_SETTEXT, 0, (LPARAM)L"");
    SendMessage(GetDlgItem(m_dialogWindow, IDC_STATUS), WM_SETTEXT, 0, (LPARAM)L"");
    SendMessage(GetDlgItem(m_dialogWindow, IDC_TIME), WM_SETTEXT, 0, (LPARAM)L"");

    _CleanupRotationManager();
}

HRESULT CJPEGAutoRotatorTestUI::_InitRotationManager()
{
    _CleanupRotationManager();

    HRESULT hr = CRotationManager::s_CreateInstance(&m_sprm);
    if (SUCCEEDED(hr))
    {
        hr = m_sprm->Advise(this, &m_adviseCookie);
    }

    return hr;
}

void CJPEGAutoRotatorTestUI::_CleanupRotationManager()
{
    if (m_sprm)
    {
        m_sprm->Shutdown();
        m_sprm = nullptr;
    }
}

void CJPEGAutoRotatorTestUI::_AddItemToListView(_In_ IRotationItem* pItem)
{
    LV_ITEM lvi = { 0 };
    lvi.iItem = ListView_GetItemCount(m_listviewWindow);
    lvi.iSubItem = 0;

    PWSTR itemPath = nullptr;
    pItem->get_Path(&itemPath);
    lvi.pszText = itemPath;

    ListView_InsertItem(m_listviewWindow, &lvi);

    // Update the sub items
    _UpdateItemInListView(lvi.iItem, pItem);
}

void CJPEGAutoRotatorTestUI::_UpdateItemInListView(_In_ int index, _In_ IRotationItem* pItem)
{
    PWSTR itemPath = nullptr;
    pItem->get_Path(&itemPath);

    LV_ITEM lvi = { 0 };

    lvi.iItem = index;
    lvi.mask = LVIF_TEXT;
    lvi.pszText = itemPath;
    lvi.iSubItem = 0;
    lvi.pszText = itemPath;
    ListView_SetItem(m_listviewWindow, &lvi);

    CoTaskMemFree(itemPath);

    HRESULT result;
    pItem->get_Result(&result);

    // Check if this item has been run through the manager
    // S_FALSE means it has not
    if (result != S_FALSE)
    {
        wchar_t columnValue[100] = { 0 };

        BOOL fResult;
        pItem->get_IsValidJPEG(&fResult);
        StringCchCopy(columnValue, ARRAYSIZE(columnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 1;
        lvi.pszText = columnValue;
        ListView_SetItem(m_listviewWindow, &lvi);

        columnValue[0] = L'\0';
        pItem->get_IsRotationLossless(&fResult);
        StringCchCopy(columnValue, ARRAYSIZE(columnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 2;
        lvi.pszText = columnValue;
        ListView_SetItem(m_listviewWindow, &lvi);

        columnValue[0] = L'\0';
        UINT originalOrientation = 1;
        pItem->get_OriginalOrientation(&originalOrientation);
        lvi.iSubItem = 3;
        PCWSTR label = (originalOrientation < ARRAYSIZE(g_rotationLabel)) ? g_rotationLabel[originalOrientation] : L"Invalid!";
        StringCchCopy(columnValue, ARRAYSIZE(columnValue), label);
        lvi.pszText = columnValue;
        ListView_SetItem(m_listviewWindow, &lvi);

        columnValue[0] = L'\0';
        pItem->get_WasRotated(&fResult);
        StringCchCopy(columnValue, ARRAYSIZE(columnValue), fResult ? L"True" : L"False");
        lvi.iSubItem = 4;
        lvi.pszText = columnValue;
        ListView_SetItem(m_listviewWindow, &lvi);

        columnValue[0] = L'\0';
        StringCchCopy(columnValue, ARRAYSIZE(columnValue), SUCCEEDED(result) ? L"SUCCEEDED" : L"FAILED");
        lvi.iSubItem = 5;
        lvi.pszText = columnValue;
        ListView_SetItem(m_listviewWindow, &lvi);
    }

    ListView_SetColumnWidth(m_listviewWindow, 0, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_listviewWindow, 1, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_listviewWindow, 2, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_listviewWindow, 3, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_listviewWindow, 4, LVSCW_AUTOSIZE);
    ListView_SetColumnWidth(m_listviewWindow, 6, LVSCW_AUTOSIZE);
}

void CJPEGAutoRotatorTestUI::_UpdateStatusPostOperation(_In_ bool wasCanceled)
{
    SendMessage(GetDlgItem(m_dialogWindow, IDC_STATUS), WM_SETTEXT, 0, wasCanceled ? (LPARAM)L"Status: Canceled" : (LPARAM)L"Status: Completed");
    ULONGLONG duration = m_operationEndTime - m_operationStartTime;
    wchar_t message[100] = { 0 };
    StringCchPrintf(message, ARRAYSIZE(message), L"Duration: %u ms", duration);
    SendMessage(GetDlgItem(m_dialogWindow, IDC_TIME), WM_SETTEXT, 0, (LPARAM)message);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE, _In_ PWSTR, _In_ int)
{
    g_hInst = hInstance;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        CJPEGAutoRotatorTestUI *pdlg = new CJPEGAutoRotatorTestUI();
        if (pdlg)
        {
            pdlg->Start();
            pdlg->Release();
        }
        CoUninitialize();
    }
    return 0;
}