// dshow-basic-2.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "framework.h"
#include "dshow-basic-2.h"

#include "util.h"
#include "Allocator.h"


#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, HWND&);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

BOOL                            VerifyVMR9(void);
HRESULT                         StartGraph(HWND);
HRESULT                         CloseGraph(HWND);
HRESULT                         SetAllocatorPresenter(IBaseFilter*, HWND);
BSTR                            GetMoviePath();

DWORD_PTR                       g_userId = 0xACDCACDC;
HWND                            g_hWnd;
SmartPtr<IGraphBuilder>         g_graph;
SmartPtr<IBaseFilter>           g_filter;
SmartPtr<IVMRSurfaceAllocator9> g_allocator;
SmartPtr<IMediaControl>         g_mediaControl;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    int ret = -1;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Verify that the VMR9 is present on this system
    if (!VerifyVMR9())
    {
        CoUninitialize();
        return FALSE;
    }


    __try
    {
        UNREFERENCED_PARAMETER(hPrevInstance);
        UNREFERENCED_PARAMETER(lpCmdLine);

        // TODO: 여기에 코드를 입력합니다.

        // 전역 문자열을 초기화합니다.
        LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
        LoadStringW(hInstance, IDC_DSHOWBASIC2, szWindowClass, MAX_LOADSTRING);
        MyRegisterClass(hInstance);

        // 애플리케이션 초기화를 수행합니다:
        if (!InitInstance(hInstance, nCmdShow, g_hWnd))
        {
            return FALSE;
        }

        HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DSHOWBASIC2));

        MSG msg;

        // 기본 메시지 루프입니다:
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        ret = (int)msg.wParam;
    }
    __finally
    {
        // make sure to release everything at the end
        // regardless of what's happening
        // CloseGraph(g_hWnd);
        CoUninitialize();
    }

    return ret;
}



//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DSHOWBASIC2));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DSHOWBASIC2);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, HWND& hWnd)
{
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HRESULT hr;

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 메뉴 선택을 구문 분석합니다:
            switch (wmId)
            {
            case IDM_PLAY_FILE:
                hr = StartGraph(g_hWnd);
                if (FAILED(hr))
                {
                    return 0;
                }
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL VerifyVMR9(void)
{
    HRESULT hr;

    // Verify that the VMR exists on this system
    IBaseFilter* pBF = NULL;
    hr = CoCreateInstance(CLSID_VideoMixingRenderer9, NULL,
        CLSCTX_INPROC,
        IID_IBaseFilter,
        (LPVOID*)&pBF);
    if (SUCCEEDED(hr))
    {
        pBF->Release();
        return TRUE;
    }
    else
    {
        MessageBox(NULL,
            TEXT("This application requires the VMR-9.\r\n\r\n")

            TEXT("The VMR-9 is not enabled when viewing through a Remote\r\n")
            TEXT(" Desktop session. You can run VMR-enabled applications only\r\n")
            TEXT("on your local computer.\r\n\r\n")

            TEXT("\r\nThis sample will now exit."),

            TEXT("Video Mixing Renderer (VMR9) capabilities are required"), MB_OK);

        return FALSE;
    }
}

HRESULT StartGraph(HWND window)
{
    // Clear DirectShow interfaces (COM smart pointers)
    CloseGraph(window);

    SmartPtr<IVMRFilterConfig9> filterConfig;


    BSTR path = GetMoviePath();
    if (!path)
    {
        return E_FAIL;
    }

    HRESULT hr;

    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&g_graph);

    if (SUCCEEDED(hr))
    {
        hr = CoCreateInstance(CLSID_VideoMixingRenderer9, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&g_filter);
    }

    if (SUCCEEDED(hr))
    {
        hr = g_filter->QueryInterface(IID_IVMRFilterConfig9, reinterpret_cast<void**>(&filterConfig));
    }

    if (SUCCEEDED(hr))
    {
        hr = filterConfig->SetRenderingMode(VMR9Mode_Renderless);

    }

    if (SUCCEEDED(hr))
    {
        hr = filterConfig->SetNumberOfStreams(2);

    }

    if (SUCCEEDED(hr))
    {
        hr = SetAllocatorPresenter(g_filter, window);
    }

    if (SUCCEEDED(hr))
    {
        hr = g_graph->AddFilter(g_filter, L"Video Mixing Renderer 9");
    }

    if (SUCCEEDED(hr))
    {
        hr = g_graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&g_mediaControl));
    }

    if (SUCCEEDED(hr))
    {
        hr = g_graph->RenderFile(path, NULL);
    }

    if (SUCCEEDED(hr))
    {
        hr = g_mediaControl->Run();
    }

    SysFreeString(path);

    return hr;
}

HRESULT CloseGraph(HWND window)
{
    if (g_mediaControl != NULL)
    {
        OAFilterState state;
        do {
            g_mediaControl->Stop();
            g_mediaControl->GetState(0, &state);
        } while (state != State_Stopped);
    }

    g_allocator = NULL;
    g_mediaControl = NULL;
    g_filter = NULL;
    g_graph = NULL;
    ::InvalidateRect(window, NULL, true);
    return S_OK;
}

BSTR GetMoviePath()
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    TCHAR  szBuffer[MAX_PATH];
    szBuffer[0] = NULL;

    static const TCHAR szFilter[]
        = TEXT("Video Files (.ASF, .AVI, .MPG, .MPEG, .VOB, .QT, .WMV)\0*.ASF;*.AVI;*.MPG;*.MPEG;*.VOB;*.QT;*.WMV\0") \
        TEXT("All Files (*.*)\0*.*\0\0");
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = g_hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.lpstrFile = szBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = TEXT("Select a video file to play...");
    ofn.Flags = OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = TEXT("AVI");
    ofn.lCustData = 0L;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (GetOpenFileName(&ofn))  // user specified a file
    {
        return SysAllocString(szBuffer);
    }

    return NULL;
}

HRESULT SetAllocatorPresenter(IBaseFilter* filter, HWND window)
{
    if (filter == NULL)
    {
        return E_FAIL;
    }

    HRESULT hr;

    SmartPtr<IVMRSurfaceAllocatorNotify9> lpIVMRSurfAllocNotify;
    FAIL_RET(filter->QueryInterface(IID_IVMRSurfaceAllocatorNotify9, reinterpret_cast<void**>(&lpIVMRSurfAllocNotify)));

    // create our surface allocator
    g_allocator.Attach(new CAllocator(hr, window));
    if (FAILED(hr))
    {
        g_allocator = NULL;
        return hr;
    }

    // let the allocator and the notify know about each other
    FAIL_RET(lpIVMRSurfAllocNotify->AdviseSurfaceAllocator(g_userId, g_allocator));
    FAIL_RET(g_allocator->AdviseNotify(lpIVMRSurfAllocNotify));

    return hr;
}