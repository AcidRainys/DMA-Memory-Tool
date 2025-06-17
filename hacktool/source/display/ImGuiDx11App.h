#pragma once
#include "ImGui/imgui.h"
#include "ImGui/backends/imgui_impl_win32.h"
#include "ImGui/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <functional>


class ImGuiDx11App
{
public:
    using RenderCallback = std::function<void()>;

    ImGuiDx11App();
    ~ImGuiDx11App();

    bool Initialize(const wchar_t* windowTitle, int width, int height);
    void SetRenderCallback(RenderCallback callback);
    void Run();
    void Shutdown();

    
    ImVec4& GetClearColor() { return m_clearColor; }

private:
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void HandleResize();

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    
    HWND m_hwnd = nullptr;
    WNDCLASSEXW m_wc = {};

    
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;

    
    bool m_swapChainOccluded = false;
    UINT m_resizeWidth = 0;
    UINT m_resizeHeight = 0;
    ImVec4 m_clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    
    RenderCallback m_renderCallback = [] {};

    
    float m_dpiScale = 1.0f;
};