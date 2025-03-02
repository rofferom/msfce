#include <assert.h>
#include <stdio.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <msfce/core/snes.h>

using Microsoft::WRL::ComPtr;

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof((x)[0]))

constexpr int kWindowInitialScale = 2;
constexpr auto kRenderPeriod = std::chrono::microseconds(16666);

class D3D11Renderer : public msfce::core::Renderer {
public:
    D3D11Renderer(
        const msfce::core::SnesConfig& snesConfig,
        HWND hwnd,
        int width,
        int height);
    ~D3D11Renderer();

    int init(const std::shared_ptr<msfce::core::Snes>& snes);
    int render();

    void windowResized(int width, int height);

private:
    void scanStarted() final;
    void drawPixel(const msfce::core::Color& c) final;
    void scanEnded() final;

    void playAudioSamples(const uint8_t* data, size_t sampleCount) final;

private:
    struct Vertex {
        float x, y, z;
        float u, v;
    };

    struct TransformConstants {
        float scale[2];
        float padding[2];
    };

private:
    int createSwapchain();
    int createRenderTargetView();

    int createSnesFramebuffer();
    int compileShaders();
    int createVertexBuffer();
    int createConstantBuffer();
    int updateConstantBuffer();

private:
    msfce::core::SnesConfig m_SnesConfig;

    HWND m_Hwnd;
    int m_Width;
    int m_Height;
    std::atomic<bool> m_WindowResized;

    ComPtr<ID3D11Device> m_Device;
    ComPtr<ID3D11DeviceContext> m_Context;
    ComPtr<IDXGISwapChain> m_SwapChain;

    ComPtr<ID3D11RenderTargetView> m_RenderTargetView;

    size_t m_SnesTextureSize;
    size_t m_SnesTexturePitch;
    uint8_t* m_SnesTextureData = nullptr;
    uint8_t* m_SnesTextureWritter = nullptr;
    ComPtr<ID3D11Texture2D> m_SnesStagingTexture;
    ComPtr<ID3D11Texture2D> m_SnesFramebuffer;
    ComPtr<ID3D11ShaderResourceView> m_SnesFramebufferSrv;
    ComPtr<ID3D11SamplerState> m_SnesFramebufferSampler;

    ComPtr<ID3D11VertexShader> m_VertexShader;
    ComPtr<ID3D11PixelShader> m_PixelShader;
    ComPtr<ID3D11InputLayout> m_InputLayout;
    ComPtr<ID3D11Buffer> m_VertexBuffer;
    size_t m_VertexCount;

    ComPtr<ID3D11Buffer> m_ConstantBuffer;

    static const char* vsSource;
    static const char* psSource;
};

const char* D3D11Renderer::vsSource = R"(
    cbuffer TransformConstants : register(b0) {
        float2 scale;
        float2 padding;
    };

    struct VS_INPUT {
        float3 Pos : POSITION;
        float2 Tex : TEXCOORD0;
    };

    struct PS_INPUT {
        float4 Pos : SV_POSITION;
        float2 Tex : TEXCOORD0;
    };

    PS_INPUT VS(VS_INPUT input) {
        PS_INPUT output;
        output.Pos = float4(input.Pos.xy * scale, input.Pos.z, 1.0);
        output.Tex = input.Tex;
        return output;
    }
)";

const char* D3D11Renderer::psSource = R"(
    Texture2D tex : register(t0);
    SamplerState samp : register(s0);

    float4 PS(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
        return tex.Sample(samp, uv);
    }
)";

D3D11Renderer::D3D11Renderer(
    const msfce::core::SnesConfig& snesConfig,
    HWND hwnd,
    int width,
    int height)
    : m_SnesConfig(snesConfig), m_Hwnd(hwnd), m_Width(width), m_Height(height)
{
}

D3D11Renderer::~D3D11Renderer()
{
    delete[] m_SnesTextureData;
}

int D3D11Renderer::init(const std::shared_ptr<msfce::core::Snes>& snes)
{
    int ret;

    ret = createSwapchain();
    if (ret < 0)
        return ret;

    ret = createRenderTargetView();
    if (ret < 0)
        return ret;

    ret = createSnesFramebuffer();
    if (ret < 0)
        return ret;

    ret = compileShaders();
    if (ret < 0)
        return ret;

    ret = createVertexBuffer();
    if (ret < 0)
        return ret;

    ret = createConstantBuffer();
    if (ret < 0)
        return ret;

    ret = updateConstantBuffer();
    if (ret < 0)
        return ret;

    return 0;
}

int D3D11Renderer::render()
{
    HRESULT hr;

    // Upload SNES framebuffer
    D3D11_MAPPED_SUBRESOURCE mappedSnesFramebuffer;
    hr = m_Context->Map(
        m_SnesStagingTexture.Get(),
        0,
        D3D11_MAP_WRITE,
        0,
        &mappedSnesFramebuffer);
    assert(SUCCEEDED(hr));

    memcpy(mappedSnesFramebuffer.pData, m_SnesTextureData, m_SnesTextureSize);

    m_Context->Unmap(m_SnesStagingTexture.Get(), 0);

    m_Context->CopyResource(
        m_SnesFramebuffer.Get(), m_SnesStagingTexture.Get());

    // Check for window resize
    if (m_WindowResized.exchange(
            false, std::memory_order::memory_order_acquire)) {
        m_RenderTargetView.Reset();

        hr = m_SwapChain->ResizeBuffers(
            0, m_Width, m_Height, DXGI_FORMAT_UNKNOWN, 0);
        assert(SUCCEEDED(hr));

        createRenderTargetView();
        updateConstantBuffer();
    }

    // Render
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_Context->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_Context->IASetVertexBuffers(
        0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
    m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_Context->IASetInputLayout(m_InputLayout.Get());

    m_Context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
    m_Context->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());

    m_Context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
    m_Context->PSSetShaderResources(0, 1, m_SnesFramebufferSrv.GetAddressOf());
    m_Context->PSSetSamplers(0, 1, m_SnesFramebufferSampler.GetAddressOf());

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_Width);
    viewport.Height = static_cast<float>(m_Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_Context->RSSetViewports(1, &viewport);

    m_Context->OMSetRenderTargets(
        1, m_RenderTargetView.GetAddressOf(), nullptr);

    m_Context->Draw(m_VertexCount, 0);
    m_SwapChain->Present(0, 0);

    return 0;
}

void D3D11Renderer::windowResized(int width, int height)
{
    m_Width = width;
    m_Height = height;
    m_WindowResized.store(true, std::memory_order::memory_order_release);
}

void D3D11Renderer::scanStarted()
{
    m_SnesTextureWritter = m_SnesTextureData;
}

void D3D11Renderer::drawPixel(const msfce::core::Color& c)
{
    m_SnesTextureWritter[0] = c.r;
    m_SnesTextureWritter[1] = c.g;
    m_SnesTextureWritter[2] = c.b;
    m_SnesTextureWritter[3] = 0xFF;

    m_SnesTextureWritter += 4;
}

void D3D11Renderer::scanEnded()
{
}

void D3D11Renderer::playAudioSamples(const uint8_t* data, size_t sampleCount)
{
}

int D3D11Renderer::createSwapchain()
{
    HRESULT hr;

    // Create SwapChain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = m_Width;
    swapChainDesc.BufferDesc.Height = m_Height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_Hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        m_SwapChain.GetAddressOf(),
        m_Device.GetAddressOf(),
        nullptr,
        m_Context.GetAddressOf());
    assert(SUCCEEDED(hr));

    // Disable Alt+Enter
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> dxgiAdapter;
    ComPtr<IDXGIFactory> dxgiFactory;

    hr = m_Device.As(&dxgiDevice);
    assert(SUCCEEDED(hr));

    hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
    assert(SUCCEEDED(hr));

    hr = dxgiAdapter->GetParent(
        __uuidof(IDXGIFactory),
        reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));
    assert(SUCCEEDED(hr));

    hr = dxgiFactory->MakeWindowAssociation(m_Hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
    assert(SUCCEEDED(hr));

    return 0;
}

int D3D11Renderer::createRenderTargetView()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr;

    hr = m_SwapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    assert(SUCCEEDED(hr));

    hr = m_Device->CreateRenderTargetView(
        backBuffer.Get(), nullptr, m_RenderTargetView.GetAddressOf());
    assert(SUCCEEDED(hr));

    return 0;
}

int D3D11Renderer::createSnesFramebuffer()
{
    HRESULT hr;

    // CPU buffer
    m_SnesTexturePitch = m_SnesConfig.displayWidth * 4;
    m_SnesTextureSize = m_SnesTexturePitch * m_SnesConfig.displayHeight;
    m_SnesTextureData = new uint8_t[m_SnesTextureSize];

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = m_SnesConfig.displayWidth;
    desc.Height = m_SnesConfig.displayHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    hr = m_Device->CreateTexture2D(
        &desc, nullptr, m_SnesStagingTexture.GetAddressOf());
    assert(SUCCEEDED(hr));

    // GPU texture
    memset(&desc, 0, sizeof(desc));
    desc.Width = m_SnesConfig.displayWidth;
    desc.Height = m_SnesConfig.displayHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    hr = m_Device->CreateTexture2D(
        &desc, nullptr, m_SnesFramebuffer.GetAddressOf());
    assert(SUCCEEDED(hr));

    // ID3D11ShaderResourceView
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_Device->CreateShaderResourceView(
        m_SnesFramebuffer.Get(), &srvDesc, m_SnesFramebufferSrv.GetAddressOf());
    assert(SUCCEEDED(hr));

    // ID3D11SamplerState
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_Device->CreateSamplerState(&samplerDesc, &m_SnesFramebufferSampler);
    assert(SUCCEEDED(hr));

    return 0;
}

int D3D11Renderer::compileShaders()
{
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr;

    // Vertex shader
    ComPtr<ID3DBlob> vsBlob;
    hr = D3DCompile(
        vsSource,
        strlen(vsSource),
        nullptr,
        nullptr,
        nullptr,
        "VS",
        "vs_5_0",
        0,
        0,
        &vsBlob,
        &errorBlob);
    if (FAILED(hr)) {
        printf(
            "VS compile failed: '%s'\n",
            reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = m_Device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &m_VertexShader);
    assert(SUCCEEDED(hr));

    // Pixel shader
    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompile(
        psSource,
        strlen(psSource),
        nullptr,
        nullptr,
        nullptr,
        "PS",
        "ps_5_0",
        0,
        0,
        &psBlob,
        &errorBlob);
    if (FAILED(hr)) {
        printf(
            "PS compile failed: '%s'\n",
            reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = m_Device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &m_PixelShader);
    assert(SUCCEEDED(hr));

    // Input layout
    // clang-format off
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    // clang-format on

    hr = m_Device->CreateInputLayout(
        layout,
        2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_InputLayout);
    assert(SUCCEEDED(hr));

    return 0;
}

int D3D11Renderer::createVertexBuffer()
{
    // clang-format off
    const Vertex vertices[] = {
        {-1.0f,  1.0f, 0.0f, 0.0f, 0.0f},  // Top-left
        { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f},   // Top-right
        {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f}, // Bottom-left
        { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f}   // Bottom-right
    };
    // clang-format on
    HRESULT hr;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    hr = m_Device->CreateBuffer(&vbDesc, &vbData, &m_VertexBuffer);
    assert(SUCCEEDED(hr));

    m_VertexCount = SIZEOF_ARRAY(vertices);

    return 0;
}

int D3D11Renderer::createConstantBuffer()
{
    HRESULT hr;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(TransformConstants);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_Device->CreateBuffer(&cbDesc, nullptr, &m_ConstantBuffer);
    assert(SUCCEEDED(hr));

    return 0;
}

int D3D11Renderer::updateConstantBuffer()
{
    HRESULT hr;

    // Compute aspect ratio
    float ppuRatio = static_cast<float>(m_SnesConfig.displayWidth) /
                     m_SnesConfig.displayHeight;
    float windowRatio = static_cast<float>(m_Width) / m_Height;

    TransformConstants constants = {};
    if (windowRatio > ppuRatio) {
        // Window is wider than expected
        constants.scale[0] = ppuRatio / windowRatio;
        constants.scale[1] = 1.0f;
    } else {
        // Window is higher than expected
        constants.scale[0] = 1.0f;
        constants.scale[1] = windowRatio / ppuRatio;
    }

    // Update the constant buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_Context->Map(
        m_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(hr));

    memcpy(mapped.pData, &constants, sizeof(constants));

    m_Context->Unmap(m_ConstantBuffer.Get(), 0);

    return 0;
}

struct WindowData {
    std::shared_ptr<D3D11Renderer> renderer;
    bool fullscreen = false;
    RECT windowedRect;
    LONG originalStyle;
    LONG originalExStyle;
};

static LRESULT CALLBACK
windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    WindowData* app =
        reinterpret_cast<WindowData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        if (app->renderer) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            app->renderer->windowResized(width, height);
        }

        return 0;

    case WM_KEYDOWN: {
        if (wParam == 0x46 /* F */) {
            app->fullscreen = !app->fullscreen;

            if (app->fullscreen) {
                // Save current window properties
                GetWindowRect(hwnd, &app->windowedRect);
                app->originalStyle = GetWindowLong(hwnd, GWL_STYLE);
                app->originalExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                // Get monitor info
                HMONITOR monitor =
                    MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo = {.cbSize = sizeof(monitorInfo)};
                GetMonitorInfo(monitor, &monitorInfo);

                // Set window to borderless and match monitor size
                SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);

                SetWindowPos(
                    hwnd,
                    HWND_TOP,
                    monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.top,
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                    SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            } else {
                // Restore windowed mode
                SetWindowLong(hwnd, GWL_STYLE, app->originalStyle);
                SetWindowLong(hwnd, GWL_EXSTYLE, app->originalExStyle);

                SetWindowPos(
                    hwnd,
                    HWND_TOP,
                    app->windowedRect.left,
                    app->windowedRect.top,
                    app->windowedRect.right - app->windowedRect.left,
                    app->windowedRect.bottom - app->windowedRect.top,
                    SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }

            return 0;
        }

        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main(int argc, char* argv[])
{
    const wchar_t* kClassName = L"Msfce";
    WNDCLASS wc = {};
    HWND hwnd;
    HMODULE instance = GetModuleHandle(nullptr);
    WindowData app;
    int ret;

    auto snes = msfce::core::Snes::create();
    auto snesConfig = snes->getConfig();

    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    RegisterClass(&wc);

    int windowWidth = snesConfig.displayWidth * kWindowInitialScale;
    int windowHeight = snesConfig.displayHeight * kWindowInitialScale;

    hwnd = CreateWindowEx(
        0,
        kClassName,
        L"Monkey Super Famicom Emulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instance,
        &app);
    assert(hwnd);
    ShowWindow(hwnd, TRUE);

    auto renderer = std::make_shared<D3D11Renderer>(
        snesConfig, hwnd, windowWidth, windowHeight);
    ret = renderer->init(snes);
    assert(ret == 0);

    ret = snes->addRenderer(renderer);
    assert(ret == 0);

    ret = snes->plugCartidge("Super Mario World (U) [!].smc");
    assert(ret == 0);

    ret = snes->start();
    assert(ret == 0);

    app.renderer = renderer;

    auto run = std::atomic<bool>(true);

    std::thread snesLoop([snes, renderer, &run] {
        auto presentTp =
            std::chrono::high_resolution_clock::now() + kRenderPeriod;

        while (run) {
            snes->renderSingleFrame();

            std::this_thread::sleep_until(presentTp);
            renderer->render();
            presentTp += kRenderPeriod;
        }
    });

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT)
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    run = false;
    snesLoop.join();

    return 0;
}
