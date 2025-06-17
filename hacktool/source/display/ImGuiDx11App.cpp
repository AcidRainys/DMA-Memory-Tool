#include "ImGuiDx11App.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiDx11App::ImGuiDx11App()
{
    m_wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L,
             GetModuleHandle(nullptr), nullptr, nullptr, nullptr,
             nullptr, L"ImGuiAppClass", nullptr };
}

ImGuiDx11App::~ImGuiDx11App()
{
    Shutdown();
}

bool ImGuiDx11App::Initialize(const wchar_t* windowTitle, int width, int height)
{
    ImGui_ImplWin32_EnableDpiAwareness();
    m_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    
    width = static_cast<int>(width * m_dpiScale);
    height = static_cast<int>(height * m_dpiScale);

    
    ::RegisterClassExW(&m_wc);

    
    m_hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW ,m_wc.lpszClassName, windowTitle,
        WS_POPUP, 0, 0,
        1, 1, nullptr, nullptr,
        m_wc.hInstance, this);

    if (!m_hwnd) return false;

    
    if (!CreateDeviceD3D(m_hwnd)) {
        CleanupDeviceD3D();
        return false;
    }

    
    ::ShowWindow(m_hwnd, SW_HIDE);
    ::UpdateWindow(m_hwnd);

    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.WantSaveIniSettings = false;
    io.IniFilename = NULL;
    
    ImGui::StyleColorsClassic();

    
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(m_dpiScale);

    ImFontConfig config;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &config, io.Fonts->GetGlyphRangesChineseFull());


    
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

    return true;
}

void ImGuiDx11App::SetRenderCallback(RenderCallback callback)
{
    m_renderCallback = callback;
}

void ImGuiDx11App::Run()
{
    bool done = false;
    while (!done)
    {
        
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

       
        if (m_swapChainOccluded && m_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(10);
            continue;
        }
        m_swapChainOccluded = false;

        
        if (m_resizeWidth && m_resizeHeight) {
            HandleResize();
        }

        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        
        m_renderCallback();

        
        ImGui::Render();
        const float clear_color[4] = {
            m_clearColor.x * m_clearColor.w,
            m_clearColor.y * m_clearColor.w,
            m_clearColor.z * m_clearColor.w,
            m_clearColor.w
        };
        m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        
        HRESULT hr = m_pSwapChain->Present(0, 0);
        m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }
}

void ImGuiDx11App::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    if (m_hwnd) ::DestroyWindow(m_hwnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

bool ImGuiDx11App::CreateDeviceD3D(HWND hWnd)
{
    
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 240;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pd3dDevice,
        &featureLevel,
        &m_pd3dDeviceContext
    );

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &m_pSwapChain,
            &m_pd3dDevice,
            &featureLevel,
            &m_pd3dDeviceContext
        );
    }

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ImGuiDx11App::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pd3dDeviceContext) {
        m_pd3dDeviceContext->Release();
        m_pd3dDeviceContext = nullptr;
    }
    if (m_pd3dDevice) {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
}

void ImGuiDx11App::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void ImGuiDx11App::CleanupRenderTarget()
{
    if (m_mainRenderTargetView) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
}

void ImGuiDx11App::HandleResize()
{
    CleanupRenderTarget();
    m_pSwapChain->ResizeBuffers(0, m_resizeWidth, m_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
    m_resizeWidth = m_resizeHeight = 0;
    CreateRenderTarget();
}

LRESULT WINAPI ImGuiDx11App::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    ImGuiDx11App* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<ImGuiDx11App*>(pCreate->lpCreateParams);
        
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else {
        pThis = reinterpret_cast<ImGuiDx11App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (!pThis) return ::DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        pThis->m_resizeWidth = (UINT)LOWORD(lParam);
        pThis->m_resizeHeight = (UINT)HIWORD(lParam);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}