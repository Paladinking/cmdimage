#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>

#include <string>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <Dshow.h>
#include <dvdmedia.h>

#include "dynamic_string.h"



void DeleteMediaType(AM_MEDIA_TYPE* pmt) {
    if (pmt)
    {
        if (pmt->cbFormat != 0)
            CoTaskMemFree(pmt->pbFormat);

        if (pmt->pUnk != nullptr)
            pmt->pUnk->Release();

        CoTaskMemFree(pmt);
    }
}

std::string WideStringToString(const wchar_t* wstr) {
    if (!wstr) return {};

    // Get the required buffer size
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0,
                                          nullptr, nullptr);

    if (size_needed <= 0) {
        return {};
    }

    std::string str(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed,
                        nullptr, nullptr);

    // Remove the trailing null
    if (!str.empty() && str.back() == '\0') {
        str.pop_back();
    }

    return str;
}

#define SAFE_RELEASE(x) { if (x) x->Release(); x = nullptr; }

bool QueryMediaSize(AM_MEDIA_TYPE* pmt, int& w, int& h) {
    if (pmt->formattype == FORMAT_DvInfo) {
        DVINFO* dvi = (DVINFO*)pmt->pbFormat;
        w = 720;
        h = (dvi->dwDVVAuxSrc & 0x00000080) ? 576 : 480;
        return true;
    } else if (pmt->formattype == FORMAT_MPEG2Video) {
        MPEG2VIDEOINFO* mpg2 = (MPEG2VIDEOINFO*)pmt->pbFormat;
        w = mpg2->hdr.bmiHeader.biWidth;
        h = std::abs(mpg2->hdr.bmiHeader.biHeight);
        return true;
    } else if (pmt->formattype == FORMAT_MPEGStreams) {
        return false;
    } else if (pmt->formattype == FORMAT_MPEGVideo) {
        MPEG1VIDEOINFO* mpg1 = (MPEG1VIDEOINFO*)pmt->pbFormat;
        w = mpg1->hdr.bmiHeader.biWidth;
        h = std::abs(mpg1->hdr.bmiHeader.biHeight);
        return true;
    } else if (pmt->formattype == FORMAT_WaveFormatEx) {
        return false;
    } else if (pmt->formattype == FORMAT_VideoInfo) {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
        w = vih->bmiHeader.biWidth;
        h = std::abs(vih->bmiHeader.biHeight);
        return true;
    } else if (pmt->formattype == FORMAT_VideoInfo2) {
        VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)pmt->pbFormat;
        w = vih->bmiHeader.biWidth;
        h = std::abs(vih->bmiHeader.biHeight);
        return true;
    } else {
        return false;
    }
}

struct CaptureDevice {
    RefString name;
    int w;
    int h;
};

bool QueryCaptureDevice(IMoniker* pMoniker, CaptureDevice& dev) {
    IBaseFilter* pFilter = nullptr;
    HRESULT hr = pMoniker->BindToObject(nullptr, nullptr, 
                                        IID_PPV_ARGS(&pFilter));
    if (FAILED(hr)) {
        return false;
    }

    bool status = false;
    IPin* pPin = nullptr;
    IEnumPins* pPinEnum = nullptr;
    hr = pFilter->EnumPins(&pPinEnum);
    if (SUCCEEDED(hr)) {
        while (pPinEnum->Next(1, &pPin, nullptr) == S_OK) {
            PIN_DIRECTION dir;
            hr = pPin->QueryDirection(&dir);
            if (SUCCEEDED(hr) && dir == PINDIR_OUTPUT) {
                // Found pin
                IAMStreamConfig* pConfig = nullptr;
                hr = pPin->QueryInterface(IID_PPV_ARGS(&pConfig));
                if (FAILED(hr)) {
                    continue;
                }
                AM_MEDIA_TYPE* pmt = nullptr;
                hr = pConfig->GetFormat(&pmt);
                if (FAILED(hr)) {
                    pConfig->Release();
                    continue;
                }
                if (!QueryMediaSize(pmt, dev.w, dev.h)) {
                    DeleteMediaType(pmt);
                    pConfig->Release();
                    continue;
                }

                DeleteMediaType(pmt);
                pConfig->Release();
                status = true;
                break;
            }
        }
    }
    SAFE_RELEASE(pPin);
    SAFE_RELEASE(pPinEnum);
    pFilter->Release();
    return status;
}

std::vector<CaptureDevice> FindCaptureDevices() {
    HRESULT hr = S_OK;
    ICreateDevEnum *pDevEnum = nullptr;
    IEnumMoniker *pClassEnum = nullptr;

    std::vector<CaptureDevice> res{};

    // Create the system device enumerator
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC,
                          IID_PPV_ARGS(&pDevEnum));
    if (FAILED(hr)) {
        printf("Couldn't create system enumerator!  hr=0x%lx", hr);
    }

    // Create an enumerator for the video capture devices

    if (SUCCEEDED(hr)) {
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
        if (FAILED(hr)) {
            printf("Couldn't create class enumerator!  hr=0x%lx", hr);
        }
    }

    if (SUCCEEDED(hr)) {
        // If there are no enumerators for the requested type, then 
        // CreateClassEnumerator will succeed, but pClassEnum will be nullptr.
        if (pClassEnum == nullptr) {
            printf("No video capture device was detected.\r\n\r\n"
                    "This sample requires a video capture device, such as a USB WebCam,\r\n"
                    "to be installed and working properly.  The sample will now close.");
            hr = E_FAIL;
        }
    }

    if (SUCCEEDED(hr)) {
        while (1) {
            IMoniker* pMoniker = nullptr;
            hr = pClassEnum->Next (1, &pMoniker, nullptr);
            if (hr == S_FALSE) {
                hr = E_FAIL;
                break;
            }
            IPropertyBag *pPropBag;
            hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
            if (SUCCEEDED(hr)) {
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, 0);

                if (SUCCEEDED(hr)) {
                    CaptureDevice dev;
                    if (!String_from_utf16_str(dev.name, var.bstrVal)) {
                        throw new std::bad_alloc();
                    }

                    if (QueryCaptureDevice(pMoniker, dev)) {
                        res.push_back(dev);
                    }
                }
                pPropBag->Release();
            }
            pMoniker->Release();
        }
    }

    SAFE_RELEASE(pDevEnum);
    SAFE_RELEASE(pClassEnum);

    return res;
}

void sample_pixel(int x, int y, const cv::Mat& mat, uint8_t& r, 
                  uint8_t &g, uint8_t &b) {
    r = mat.at<cv::Vec3b>(y, x)[2];
    g = mat.at<cv::Vec3b>(y, x)[1];
    b = mat.at<cv::Vec3b>(y, x)[0];
}

void convert_frame(RefString& dest, const cv::Mat& mat) {
    int pw = mat.cols;
    int ph = mat.rows;
    ph = (ph + 1) / 2;

    for (uint32_t y = 0; y < ph; ++y) {
        uint32_t last_rgb1 = 0xffffffff, last_rgb2 = 0xffffffff;
        for (uint32_t x = 0; x < pw; ++x) {
            uint8_t r1, g1, b1;
            uint8_t r2 = 0, g2 = 0, b2 = 0;
            sample_pixel(x, 2 * y, mat, r1, g1, b1);
            if ((2 * y + 1) < mat.rows) {
                sample_pixel(x, (2 * y + 1), mat, r2, g2, b2);
            }
            uint32_t rgb1 = r1 | (g1 << 8) | (b1 << 16);
            uint32_t rgb2 = r2 | (g2 << 8) | (b2 << 16);
            if (rgb1 == last_rgb1) {
                if (rgb2 == last_rgb2) {
                    if (rgb1 == rgb2) {
                        String_append(dest, ' ');
                    } else {
                        String_append_count(dest,  "\xe2\x96\x80", 3);
                    }
                } else {
                    if (rgb1 == rgb2) {
                        String_format_append(dest, 
                            "\x1b[48;2;%u;%u;%um ",
                            (unsigned)r2, (unsigned)g2, (unsigned)b2);
                    } else {
                        String_format_append(dest, 
                            "\x1b[48;2;%u;%u;%um\xe2\x96\x80",
                            (unsigned)r2, (unsigned)g2, (unsigned)b2);
                    }
                }
            } else if (rgb2 == last_rgb2) {
                if (rgb1 == rgb2) {
                    String_format_append(dest, 
                        "\x1b[38;2;%u;%u;%um ",
                        (unsigned)r1, (unsigned)g1, (unsigned)b1);
                } else {
                    String_format_append(dest, 
                        "\x1b[38;2;%u;%u;%um\xe2\x96\x80",
                        (unsigned)r1, (unsigned)g1, (unsigned)b1);
                }
            } else if (rgb1 == rgb2) {
                String_format_append(dest, 
                    "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um ",
                    (unsigned)r1, (unsigned)g1, (unsigned)b1, 
                    (unsigned)r2, (unsigned)g2, (unsigned)b2);
            } else {
                String_format_append(dest, 
                    "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um\xe2\x96\x80",
                    (unsigned)r1, (unsigned)g1, (unsigned)b1, 
                    (unsigned)r2, (unsigned)g2, (unsigned)b2);
            }
            last_rgb1 = rgb1;
            last_rgb2 = rgb2;
        }
        String_append_count(dest, "\x1b[0m\n", 5);
    }
    String_extend(dest, "\x1b[0m");
}

void get_console_size(int* w, int* h) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO i;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &i)) {
        if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &i)) {
            HANDLE hn = CreateConsoleScreenBuffer(GENERIC_WRITE, 0, NULL, 
                    CONSOLE_TEXTMODE_BUFFER, 0);
            if (!GetConsoleScreenBufferInfo(hn, &i)) {
                *w = 80;
                *h = 20;
                return;
            }
            CloseHandle(hn);
        }
    }

    *w = i.dwSize.X;
    *h = i.dwSize.Y;
#else
    struct winsize sz;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz) == 0) {
        *w = sz.ws_col;
        *h = sz.ws_row;
    } else {
        *w = 80;
        *h = 20;
    }
#endif
}


int main() {

    int cw, ch;
    get_console_size(&cw, &ch);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    auto devices = FindCaptureDevices();

    int ix = -1;
    int w = 0;
    int h = 0;

    for (auto dev: devices) {
        ++ix;
        printf("Name: %s, w: %d, h: %d\n", dev.name->buffer, dev.w, dev.h);
        if (dev.name == "OBS Virtual Camera") {
            w = dev.w;
            h = dev.h;
            break;
        }
    }
    if (ix < 0 || ix >= devices.size()) {
        printf("Device not found\n");
        CoUninitialize();
        return 1;
    }


    ch = ch * 2; // Two pixels per row

    int scale = 1;
    int pw = w;
    int ph = h;

    while (pw > cw || ph > ch) {
        ++scale;
        pw = (w + scale - 1) / scale;
        ph = (h + scale - 1) / scale;
    }

    auto cam = cv::VideoCapture();

    cam.open(ix, cv::CAP_DSHOW);

    if (cam.isOpened()) {
        std::printf("Open\n");
    }

    RefString s;
    RefWString out;

    cam.set(cv::CAP_PROP_FRAME_WIDTH, w);
    cam.set(cv::CAP_PROP_FRAME_HEIGHT, h);

    cv::Mat m, converted;
    cam.read(m);


    printf("Dims: %d, %d\n", pw, ph);
    while (cam.read(m)) {
        cv::resize(m, converted, cv::Size(pw, ph), cv::INTER_LINEAR);

        String_clear(s);
        String_format_append(s, "\x1b[1;1H");
        convert_frame(s, converted);

        WString_from_utf8_bytes(out, s->buffer, s->length);
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), out->buffer,
                      out->length, NULL, NULL);

        Sleep(10);
    }
    printf("Exit\n");

    CoUninitialize();

    return 0;
}
