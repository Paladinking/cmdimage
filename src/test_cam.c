#include <windows.h>
#include <dshow.h>
#include <stdio.h>

int main() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    ICreateDevEnum *pDevEnum = NULL;
IEnumMoniker *pEnum = NULL;

    HRESULT hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                      &IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr)) {
        printf("Failed to create system device enumerator: 0x%x\n", hr);
        return -1;
    }
    hr = pDevEnum->lpVtbl->CreateClassEnumerator(pDevEnum, &CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr != S_OK) {
        printf("No video capture devices found\n");
        pDevEnum->lpVtbl->Release(pDevEnum);
        return 0;
    }

    printf("Video capture devices:\n");

    IMoniker *pMoniker = NULL;
    ULONG fetched = 0;
    int index = 0;
    while (pEnum->lpVtbl->Next(pEnum, 1, &pMoniker, &fetched) == S_OK) {
        IPropertyBag *pPropBag = NULL;
        hr = pMoniker->lpVtbl->BindToStorage(pMoniker, NULL, NULL, &IID_IPropertyBag, (void**)&pPropBag);
        if (SUCCEEDED(hr)) {
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->lpVtbl->Read(pPropBag, L"FriendlyName", &varName, NULL);
            if (SUCCEEDED(hr)) {
                wprintf(L"Device %d: %s\n", index, varName.bstrVal);
            }
            VariantClear(&varName);
            pPropBag->lpVtbl->Release(pPropBag);
        }
        pMoniker->lpVtbl->Release(pMoniker);
        index++;
    }

    pEnum->lpVtbl->Release(pEnum);
    pDevEnum->lpVtbl->Release(pDevEnum);



    CoUninitialize();
}
