#include "render.hpp"

#include <Windows.h>
#include <iostream>
#include <string>
#include <d3d9.h>
#include <d3d11.h>
#include <dwmapi.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include <dependencies/framework/imgui.h>
#include <dependencies/framework/backends/imgui_impl_dx11.h>
#include <dependencies/framework/backends/imgui_impl_win32.h>

#include <core/game/sdk.hpp>
#include <core/game/features/visuals/visuals.hpp>

#include <dependencies/framework/custom/imgui_freetype.h>
#include <dependencies/framework/custom/custom_widgets.h>
#include <dependencies/framework/custom/custom_settings.h>
#include <dependencies/framework/custom/custom_colors.h>
#include <dependencies/framework/custom/fonts.h>
#include <dependencies/framework/custom/images.h>

#include <d3d11.h>
#include <tchar.h>
#include <dependencies/framework/d3d/d3dx11.h>
#pragma comment(lib, "d3dx11.lib")

ID3D11Device* D3DDevice;
ID3D11DeviceContext* D3DDeviceContext;
IDXGISwapChain* D3DSwapChain;
ID3D11RenderTargetView* D3DRenderTarget;
D3DPRESENT_PARAMETERS D3DPresentParams;
HWND hWindowHandle = 0;

#pragma comment(lib, "gdi32.lib")

enum ZBID
{
    ZBID_DEFAULT = 0,
    ZBID_DESKTOP = 1,
    ZBID_UIACCESS = 2,
    ZBID_IMMERSIVE_IHM = 3,
    ZBID_IMMERSIVE_NOTIFICATION = 4,
    ZBID_IMMERSIVE_APPCHROME = 5,
    ZBID_IMMERSIVE_MOGO = 6,
    ZBID_IMMERSIVE_EDGY = 7,
    ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
    ZBID_IMMERSIVE_INACTIVEDOCK = 9,
    ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
    ZBID_IMMERSIVE_ACTIVEDOCK = 11,
    ZBID_IMMERSIVE_BACKGROUND = 12,
    ZBID_IMMERSIVE_SEARCH = 13,
    ZBID_GENUINE_WINDOWS = 14,
    ZBID_IMMERSIVE_RESTRICTED = 15,
    ZBID_SYSTEM_TOOLS = 16,
    ZBID_LOCK = 17,
    ZBID_ABOVELOCK_UX = 18,
};

LRESULT CALLBACK TrashParentWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_CREATE:
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    case WM_WINDOWPOSCHANGING:
        return 0;

    case WM_CLOSE:
    {
        HANDLE myself;
        myself = OpenProcess( PROCESS_ALL_ACCESS, false, GetCurrentProcessId( ) );
        TerminateProcess( myself, 0 );
        return true;
    }

    default:
        break;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

HWND hwnd = NULL;
typedef HWND( WINAPI* CreateWindowInBand )( DWORD dwExStyle, ATOM atom, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam, DWORD band );

HWND CreateWin( HMODULE hModule, UINT zbid, const wchar_t* title, const wchar_t* classname )
{
    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

    HINSTANCE hInstance = hModule;

    WNDCLASSEX wndParentClass = { };
    wndParentClass.cbSize = sizeof( WNDCLASSEX );

    wndParentClass.cbClsExtra = 0;
    wndParentClass.hIcon = NULL;
    wndParentClass.lpszMenuName = NULL;
    wndParentClass.hIconSm = NULL;
    wndParentClass.lpfnWndProc = TrashParentWndProc;
    wndParentClass.hInstance = hInstance;
    wndParentClass.style = CS_HREDRAW | CS_VREDRAW;
    wndParentClass.hCursor = LoadCursor( 0, IDC_ARROW );
    wndParentClass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
    wndParentClass.lpszClassName = classname;

    auto res = RegisterClassEx( &wndParentClass );

    const auto hpath = LoadLibrary( _( L"user32.dll" ) );
    const auto pCreateWindowInBand = ( CreateWindowInBand )GetProcAddress( hpath, _( "CreateWindowInBand" ) ); 

    auto hwndParent = pCreateWindowInBand( WS_EX_LAYERED | WS_EX_TOOLWINDOW, res, title, WS_POPUP, 0, 0, GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ), NULL, NULL, hInstance, LPVOID( res ), zbid );

    SetLayeredWindowAttributes( hwndParent, RGB( 0, 0, 0 ), 255, LWA_ALPHA );

    SetWindowLongA( hwndParent, GWL_EXSTYLE, GetWindowLong( hwndParent, GWL_EXSTYLE ) | WS_EX_TRANSPARENT );

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea( hwndParent, &margins );

    ShowWindow( hwndParent, SW_SHOW );
    UpdateWindow( hwndParent );

    return hwndParent;
}

auto atx::render_c::Initialize( ) -> bool
{
    HINSTANCE hInstance = GetModuleHandle( nullptr );
    hWindowHandle = CreateWin( hInstance, ZBID_UIACCESS, _( L" " ), _( L"  " ) );

    DXGI_SWAP_CHAIN_DESC SwapChainDescription;
    ZeroMemory( &SwapChainDescription, sizeof( SwapChainDescription ) );
    SwapChainDescription.BufferCount = 2;
    SwapChainDescription.BufferDesc.Width = 0;
    SwapChainDescription.BufferDesc.Height = 0;
    SwapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
    SwapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDescription.OutputWindow = hWindowHandle;
    SwapChainDescription.SampleDesc.Count = 1;
    SwapChainDescription.SampleDesc.Quality = 0;
    SwapChainDescription.Windowed = 1;
    SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL d3d_feature_lvl;
    const D3D_FEATURE_LEVEL d3d_feature_array[ 2 ] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, d3d_feature_array, 2, D3D11_SDK_VERSION,
        &SwapChainDescription, &D3DSwapChain, &D3DDevice, &d3d_feature_lvl, &D3DDeviceContext );

    if ( FAILED( hr ) )
        return false;

    ID3D11Texture2D* pBackBuffer;
    hr = D3DSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
    if ( FAILED( hr ) )
        return false;

    hr = D3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &D3DRenderTarget );
    pBackBuffer->Release( );
    if ( FAILED( hr ) )
        return false;

    IMGUI_CHECKVERSION( );
    ImGui::CreateContext( );
    ImGuiIO& io = ImGui::GetIO( );
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    io.Fonts->AddFontFromFileTTF( _( "C:\\Windows\\Fonts\\corbelb.ttf" ), 11.f );

    // Load Fonts
    ImFontConfig cfg;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_Bitmap;
    settings::lexend_deca_medium = io.Fonts->AddFontFromMemoryTTF( lexend_deca_medium_binary, sizeof lexend_deca_medium_binary, 14.f, &cfg, io.Fonts->GetGlyphRangesCyrillic( ) );
    settings::lexend_deca_medium_widgets = io.Fonts->AddFontFromMemoryTTF( lexend_deca_medium_binary, sizeof lexend_deca_medium_binary, 12.f, &cfg, io.Fonts->GetGlyphRangesCyrillic( ) );
    settings::combo_expand = io.Fonts->AddFontFromMemoryTTF( icons_binary, sizeof icons_binary, 10.f, &cfg, io.Fonts->GetGlyphRangesCyrillic( ) );
    settings::pick_button = io.Fonts->AddFontFromMemoryTTF( icons_binary, sizeof icons_binary, 12.f, &cfg, io.Fonts->GetGlyphRangesCyrillic( ) );

    if ( !ImGui_ImplWin32_Init( hWindowHandle ) || !ImGui_ImplDX11_Init( D3DDevice, D3DDeviceContext ) )
        return false;

    return true;
}

auto LoadStyles( ) -> void
{
    ImGuiStyle* iStyle = &ImGui::GetStyle( );
    iStyle->FramePadding = { -1, -1 };
}

bool bShowMenu = true;
int iMenuTab = 0;
auto atx::render_c::RenderMenu( ) -> void
{
    if ( GetAsyncKeyState( VK_INSERT ) & 1 )
        bShowMenu = !bShowMenu;

    if ( !bShowMenu )
        return;

    LoadStyles( );

    ImGui::SetNextWindowSize( settings::window_size );
    ImGui::SetNextWindowPos( ImVec2( 1920 / 2 - 800 / 2, 1080 / 2 - 500 / 2 ), ImGuiCond_Once );
    ImGui::Begin( _("Menu"), nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground );
    {
        const ImVec2 pos = ImGui::GetWindowPos( );
        const ImVec2 size = ImGui::GetWindowSize( );
        ImDrawList* draw = ImGui::GetWindowDrawList( );
        ImGuiStyle* style = &ImGui::GetStyle( );

        // style
        {
            style->WindowPadding = settings::window_padding;
            style->WindowBorderSize = settings::window_border_size;
            style->WindowRounding = settings::window_rounding;
            style->ItemSpacing = settings::window_item_spacing;

            settings::tab_change_alpha = ImClamp( settings::tab_change_alpha + ( custom::FixedSpeed( 6.f ) * ( settings::tab_count == settings::tab_active ? 1.f : -1.f ) ), 0.f, 1.f );
            if ( settings::tab_change_alpha == 0.f )  settings::tab_active = settings::tab_count;
        }

        // decorations
        {
            draw->AddRectFilled( pos, pos + size, custom::GetColor( colors::window_background ), settings::window_rounding ); // background
            draw->AddRectFilled( pos + ImVec2( 20, 20 ), pos + ImVec2( 124, size.y - 20 ), custom::GetColor( colors::window_sidebar ), settings::window_rounding ); // sidebar
            //draw->AddImage( settings::logo_texture_dx11, pos + ImVec2( 40, 40 ), pos + ImVec2( 104, 104 ) ); // logotype

            // active tab checkmark
            draw->AddRectFilled( pos + ImVec2( 124, 138 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), pos + ImVec2( 144, 214 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), custom::GetColor( colors::window_sidebar, settings::tab_change_alpha ) );
            draw->AddRectFilled( pos + ImVec2( 124, 132 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), pos + ImVec2( 144, 144 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), custom::GetColor( colors::window_background ), 8.f );
            draw->AddRectFilled( pos + ImVec2( 124, 208 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), pos + ImVec2( 144, 220 + ( 64 + style->ItemSpacing.y ) * settings::tab_active ), custom::GetColor( colors::window_background ), 8.f );
        }

        // tabs
        {
            ImGui::SetCursorPos( ImVec2( 32.0f, 30 ) );
            ImGui::BeginGroup( );
            {
                ImGui::PushStyleColor( ImGuiCol_Button, ImColor( ).Value );
                ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImColor( ).Value );
                ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImColor( ).Value );

                ImGui::PushStyleColor( ImGuiCol_Text, settings::tab_count == 0 ? ImColor( 255, 255, 255 ).Value : ImColor( 180, 180, 180 ).Value );
                if ( ImGui::Button( settings::tab_labels[ 0 ], { 80, 50 } ) ) 
                    settings::tab_count = 0;

                ImGui::PushStyleColor( ImGuiCol_Text, settings::tab_count == 1 ? ImColor( 255, 255, 255 ).Value : ImColor( 180, 180, 180 ).Value );
                if ( ImGui::Button( settings::tab_labels[ 1 ], { 80, 50 } ) )
                    settings::tab_count = 1;

                ImGui::PopStyleColor( 5 );

                draw->AddText( pos + ImVec2( 50, size.y - 40 ), ImColor( 223, 192, 235 ), _( "Argon Private" ) );
            }
            ImGui::EndGroup( );
        } 

        // content
        {
            ImGui::SetCursorPos( ImVec2( 144, 20 ) );
            custom::BeginContent( );
            {
                if ( settings::tab_active == 0 )
                {
                    ImGui::BeginGroup( );
                    {
                        custom::BeginChild( _( "General" ) );
                        {
                            custom::Checkbox( _( "Mouse Aimbot" ), &atx::settings.bAimbot );
               
                            custom::Checkbox( _( "Prediction" ), &atx::settings.bPrediction );
                            custom::Checkbox( _( "Visible Check" ), &atx::settings.bVisibleCheck );
                            custom::Checkbox( _( "Humanization" ), &atx::settings.bHumanization );
                            custom::Checkbox( _( "Ignore Downed" ), &atx::settings.IgnoreDowned );
                            custom::Checkbox( _( "Render FOV" ), &atx::settings.bRenderFOV );
                            custom::Checkbox( _( "Triggerbot" ), &atx::settings.bTriggerbot );
                            if ( atx::settings.bTriggerbot )
                            {
                                custom::Checkbox( _( "Shotgun Only" ), &atx::settings.bShotgunOnly );
                                custom::Checkbox( _( "Ignore Keybind" ), &atx::settings.bIgnoreKeybind );
                            }
                        }
                        custom::EndChild( );
                    }
                    ImGui::EndGroup( );

                    ImGui::SameLine( );

                    ImGui::BeginGroup( );
                    {
                        custom::BeginChild( _("Settings") );
                        {
                            custom::SliderInt( _( "Field of View" ), &atx::settings.iFovRadius, 1, 180, "%d\xC2\xB0" );
                            custom::SliderInt( _( "Smooth" ), &atx::settings.iSmooth, 1, 20, "%d" );

                            const char* cHitBoxes[ 4 ] = { "Head", "Neck", "Chest", "Pelvis" };
                            custom::Combo( _( "Hitbox" ), &atx::settings.iHitBox, cHitBoxes, IM_ARRAYSIZE( cHitBoxes ) );

                            if ( atx::settings.bHumanization )
                            {
                                ImGui::TextColored( ImColor( 200, 200, 200 ), _( "Humanization:" ), 0 );
                                custom::SliderInt( _( "Mistake Size" ), &atx::settings.iMistakeSize, 1, 20, "%d px" );
                                custom::SliderInt( _( "Correction Delay" ), &atx::settings.iMistakeCorrection, 1, 20, "%d ms" );
                            }
                            if ( atx::settings.bTriggerbot )
                            {
                                ImGui::TextColored( ImColor(200, 200, 200), _( "Triggerbot:" ) );
                                custom::SliderInt( _( "Delay" ), &atx::settings.iCustomDelay, 50, 300, "%d ms" );
                            }
                            ImGui::TextColored( ImColor( 200, 200, 200 ), _( "Keybinds:" ), 0 );
                            custom::Keybind( _( "Triggerbot Keybind" ), &atx::settings.iTriggerbotKeybind, 0 );
                            custom::Keybind( _( "Aimbot Keybind" ), &atx::settings.iAimbotKeybind, 0 );
                        }
                        custom::EndChild( );
                    }
                    ImGui::EndGroup( );
                }
                else if ( settings::tab_active == 1 )
                {
                    ImGui::BeginGroup( );
                    {
                        custom::BeginChild( _( "General" ) );
                        {
                            custom::Checkbox( _( "Box" ), &atx::settings.bBox );

                            const char* cBoxTypes[ 2 ] = { "2D", "Cornered" };
                            custom::Combo( _( "Box Type" ), &atx::settings.iBoxType, cBoxTypes, IM_ARRAYSIZE( cBoxTypes ) );

                            custom::Checkbox( _( "Skeletons" ), &atx::settings.bSkeletons );
                            custom::Checkbox( _( "Username" ), &atx::settings.bUsername );
                            custom::Checkbox( _( "Distance" ), &atx::settings.bDistance );
                            custom::Checkbox( _( "Weapon" ), &atx::settings.bHeldWeapon );
                            custom::Checkbox( _( "Off Screen Indicator" ), &atx::settings.bOffScreenIndicator );
                        }
                        custom::EndChild( );
                    }
                    ImGui::EndGroup( );

                    ImGui::SameLine( );

                    ImGui::BeginGroup( );
                    {
                        custom::BeginChild( _( "Settings" ) );
                        {
                            custom::Checkbox( _( "Text Outline" ), &atx::settings.bTextOutline );
                            custom::Checkbox( _( "Box Outline" ), &atx::settings.bBoxOutline );
                            custom::SliderInt( _( "Render Distance" ), &atx::settings.iRenderDistance, 1, 270, "%d" );
                            custom::ColorEdit( _( "Visible Color" ), ( float* )&atx::settings.iVisibleColor, true );
                            custom::ColorEdit( _( "Invisible Color"), ( float* )&atx::settings.iInvisibleColor, true );
                        }
                        custom::EndChild( );
                    }
                    ImGui::EndGroup( );
                }
            }
            custom::EndContent( );
        }
    }
    ImGui::End( );
}

auto atx::render_c::RenderThread( ) -> void
{
    ImVec4 vClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    const float fClearColor[ 4 ] = { vClearColor.x * vClearColor.w, vClearColor.y * vClearColor.w, vClearColor.z * vClearColor.w, vClearColor.w };
    auto& io = ImGui::GetIO( );

    SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_HIGHEST );

    for ( ;; )
    {
        MSG msg;
        while ( PeekMessage( &msg, hWindowHandle, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );

            if ( msg.message == WM_QUIT )
                return;
        }

        io.DeltaTime = 1.0f / 165.f;
        io.ImeWindowHandle = hWindowHandle;

        POINT p_cursor;
        GetCursorPos( &p_cursor );
        io.MousePos.x = p_cursor.x;
        io.MousePos.y = p_cursor.y;

        io.MouseDown[ 0 ] = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
        io.MouseClicked[ 0 ] = io.MouseDown[ 0 ];

        io.WantCaptureMouse = io.WantCaptureKeyboard = io.WantCaptureMouse || io.WantCaptureKeyboard;

        ImGui_ImplDX11_NewFrame( );
        ImGui_ImplWin32_NewFrame( );
        ImGui::NewFrame( );
        {
            atx::visuals.ActorLoop( );

            if ( atx::settings.bRenderFOV )
                ImGui::GetBackgroundDrawList( )->AddCircle( ImVec2( atx::screen.fWidth / 2, atx::screen.fHeight / 2 ), atx::settings.iFovRadius * 10, ImColor( 255, 255, 255 ), 64, 1.f );

            this->RenderMenu( );

          
        }
        ImGui::Render( );
        D3DDeviceContext->OMSetRenderTargets( 1, &D3DRenderTarget, nullptr );
        D3DDeviceContext->ClearRenderTargetView( D3DRenderTarget, fClearColor );
        ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

        D3DSwapChain->Present( 1, 0 );
    }

    ImGui_ImplDX11_Shutdown( );
    ImGui_ImplWin32_Shutdown( );
    ImGui::DestroyContext( );

    if ( D3DRenderTarget )
        D3DRenderTarget->Release( );

    if ( D3DSwapChain )
        D3DSwapChain->Release( );

    if ( D3DDeviceContext )
        D3DDeviceContext->Release( );

    DestroyWindow( hWindowHandle );
}