#pragma once
#include "../imgui.h"
#include <d3d9.h>
#include <d3d11.h>

namespace settings
{
    inline ImVec2 window_size = ImVec2(800, 500);
    inline ImVec2 window_padding = ImVec2(0, 0);
    inline float window_border_size = 0.f;
    inline float window_rounding = 12.f;
    inline ImVec2 window_item_spacing = ImVec2(20, 20);

    inline ImVec2 content_size = ImVec2(636, 460);
    inline float content_rounding = 12.f;
    inline ImVec2 content_padding = ImVec2(13, 20);
    inline ImVec2 content_item_spacing = ImVec2(46, 30);

    inline int tab_count = 0;
    inline int tab_active = 0;
    inline float tab_rounding = 12.f;
    inline const char* tab_labels[4] = { "Aimbot", "Visuals", "Misc", "Settings" };
    inline IDirect3DTexture9* tab_textures_dx9[4];
    inline ID3D11ShaderResourceView* tab_textures_dx11[4];
    inline float tab_change_alpha = 0.f;

    inline float widgets_rounding = 8.f;
    inline ImVec2 widgets_spacing = ImVec2(5, 5);

    inline ImFont* lexend_deca_medium = nullptr;
    inline ImFont* lexend_deca_medium_widgets = nullptr;
    inline ImFont* combo_expand = nullptr;
    inline ImFont* pick_button = nullptr;

    inline IDirect3DTexture9* logo_texture_dx9 = nullptr;
    inline ID3D11ShaderResourceView* logo_texture_dx11 = nullptr;
}
