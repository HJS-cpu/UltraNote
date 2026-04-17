#include <windows.h>
#include <ole2.h>
#include <commctrl.h>
#include "Application.h"

int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPWSTR    /*lpCmdLine*/,
    _In_     int       /*nCmdShow*/)
{
    // Initialize COM/OLE (needed for SHGetFileInfoW on .lnk files, drag & drop)
    OleInitialize(nullptr);

    // Enable visual styles (ComCtl v6)
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icc);

    Application& app = Application::Get();
    if (!app.Initialize(hInstance))
        return 1;

    int result = app.Run();
    OleUninitialize();
    return result;
}
