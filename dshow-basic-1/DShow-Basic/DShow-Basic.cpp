

#include <dshow.h>
#include <Windows.h>

#pragma comment(lib, "strmiids")

IBaseFilter* CreateCLSID(CLSID id, HRESULT* resultCallback) {
    IBaseFilter* filter = NULL;

    HRESULT r = (CoCreateInstance(id, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&filter)));

    if (resultCallback != NULL)
        *resultCallback = r;

    return filter;
}

void CheckError(HRESULT r) {
    if (FAILED(r)) {
        printf("FAILED : %x\n", r);
    }
    else if (SUCCEEDED(r)) {
        printf("SUCCEEDED\n");
    }
}

int main()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (SUCCEEDED(hr))
    {
        IGraphBuilder* graph;
        IMediaControl* graphControl;
        IMediaEvent* graphEvent;

        HRESULT r = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&graph);

        if (FAILED(r)) throw;
        else {
            graph->QueryInterface(IID_IMediaControl, (void**)&graphControl);
            graph->QueryInterface(IID_IMediaEvent, (void**)&graphEvent);
        }

        IBaseFilter* aviSource, * aviSpliter, * videoDecorder, * vmr9Renderer;
        IPin* aviOutput, * aviSpliterInput, * aviSpliterOutput, * videoDecorderInput, * videoDecorderOutput, * vmr9RendererInput;

        long graphEvCode;

        aviSource = CreateCLSID(CLSID_AsyncReader, NULL);
        aviSpliter = CreateCLSID(CLSID_AviSplitter, NULL);

        CLSID decoderCLSID = GUID_NULL;
        hr = CLSIDFromString(L"{212690FB-83E5-4526-8FD7-74478B7939CD}", &decoderCLSID);
        //if (SUCCEEDED(hr))
        {
            videoDecorder = CreateCLSID(decoderCLSID, NULL);
        }
        
        vmr9Renderer = CreateCLSID(CLSID_VideoMixingRenderer9, NULL);

        printf("Add Filter\n");

        CheckError(graph->AddSourceFilter(L"C:\\avi_example.avi", NULL, &aviSource));
        CheckError(graph->AddFilter(aviSpliter, NULL));
        CheckError(graph->AddFilter(videoDecorder, NULL));
        CheckError(graph->AddFilter(vmr9Renderer, NULL));

        // 참고로 Input을 먼저 정의하는게 Input이 정의되고 Connect하여야 Output Pin이 생성되기 때문이다.
        CheckError(aviSource->FindPin(L"Output", &aviOutput));
        CheckError(aviSpliter->FindPin(L"input pin", &aviSpliterInput));
        CheckError(videoDecorder->FindPin(L"Video Input", &videoDecorderInput));
        CheckError(vmr9Renderer->FindPin(L"VMR Input0", &vmr9RendererInput));

        printf("Connect Source -> Spliter\n");
        CheckError(graph->Connect(aviOutput, aviSpliterInput));

        printf("Get Spliter Video Output\n");
        CheckError(aviSpliter->FindPin(L"Stream 00", &aviSpliterOutput));

        printf("Connect Spliter Video Output -> Microsoft DTV-DVD Decorder\n");
        CheckError(graph->Connect(aviSpliterOutput, videoDecorderInput));

        printf("Get Microsoft DTV-DVD Decorder Output\n");
        CheckError(videoDecorder->FindPin(L"Video Output 1", &videoDecorderOutput));

        printf("Connect Microsoft DTV-DVD Decorder -> VMR9 Renderer\n");
        CheckError(graph->Connect(videoDecorderOutput, vmr9RendererInput));

        printf("Run\n");
        CheckError(graphControl->Run());

        graphEvent->WaitForCompletion(INFINITE, &graphEvCode);

        printf("%ld\n", graphEvCode);

        if (graphEvCode == EC_COMPLETE) {
            printf("Done \n");
        }

        CoUninitialize();
    }
}

