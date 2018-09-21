#pragma once
#include <ShlObj.h>
#include "RotationInterfaces.h"
#include <vector>
#include "srwlock.h"

class CRotationItem : public IRotationItem
{
public:
    CRotationItem();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CRotationItem, IRotationItem),
            { 0, 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    IFACEMETHODIMP get_Path(__deref_out PWSTR* ppszPath);
    IFACEMETHODIMP put_Path(__in PCWSTR pszPath);
    IFACEMETHODIMP get_WasRotated(__out BOOL* pfWasRotated);
    IFACEMETHODIMP get_IsValidJPEG(__out BOOL* pfIsValidJPEG);
    IFACEMETHODIMP get_IsRotationLossless(__out BOOL* pfIsRotationLossless);
    IFACEMETHODIMP get_OriginalOrientation(__out UINT* puOriginalOrientation);
    IFACEMETHODIMP get_Result(__out HRESULT* phrResult);
    IFACEMETHODIMP put_Result(__in HRESULT hrResult);
    IFACEMETHODIMP Rotate();

    static HRESULT s_CreateInstance(__in PCWSTR pszPath, __deref_out IRotationItem** ppri);

private:
    ~CRotationItem();

private:
    long m_cRef = 1;
    PWSTR m_pszPath = nullptr;
    bool m_fWasRotated = false;
    bool m_fIsValidJPEG = false;
    bool m_fIsRotationLossless = true;
    UINT m_uOriginalOrientation = 1;
    HRESULT m_hrResult = S_FALSE;  // We init to S_FALSE which means Not Run Yet.  S_OK on success.  Otherwise an error code.
    CSRWLock m_lock;
    static UINT s_uTagOrientationPropSize;
};

// TODO: Consider modifying the below or making them customizable via the interface.  We will likely want to control
// TODO: these from unit tests.

// Maximum number of running worker threads
// We should never exceed the number of logical processors
// TODO: setting this to 1 for testing for now
#define MAX_ROTATION_WORKER_THREADS 64

// Minimum amount of work to schedule to a worker thread
#define MIN_ROTATION_WORK_SIZE 5

class CRotationManager : 
    public IRotationManager,
    public IRotationManagerEvents,
    public IRotationManagerDiagnostics
{
public:
    CRotationManager();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv)
    {
        static const QITAB qit[] =
        {
            QITABENT(CRotationManager, IRotationManager),
            QITABENT(CRotationManager, IRotationManagerEvents),
            QITABENT(CRotationManager, IRotationManagerDiagnostics),
            { 0, 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }

    IFACEMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release()
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
        {
            delete this;
        }
        return cRef;
    }

    // IRotationManager
    IFACEMETHODIMP Advise(__in IRotationManagerEvents* prme, __out DWORD* pdwCookie);
    IFACEMETHODIMP UnAdvise(__in DWORD dwCookie);
    IFACEMETHODIMP Start();
    IFACEMETHODIMP Cancel();
    IFACEMETHODIMP Shutdown();
    IFACEMETHODIMP AddItem(__in IRotationItem* pri);
    IFACEMETHODIMP GetItem(__in UINT uIndex, __deref_out IRotationItem** ppri);
    IFACEMETHODIMP GetItemCount(__out UINT* puCount);

    // IRotationManagerDiagnostics
    IFACEMETHODIMP get_MaxWorkerThreadCount(__out UINT* puMaxThreadCount);
    IFACEMETHODIMP put_MaxWorkerThreadCount(__in UINT uMaxThreadCount);
    IFACEMETHODIMP get_WorkerThreadCount(__out UINT* puThreadCount);
    IFACEMETHODIMP put_WorkerThreadCount(__in UINT uThreadCount);
    IFACEMETHODIMP get_MinItemsPerWorkerThread(__out UINT* puMinItemsPerThread);
    IFACEMETHODIMP put_MinItemsPerWorkerThread(__in UINT uMinItemsPerThread);
    IFACEMETHODIMP get_ItemsPerWorkerThread(__out UINT* puNumItemsPerThread);
    IFACEMETHODIMP put_ItemsPerWorkerThread(__in UINT uNumItemsPerThread);

    // IRotationManagerEvents
    IFACEMETHODIMP OnItemAdded(__in UINT uIndex);
    IFACEMETHODIMP OnItemProcessed(__in UINT uIndex);
    IFACEMETHODIMP OnProgress(__in UINT uCompleted, __in UINT uTotal);
    IFACEMETHODIMP OnCanceled();
    IFACEMETHODIMP OnCompleted();

    static HRESULT s_CreateInstance(__deref_out IRotationManager** pprm);

private:
    ~CRotationManager();

    HRESULT _Init();
    void _Cleanup();
    HRESULT _PerformRotation();
    HRESULT _CreateWorkerThreads();
    HRESULT _GetWorkerThreadDimensions();

    void _ClearRotationItems();
    void _ClearEventHandlers();

    static DWORD WINAPI s_rotationWorkerThread(__in void* pv);

private:
    struct ROTATION_WORKER_THREAD_INFO
    {
        HANDLE hWorker;
        DWORD dwThreadId;
        UINT uCompletedItems;
        UINT uTotalItems;
    };

    struct ROTATION_MANAGER_EVENT
    {
        IRotationManagerEvents* prme;
        DWORD dwCookie;
    };

    long  m_cRef = 1;
    ROTATION_WORKER_THREAD_INFO m_workerThreadInfo[MAX_ROTATION_WORKER_THREADS];
    bool m_diagnosticsMode = false;
    HANDLE m_workerThreadHandles[MAX_ROTATION_WORKER_THREADS];
    UINT m_uWorkerThreadCount = 0;
    UINT m_uMaxWorkerThreadCount = MAX_ROTATION_WORKER_THREADS;
    UINT m_uMinItemsPerWorkerThread = 0;
    UINT m_uMaxItemsPerWorkerThread = 0;
    UINT m_uItemsPerWorkerThread = 0;
    ULONG_PTR m_gdiplusToken;
    HANDLE m_hCancelEvent = nullptr;
    HANDLE m_hStartEvent = nullptr;
    DWORD m_dwCookie = 0;
    CSRWLock m_lockEvents;
    CSRWLock m_lockItems;
    _Guarded_by_(m_lockEvents) std::vector<ROTATION_MANAGER_EVENT> m_rotationManagerEvents;
    _Guarded_by_(m_lockItems) std::vector<IRotationItem*> m_rotationItems;
};
