// dshow-basic-2.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#ifdef _DEBUG
    #ifdef UNICODE
        #pragma comment(linker, "/entry:wWinMainCRTStartup /subsystem:console")
    #else
        #pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
    #endif
#endif

#pragma warning(disable: 4996)  // use strcpy
#include "framework.h"
#include "dshow-basic-2.h"

/*
* https://github.com/chuckfairy/node-webcam/blob/master/src/bindings/CommandCam/CommandCam.cpp
*/

#include <dshow.h>
#import "qedit.dll" raw_interfaces_only named_guids

EXTERN_C const CLSID CLSID_NullRenderer;
EXTERN_C const CLSID CLSID_SampleGrabber;

#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// DirectShow objects
HRESULT hr;
ICreateDevEnum* pDevEnum = NULL;
IEnumMoniker* pEnum = NULL;
IMoniker* pMoniker = NULL;
IPropertyBag* pPropBag = NULL;
IGraphBuilder* pGraph = NULL;
ICaptureGraphBuilder2* pBuilder = NULL;
IBaseFilter* pCap = NULL;
IBaseFilter* pSampleGrabberFilter = NULL;
DexterLib::ISampleGrabber* pSampleGrabber = NULL;
IBaseFilter* pNullRenderer = NULL;
IMediaControl* pMediaControl = NULL;
char* pBuffer = NULL;

void CaptureStart(void);

void exit_message(const char* error_message, int error)
{
    // Print an error message
    fprintf(stderr, error_message);
    fprintf(stderr, "\n");

    // Clean up DirectShow / COM stuff
    if (pBuffer != NULL) delete[] pBuffer;
    if (pMediaControl != NULL) pMediaControl->Release();
    if (pNullRenderer != NULL) pNullRenderer->Release();
    if (pSampleGrabber != NULL) pSampleGrabber->Release();
    if (pSampleGrabberFilter != NULL)
        pSampleGrabberFilter->Release();
    if (pCap != NULL) pCap->Release();
    if (pBuilder != NULL) pBuilder->Release();
    if (pGraph != NULL) pGraph->Release();
    if (pPropBag != NULL) pPropBag->Release();
    if (pMoniker != NULL) pMoniker->Release();
    if (pEnum != NULL) pEnum->Release();
    if (pDevEnum != NULL) pDevEnum->Release();
    CoUninitialize();

    // Exit the program
    exit(error);
}

void CaptureStart(void)
{
    // Capture settings
    int snapshot_delay = 2000;
    int show_preview_window = 1;
    int list_devices = 0;           // print device list
    int device_number = 1;
    char device_name[100];
    //char filename[100];

    // Other variables
    char char_buffer[100];

    // Default device name and output filename
    strcpy(device_name, "");
    //strcpy(filename, "image.bmp");

    // Intialise COM
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr != S_OK)
        exit_message("Could not initialise COM", 1);

    // Create filter graph
    hr = CoCreateInstance(CLSID_FilterGraph, NULL,
        CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
        (void**)&pGraph);
    if (hr != S_OK)
        exit_message("Could not create filter graph", 1);

    // Create capture graph builder.
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
        CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
        (void**)&pBuilder);
    if (hr != S_OK)
        exit_message("Could not create capture graph builder", 1);

    // Attach capture graph builder to graph
    hr = pBuilder->SetFiltergraph(pGraph);
    if (hr != S_OK)
        exit_message("Could not attach capture graph builder to graph", 1);

    // Create system device enumerator
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if (hr != S_OK)
        exit_message("Could not crerate system device enumerator", 1);

    // Video input device enumerator
    hr = pDevEnum->CreateClassEnumerator(
        CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr != S_OK)
        exit_message("No video devices found", 1);
    
    int n = 0;
    // If the user has included the "/list" command line
    // argument, just list available devices, then exit.
    if (list_devices != 0)
    {
        fprintf(stderr, "Available capture devices:\n");
        n = 0;
        while (1)
        {
            // Find next device
            hr = pEnum->Next(1, &pMoniker, NULL);
            if (hr == S_OK)
            {
                // Increment device counter
                n++;

                // Get device name
                hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, 0);
                fprintf(stderr, "%ls\n", var.bstrVal);
                VariantClear(&var);
            }
            else
            {
                // Finished listing device, so exit program
                if (n == 0) exit_message("No devices found", 0);
                else break;//exit_message("", 0);
            }
        }
    }
   

    // Get moniker for specified video input device,
    // or for the first device if no device number
    // was specified.
    VARIANT var;
    n = 0;
    while (1)
    {
        // Access next device
        hr = pEnum->Next(1, &pMoniker, NULL);
        if (hr == S_OK)
        {
            n++; // increment device count
        }
        else
        {
            if (device_number == 0)
            {
                fprintf(stderr,
                    "Video capture device %s not found\n",
                    device_name);
            }
            else
            {
                fprintf(stderr,
                    "Video capture device %d not found\n",
                    device_number);
            }
            exit_message("", 1);
        }

        // If device was specified by name rather than number...
        if (device_number == 0)
        {
            // Get video input device name
            hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
            if (hr == S_OK)
            {
                // Get current device name
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, 0);

                // Convert to a normal C string, i.e. char*
                sprintf(char_buffer, "%ls", var.bstrVal);
                VariantClear(&var);
                pPropBag->Release();
                pPropBag = NULL;

                // Exit loop if current device name matched devname
                if (strcmp(device_name, char_buffer) == 0) break;
            }
            else
            {
                exit_message("Error getting device names", 1);
            }
        }
        else if (n >= device_number) break;
    }

    // Get video input device name
    hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
    VariantInit(&var);
    hr = pPropBag->Read(L"FriendlyName", &var, 0);
    fprintf(stderr, "Capture device: %ls\n", var.bstrVal);
    VariantClear(&var);

    // Create capture filter and add to graph
    hr = pMoniker->BindToObject(0, 0,
        IID_IBaseFilter, (void**)&pCap);
    if (hr != S_OK) exit_message("Could not create capture filter", 1);

    // Add capture filter to graph
    hr = pGraph->AddFilter(pCap, L"Capture Filter");
    if (hr != S_OK) exit_message("Could not add capture filter to graph", 1);

    // Create sample grabber filter
    hr = CoCreateInstance(CLSID_SampleGrabber, NULL,
        CLSCTX_INPROC_SERVER, IID_IBaseFilter,
        (void**)&pSampleGrabberFilter);
    if (hr != S_OK)
        exit_message("Could not create Sample Grabber filter", 1);

    // Query the ISampleGrabber interface of the sample grabber filter
    hr = pSampleGrabberFilter->QueryInterface(
        DexterLib::IID_ISampleGrabber, (void**)&pSampleGrabber);
    if (hr != S_OK)
        exit_message("Could not get ISampleGrabber interface to sample grabber filter", 1);

    // Enable sample buffering in the sample grabber filter
    hr = pSampleGrabber->SetBufferSamples(TRUE);
    if (hr != S_OK)
        exit_message("Could not enable sample buffering in the sample grabber", 1);

    // Set media type in sample grabber filter
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    hr = pSampleGrabber->SetMediaType((DexterLib::_AMMediaType*)&mt);
    if (hr != S_OK)
        exit_message("Could not set media type in sample grabber", 1);

    // Add sample grabber filter to filter graph
    hr = pGraph->AddFilter(pSampleGrabberFilter, L"SampleGrab");
    if (hr != S_OK)
        exit_message("Could not add Sample Grabber to filter graph", 1);

    // Create Null Renderer filter
    hr = CoCreateInstance(CLSID_NullRenderer, NULL,
        CLSCTX_INPROC_SERVER, IID_IBaseFilter,
        (void**)&pNullRenderer);
    if (hr != S_OK)
        exit_message("Could not create Null Renderer filter", 1);

    // Add Null Renderer filter to filter graph
    hr = pGraph->AddFilter(pNullRenderer, L"NullRender");
    if (hr != S_OK)
        exit_message("Could not add Null Renderer to filter graph", 1);

    // Connect up the filter graph's capture stream
    hr = pBuilder->RenderStream(
        &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
        pCap, pSampleGrabberFilter, pNullRenderer);
    if (hr != S_OK)
        exit_message("Could not render capture video stream", 1);

    // Connect up the filter graph's preview stream
    if (show_preview_window > 0)
    {
        hr = pBuilder->RenderStream(
            &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video,
            pCap, NULL, NULL);
        if (hr != S_OK && hr != VFW_S_NOPREVIEWPIN)
            exit_message("Could not render preview video stream", 1);
    }

    // Get media control interfaces to graph builder object
    hr = pGraph->QueryInterface(IID_IMediaControl,
        (void**)&pMediaControl);
    if (hr != S_OK) exit_message("Could not get media control interface", 1);

    // Run graph
    while (1)
    {
        hr = pMediaControl->Run();

        // Hopefully, the return value was S_OK or S_FALSE
        if (hr == S_OK) break; // graph is now running
        if (hr == S_FALSE) continue; // graph still preparing to run

        // If the Run function returned something else,
        // there must be a problem
        fprintf(stderr, "Error: %u\n", hr);
        exit_message("Could not run filter graph", 1);
    }

    // Wait for specified time delay (if any)
    Sleep(snapshot_delay);

    // Grab a sample
    // First, find the required buffer size
    long buffer_size = 0;
    while (1)
    {
        // Passing in a NULL pointer signals that we're just checking
        // the required buffer size; not looking for actual data yet.
        hr = pSampleGrabber->GetCurrentBuffer(&buffer_size, NULL);

        // Keep trying until buffer_size is set to non-zero value.
        if (hr == S_OK && buffer_size != 0) break;

        // If the return value isn't S_OK or VFW_E_WRONG_STATE
        // then something has gone wrong. VFW_E_WRONG_STATE just
        // means that the filter graph is still starting up and
        // no data has arrived yet in the sample grabber filter.
        if (hr != S_OK && hr != VFW_E_WRONG_STATE)
            exit_message("Could not get buffer size", 1);
    }

    // Stop the graph
    pMediaControl->Stop();

    // Allocate buffer for image
    pBuffer = new char[buffer_size];
    if (!pBuffer)
        exit_message("Could not allocate data buffer for image", 1);

    // Retrieve image data from sample grabber buffer
    hr = pSampleGrabber->GetCurrentBuffer(
        &buffer_size, (long*)pBuffer);
    if (hr != S_OK)
        exit_message("Could not get buffer data from sample grabber", 1);

    // Get the media type from the sample grabber filter
    hr = pSampleGrabber->GetConnectedMediaType(
        (DexterLib::_AMMediaType*)&mt);
    if (hr != S_OK) exit_message("Could not get media type", 1);

    // Retrieve format information
    VIDEOINFOHEADER* pVih = NULL;
    if ((mt.formattype == FORMAT_VideoInfo) &&
        (mt.cbFormat >= sizeof(VIDEOINFOHEADER)) &&
        (mt.pbFormat != NULL))
    {
        // Get video info header structure from media type
        pVih = (VIDEOINFOHEADER*)mt.pbFormat;

        //pVih->bmiHeader.biWidth = 4000;

        // Print the resolution of the captured image
        fprintf(stderr, "Capture resolution: %dx%d\n",
            pVih->bmiHeader.biWidth,
            pVih->bmiHeader.biHeight);

        // Create bitmap structure
        long cbBitmapInfoSize = mt.cbFormat - SIZE_PREHEADER;
        BITMAPFILEHEADER bfh;
        ZeroMemory(&bfh, sizeof(bfh));
        bfh.bfType = 'MB'; // Little-endian for "BM".
        bfh.bfSize = sizeof(bfh) + buffer_size + cbBitmapInfoSize;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + cbBitmapInfoSize;

        // Open output file
        HANDLE hf = CreateFile(L"image.bmp"/*filename*/, GENERIC_WRITE,
            FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
        if (hf == INVALID_HANDLE_VALUE)
            exit_message("Error opening output file", 1);

        // Write the file header.
        DWORD dwWritten = 0;
        WriteFile(hf, &bfh, sizeof(bfh), &dwWritten, NULL);
        WriteFile(hf, HEADER(pVih),
            cbBitmapInfoSize, &dwWritten, NULL);

        // Write pixel data to file
        WriteFile(hf, pBuffer, buffer_size, &dwWritten, NULL);
        CloseHandle(hf);
    }
    else
    {
        exit_message("Wrong media type", 1);
    }

    // Free the format block
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }

    // Clean up and exit
    fprintf(stderr, "Captured image to %s", L"image.bmp");
    exit_message("", 0);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DSHOWBASIC2, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance (hInstance, nCmdShow))
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

    return (int) msg.wParam;
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
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
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
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 메뉴 선택을 구문 분석합니다:
            switch (wmId)
            {
            case IDM_STARTCAPTURE:
                CaptureStart();
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
