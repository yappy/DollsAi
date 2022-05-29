#pragma once
#include <winrt/Windows.UI.Composition.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <d2d1_1.h>

extern "C"
{
    HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice* dxgiDevice,
        ::IInspectable** graphicsDevice);

    HRESULT __stdcall CreateDirect3D11SurfaceFromDXGISurface(::IDXGISurface* dgxiSurface,
        ::IInspectable** graphicsSurface);
}

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
    IDirect3DDxgiInterfaceAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
};

inline auto CreateDirect3DDevice(IDXGIDevice* dxgi_device)
{
    winrt::com_ptr<::IInspectable> d3d_device;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, d3d_device.put()));
    return d3d_device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

inline auto CreateDirect3DSurface(IDXGISurface* dxgi_surface)
{
    winrt::com_ptr<::IInspectable> d3d_surface;
    winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgi_surface, d3d_surface.put()));
    return d3d_surface.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
}

template <typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
    return result;
}

inline auto CreateCaptureItemForWindow(HWND hwnd)
{
    auto activation_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
    auto interop_factory = activation_factory.as<IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = { nullptr };
    interop_factory->CreateForWindow(hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), reinterpret_cast<void**>(winrt::put_abi(item)));
    return item;
}

inline auto SurfaceBeginDraw(
    winrt::Windows::UI::Composition::CompositionDrawingSurface const& surface)
{
    auto surfaceInterop = surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
    winrt::com_ptr<ID2D1DeviceContext> context;
    POINT offset = {};
    winrt::check_hresult(surfaceInterop->BeginDraw(nullptr, __uuidof(ID2D1DeviceContext), context.put_void(), &offset));
    context->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)offset.x, (FLOAT)offset.y));
    return context;
}

inline void SurfaceEndDraw(
    winrt::Windows::UI::Composition::CompositionDrawingSurface const& surface)
{
    auto surfaceInterop = surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
    winrt::check_hresult(surfaceInterop->EndDraw());
}

inline auto CreateCompositionSurfaceForSwapChain(
    winrt::Windows::UI::Composition::Compositor const& compositor,
    ::IUnknown* swapChain)
{
    winrt::Windows::UI::Composition::ICompositionSurface surface{ nullptr };
    auto compositorInterop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
    winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionSurface> surfaceInterop;
    winrt::check_hresult(compositorInterop->CreateCompositionSurfaceForSwapChain(swapChain, surfaceInterop.put()));
    winrt::check_hresult(surfaceInterop->QueryInterface(winrt::guid_of<winrt::Windows::UI::Composition::ICompositionSurface>(),
        reinterpret_cast<void**>(winrt::put_abi(surface))));
    return surface;
}

struct SurfaceContext
{
public:
    SurfaceContext(std::nullptr_t) {}
    SurfaceContext(
        winrt::Windows::UI::Composition::CompositionDrawingSurface surface)
    {
        m_surface = surface;
        m_d2dContext = SurfaceBeginDraw(m_surface);
    }
    ~SurfaceContext()
    {
        SurfaceEndDraw(m_surface);
        m_d2dContext = nullptr;
        m_surface = nullptr;
    }

    winrt::com_ptr<ID2D1DeviceContext> GetDeviceContext() { return m_d2dContext; }

private:
    winrt::com_ptr<ID2D1DeviceContext> m_d2dContext;
    winrt::Windows::UI::Composition::CompositionDrawingSurface m_surface{ nullptr };
};

struct D3D11DeviceLock
{
public:
    D3D11DeviceLock(std::nullopt_t) {}
    D3D11DeviceLock(ID3D11Multithread* pMultithread)
    {
        m_multithread.copy_from(pMultithread);
        m_multithread->Enter();
    }
    ~D3D11DeviceLock()
    {
        m_multithread->Leave();
        m_multithread = nullptr;
    }
private:
    winrt::com_ptr<ID3D11Multithread> m_multithread;
};

inline auto
CreateWICFactory()
{
    winrt::com_ptr<IWICImagingFactory2> wicFactory;
    winrt::check_hresult(
        ::CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            winrt::guid_of<IWICImagingFactory>(),
            wicFactory.put_void()));

    return wicFactory;
}

inline auto
CreateD2DDevice(
    winrt::com_ptr<ID2D1Factory1> const& factory,
    winrt::com_ptr<ID3D11Device> const& device)
{
    winrt::com_ptr<ID2D1Device> result;
    winrt::check_hresult(factory->CreateDevice(device.as<IDXGIDevice>().get(), result.put()));
    return result;
}

inline auto
CreateD3DDevice(
    D3D_DRIVER_TYPE const type,
    winrt::com_ptr<ID3D11Device>& device)
{
    WINRT_ASSERT(!device);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    //#ifdef _DEBUG
    //	flags |= D3D11_CREATE_DEVICE_DEBUG;
    //#endif

    return D3D11CreateDevice(
        nullptr,
        type,
        nullptr,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        device.put(),
        nullptr,
        nullptr);
}

inline auto
CreateD3DDevice()
{
    winrt::com_ptr<ID3D11Device> device;
    HRESULT hr = CreateD3DDevice(D3D_DRIVER_TYPE_HARDWARE, device);

    if (DXGI_ERROR_UNSUPPORTED == hr)
    {
        hr = CreateD3DDevice(D3D_DRIVER_TYPE_WARP, device);
    }

    winrt::check_hresult(hr);
    return device;
}

inline auto
CreateD2DFactory()
{
    D2D1_FACTORY_OPTIONS options{};

    //#ifdef _DEBUG
    //	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
    //#endif

    winrt::com_ptr<ID2D1Factory1> factory;

    winrt::check_hresult(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        options,
        factory.put()));

    return factory;
}

inline auto
CreateDXGISwapChain(
    winrt::com_ptr<ID3D11Device> const& device,
    const DXGI_SWAP_CHAIN_DESC1* desc)
{
    auto dxgiDevice = device.as<IDXGIDevice2>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgiDevice->GetParent(winrt::guid_of<IDXGIAdapter>(), adapter.put_void()));
    winrt::com_ptr<IDXGIFactory2> factory;
    winrt::check_hresult(adapter->GetParent(winrt::guid_of<IDXGIFactory2>(), factory.put_void()));

    winrt::com_ptr<IDXGISwapChain1> swapchain;
    winrt::check_hresult(factory->CreateSwapChainForComposition(
        device.get(),
        desc,
        nullptr,
        swapchain.put()));

    return swapchain;
}

inline auto
CreateDXGISwapChain(
    winrt::com_ptr<ID3D11Device> const& device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    uint32_t bufferCount)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferCount = bufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    return CreateDXGISwapChain(device, &desc);
}

inline auto CreateCompositionGraphicsDevice(
    winrt::Windows::UI::Composition::Compositor const& compositor,
    ::IUnknown* device)
{
    winrt::Windows::UI::Composition::CompositionGraphicsDevice graphicsDevice{ nullptr };
    auto compositorInterop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
    winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionGraphicsDevice> graphicsInterop;
    winrt::check_hresult(compositorInterop->CreateGraphicsDevice(device, graphicsInterop.put()));
    winrt::check_hresult(graphicsInterop->QueryInterface(winrt::guid_of<winrt::Windows::UI::Composition::CompositionGraphicsDevice>(),
        reinterpret_cast<void**>(winrt::put_abi(graphicsDevice))));
    return graphicsDevice;
}

inline void ResizeSurface(
    winrt::Windows::UI::Composition::CompositionDrawingSurface const& surface,
    winrt::Windows::Foundation::Size const& size)
{
    auto surfaceInterop = surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
    SIZE newSize = {};
    newSize.cx = static_cast<LONG>(std::round(size.Width));
    newSize.cy = static_cast<LONG>(std::round(size.Height));
    winrt::check_hresult(surfaceInterop->Resize(newSize));
}
