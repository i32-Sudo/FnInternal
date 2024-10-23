#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "../imgui.h"
#include "../imgui_internal.h"
#include "custom_colors.h"
#include "custom_settings.h"
#include <map>
#include <string>

namespace custom
{
    ImU32 GetColor(const ImVec4& col, float alpha = 1.f);
    inline float FixedSpeed(float speed) { return speed / ImGui::GetIO().Framerate; };

    bool Tab(const char* label, ImTextureID texture_id, int tab_id, int& tab_variable);
    bool BeginChild(const char* name, const ImVec2& size_arg = ImVec2(0, 0), ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    void EndChild();
    void BeginContent();
    void EndContent();
    bool Checkbox(const char* label, bool* callback);
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool Selectable(const char* label, bool* p_selected);
    bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1);
    void MultiCombo(const char* label, bool variable[], const char* labels[], int count);
    static ImVec2 InputTextCalcTextSizeW(ImGuiContext* ctx, const ImWchar* text_begin, const ImWchar* text_end, const ImWchar** remaining = NULL, ImVec2* out_offset = NULL, bool stop_on_new_line = false);
    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = 0, void* user_data = NULL);
    bool Button(const char* label, const ImVec2& size_arg = ImVec2(0.f, 0.f));
    bool ShortButton(const char* label);
    bool ColorEdit(const char* label, float col[4], bool alpha = true);
    bool Keybind(const char* label, int* key, int* mode);
}
