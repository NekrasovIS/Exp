#include "devices/WindowsScreenCaptureBackend.h"

#include <QMetaObject>
#include <QScreen>
#include <QVideoFrameFormat>
#include <QtCore/qnativeinterface.h>

// Заголовки WinRT/D3D11 не собираются чисто под /W4 — предупреждения
// глушатся только вокруг них, а не для всего файла, чтобы не терять
// диагностику в собственном коде ниже.
#pragma warning(push)
#pragma warning(disable : 4265 4266 4365 4514 4571 4623 4625 4626 4668 4820 4986 5026 5027 5039 5204 5220)
#include <d3d11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <wrl/client.h>
#pragma warning(pop)

#include <algorithm>
#include <cstring>

namespace devicehub {

namespace {

using Microsoft::WRL::ComPtr;
namespace WGC = winrt::Windows::Graphics::Capture;
namespace WGDD3D = winrt::Windows::Graphics::DirectX::Direct3D11;
namespace WGD = winrt::Windows::Graphics::DirectX;

WGC::GraphicsCaptureItem createCaptureItemForMonitor(HMONITOR monitor) {
    auto interopFactory = winrt::get_activation_factory<WGC::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    WGC::GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(interopFactory->CreateForMonitor(
        monitor, winrt::guid_of<WGC::GraphicsCaptureItem>(), winrt::put_abi(item)));
    return item;
}

WGDD3D::IDirect3DDevice createDirect3DDeviceFrom(ID3D11Device* device) {
    ComPtr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(device->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf())));
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put()));
    return inspectable.as<WGDD3D::IDirect3DDevice>();
}

ComPtr<ID3D11Texture2D> textureFromSurface(WGDD3D::IDirect3DSurface const& surface) {
    auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(texture.GetAddressOf())));
    return texture;
}

}  // namespace

struct WindowsScreenCaptureBackend::Impl {
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    WGDD3D::IDirect3DDevice winrtDevice{nullptr};
    WGC::GraphicsCaptureItem item{nullptr};
    WGC::Direct3D11CaptureFramePool framePool{nullptr};
    WGC::GraphicsCaptureSession session{nullptr};
    winrt::event_token frameArrivedToken;
    winrt::event_token itemClosedToken;
    QSize poolSize;
    bool active = false;
};

WindowsScreenCaptureBackend::WindowsScreenCaptureBackend(QObject* parent)
    : IScreenCaptureBackend(parent), impl_(std::make_unique<Impl>()) {
    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    // D3D11_CREATE_DEVICE_DEBUG требует установленный Graphics Tools
    // компонент Windows — без него D3D11CreateDevice падает с
    // DXGI_ERROR_SDK_COMPONENT_MISSING, так что не включаем его
    // безусловно даже в отладочной сборке.
#endif
    const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0,
                                          D3D11_SDK_VERSION, impl_->d3dDevice.GetAddressOf(), nullptr,
                                          impl_->d3dContext.GetAddressOf());
    if (FAILED(hr)) {
        return;
    }
    impl_->winrtDevice = createDirect3DDeviceFrom(impl_->d3dDevice.Get());
}

WindowsScreenCaptureBackend::~WindowsScreenCaptureBackend() {
    stop();
}

void WindowsScreenCaptureBackend::setScreen(QScreen* screen) {
    const bool wasActive = isActive();
    stop();

    if (screen == nullptr) {
        return;
    }
    auto* windowsScreen = screen->nativeInterface<QNativeInterface::QWindowsScreen>();
    if (windowsScreen == nullptr) {
        emit errorOccurred(tr("Screen has no native Windows handle."));
        return;
    }

    try {
        impl_->item = createCaptureItemForMonitor(windowsScreen->handle());
    } catch (const winrt::hresult_error& error) {
        emit errorOccurred(QString::fromWCharArray(error.message().c_str()));
        impl_->item = nullptr;
        return;
    }

    impl_->itemClosedToken = impl_->item.Closed(
        [this](WGC::GraphicsCaptureItem const&, winrt::Windows::Foundation::IInspectable const&) {
            QMetaObject::invokeMethod(
                this, [this]() { emit errorOccurred(tr("Captured screen became unavailable.")); },
                Qt::QueuedConnection);
        });

    if (wasActive) {
        start();
    }
}

void WindowsScreenCaptureBackend::start() {
    if (impl_->active || impl_->item == nullptr || impl_->winrtDevice == nullptr) {
        return;
    }

    try {
        impl_->poolSize = QSize(impl_->item.Size().Width, impl_->item.Size().Height);
        impl_->framePool = WGC::Direct3D11CaptureFramePool::CreateFreeThreaded(
            impl_->winrtDevice, WGD::DirectXPixelFormat::B8G8R8A8UIntNormalized, /*numberOfBuffers=*/2,
            impl_->item.Size());
        impl_->session = impl_->framePool.CreateCaptureSession(impl_->item);
        impl_->frameArrivedToken =
            impl_->framePool.FrameArrived([this](WGC::Direct3D11CaptureFramePool const& sender,
                                                  winrt::Windows::Foundation::IInspectable const&) {
                auto frame = sender.TryGetNextFrame();
                if (frame == nullptr) {
                    return;
                }

                const winrt::Windows::Graphics::SizeInt32 contentSize = frame.ContentSize();
                if (contentSize.Width != impl_->poolSize.width() || contentSize.Height != impl_->poolSize.height()) {
                    impl_->poolSize = QSize(contentSize.Width, contentSize.Height);
                    impl_->framePool.Recreate(impl_->winrtDevice, WGD::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                                               contentSize);
                }

                ComPtr<ID3D11Texture2D> texture;
                try {
                    texture = textureFromSurface(frame.Surface());
                } catch (const winrt::hresult_error&) {
                    return;
                }

                D3D11_TEXTURE2D_DESC desc{};
                texture->GetDesc(&desc);

                D3D11_TEXTURE2D_DESC stagingDesc = desc;
                stagingDesc.Usage = D3D11_USAGE_STAGING;
                stagingDesc.BindFlags = 0;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc.MiscFlags = 0;
                ComPtr<ID3D11Texture2D> staging;
                if (FAILED(impl_->d3dDevice->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf()))) {
                    return;
                }
                impl_->d3dContext->CopyResource(staging.Get(), texture.Get());

                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (FAILED(impl_->d3dContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                    return;
                }

                QVideoFrameFormat format(QSize(static_cast<int>(desc.Width), static_cast<int>(desc.Height)),
                                          QVideoFrameFormat::Format_BGRA8888);
                QVideoFrame videoFrame(format);
                if (videoFrame.map(QVideoFrame::WriteOnly)) {
                    uchar* dst = videoFrame.bits(0);
                    const int dstStride = videoFrame.bytesPerLine(0);
                    const auto* src = static_cast<const uchar*>(mapped.pData);
                    const int rowBytes = std::min<int>(dstStride, static_cast<int>(mapped.RowPitch));
                    for (int y = 0; y < static_cast<int>(desc.Height); ++y) {
                        std::memcpy(dst + (static_cast<qsizetype>(y) * dstStride),
                                    src + (static_cast<qsizetype>(y) * mapped.RowPitch),
                                    static_cast<size_t>(rowBytes));
                    }
                    videoFrame.unmap();
                    QMetaObject::invokeMethod(
                        this, [this, videoFrame]() { emit frameAvailable(videoFrame); }, Qt::QueuedConnection);
                }
                impl_->d3dContext->Unmap(staging.Get(), 0);
            });
        impl_->session.StartCapture();
        impl_->active = true;
    } catch (const winrt::hresult_error& error) {
        emit errorOccurred(QString::fromWCharArray(error.message().c_str()));
        impl_->active = false;
    }
}

void WindowsScreenCaptureBackend::stop() {
    if (impl_->framePool != nullptr) {
        impl_->framePool.FrameArrived(impl_->frameArrivedToken);
    }
    if (impl_->session != nullptr) {
        impl_->session.Close();
        impl_->session = nullptr;
    }
    if (impl_->framePool != nullptr) {
        impl_->framePool.Close();
        impl_->framePool = nullptr;
    }
    impl_->active = false;
}

bool WindowsScreenCaptureBackend::isActive() const {
    return impl_->active;
}

}  // namespace devicehub
