// JPEGAutoRotator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <RotationInterfaces.h>
#include "RotationUI.h"
#include "RotationManager.h"
#include "pathcch.h"
#include <atlbase.h>

bool GetCurrentFolderPath(_In_ UINT count, _Out_ PWSTR path)
{
    bool ret = false;
    if (GetModuleFileName(nullptr, path, count))
    {
        PathRemoveFileSpec(path);
        ret = true;
    }
    return ret;
}

bool GetTestFolderPath(_In_ PCWSTR testFolderPathAppend, _In_ UINT testFolderPathLen, _Out_ PWSTR testFolderPath)
{
    bool ret = false;
    if (GetCurrentFolderPath(testFolderPathLen, testFolderPath))
    {
        if (SUCCEEDED(PathCchAppend(testFolderPath, testFolderPathLen, testFolderPathAppend)))
        {
            ret = true;
        }
    }

    return ret;
}

bool CopyHelper(_In_ PCWSTR src, _In_ PCWSTR dest)
{
    SHFILEOPSTRUCT fileStruct = { 0 };
    fileStruct.pFrom = src;
    fileStruct.pTo = dest;
    fileStruct.wFunc = FO_COPY;
    fileStruct.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NO_UI | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
    return (SHFileOperation(&fileStruct) == 0);
}

bool DeleteHelper(_In_ PCWSTR path)
{
    SHFILEOPSTRUCT fileStruct = { 0 };
    fileStruct.pFrom = path;
    fileStruct.wFunc = FO_DELETE;
    fileStruct.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NO_UI;
    return (SHFileOperation(&fileStruct) == 0);
}

HRESULT CreateDataObjectFromPath(_In_ PCWSTR pathToFile, _COM_Outptr_ IDataObject** dataObject)
{
    *dataObject = nullptr;

    CComPtr<IShellItem> spsi;
    SHCreateItemFromParsingName(pathToFile, nullptr /* IBindCtx */, IID_PPV_ARGS(&spsi));
    spsi->BindToHandler(nullptr /* IBindCtx */, BHID_DataObject, IID_PPV_ARGS(dataObject));

    return S_OK;
}

constexpr auto JPEGWITHEXIFROTATION_TESTFOLDER = L"TestFiles\\JPEGWithExifRotation\\";

PCWSTR g_rgTestJPEGFiles[] =
{
    L"Landscape_1.jpg",
    L"Landscape_2.jpg",
    L"Landscape_3.jpg",
    L"Landscape_4.jpg",
    L"Landscape_5.jpg",
    L"Landscape_6.jpg",
    L"Landscape_7.jpg",
    L"Landscape_8.jpg",
    L"Portrait_1.jpg",
    L"Portrait_2.jpg",
    L"Portrait_3.jpg",
    L"Portrait_4.jpg",
    L"Portrait_5.jpg",
    L"Portrait_6.jpg",
    L"Portrait_7.jpg",
    L"Portrait_8.jpg"
};

struct WorkerThreadInfo
{
    UINT uNumItems;
    UINT uNumItemsPerThread;
    UINT uNumWorkerThreads;
    ULONGLONG uEnumBegin;
    ULONGLONG uEnumEnd;
    ULONGLONG uEnumDuration;
    ULONGLONG uOperationBegin;
    ULONGLONG uOperationEnd;
    ULONGLONG uOperationDuration;
    ULONGLONG uTotalDuration;
};


WorkerThreadInfo g_workerThreadTestData[]
{
    // uNumItems      uNumItemsPerThread        uNumWorkerThreads
    {     1,                  1,                        1,      0,0,0,0,0,0,0},
    {     10,                 2,                        5,      0,0,0,0,0,0,0},
    {     10,                 5,                        2,      0,0,0,0,0,0,0},
    {     10,                 10,                       1,      0,0,0,0,0,0,0},
    {     50,                 5,                        10,     0,0,0,0,0,0,0},
    {     50,                 10,                       5,      0,0,0,0,0,0,0},
    {     50,                 50,                       1,      0,0,0,0,0,0,0},
    {     100,                10,                       10,     0,0,0,0,0,0,0},
    {     100,                20,                       5,      0,0,0,0,0,0,0},
    {     100,                30,                       4,      0,0,0,0,0,0,0},
    {     100,                50,                       2,      0,0,0,0,0,0,0},
    {     100,                100,                      1,      0,0,0,0,0,0,0},
    {     1000,               50,                       20,     0,0,0,0,0,0,0},
    {     1000,               100,                      10,     0,0,0,0,0,0,0},
    {     1000,               250,                      4,      0,0,0,0,0,0,0},
    {     1000,               300,                      4,      0,0,0,0,0,0,0},
    {     1000,               500,                      2,      0,0,0,0,0,0,0},
    {     1000,               1000,                     1,      0,0,0,0,0,0,0},
    {     10000,              1000,                     10,     0,0,0,0,0,0,0},
    {     10000,              2000,                     5,      0,0,0,0,0,0,0},
    {     10000,              5000,                     2,      0,0,0,0,0,0,0},
    {     10000,              10000,                    1,      0,0,0,0,0,0,0},
};

#define NUM_TEST_RUNS 10

int main()
{
    UINT numTestResultEntries = ARRAYSIZE(g_workerThreadTestData) * NUM_TEST_RUNS;
    WorkerThreadInfo* workerThreadResults = new WorkerThreadInfo[numTestResultEntries];
    for (UINT x = 0; x < numTestResultEntries; x++)
    {
        for (UINT u = 0; u < ARRAYSIZE(g_workerThreadTestData); u++)
        {
            // Get test file source folder
            wchar_t testFolderSource[MAX_PATH];
            GetTestFolderPath(JPEGWITHEXIFROTATION_TESTFOLDER, ARRAYSIZE(testFolderSource), testFolderSource);

            // Get test file working folder
            wchar_t testFolderWorking[MAX_PATH];
            GetTestFolderPath(L"TestWorkThreadAllocations", ARRAYSIZE(testFolderWorking), testFolderWorking);

            // Ensure test file working folder doesn't exist
            DeleteHelper(testFolderWorking);

            CreateDirectory(testFolderWorking, nullptr);

            // Copy the test files individually the specified number of times
            for (UINT index = 0; index < ARRAYSIZE(g_rgTestJPEGFiles); index++)
            {
                CopyHelper(testFolderSource, testFolderWorking);
            }

            CComPtr<IRotationManager> spRotationManager;
            CRotationManager::s_CreateInstance(&spRotationManager);

            for (int i = 0; i < ARRAYSIZE(g_rgTestJPEGFiles); i++)
            {
                wchar_t testFilePath[MAX_PATH];
                PathCchCombine(testFilePath, ARRAYSIZE(testFilePath), testFolderWorking, g_rgTestJPEGFiles[i]);
                CComPtr<IRotationItem> spRotationItem;
                CRotationItem::s_CreateInstance(testFilePath, &spRotationItem);
                spRotationManager->AddItem(spRotationItem);
            }

            spRotationManager->Start();

            // Cleanup working folder
            DeleteHelper(testFolderWorking);
        }
    }

    std::cout << "Hello World!\n"; 
}

