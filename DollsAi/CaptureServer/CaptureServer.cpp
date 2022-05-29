#include "stdafx.h"
#include "interop.h"
#include "SimpleCapture.h"
#include "winenum.h"

#include <stdio.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "dwmapi.lib")

#pragma pack(push, 2)
struct BitmapHeader
{
    unsigned short bfType;
    unsigned long bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned long bfOffBits;
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPixPerMeter;
    int biYPixPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImporant;
};
#pragma pack(pop)

void write_bmp(const std::vector<uint8_t> &buf, int w, int h)
{
    FILE* fp = nullptr;
    fopen_s(&fp, "test.bmp", "wb");
    if (fp == nullptr) {
        return;
    }

    int pitch = (w * 3 + 3) / 4 * 4;
    std::vector<uint8_t> bmpbuf;

    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            int offset = 4 * (y * w + x);
            uint32_t b = buf.at(offset + 0);
            uint32_t g = buf.at(offset + 1);
            uint32_t r = buf.at(offset + 2);
            uint32_t a = buf.at(offset + 3);
            bmpbuf.push_back(b);
            bmpbuf.push_back(g);
            bmpbuf.push_back(r);
            for (int i = 0; i < pitch - w * 3; i++) {
                bmpbuf.push_back(0);
            }
        }
    }

    BitmapHeader header = {};
    header.bfType = 0x4d42; // "BM"
    header.bfSize = pitch * h + 54;
    header.bfReserved1 = 0;
    header.bfReserved2 = 0;
    header.bfOffBits = 54;
    header.biSize = 40;
    header.biWidth = w;
    header.biHeight = h;
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = 0;
    header.biSizeImage = 0;
    header.biXPixPerMeter = 0;
    header.biYPixPerMeter = 0;
    header.biClrUsed = 0;
    header.biClrImporant = 0;
    fwrite(&header, 1, sizeof(header), fp);
    fwrite(bmpbuf.data(), 1, bmpbuf.size(), fp);
}

int main(int argc, char *argv[])
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    auto windows = EnumerateWindows();
    for (const auto &win : windows) {
        wprintf(L"%p %s\n", win.Hwnd(), win.Title().c_str());
    }

    HWND hwnd = nullptr;
    wprintf(L"\nInput HWND: ");
    wscanf_s(L"%p", &hwnd);

    _putws(L"Initializing...");
    auto d3dDevice = CreateD3DDevice();
    auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
    auto device = CreateDirect3DDevice(dxgiDevice.get());
    auto item = CreateCaptureItemForWindow(hwnd);
    auto cap = std::make_unique<SimpleCapture>(device, item);

    cap->StartCapture();
    for (;;) {
        auto[buf, w, h] = cap->TryGetNextFrame();
        if (w == 0 && h == 0) {
            continue;
        }
        write_bmp(buf, w, h);
        break;
    }

    return 0;
}
