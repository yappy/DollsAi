#include "stdafx.h"
#include "SimpleCapture.h"
#include "interop.h"
#include <winrt/windows.graphics.directx.direct3d11.h>

using namespace winrt;
using namespace Windows;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Composition;

SimpleCapture::SimpleCapture(
    IDirect3DDevice const& device,
    GraphicsCaptureItem const& item)
{
    m_item = item;
    m_device = device;

    // Set up 
    auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
    d3dDevice->GetImmediateContext(m_d3dContext.put());

    auto size = m_item.Size();

    m_swapChain = CreateDXGISwapChain(
        d3dDevice,
        static_cast<uint32_t>(size.Width),
        static_cast<uint32_t>(size.Height),
        static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
        2);

    // Create framepool, define pixel format (DXGI_FORMAT_B8G8R8A8_UNORM), and frame size. 
    m_framePool = Direct3D11CaptureFramePool::Create(
        m_device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        size);
    m_session = m_framePool.CreateCaptureSession(m_item);
    m_lastSize = size;
    //m_frameArrived = m_framePool.FrameArrived(auto_revoke, { this, &SimpleCapture::OnFrameArrived });
}

// Start sending capture frames
void SimpleCapture::StartCapture()
{
    CheckClosed();
    m_session.StartCapture();
}

ICompositionSurface SimpleCapture::CreateSurface(
    Compositor const& compositor)
{
    CheckClosed();
    return CreateCompositionSurfaceForSwapChain(compositor, m_swapChain.get());
}

// Process captured frames
void SimpleCapture::Close()
{
    auto expected = false;
    if (m_closed.compare_exchange_strong(expected, true))
    {
        m_frameArrived.revoke();
        m_framePool.Close();
        m_session.Close();

        m_swapChain = nullptr;
        m_framePool = nullptr;
        m_session = nullptr;
        m_item = nullptr;
    }
}

#include <stdio.h>
std::tuple<std::vector<uint8_t>, int, int> SimpleCapture::TryGetNextFrame()
{
    std::vector<uint8_t> buf;
    int width = 0;
    int height = 0;

    bool newSize = false;
    {
        auto frame = m_framePool.TryGetNextFrame();
        if (!frame) {
            putwchar(L'.');
            return { buf, 0, 0 };
        }
        auto frameContentSize = frame.ContentSize();
        if (frameContentSize.Width != m_lastSize.Width ||
            frameContentSize.Height != m_lastSize.Height) {
            m_lastSize = frameContentSize;
            newSize = true;
        }
        wprintf(L"\nCaptured %dx%d\n", frameContentSize.Width, frameContentSize.Height);

        auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        D3D11_MAPPED_SUBRESOURCE mapInfo = {};
        HRESULT hr = m_d3dContext->Map(frameSurface.get(), 0, D3D11_MAP_READ, 0, &mapInfo);
        if (hr == E_INVALIDARG) {
            // copy the texture and try map again
            D3D11_TEXTURE2D_DESC desc;
            frameSurface->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;

            auto d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(m_device);
            winrt::com_ptr<ID3D11Texture2D> stagingTexture;
            check_hresult(d3dDevice->CreateTexture2D(&desc, nullptr, stagingTexture.put()));

            // copy the texture to a staging resource
            m_d3dContext->CopyResource(stagingTexture.get(), frameSurface.get());

            // now, map the staging resource
            HRESULT hr = m_d3dContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapInfo);
            frameSurface = std::move(stagingTexture);
        }
        else {
            // throw
            check_hresult(hr);
        }
        // copy
        const int depth = 4;
        buf.resize(depth * frameContentSize.Width * frameContentSize.Height);
        for (int y = 0; y < frameContentSize.Height; y++) {
            size_t srcoffset = y * mapInfo.RowPitch;
            size_t dstoffset = y * depth * frameContentSize.Width;
            const uint8_t* src = static_cast<uint8_t*>(mapInfo.pData) + srcoffset;
            uint8_t* dst = buf.data() + dstoffset;
            memcpy_s(dst, buf.data() + buf.size() - dst, src, depth * frameContentSize.Width);
        }
        width = frameContentSize.Width;
        height = frameContentSize.Height;
        m_d3dContext->Unmap(frameSurface.get(), 0);
    }

    if (newSize) {
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize);
    }

    return { buf, width, height };
}

/*
void SimpleCapture::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&)
{
    auto newSize = false;

    {
        auto frame = sender.TryGetNextFrame();
        auto frameContentSize = frame.ContentSize();

        if (frameContentSize.Width != m_lastSize.Width ||
            frameContentSize.Height != m_lastSize.Height)
        {
            // The thing we have been capturing has changed size.
            // We need to resize our swap chain first, then blit the pixels.
            // After we do that, retire the frame and then recreate our frame pool.
            newSize = true;
            m_lastSize = frameContentSize;
            m_swapChain->ResizeBuffers(
                2,
                static_cast<uint32_t>(m_lastSize.Width),
                static_cast<uint32_t>(m_lastSize.Height),
                static_cast<DXGI_FORMAT>(DirectXPixelFormat::B8G8R8A8UIntNormalized),
                0);
        }

        {
            auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

            com_ptr<ID3D11Texture2D> backBuffer;
            check_hresult(m_swapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), backBuffer.put_void()));

            m_d3dContext->CopyResource(backBuffer.get(), frameSurface.get());
        }
    }

    DXGI_PRESENT_PARAMETERS presentParameters = { 0 };
    m_swapChain->Present1(1, 0, &presentParameters);

    if (newSize)
    {
        m_framePool.Recreate(
            m_device,
            DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_lastSize);
    }
}
*/
