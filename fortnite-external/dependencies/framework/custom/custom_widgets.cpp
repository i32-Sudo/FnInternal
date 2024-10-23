#include "custom_widgets.h"
#include "../imstb_textedit.h"

using namespace ImGui;
using namespace ImStb;

namespace ImStb
{

    static int     STB_TEXTEDIT_STRINGLEN(const ImGuiInputTextState* obj) { return obj->CurLenW; }
    static ImWchar STB_TEXTEDIT_GETCHAR(const ImGuiInputTextState* obj, int idx) { IM_ASSERT(idx <= obj->CurLenW); return obj->TextW[idx]; }
    static float   STB_TEXTEDIT_GETWIDTH(ImGuiInputTextState* obj, int line_start_idx, int char_idx) { ImWchar c = obj->TextW[line_start_idx + char_idx]; if (c == '\n') return STB_TEXTEDIT_GETWIDTH_NEWLINE; ImGuiContext& g = *obj->Ctx; return g.Font->GetCharAdvance(c) * (g.FontSize / g.Font->FontSize); }
    static int     STB_TEXTEDIT_KEYTOTEXT(int key) { return key >= 0x200000 ? 0 : key; }
    static ImWchar STB_TEXTEDIT_NEWLINE = '\n';
    static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, ImGuiInputTextState* obj, int line_start_idx)
    {
        const ImWchar* text = obj->TextW.Data;
        const ImWchar* text_remaining = NULL;
        const ImVec2 size = custom::InputTextCalcTextSizeW(obj->Ctx, text + line_start_idx, text + obj->CurLenW, &text_remaining, NULL, true);
        r->x0 = 0.0f;
        r->x1 = size.x;
        r->baseline_y_delta = size.y;
        r->ymin = 0.0f;
        r->ymax = size.y;
        r->num_chars = (int)(text_remaining - (text + line_start_idx));
    }

    static bool is_separator(unsigned int c)
    {
        return c == ',' || c == ';' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' || c == '|' || c == '\n' || c == '\r' || c == '.' || c == '!';
    }

    static int is_word_boundary_from_right(ImGuiInputTextState* obj, int idx)
    {
        // When ImGuiInputTextFlags_Password is set, we don't want actions such as CTRL+Arrow to leak the fact that underlying data are blanks or separators.
        if ((obj->Flags & ImGuiInputTextFlags_Password) || idx <= 0)
            return 0;

        bool prev_white = ImCharIsBlankW(obj->TextW[idx - 1]);
        bool prev_separ = is_separator(obj->TextW[idx - 1]);
        bool curr_white = ImCharIsBlankW(obj->TextW[idx]);
        bool curr_separ = is_separator(obj->TextW[idx]);
        return ((prev_white || prev_separ) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
    }
    static int is_word_boundary_from_left(ImGuiInputTextState* obj, int idx)
    {
        if ((obj->Flags & ImGuiInputTextFlags_Password) || idx <= 0)
            return 0;

        bool prev_white = ImCharIsBlankW(obj->TextW[idx]);
        bool prev_separ = is_separator(obj->TextW[idx]);
        bool curr_white = ImCharIsBlankW(obj->TextW[idx - 1]);
        bool curr_separ = is_separator(obj->TextW[idx - 1]);
        return ((prev_white) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
    }
    static int  STB_TEXTEDIT_MOVEWORDLEFT_IMPL(ImGuiInputTextState* obj, int idx) { idx--; while (idx >= 0 && !is_word_boundary_from_right(obj, idx)) idx--; return idx < 0 ? 0 : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_MAC(ImGuiInputTextState* obj, int idx) { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_left(obj, idx)) idx++; return idx > len ? len : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_WIN(ImGuiInputTextState* obj, int idx) { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_right(obj, idx)) idx++; return idx > len ? len : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(ImGuiInputTextState* obj, int idx) { ImGuiContext& g = *obj->Ctx; if (g.IO.ConfigMacOSXBehaviors) return STB_TEXTEDIT_MOVEWORDRIGHT_MAC(obj, idx); else return STB_TEXTEDIT_MOVEWORDRIGHT_WIN(obj, idx); }
#define STB_TEXTEDIT_MOVEWORDLEFT   STB_TEXTEDIT_MOVEWORDLEFT_IMPL  // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

    static void STB_TEXTEDIT_DELETECHARS(ImGuiInputTextState* obj, int pos, int n)
    {
        ImWchar* dst = obj->TextW.Data + pos;

        // We maintain our buffer length in both UTF-8 and wchar formats
        obj->Edited = true;
        obj->CurLenA -= ImTextCountUtf8BytesFromStr(dst, dst + n);
        obj->CurLenW -= n;

        // Offset remaining text (FIXME-OPT: Use memmove)
        const ImWchar* src = obj->TextW.Data + pos + n;
        while (ImWchar c = *src++)
            *dst++ = c;
        *dst = '\0';
    }

    static bool STB_TEXTEDIT_INSERTCHARS(ImGuiInputTextState* obj, int pos, const ImWchar* new_text, int new_text_len)
    {
        const bool is_resizable = (obj->Flags & ImGuiInputTextFlags_CallbackResize) != 0;
        const int text_len = obj->CurLenW;
        IM_ASSERT(pos <= text_len);

        const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
        if (!is_resizable && (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufCapacityA))
            return false;

        // Grow internal buffer if needed
        if (new_text_len + text_len + 1 > obj->TextW.Size)
        {
            if (!is_resizable)
                return false;
            IM_ASSERT(text_len < obj->TextW.Size);
            obj->TextW.resize(text_len + ImClamp(new_text_len * 4, 32, ImMax(256, new_text_len)) + 1);
        }

        ImWchar* text = obj->TextW.Data;
        if (pos != text_len)
            memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos) * sizeof(ImWchar));
        memcpy(text + pos, new_text, (size_t)new_text_len * sizeof(ImWchar));

        obj->Edited = true;
        obj->CurLenW += new_text_len;
        obj->CurLenA += new_text_len_utf8;
        obj->TextW[obj->CurLenW] = '\0';

        return true;
    }

    // We don't use an enum so we can build even with conflicting symbols (if another user of stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT         0x200000 // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT        0x200001 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP           0x200002 // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN         0x200003 // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART    0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      0x200005 // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      0x200007 // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         0x20000A // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO         0x20000B // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT     0x20000C // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT    0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_PGUP         0x20000E // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN       0x20000F // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_SHIFT        0x400000

#define STB_TEXTEDIT_IMPLEMENTATION
#define STB_TEXTEDIT_memmove memmove
#include "../../imstb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but somehow, calling
// the stb_textedit_paste() function creates two separate records, so we perform it manually. (FIXME: Report to nothings/stb?)
    static void stb_textedit_replace(ImGuiInputTextState* str, STB_TexteditState* state, const STB_TEXTEDIT_CHARTYPE* text, int text_len)
    {
        stb_text_makeundo_replace(str, state, 0, str->CurLenW, text_len);
        ImStb::STB_TEXTEDIT_DELETECHARS(str, 0, str->CurLenW);
        state->cursor = state->select_start = state->select_end = 0;
        if (text_len <= 0)
            return;
        if (ImStb::STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len))
        {
            state->cursor = state->select_start = state->select_end = text_len;
            state->has_preferred_x = 0;
            return;
        }
        IM_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we currently use stb_textedit_replace()
    }

} // namespace ImStb

namespace custom
{
    static inline ImDrawFlags FixRectCornerFlags(ImDrawFlags flags)
    {
        /*
        IM_STATIC_ASSERT(ImDrawFlags_RoundCornersTopLeft == (1 << 4));
    #ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        // Obsoleted in 1.82 (from February 2021). This code was stripped/simplified and mostly commented in 1.90 (from September 2023)
        // - Legacy Support for hard coded ~0 (used to be a suggested equivalent to ImDrawCornerFlags_All)
        if (flags == ~0)                    { return ImDrawFlags_RoundCornersAll; }
        // - Legacy Support for hard coded 0x01 to 0x0F (matching 15 out of 16 old flags combinations). Read details in older version of this code.
        if (flags >= 0x01 && flags <= 0x0F) { return (flags << 4); }
        // We cannot support hard coded 0x00 with 'float rounding > 0.0f' --> replace with ImDrawFlags_RoundCornersNone or use 'float rounding = 0.0f'
    #endif
        */
        // If this assert triggers, please update your code replacing hardcoded values with new ImDrawFlags_RoundCorners* values.
        // Note that ImDrawFlags_Closed (== 0x01) is an invalid flag for AddRect(), AddRectFilled(), PathRect() etc. anyway.
        // See details in 1.82 Changelog as well as 2021/03/12 and 2023/09/08 entries in "API BREAKING CHANGES" section.
        IM_ASSERT((flags & 0x0F) == 0 && "Misuse of legacy hardcoded ImDrawCornerFlags values!");

        if ((flags & ImDrawFlags_RoundCornersMask_) == 0)
            flags |= ImDrawFlags_RoundCornersAll;

        return flags;
    }

    void AddRectFilledMultiColor(ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left, float rounding, ImDrawFlags flags = 0)
    {
        if (((col_upr_left | col_upr_right | col_bot_right | col_bot_left) & IM_COL32_A_MASK) == 0)
            return;

        flags = FixRectCornerFlags(flags);
        rounding = ImMin(rounding, ImFabs(p_max.x - p_min.x) * (((flags & ImDrawFlags_RoundCornersTop) == ImDrawFlags_RoundCornersTop) || ((flags & ImDrawFlags_RoundCornersBottom) == ImDrawFlags_RoundCornersBottom) ? 0.5f : 1.0f) - 1.0f);
        rounding = ImMin(rounding, ImFabs(p_max.y - p_min.y) * (((flags & ImDrawFlags_RoundCornersLeft) == ImDrawFlags_RoundCornersLeft) || ((flags & ImDrawFlags_RoundCornersRight) == ImDrawFlags_RoundCornersRight) ? 0.5f : 1.0f) - 1.0f);

        // https://github.com/ocornut/imgui/issues/3710#issuecomment-758555966
        if (rounding > 0.0f)
        {
            const int size_before = draw->VtxBuffer.Size;
            draw->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
            const int size_after = draw->VtxBuffer.Size;

            for (int i = size_before; i < size_after; i++)
            {
                ImDrawVert* vert = draw->VtxBuffer.Data + i;

                ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4(col_upr_left);
                ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4(col_bot_left);
                ImVec4 up_right = ImGui::ColorConvertU32ToFloat4(col_upr_right);
                ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4(col_bot_right);

                float X = ImClamp((vert->pos.x - p_min.x) / (p_max.x - p_min.x), 0.0f, 1.0f);

                // 4 colors - 8 deltas

                float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
                float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

                float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
                float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

                float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
                float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

                float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
                float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


                float Y = ImClamp((vert->pos.y - p_min.y) / (p_max.y - p_min.y), 0.0f, 1.0f);
                float r = r1 + (r2 - r1) * Y;
                float g = g1 + (g2 - g1) * Y;
                float b = b1 + (b2 - b1) * Y;
                float a = a1 + (a2 - a1) * Y;
                ImVec4 RGBA(r, g, b, a);

                RGBA = RGBA * ImGui::ColorConvertU32ToFloat4(vert->col);

                vert->col = ImColor(RGBA);
            }
            return;
        }

        const ImVec2 uv = draw->_Data->TexUvWhitePixel;
        draw->PrimReserve(6, 4);
        draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 1)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2));
        draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 3));
        draw->PrimWriteVtx(p_min, uv, col_upr_left);
        draw->PrimWriteVtx(ImVec2(p_max.x, p_min.y), uv, col_upr_right);
        draw->PrimWriteVtx(p_max, uv, col_bot_right);
        draw->PrimWriteVtx(ImVec2(p_min.x, p_max.y), uv, col_bot_left);
    }

    ImU32 GetColor(const ImVec4& col, float alpha)
    {
        ImGuiStyle& style = GImGui->Style;
        ImVec4 c = col;
        c.w *= style.Alpha * alpha;
        return ColorConvertFloat4ToU32(c);
    }

    bool Tab(const char* label, ImTextureID texture_id, int tab_id, int& tab_variable)
    {
        struct tab_state
        {
            float background_alpha = 0.f;
            ImVec4 icon_color = colors::tab_icon;
            bool clicked = false;
            ImVec2 icon_squeeze = ImVec2(0, 0);
        };

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const bool selected = tab_id == tab_variable;

        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 icon_padding(17, 17);

        const ImRect rect(pos, pos + ImVec2(64, 64));
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);
        if (pressed)
            tab_variable = tab_id;

        static std::map<ImGuiID, tab_state> anim;
        tab_state& state = anim[id];

        state.background_alpha = ImClamp(state.background_alpha + (FixedSpeed(4.f) * (selected ? 1.f : -1.f)), 0.f, 1.f);
        state.icon_color = ImLerp(state.icon_color, selected ? colors::accent : colors::tab_icon, FixedSpeed(8.f));
        state.icon_squeeze = ImLerp(state.icon_squeeze, state.clicked ? ImVec2(2, 2) : ImVec2(0, 0), FixedSpeed(12.f));

        if (pressed && !selected)
            state.clicked = true;
        if (state.icon_squeeze.x >= 1.9f)
            state.clicked = false;

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::tab_background, state.background_alpha), settings::tab_rounding);
        window->DrawList->AddImageRounded(texture_id, rect.Min + icon_padding + state.icon_squeeze, rect.Max - icon_padding - state.icon_squeeze, ImVec2(0, 0), ImVec2(1, 1), GetColor(state.icon_color), 1.f);

        return pressed;
    }

    bool BeginChild(const char* name, const ImVec2& size_arg, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* parent_window = g.CurrentWindow;
        ImGuiID id = GetCurrentWindow()->GetID(name);
        IM_ASSERT(id != 0);

        // Sanity check as it is likely that some user will accidentally pass ImGuiWindowFlags into the ImGuiChildFlags argument.
        const ImGuiChildFlags ImGuiChildFlags_SupportedMask_ = ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_FrameStyle;
        IM_UNUSED(ImGuiChildFlags_SupportedMask_);
        IM_ASSERT((child_flags & ~ImGuiChildFlags_SupportedMask_) == 0 && "Illegal ImGuiChildFlags value. Did you pass ImGuiWindowFlags values instead of ImGuiChildFlags?");
        IM_ASSERT((window_flags & ImGuiWindowFlags_AlwaysAutoResize) == 0 && "Cannot specify ImGuiWindowFlags_AlwaysAutoResize for BeginChild(). Use ImGuiChildFlags_AlwaysAutoResize!");
        if (child_flags & ImGuiChildFlags_AlwaysAutoResize)
        {
            IM_ASSERT((child_flags & (ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY)) == 0 && "Cannot use ImGuiChildFlags_ResizeX or ImGuiChildFlags_ResizeY with ImGuiChildFlags_AlwaysAutoResize!");
            IM_ASSERT((child_flags & (ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) != 0 && "Must use ImGuiChildFlags_AutoResizeX or ImGuiChildFlags_AutoResizeY with ImGuiChildFlags_AlwaysAutoResize!");
        }
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        if (window_flags & ImGuiWindowFlags_AlwaysUseWindowPadding)
            child_flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
#endif
        if (child_flags & ImGuiChildFlags_AutoResizeX)
            child_flags &= ~ImGuiChildFlags_ResizeX;
        if (child_flags & ImGuiChildFlags_AutoResizeY)
            child_flags &= ~ImGuiChildFlags_ResizeY;

        // Set window flags
        window_flags |= ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoTitleBar;
        window_flags |= (parent_window->Flags & ImGuiWindowFlags_NoMove); // Inherit the NoMove flag
        if (child_flags & (ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize))
            window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
        if ((child_flags & (ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY)) == 0)
            window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

        // Special framed style
        if (child_flags & ImGuiChildFlags_FrameStyle)
        {
            PushStyleColor(ImGuiCol_ChildBg, g.Style.Colors[ImGuiCol_FrameBg]);
            PushStyleVar(ImGuiStyleVar_ChildRounding, g.Style.FrameRounding);
            PushStyleVar(ImGuiStyleVar_ChildBorderSize, g.Style.FrameBorderSize);
            PushStyleVar(ImGuiStyleVar_WindowPadding, g.Style.FramePadding);
            child_flags |= ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding;
            window_flags |= ImGuiWindowFlags_NoMove;
        }

        // Forward child flags
        g.NextWindowData.Flags |= ImGuiNextWindowDataFlags_HasChildFlags;
        g.NextWindowData.ChildFlags = child_flags;

        // Forward size
        // Important: Begin() has special processing to switch condition to ImGuiCond_FirstUseEver for a given axis when ImGuiChildFlags_ResizeXXX is set.
        // (the alternative would to store conditional flags per axis, which is possible but more code)
        ImVec2 size = size_arg;
        const float topbar_height = 24.f;

        static std::map<ImGuiID, float> child_height;
        float& state = child_height[id];

        if (size.x <= 0)
            size.x = ((GetWindowWidth() - (g.Style.WindowPadding.x * 2) - g.Style.ItemSpacing.x)) / 2;
        if (size.y <= 0)
            size.y = state;

        SetNextWindowSize(size);
        SetNextWindowPos(parent_window->DC.CursorPos + ImVec2(0, topbar_height));
        // Build up name. If you need to append to a same child from multiple location in the ID stack, use BeginChild(ImGuiID id) with a stable value.
        // FIXME: 2023/11/14: commented out shorted version. We had an issue with multiple ### in child window path names, which the trailing hash helped workaround.
        // e.g. "ParentName###ParentIdentifier/ChildName###ChildIdentifier" would get hashed incorrectly by ImHashStr(), trailing _%08X somehow fixes it.
        const char* temp_window_name;
        /*if (name && parent_window->IDStack.back() == parent_window->ID)
            ImFormatStringToTempBuffer(&temp_window_name, NULL, "%s/%s", parent_window->Name, name); // May omit ID if in root of ID stack
        else*/
        if (name)
            ImFormatStringToTempBuffer(&temp_window_name, NULL, "%s/%s_%08X", parent_window->Name, name, id);
        else
            ImFormatStringToTempBuffer(&temp_window_name, NULL, "%s/%08X", parent_window->Name, id);

        // Set style
        const float backup_border_size = g.Style.ChildBorderSize;
        if ((child_flags & ImGuiChildFlags_Border) == 0)
            g.Style.ChildBorderSize = 0.0f;

        parent_window->DrawList->AddText(parent_window->DC.CursorPos - ImVec2(0, 4), GetColor(colors::child_label), name);

        // Begin into window
        PushStyleVar(ImGuiStyleVar_ChildRounding, 0.f);
        const bool ret = Begin(temp_window_name, NULL, window_flags);

        // Restore style
        g.Style.ChildBorderSize = backup_border_size;
        if (child_flags & ImGuiChildFlags_FrameStyle)
        {
            PopStyleVar(3);
            PopStyleColor();
        }

        ImGuiWindow* child_window = g.CurrentWindow;
        child_window->ChildId = id;
        state = child_window->ContentSize.y;

        // Set the cursor to handle case where the user called SetNextWindowPos()+BeginChild() manually.
        // While this is not really documented/defined, it seems that the expected thing to do.
        if (child_window->BeginCount == 1)
            parent_window->DC.CursorPos = child_window->Pos;

        // Process navigation-in immediately so NavInit can run on first frame
        // Can enter a child if (A) it has navigable items or (B) it can be scrolled.
        const ImGuiID temp_id_for_activation = ImHashStr("##Child", 0, id);
        if (g.ActiveId == temp_id_for_activation)
            ClearActiveID();
        if (g.NavActivateId == id && !(window_flags & ImGuiWindowFlags_NavFlattened) && (child_window->DC.NavLayersActiveMask != 0 || child_window->DC.NavWindowHasScrollY))
        {
            FocusWindow(child_window);
            NavInitWindow(child_window, false);
            SetActiveID(temp_id_for_activation, child_window); // Steal ActiveId with another arbitrary id so that key-press won't activate child item
            g.ActiveIdSource = g.NavInputSource;
        }
        PushStyleVar(ImGuiStyleVar_ItemSpacing, settings::widgets_spacing);
        return ret;
    }

    void EndChild()
    {
        PopStyleVar();
        ImGuiContext& g = *GImGui;
        ImGuiWindow* child_window = g.CurrentWindow;

        IM_ASSERT(g.WithinEndChild == false);
        IM_ASSERT(child_window->Flags & ImGuiWindowFlags_ChildWindow);   // Mismatched BeginChild()/EndChild() calls

        g.WithinEndChild = true;
        ImVec2 child_size = child_window->Size;
        End();
        if (child_window->BeginCount == 1)
        {
            ImGuiWindow* parent_window = g.CurrentWindow;
            ImRect bb(parent_window->DC.CursorPos, parent_window->DC.CursorPos + child_size);
            ItemSize(child_size);
            if ((child_window->DC.NavLayersActiveMask != 0 || child_window->DC.NavWindowHasScrollY) && !(child_window->Flags & ImGuiWindowFlags_NavFlattened))
            {
                ItemAdd(bb, child_window->ChildId);
                RenderNavHighlight(bb, child_window->ChildId);

                // When browsing a window that has no activable items (scroll only) we keep a highlight on the child (pass g.NavId to trick into always displaying)
                if (child_window->DC.NavLayersActiveMask == 0 && child_window == g.NavWindow)
                    RenderNavHighlight(ImRect(bb.Min - ImVec2(2, 2), bb.Max + ImVec2(2, 2)), g.NavId, ImGuiNavHighlightFlags_TypeThin);
            }
            else
            {
                // Not navigable into
                ItemAdd(bb, 0);

                // But when flattened we directly reach items, adjust active layer mask accordingly
                if (child_window->Flags & ImGuiWindowFlags_NavFlattened)
                    parent_window->DC.NavLayersActiveMaskNext |= child_window->DC.NavLayersActiveMaskNext;
            }
            if (g.HoveredWindow == child_window)
                g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
        }
        g.WithinEndChild = false;
        g.LogLinePosY = -FLT_MAX; // To enforce a carriage return
        PopStyleVar();
    }

    void BeginContent()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, custom::GetColor(colors::content_background));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, settings::content_rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, settings::content_padding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, settings::content_item_spacing);
        ImGui::BeginChild("content area", settings::content_size, 0, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, settings::tab_change_alpha);
	}

    void EndContent()
    {
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    bool Checkbox(const char* label, bool* callback)
    {
        struct checkbox_state
        {
            ImVec4 label_color = colors::checkbox_label_inactive;
            ImVec4 rect_color = colors::checkbox_rect;
            float cirlce_offset = 0.f;
        };

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        const ImRect rect(pos, pos + ImVec2(width, 40));
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id))
        {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);
        if (pressed)
        {
            *callback = !(*callback);
            MarkItemEdited(id);
        }

        static std::map<ImGuiID, checkbox_state> anim;
        checkbox_state& state = anim[id];

        state.label_color = ImLerp(state.label_color, *callback ? colors::checkbox_label_active : colors::checkbox_label_inactive, FixedSpeed(8.f));
        state.rect_color = ImLerp(state.rect_color, *callback ? colors::accent : colors::checkbox_rect, FixedSpeed(8.f));
        state.cirlce_offset = ImLerp(state.cirlce_offset, *callback ? 13.f : 0.f, FixedSpeed(8.f));

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::checkbox_background), settings::widgets_rounding);
        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(state.label_color), label);

        window->DrawList->AddRectFilled(ImVec2(rect.Max.x - 42, rect.Min.y + 14), rect.Max - ImVec2(13, 14), GetColor(state.rect_color), 6.f);
        window->DrawList->AddCircleFilled(ImVec2(rect.Max.x - 34 + state.cirlce_offset, rect.Min.y + 20), 8.f, GetColor(colors::checkbox_background));
        window->DrawList->AddCircle(ImVec2(rect.Max.x - 34 + state.cirlce_offset, rect.Min.y + 20), 8.f, GetColor(state.rect_color), 30, 2.f);

        return pressed;
    }

    bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
    {
        struct slider_state
        {
            float grab_slide = 0.f;
            float circle_size = 5.f;
            bool clicked = false;
        };

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        const ImRect rect(pos, pos + ImVec2(width, 40));
        const ImRect clickable(pos + ImVec2(width - 126, 22), pos + ImVec2(width - 12, 36));
        const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id, &clickable, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
            return false;

        // Default format string when passing NULL
        if (format == NULL)
            format = DataTypeGetInfo(data_type)->PrintFmt;

        const bool hovered = ItemHoverable(clickable, id, g.LastItemData.InFlags);
        bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
        if (!temp_input_is_active)
        {
            // Tabbing or CTRL-clicking on Slider turns it into an input box
            const bool input_requested_by_tabbing = temp_input_allowed && (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
            const bool clicked = hovered && IsMouseClicked(0, id);
            const bool make_active = (input_requested_by_tabbing || clicked || g.NavActivateId == id);
            if (make_active && clicked)
                SetKeyOwner(ImGuiKey_MouseLeft, id);
            if (make_active && temp_input_allowed)
                if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                    temp_input_is_active = true;

            if (make_active && !temp_input_is_active)
            {
                SetActiveID(id, window);
                SetFocusID(id, window);
                FocusWindow(window);
                g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            }
        }

        // Slider behavior
        ImRect grab_bb;
        const bool value_changed = SliderBehavior(ImRect(clickable.Min - ImVec2(3, 0), clickable.Max + ImVec2(3, 0)), id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
        if (value_changed)
            MarkItemEdited(id);

        // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
        char value_buf[64];
        const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);

        static std::map<ImGuiID, slider_state> anim;
        slider_state& state = anim[id];

        state.grab_slide = ImLerp(state.grab_slide, grab_bb.Min.x - clickable.Min.x, FixedSpeed(20.f));
        state.circle_size = ImLerp(state.circle_size, state.clicked ? 6.f : 5.f, FixedSpeed(8.f));
        state.clicked = IsItemActive();

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::checkbox_background), settings::widgets_rounding);
        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(colors::slider_label), label);

        window->DrawList->AddRectFilled(clickable.Min + ImVec2(0, 4), clickable.Max - ImVec2(0, 4), GetColor(colors::slider_rect), 2.f);
        window->DrawList->AddRectFilled(clickable.Min + ImVec2(0, 4), ImVec2(clickable.Min.x + state.grab_slide + 6, clickable.Max.y - 4), GetColor(colors::accent), 2.f);
        window->DrawList->AddCircleFilled(clickable.Min + ImVec2(state.grab_slide + 6, 7), state.circle_size, GetColor(colors::slider_background));
        window->DrawList->AddCircle(clickable.Min + ImVec2(state.grab_slide + 6, 7), state.circle_size, GetColor(colors::accent), 30, 1.5f);

        PushFont(settings::lexend_deca_medium_widgets);
        PushStyleColor(ImGuiCol_Text, GetColor(colors::slider_value));
        RenderTextClipped(rect.Min + ImVec2(0, 7), rect.Max - ImVec2(12, 0), value_buf, value_buf_end, NULL, ImVec2(1.f, 0.f));
        PopStyleColor();
        PopFont();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0));
        return value_changed;
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
    }

    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
    }

    bool SelectableEx(const char* label, bool active)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const float width = GetWindowWidth();
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect rect(pos, pos + ImVec2(width, 24));
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id))
            return false;

        bool hovered = IsItemHovered();
        bool pressed = hovered && g.IO.MouseClicked[0];
        if (pressed)
            MarkItemEdited(id);

        static std::map<ImGuiID, ImVec4> anim;
        ImVec4& state = anim[id];

        state = ImLerp(state, active ? colors::selectable_label_active : colors::selectable_label_inactive, FixedSpeed(4.f));

        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(5, 5), GetColor(state), label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool Selectable(const char* label, bool* p_selected)
    {
        if (SelectableEx(label, *p_selected))
        {
            *p_selected = !*p_selected;
            return true;
        }
        return false;
    }

    static float CalcMaxPopupHeightFromItemCount(int items_count, float item_size)
    {
        ImGuiContext& g = *GImGui;
        if (items_count <= 0)
            return FLT_MAX;
        return item_size * items_count + g.Style.ItemSpacing.y * (items_count - 1);
    }

    bool BeginCombo(const char* label, const char* preview_value, int val, ImGuiComboFlags flags, bool multi)
    {
        struct combo_state
        {
            bool combo_opened = false;
            bool hovered = false;
            float alpha = 0.f;
        };

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();

        ImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.Flags;
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
        if (window->SkipItems)
            return false;

        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together
        if (flags & ImGuiComboFlags_WidthFitPreview)
            IM_ASSERT((flags & (ImGuiComboFlags_NoPreview | ImGuiComboFlags_CustomPreview)) == 0);

        const ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        const ImRect rect(pos, pos + ImVec2(width, 40));
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id))
            return false;

        // Open on click
        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);

        static std::map<ImGuiID, combo_state> anim;
        combo_state& state = anim[id];

        if (hovered && g.IO.MouseClicked[0] || state.combo_opened && g.IO.MouseClicked[0] && !state.hovered)
            state.combo_opened = !state.combo_opened;

        state.alpha = ImClamp(state.alpha + (FixedSpeed(8.f) * (state.combo_opened ? 1.f : -1.f)), 0.f, 1.f);

        if (!IsRectVisible(rect.Min, rect.Max + ImVec2(0, 2)))
        {
            state.combo_opened = false;
            state.alpha = 0.f;
        }

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::combo_background), settings::widgets_rounding);
        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(colors::combo_label), label);
        window->DrawList->AddRectFilled(ImVec2(rect.Max.x - 126, rect.Min.y + 8), rect.Max - ImVec2(12, 8), GetColor(colors::combo_rect), 4.f, state.combo_opened ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);

        PushFont(settings::lexend_deca_medium_widgets);
        PushStyleColor(ImGuiCol_Text, GetColor(colors::combo_value));
        RenderTextClipped(ImVec2(rect.Max.x - 120, rect.Min.y + 7), rect.Max - ImVec2(25, 9), preview_value, NULL, NULL, ImVec2(0.f, 0.5f));
        PopStyleColor();
        PopFont();

        window->DrawList->AddRectFilledMultiColor(ImVec2(rect.Max.x - 120, rect.Min.y + 8), rect.Max - ImVec2(18, 8), GetColor(colors::combo_rect, 0.f), GetColor(colors::combo_rect), GetColor(colors::combo_rect), GetColor(colors::combo_rect, 0.f));
        window->DrawList->AddText(settings::combo_expand, settings::combo_expand->FontSize, rect.Max - ImVec2(27, 25), GetColor(colors::combo_expand), "A");

        if (!state.combo_opened && state.alpha < 0.1f)
            return false;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;

        PushStyleVar(ImGuiStyleVar_Alpha, state.alpha);
        PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        SetNextWindowPos(rect.GetBR() - ImVec2(126, 8));
        SetNextWindowSize(ImVec2(114, CalcMaxPopupHeightFromItemCount(val, 24.f)));
        Begin(label, NULL, window_flags);
        {

            GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), GetColor(colors::combo_rect), 4.f, ImDrawFlags_RoundCornersBottom);

            state.hovered = IsWindowHovered();

            if (!multi)
                if (IsWindowHovered() && g.IO.MouseClicked[0])
                    state.combo_opened = false;
        }
        return true;
    }

    void EndCombo()
    {
        PopStyleVar(3);
        End();
    }

    static const char* Items_ArrayGetter(void* data, int idx)
    {
        const char* const* items = (const char* const*)data;
        return items[idx];
    }

    bool Combo(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int popup_max_height_in_items)
    {
        ImGuiContext& g = *GImGui;

        // Call the getter to obtain the preview string which is a parameter to BeginCombo()
        const char* preview_value = NULL;
        if (*current_item >= 0 && *current_item < items_count)
            preview_value = getter(user_data, *current_item);

        // The old Combo() API exposed "popup_max_height_in_items". The new more general BeginCombo() API doesn't have/need it, but we emulate it here.
        if (popup_max_height_in_items != -1 && !(g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint))
            SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items, 24.f)));

        if (!BeginCombo(label, preview_value, items_count, ImGuiComboFlags_None, false))
            return false;

        // Display items
        // FIXME-OPT: Use clipper (but we need to disable it on the appearing frame to make sure our call to SetItemDefaultFocus() is processed)
        bool value_changed = false;
        for (int i = 0; i < items_count; i++)
        {
            const char* item_text = getter(user_data, i);
            if (item_text == NULL)
                item_text = "*Unknown item*";

            PushID(i);
            const bool item_selected = (i == *current_item);
            if (SelectableEx(item_text, item_selected) && *current_item != i)
            {
                value_changed = true;
                *current_item = i;
            }
            if (item_selected)
                SetItemDefaultFocus();
            PopID();
        }

        EndCombo();

        if (value_changed)
            MarkItemEdited(g.LastItemData.ID);

        return value_changed;
    }

    bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items)
    {
        const bool value_changed = Combo(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_in_items);
        return value_changed;
    }

    void MultiCombo(const char* label, bool variable[], const char* labels[], int count)
    {
        ImGuiContext& g = *GImGui;

        std::string preview = "Select";

        for (auto i = 0, j = 0; i < count; i++)
        {
            if (variable[i])
            {
                if (j)
                    preview += (", ") + (std::string)labels[i];
                else
                    preview = labels[i];

                j++;
            }
        }

        if (BeginCombo(label, preview.c_str(), count, 0, true))
        {
            for (auto i = 0; i < count; i++)
                Selectable(labels[i], &variable[i]);
            EndCombo();
        }

        preview = ("Select");
    }

    static int InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end)
    {
        int line_count = 0;
        const char* s = text_begin;
        while (char c = *s++) // We are only matching for \n so we can ignore UTF-8 decoding
            if (c == '\n')
                line_count++;
        s--;
        if (s[0] != '\n' && s[0] != '\r')
            line_count++;
        *out_text_end = s;
        return line_count;
    }

    static ImVec2 InputTextCalcTextSizeW(ImGuiContext* ctx, const ImWchar* text_begin, const ImWchar* text_end, const ImWchar** remaining, ImVec2* out_offset, bool stop_on_new_line)
    {
        ImGuiContext& g = *ctx;
        ImFont* font = g.Font;
        const float line_height = g.FontSize;
        const float scale = line_height / font->FontSize;

        ImVec2 text_size = ImVec2(0, 0);
        float line_width = 0.0f;

        const ImWchar* s = text_begin;
        while (s < text_end)
        {
            unsigned int c = (unsigned int)(*s++);
            if (c == '\n')
            {
                text_size.x = ImMax(text_size.x, line_width);
                text_size.y += line_height;
                line_width = 0.0f;
                if (stop_on_new_line)
                    break;
                continue;
            }
            if (c == '\r')
                continue;

            const float char_width = font->GetCharAdvance((ImWchar)c) * scale;
            line_width += char_width;
        }

        if (text_size.x < line_width)
            text_size.x = line_width;

        if (out_offset)
            *out_offset = ImVec2(line_width, text_size.y + line_height);  // offset allow for the possibility of sitting after a trailing \n

        if (line_width > 0 || text_size.y == 0.0f)                        // whereas size.y will ignore the trailing \n
            text_size.y += line_height;

        if (remaining)
            *remaining = s;

        return text_size;
    }

    static bool InputTextFilterCharacter(ImGuiContext* ctx, unsigned int* p_char, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data, ImGuiInputSource input_source)
    {
        IM_ASSERT(input_source == ImGuiInputSource_Keyboard || input_source == ImGuiInputSource_Clipboard);
        unsigned int c = *p_char;

        // Filter non-printable (NB: isprint is unreliable! see #2467)
        bool apply_named_filters = true;
        if (c < 0x20)
        {
            bool pass = false;
            pass |= (c == '\n' && (flags & ImGuiInputTextFlags_Multiline)); // Note that an Enter KEY will emit \r and be ignored (we poll for KEY in InputText() code)
            pass |= (c == '\t' && (flags & ImGuiInputTextFlags_AllowTabInput));
            if (!pass)
                return false;
            apply_named_filters = false; // Override named filters below so newline and tabs can still be inserted.
        }

        if (input_source != ImGuiInputSource_Clipboard)
        {
            // We ignore Ascii representation of delete (emitted from Backspace on OSX, see #2578, #2817)
            if (c == 127)
                return false;

            // Filter private Unicode range. GLFW on OSX seems to send private characters for special keys like arrow keys (FIXME)
            if (c >= 0xE000 && c <= 0xF8FF)
                return false;
        }

        // Filter Unicode ranges we are not handling in this build
        if (c > IM_UNICODE_CODEPOINT_MAX)
            return false;

        // Generic named filters
        if (apply_named_filters && (flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsScientific)))
        {
            // The libc allows overriding locale, with e.g. 'setlocale(LC_NUMERIC, "de_DE.UTF-8");' which affect the output/input of printf/scanf to use e.g. ',' instead of '.'.
            // The standard mandate that programs starts in the "C" locale where the decimal point is '.'.
            // We don't really intend to provide widespread support for it, but out of empathy for people stuck with using odd API, we support the bare minimum aka overriding the decimal point.
            // Change the default decimal_point with:
            //   ImGui::GetIO()->PlatformLocaleDecimalPoint = *localeconv()->decimal_point;
            // Users of non-default decimal point (in particular ',') may be affected by word-selection logic (is_word_boundary_from_right/is_word_boundary_from_left) functions.
            ImGuiContext& g = *ctx;
            const unsigned c_decimal_point = (unsigned int)g.IO.PlatformLocaleDecimalPoint;
            if (flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsScientific))
                if (c == '.' || c == ',')
                    c = c_decimal_point;

            // Full-width -> half-width conversion for numeric fields (https://en.wikipedia.org/wiki/Halfwidth_and_Fullwidth_Forms_(Unicode_block)
            // While this is mostly convenient, this has the side-effect for uninformed users accidentally inputting full-width characters that they may
            // scratch their head as to why it works in numerical fields vs in generic text fields it would require support in the font.
            if (flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_CharsHexadecimal))
                if (c >= 0xFF01 && c <= 0xFF5E)
                    c = c - 0xFF01 + 0x21;

            // Allow 0-9 . - + * /
            if (flags & ImGuiInputTextFlags_CharsDecimal)
                if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/'))
                    return false;

            // Allow 0-9 . - + * / e E
            if (flags & ImGuiInputTextFlags_CharsScientific)
                if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/') && (c != 'e') && (c != 'E'))
                    return false;

            // Allow 0-9 a-F A-F
            if (flags & ImGuiInputTextFlags_CharsHexadecimal)
                if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                    return false;

            // Turn a-z into A-Z
            if (flags & ImGuiInputTextFlags_CharsUppercase)
                if (c >= 'a' && c <= 'z')
                    c += (unsigned int)('A' - 'a');

            if (flags & ImGuiInputTextFlags_CharsNoBlank)
                if (ImCharIsBlankW(c))
                    return false;

            *p_char = c;
        }

        // Custom callback filter
        if (flags & ImGuiInputTextFlags_CallbackCharFilter)
        {
            ImGuiContext& g = *GImGui;
            ImGuiInputTextCallbackData callback_data;
            callback_data.Ctx = &g;
            callback_data.EventFlag = ImGuiInputTextFlags_CallbackCharFilter;
            callback_data.EventChar = (ImWchar)c;
            callback_data.Flags = flags;
            callback_data.UserData = user_data;
            if (callback(&callback_data) != 0)
                return false;
            *p_char = callback_data.EventChar;
            if (!callback_data.EventChar)
                return false;
        }

        return true;
    }

    static void InputTextReconcileUndoStateAfterUserCallback(ImGuiInputTextState* state, const char* new_buf_a, int new_length_a)
    {
        ImGuiContext& g = *GImGui;
        const ImWchar* old_buf = state->TextW.Data;
        const int old_length = state->CurLenW;
        const int new_length = ImTextCountCharsFromUtf8(new_buf_a, new_buf_a + new_length_a);
        g.TempBuffer.reserve_discard((new_length + 1) * sizeof(ImWchar));
        ImWchar* new_buf = (ImWchar*)(void*)g.TempBuffer.Data;
        ImTextStrFromUtf8(new_buf, new_length + 1, new_buf_a, new_buf_a + new_length_a);

        const int shorter_length = ImMin(old_length, new_length);
        int first_diff;
        for (first_diff = 0; first_diff < shorter_length; first_diff++)
            if (old_buf[first_diff] != new_buf[first_diff])
                break;
        if (first_diff == old_length && first_diff == new_length)
            return;

        int old_last_diff = old_length - 1;
        int new_last_diff = new_length - 1;
        for (; old_last_diff >= first_diff && new_last_diff >= first_diff; old_last_diff--, new_last_diff--)
            if (old_buf[old_last_diff] != new_buf[new_last_diff])
                break;

        const int insert_len = new_last_diff - first_diff + 1;
        const int delete_len = old_last_diff - first_diff + 1;
        if (insert_len > 0 || delete_len > 0)
            if (STB_TEXTEDIT_CHARTYPE* p = stb_text_createundo(&state->Stb.undostate, first_diff, delete_len, insert_len))
                for (int i = 0; i < delete_len; i++)
                    p[i] = ImStb::STB_TEXTEDIT_GETCHAR(state, first_diff + i);
    }

    bool InputTextEx(const char* label, const char* hint, char* buf, int buf_size, const ImVec2& size_arg, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* callback_user_data)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        IM_ASSERT(buf != NULL && buf_size >= 0);
        IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackHistory) && (flags & ImGuiInputTextFlags_Multiline)));        // Can't use both together (they both use up/down keys)
        IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackCompletion) && (flags & ImGuiInputTextFlags_AllowTabInput))); // Can't use both together (they both use tab key)

        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        const ImGuiStyle& style = g.Style;

        const bool RENDER_SELECTION_WHEN_INACTIVE = false;
        const bool is_multiline = (flags & ImGuiInputTextFlags_Multiline) != 0;
        const bool is_readonly = (flags & ImGuiInputTextFlags_ReadOnly) != 0;
        const bool is_password = (flags & ImGuiInputTextFlags_Password) != 0;
        const bool is_undoable = (flags & ImGuiInputTextFlags_NoUndoRedo) == 0;
        const bool is_resizable = (flags & ImGuiInputTextFlags_CallbackResize) != 0;
        if (is_resizable)
            IM_ASSERT(callback != NULL); // Must provide a callback if you set the ImGuiInputTextFlags_CallbackResize flag!

        if (is_multiline) // Open group before calling GetID() because groups tracks id created within their scope (including the scrollbar)
            BeginGroup();
        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        const ImRect rect(pos, pos + ImVec2(width, 40));
        const ImRect clickable(pos + ImVec2(width - 126, 8), pos + ImVec2(width - 12, 32));

        ImGuiWindow* draw_window = window;
        ImVec2 inner_size = clickable.GetSize();
        ImGuiItemStatusFlags item_status_flags = 0;
        ImGuiLastItemData item_data_backup;
        if (is_multiline)
        {
            ImVec2 backup_pos = window->DC.CursorPos;
            ItemSize(rect, style.FramePadding.y);
            if (!ItemAdd(rect, id, &clickable, ImGuiItemFlags_Inputable))
            {
                EndGroup();
                return false;
            }
            item_status_flags = g.LastItemData.StatusFlags;
            item_data_backup = g.LastItemData;
            window->DC.CursorPos = backup_pos;

            // Prevent NavActivate reactivating in BeginChild().
            const ImGuiID backup_activate_id = g.NavActivateId;
            if (g.ActiveId == id) // Prevent reactivation
                g.NavActivateId = 0;

            // We reproduce the contents of BeginChildFrame() in order to provide 'label' so our window internal data are easier to read/debug.
            PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
            PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
            PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
            bool child_visible = BeginChildEx(label, id, clickable.GetSize(), true, ImGuiWindowFlags_NoMove);
            g.NavActivateId = backup_activate_id;
            PopStyleVar(3);
            PopStyleColor();
            if (!child_visible)
            {
                EndChild();
                EndGroup();
                return false;
            }
            draw_window = g.CurrentWindow; // Child window
            draw_window->DC.NavLayersActiveMaskNext |= (1 << draw_window->DC.NavLayerCurrent); // This is to ensure that EndChild() will display a navigation highlight so we can "enter" into it.
            draw_window->DC.CursorPos += style.FramePadding;
            inner_size.x -= draw_window->ScrollbarSizes.x;
        }
        else
        {
            // Support for internal ImGuiInputTextFlags_MergedItem flag, which could be redesigned as an ItemFlags if needed (with test performed in ItemAdd)
            ItemSize(rect, style.FramePadding.y);
            if (!(flags & ImGuiInputTextFlags_MergedItem))
                if (!ItemAdd(rect, id, &clickable, ImGuiItemFlags_Inputable))
                    return false;
            item_status_flags = g.LastItemData.StatusFlags;
        }
        const bool hovered = ItemHoverable(clickable, id, g.LastItemData.InFlags);
        PushFont(settings::lexend_deca_medium_widgets);

        // We are only allowed to access the state if we are already the active widget.
        ImGuiInputTextState* state = GetInputTextState(id);

        const bool input_requested_by_tabbing = (item_status_flags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
        const bool input_requested_by_nav = (g.ActiveId != id) && ((g.NavActivateId == id) && ((g.NavActivateFlags & ImGuiActivateFlags_PreferInput) || (g.NavInputSource == ImGuiInputSource_Keyboard)));

        const bool user_clicked = hovered && io.MouseClicked[0];
        const bool user_scroll_finish = is_multiline && state != NULL && g.ActiveId == 0 && g.ActiveIdPreviousFrame == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
        const bool user_scroll_active = is_multiline && state != NULL && g.ActiveId == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
        bool clear_active_id = false;
        bool select_all = false;

        float scroll_y = is_multiline ? draw_window->Scroll.y : FLT_MAX;

        const bool init_changed_specs = (state != NULL && state->Stb.single_line != !is_multiline); // state != NULL means its our state.
        const bool init_make_active = (user_clicked || user_scroll_finish || input_requested_by_nav || input_requested_by_tabbing);
        const bool init_state = (init_make_active || user_scroll_active);
        if ((init_state && g.ActiveId != id) || init_changed_specs)
        {
            // Access state even if we don't own it yet.
            state = &g.InputTextState;
            state->CursorAnimReset();

            // Backup state of deactivating item so they'll have a chance to do a write to output buffer on the same frame they report IsItemDeactivatedAfterEdit (#4714)
            InputTextDeactivateHook(state->ID);

            // Take a copy of the initial buffer value (both in original UTF-8 format and converted to wchar)
            // From the moment we focused we are ignoring the content of 'buf' (unless we are in read-only mode)
            const int buf_len = (int)strlen(buf);
            state->InitialTextA.resize(buf_len + 1);    // UTF-8. we use +1 to make sure that .Data is always pointing to at least an empty string.
            memcpy(state->InitialTextA.Data, buf, buf_len + 1);

            // Preserve cursor position and undo/redo stack if we come back to same widget
            // FIXME: Since we reworked this on 2022/06, may want to differenciate recycle_cursor vs recycle_undostate?
            bool recycle_state = (state->ID == id && !init_changed_specs);
            if (recycle_state && (state->CurLenA != buf_len || (state->TextAIsValid && strncmp(state->TextA.Data, buf, buf_len) != 0)))
                recycle_state = false;

            // Start edition
            const char* buf_end = NULL;
            state->ID = id;
            state->TextW.resize(buf_size + 1);          // wchar count <= UTF-8 count. we use +1 to make sure that .Data is always pointing to at least an empty string.
            state->TextA.resize(0);
            state->TextAIsValid = false;                // TextA is not valid yet (we will display buf until then)
            state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, buf_size, buf, NULL, &buf_end);
            state->CurLenA = (int)(buf_end - buf);      // We can't get the result from ImStrncpy() above because it is not UTF-8 aware. Here we'll cut off malformed UTF-8.

            if (recycle_state)
            {
                // Recycle existing cursor/selection/undo stack but clamp position
                // Note a single mouse click will override the cursor/position immediately by calling stb_textedit_click handler.
                state->CursorClamp();
            }
            else
            {
                state->ScrollX = 0.0f;
                stb_textedit_initialize_state(&state->Stb, !is_multiline);
            }

            if (!is_multiline)
            {
                if (flags & ImGuiInputTextFlags_AutoSelectAll)
                    select_all = true;
                if (input_requested_by_nav && (!recycle_state || !(g.NavActivateFlags & ImGuiActivateFlags_TryToPreserveState)))
                    select_all = true;
                if (input_requested_by_tabbing || (user_clicked && io.KeyCtrl))
                    select_all = true;
            }

            if (flags & ImGuiInputTextFlags_AlwaysOverwrite)
                state->Stb.insert_mode = 1; // stb field name is indeed incorrect (see #2863)
        }

        const bool is_osx = io.ConfigMacOSXBehaviors;
        if (g.ActiveId != id && init_make_active)
        {
            IM_ASSERT(state && state->ID == id);
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
        }
        if (g.ActiveId == id)
        {
            // Declare some inputs, the other are registered and polled via Shortcut() routing system.
            if (user_clicked)
                SetKeyOwner(ImGuiKey_MouseLeft, id);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            if (is_multiline || (flags & ImGuiInputTextFlags_CallbackHistory))
                g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
            SetKeyOwner(ImGuiKey_Home, id);
            SetKeyOwner(ImGuiKey_End, id);
            if (is_multiline)
            {
                SetKeyOwner(ImGuiKey_PageUp, id);
                SetKeyOwner(ImGuiKey_PageDown, id);
            }
            if (is_osx)
                SetKeyOwner(ImGuiMod_Alt, id);
            if (flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_AllowTabInput)) // Disable keyboard tabbing out as we will use the \t character.
                SetShortcutRouting(ImGuiKey_Tab, id);
        }

        // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped), clear id immediately (don't wait until the end of the function)
        if (g.ActiveId == id && state == NULL)
            ClearActiveID();

        // Release focus when we click outside
        if (g.ActiveId == id && io.MouseClicked[0] && !init_state && !init_make_active) //-V560
            clear_active_id = true;

        // Lock the decision of whether we are going to take the path displaying the cursor or selection
        bool render_cursor = (g.ActiveId == id) || (state && user_scroll_active);
        bool render_selection = state && (state->HasSelection() || select_all) && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
        bool value_changed = false;
        bool validated = false;

        // When read-only we always use the live data passed to the function
        // FIXME-OPT: Because our selection/cursor code currently needs the wide text we need to convert it when active, which is not ideal :(
        if (is_readonly && state != NULL && (render_cursor || render_selection))
        {
            const char* buf_end = NULL;
            state->TextW.resize(buf_size + 1);
            state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, buf, NULL, &buf_end);
            state->CurLenA = (int)(buf_end - buf);
            state->CursorClamp();
            render_selection &= state->HasSelection();
        }

        // Select the buffer to render.
        const bool buf_display_from_state = (render_cursor || render_selection || g.ActiveId == id) && !is_readonly && state && state->TextAIsValid;
        const bool is_displaying_hint = (hint != NULL && (buf_display_from_state ? state->TextA.Data : buf)[0] == 0);

        // Password pushes a temporary font with only a fallback glyph
        if (is_password && !is_displaying_hint)
        {
            const ImFontGlyph* glyph = g.Font->FindGlyph('*');
            ImFont* password_font = &g.InputTextPasswordFont;
            password_font->FontSize = g.Font->FontSize;
            password_font->Scale = g.Font->Scale;
            password_font->Ascent = g.Font->Ascent;
            password_font->Descent = g.Font->Descent;
            password_font->ContainerAtlas = g.Font->ContainerAtlas;
            password_font->FallbackGlyph = glyph;
            password_font->FallbackAdvanceX = glyph->AdvanceX;
            IM_ASSERT(password_font->Glyphs.empty() && password_font->IndexAdvanceX.empty() && password_font->IndexLookup.empty());
            PushFont(password_font);
        }

        // Process mouse inputs and character inputs
        int backup_current_text_length = 0;
        if (g.ActiveId == id)
        {
            IM_ASSERT(state != NULL);
            backup_current_text_length = state->CurLenA;
            state->Edited = false;
            state->BufCapacityA = buf_size;
            state->Flags = flags;

            // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
            // Down the line we should have a cleaner library-wide concept of Selected vs Active.
            g.ActiveIdAllowOverlap = !io.MouseDown[0];

            // Edit in progress
            const float mouse_x = (io.MousePos.x - clickable.Min.x - style.FramePadding.x) + state->ScrollX;
            const float mouse_y = (is_multiline ? (io.MousePos.y - draw_window->DC.CursorPos.y) : (g.FontSize * 0.5f));

            if (select_all)
            {
                state->SelectAll();
                state->SelectedAllMouseLock = true;
            }
            else if (hovered && io.MouseClickedCount[0] >= 2 && !io.KeyShift)
            {
                stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                const int multiclick_count = (io.MouseClickedCount[0] - 2);
                if ((multiclick_count % 2) == 0)
                {
                    // Double-click: Select word
                    // We always use the "Mac" word advance for double-click select vs CTRL+Right which use the platform dependent variant:
                    // FIXME: There are likely many ways to improve this behavior, but there's no "right" behavior (depends on use-case, software, OS)
                    const bool is_bol = (state->Stb.cursor == 0) || ImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb.cursor - 1) == '\n';
                    if (STB_TEXT_HAS_SELECTION(&state->Stb) || !is_bol)
                        state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT);
                    //state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
                    if (!STB_TEXT_HAS_SELECTION(&state->Stb))
                        ImStb::stb_textedit_prep_selection_at_cursor(&state->Stb);
                    state->Stb.cursor = ImStb::STB_TEXTEDIT_MOVEWORDRIGHT_MAC(state, state->Stb.cursor);
                    state->Stb.select_end = state->Stb.cursor;
                    ImStb::stb_textedit_clamp(state, &state->Stb);
                }
                else
                {
                    // Triple-click: Select line
                    const bool is_eol = ImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb.cursor) == '\n';
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART);
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINEEND | STB_TEXTEDIT_K_SHIFT);
                    state->OnKeyPressed(STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_SHIFT);
                    if (!is_eol && is_multiline)
                    {
                        ImSwap(state->Stb.select_start, state->Stb.select_end);
                        state->Stb.cursor = state->Stb.select_end;
                    }
                    state->CursorFollow = false;
                }
                state->CursorAnimReset();
            }
            else if (io.MouseClicked[0] && !state->SelectedAllMouseLock)
            {
                if (hovered)
                {
                    if (io.KeyShift)
                        stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
                    else
                        stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                    state->CursorAnimReset();
                }
            }
            else if (io.MouseDown[0] && !state->SelectedAllMouseLock && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
            {
                stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
                state->CursorAnimReset();
                state->CursorFollow = true;
            }
            if (state->SelectedAllMouseLock && !io.MouseDown[0])
                state->SelectedAllMouseLock = false;

            // We expect backends to emit a Tab key but some also emit a Tab character which we ignore (#2467, #1336)
            // (For Tab and Enter: Win32/SFML/Allegro are sending both keys and chars, GLFW and SDL are only sending keys. For Space they all send all threes)
            if ((flags & ImGuiInputTextFlags_AllowTabInput) && Shortcut(ImGuiKey_Tab, id) && !is_readonly)
            {
                unsigned int c = '\t'; // Insert TAB
                if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data, ImGuiInputSource_Keyboard))
                    state->OnKeyPressed((int)c);
            }

            // Process regular text input (before we check for Return because using some IME will effectively send a Return?)
            // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
            const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (is_osx && io.KeySuper);
            if (io.InputQueueCharacters.Size > 0)
            {
                if (!ignore_char_inputs && !is_readonly && !input_requested_by_nav)
                    for (int n = 0; n < io.InputQueueCharacters.Size; n++)
                    {
                        // Insert character if they pass filtering
                        unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                        if (c == '\t') // Skip Tab, see above.
                            continue;
                        if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data, ImGuiInputSource_Keyboard))
                            state->OnKeyPressed((int)c);
                    }

                // Consume characters
                io.InputQueueCharacters.resize(0);
            }
        }

        // Process other shortcuts/key-presses
        bool revert_edit = false;
        if (g.ActiveId == id && !g.ActiveIdIsJustActivated && !clear_active_id)
        {
            IM_ASSERT(state != NULL);

            const int row_count_per_page = ImMax((int)((inner_size.y - style.FramePadding.y) / g.FontSize), 1);
            state->Stb.row_count_per_page = row_count_per_page;

            const int k_mask = (io.KeyShift ? STB_TEXTEDIT_K_SHIFT : 0);
            const bool is_wordmove_key_down = is_osx ? io.KeyAlt : io.KeyCtrl;                     // OS X style: Text editing cursor movement using Alt instead of Ctrl
            const bool is_startend_key_down = is_osx && io.KeySuper && !io.KeyCtrl && !io.KeyAlt;  // OS X style: Line/Text Start and End using Cmd+Arrows instead of Home/End

            // Using Shortcut() with ImGuiInputFlags_RouteFocused (default policy) to allow routing operations for other code (e.g. calling window trying to use CTRL+A and CTRL+B: formet would be handled by InputText)
            // Otherwise we could simply assume that we own the keys as we are active.
            const ImGuiInputFlags f_repeat = ImGuiInputFlags_Repeat;
            const bool is_cut = (Shortcut(ImGuiMod_Shortcut | ImGuiKey_X, id, f_repeat) || Shortcut(ImGuiMod_Shift | ImGuiKey_Delete, id, f_repeat)) && !is_readonly && !is_password && (!is_multiline || state->HasSelection());
            const bool is_copy = (Shortcut(ImGuiMod_Shortcut | ImGuiKey_C, id) || Shortcut(ImGuiMod_Ctrl | ImGuiKey_Insert, id)) && !is_password && (!is_multiline || state->HasSelection());
            const bool is_paste = (Shortcut(ImGuiMod_Shortcut | ImGuiKey_V, id, f_repeat) || Shortcut(ImGuiMod_Shift | ImGuiKey_Insert, id, f_repeat)) && !is_readonly;
            const bool is_undo = (Shortcut(ImGuiMod_Shortcut | ImGuiKey_Z, id, f_repeat)) && !is_readonly && is_undoable;
            const bool is_redo = (Shortcut(ImGuiMod_Shortcut | ImGuiKey_Y, id, f_repeat) || (is_osx && Shortcut(ImGuiMod_Shortcut | ImGuiMod_Shift | ImGuiKey_Z, id, f_repeat))) && !is_readonly && is_undoable;
            const bool is_select_all = Shortcut(ImGuiMod_Shortcut | ImGuiKey_A, id);

            // We allow validate/cancel with Nav source (gamepad) to makes it easier to undo an accidental NavInput press with no keyboard wired, but otherwise it isn't very useful.
            const bool nav_gamepad_active = (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0 && (io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0;
            const bool is_enter_pressed = IsKeyPressed(ImGuiKey_Enter, true) || IsKeyPressed(ImGuiKey_KeypadEnter, true);
            const bool is_gamepad_validate = nav_gamepad_active && (IsKeyPressed(ImGuiKey_NavGamepadActivate, false) || IsKeyPressed(ImGuiKey_NavGamepadInput, false));
            const bool is_cancel = Shortcut(ImGuiKey_Escape, id, f_repeat) || (nav_gamepad_active && Shortcut(ImGuiKey_NavGamepadCancel, id, f_repeat));

            // FIXME: Should use more Shortcut() and reduce IsKeyPressed()+SetKeyOwner(), but requires modifiers combination to be taken account of.
            if (IsKeyPressed(ImGuiKey_LeftArrow)) { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINESTART : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT) | k_mask); }
            else if (IsKeyPressed(ImGuiKey_RightArrow)) { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINEEND : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT) | k_mask); }
            else if (IsKeyPressed(ImGuiKey_UpArrow) && is_multiline) { if (io.KeyCtrl) SetScrollY(draw_window, ImMax(draw_window->Scroll.y - g.FontSize, 0.0f)); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_UP) | k_mask); }
            else if (IsKeyPressed(ImGuiKey_DownArrow) && is_multiline) { if (io.KeyCtrl) SetScrollY(draw_window, ImMin(draw_window->Scroll.y + g.FontSize, GetScrollMaxY())); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_DOWN) | k_mask); }
            else if (IsKeyPressed(ImGuiKey_PageUp) && is_multiline) { state->OnKeyPressed(STB_TEXTEDIT_K_PGUP | k_mask); scroll_y -= row_count_per_page * g.FontSize; }
            else if (IsKeyPressed(ImGuiKey_PageDown) && is_multiline) { state->OnKeyPressed(STB_TEXTEDIT_K_PGDOWN | k_mask); scroll_y += row_count_per_page * g.FontSize; }
            else if (IsKeyPressed(ImGuiKey_Home)) { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTSTART | k_mask : STB_TEXTEDIT_K_LINESTART | k_mask); }
            else if (IsKeyPressed(ImGuiKey_End)) { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTEND | k_mask : STB_TEXTEDIT_K_LINEEND | k_mask); }
            else if (IsKeyPressed(ImGuiKey_Delete) && !is_readonly && !is_cut)
            {
                if (!state->HasSelection())
                {
                    // OSX doesn't seem to have Super+Delete to delete until end-of-line, so we don't emulate that (as opposed to Super+Backspace)
                    if (is_wordmove_key_down)
                        state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
                }
                state->OnKeyPressed(STB_TEXTEDIT_K_DELETE | k_mask);
            }
            else if (IsKeyPressed(ImGuiKey_Backspace) && !is_readonly)
            {
                if (!state->HasSelection())
                {
                    if (is_wordmove_key_down)
                        state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT | STB_TEXTEDIT_K_SHIFT);
                    else if (is_osx && io.KeySuper && !io.KeyAlt && !io.KeyCtrl)
                        state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_SHIFT);
                }
                state->OnKeyPressed(STB_TEXTEDIT_K_BACKSPACE | k_mask);
            }
            else if (is_enter_pressed || is_gamepad_validate)
            {
                // Determine if we turn Enter into a \n character
                bool ctrl_enter_for_new_line = (flags & ImGuiInputTextFlags_CtrlEnterForNewLine) != 0;
                if (!is_multiline || is_gamepad_validate || (ctrl_enter_for_new_line && !io.KeyCtrl) || (!ctrl_enter_for_new_line && io.KeyCtrl))
                {
                    validated = true;
                    if (io.ConfigInputTextEnterKeepActive && !is_multiline)
                        state->SelectAll(); // No need to scroll
                    else
                        clear_active_id = true;
                }
                else if (!is_readonly)
                {
                    unsigned int c = '\n'; // Insert new line
                    if (InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data, ImGuiInputSource_Keyboard))
                        state->OnKeyPressed((int)c);
                }
            }
            else if (is_cancel)
            {
                if (flags & ImGuiInputTextFlags_EscapeClearsAll)
                {
                    if (buf[0] != 0)
                    {
                        revert_edit = true;
                    }
                    else
                    {
                        render_cursor = render_selection = false;
                        clear_active_id = true;
                    }
                }
                else
                {
                    clear_active_id = revert_edit = true;
                    render_cursor = render_selection = false;
                }
            }
            else if (is_undo || is_redo)
            {
                state->OnKeyPressed(is_undo ? STB_TEXTEDIT_K_UNDO : STB_TEXTEDIT_K_REDO);
                state->ClearSelection();
            }
            else if (is_select_all)
            {
                state->SelectAll();
                state->CursorFollow = true;
            }
            else if (is_cut || is_copy)
            {
                // Cut, Copy
                if (io.SetClipboardTextFn)
                {
                    const int ib = state->HasSelection() ? ImMin(state->Stb.select_start, state->Stb.select_end) : 0;
                    const int ie = state->HasSelection() ? ImMax(state->Stb.select_start, state->Stb.select_end) : state->CurLenW;
                    const int clipboard_data_len = ImTextCountUtf8BytesFromStr(state->TextW.Data + ib, state->TextW.Data + ie) + 1;
                    char* clipboard_data = (char*)IM_ALLOC(clipboard_data_len * sizeof(char));
                    ImTextStrToUtf8(clipboard_data, clipboard_data_len, state->TextW.Data + ib, state->TextW.Data + ie);
                    SetClipboardText(clipboard_data);
                    MemFree(clipboard_data);
                }
                if (is_cut)
                {
                    if (!state->HasSelection())
                        state->SelectAll();
                    state->CursorFollow = true;
                    stb_textedit_cut(state, &state->Stb);
                }
            }
            else if (is_paste)
            {
                if (const char* clipboard = GetClipboardText())
                {
                    // Filter pasted buffer
                    const int clipboard_len = (int)strlen(clipboard);
                    ImWchar* clipboard_filtered = (ImWchar*)IM_ALLOC((clipboard_len + 1) * sizeof(ImWchar));
                    int clipboard_filtered_len = 0;
                    for (const char* s = clipboard; *s != 0; )
                    {
                        unsigned int c;
                        s += ImTextCharFromUtf8(&c, s, NULL);
                        if (!InputTextFilterCharacter(&g, &c, flags, callback, callback_user_data, ImGuiInputSource_Clipboard))
                            continue;
                        clipboard_filtered[clipboard_filtered_len++] = (ImWchar)c;
                    }
                    clipboard_filtered[clipboard_filtered_len] = 0;
                    if (clipboard_filtered_len > 0) // If everything was filtered, ignore the pasting operation
                    {
                        stb_textedit_paste(state, &state->Stb, clipboard_filtered, clipboard_filtered_len);
                        state->CursorFollow = true;
                    }
                    MemFree(clipboard_filtered);
                }
            }

            // Update render selection flag after events have been handled, so selection highlight can be displayed during the same frame.
            render_selection |= state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
        }

        // Process callbacks and apply result back to user's buffer.
        const char* apply_new_text = NULL;
        int apply_new_text_length = 0;
        if (g.ActiveId == id)
        {
            IM_ASSERT(state != NULL);
            if (revert_edit && !is_readonly)
            {
                if (flags & ImGuiInputTextFlags_EscapeClearsAll)
                {
                    // Clear input
                    IM_ASSERT(buf[0] != 0);
                    apply_new_text = "";
                    apply_new_text_length = 0;
                    value_changed = true;
                    STB_TEXTEDIT_CHARTYPE empty_string;
                    stb_textedit_replace(state, &state->Stb, &empty_string, 0);
                }
                else if (strcmp(buf, state->InitialTextA.Data) != 0)
                {
                    // Restore initial value. Only return true if restoring to the initial value changes the current buffer contents.
                    // Push records into the undo stack so we can CTRL+Z the revert operation itself
                    apply_new_text = state->InitialTextA.Data;
                    apply_new_text_length = state->InitialTextA.Size - 1;
                    value_changed = true;
                    ImVector<ImWchar> w_text;
                    if (apply_new_text_length > 0)
                    {
                        w_text.resize(ImTextCountCharsFromUtf8(apply_new_text, apply_new_text + apply_new_text_length) + 1);
                        ImTextStrFromUtf8(w_text.Data, w_text.Size, apply_new_text, apply_new_text + apply_new_text_length);
                    }
                    stb_textedit_replace(state, &state->Stb, w_text.Data, (apply_new_text_length > 0) ? (w_text.Size - 1) : 0);
                }
            }

            // Apply ASCII value
            if (!is_readonly)
            {
                state->TextAIsValid = true;
                state->TextA.resize(state->TextW.Size * 4 + 1);
                ImTextStrToUtf8(state->TextA.Data, state->TextA.Size, state->TextW.Data, NULL);
            }

            // When using 'ImGuiInputTextFlags_EnterReturnsTrue' as a special case we reapply the live buffer back to the input buffer
            // before clearing ActiveId, even though strictly speaking it wasn't modified on this frame.
            // If we didn't do that, code like InputInt() with ImGuiInputTextFlags_EnterReturnsTrue would fail.
            // This also allows the user to use InputText() with ImGuiInputTextFlags_EnterReturnsTrue without maintaining any user-side storage
            // (please note that if you use this property along ImGuiInputTextFlags_CallbackResize you can end up with your temporary string object
            // unnecessarily allocating once a frame, either store your string data, either if you don't then don't use ImGuiInputTextFlags_CallbackResize).
            const bool apply_edit_back_to_user_buffer = !revert_edit || (validated && (flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0);
            if (apply_edit_back_to_user_buffer)
            {
                // Apply new value immediately - copy modified buffer back
                // Note that as soon as the input box is active, the in-widget value gets priority over any underlying modification of the input buffer
                // FIXME: We actually always render 'buf' when calling DrawList->AddText, making the comment above incorrect.
                // FIXME-OPT: CPU waste to do this every time the widget is active, should mark dirty state from the stb_textedit callbacks.

                // User callback
                if ((flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways)) != 0)
                {
                    IM_ASSERT(callback != NULL);

                    // The reason we specify the usage semantic (Completion/History) is that Completion needs to disable keyboard TABBING at the moment.
                    ImGuiInputTextFlags event_flag = 0;
                    ImGuiKey event_key = ImGuiKey_None;
                    if ((flags & ImGuiInputTextFlags_CallbackCompletion) != 0 && Shortcut(ImGuiKey_Tab, id))
                    {
                        event_flag = ImGuiInputTextFlags_CallbackCompletion;
                        event_key = ImGuiKey_Tab;
                    }
                    else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(ImGuiKey_UpArrow))
                    {
                        event_flag = ImGuiInputTextFlags_CallbackHistory;
                        event_key = ImGuiKey_UpArrow;
                    }
                    else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(ImGuiKey_DownArrow))
                    {
                        event_flag = ImGuiInputTextFlags_CallbackHistory;
                        event_key = ImGuiKey_DownArrow;
                    }
                    else if ((flags & ImGuiInputTextFlags_CallbackEdit) && state->Edited)
                    {
                        event_flag = ImGuiInputTextFlags_CallbackEdit;
                    }
                    else if (flags & ImGuiInputTextFlags_CallbackAlways)
                    {
                        event_flag = ImGuiInputTextFlags_CallbackAlways;
                    }

                    if (event_flag)
                    {
                        ImGuiInputTextCallbackData callback_data;
                        callback_data.Ctx = &g;
                        callback_data.EventFlag = event_flag;
                        callback_data.Flags = flags;
                        callback_data.UserData = callback_user_data;

                        char* callback_buf = is_readonly ? buf : state->TextA.Data;
                        callback_data.EventKey = event_key;
                        callback_data.Buf = callback_buf;
                        callback_data.BufTextLen = state->CurLenA;
                        callback_data.BufSize = state->BufCapacityA;
                        callback_data.BufDirty = false;

                        // We have to convert from wchar-positions to UTF-8-positions, which can be pretty slow (an incentive to ditch the ImWchar buffer, see https://github.com/nothings/stb/issues/188)
                        ImWchar* text = state->TextW.Data;
                        const int utf8_cursor_pos = callback_data.CursorPos = ImTextCountUtf8BytesFromStr(text, text + state->Stb.cursor);
                        const int utf8_selection_start = callback_data.SelectionStart = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_start);
                        const int utf8_selection_end = callback_data.SelectionEnd = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_end);

                        // Call user code
                        callback(&callback_data);

                        // Read back what user may have modified
                        callback_buf = is_readonly ? buf : state->TextA.Data; // Pointer may have been invalidated by a resize callback
                        IM_ASSERT(callback_data.Buf == callback_buf);         // Invalid to modify those fields
                        IM_ASSERT(callback_data.BufSize == state->BufCapacityA);
                        IM_ASSERT(callback_data.Flags == flags);
                        const bool buf_dirty = callback_data.BufDirty;
                        if (callback_data.CursorPos != utf8_cursor_pos || buf_dirty) { state->Stb.cursor = ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.CursorPos); state->CursorFollow = true; }
                        if (callback_data.SelectionStart != utf8_selection_start || buf_dirty) { state->Stb.select_start = (callback_data.SelectionStart == callback_data.CursorPos) ? state->Stb.cursor : ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionStart); }
                        if (callback_data.SelectionEnd != utf8_selection_end || buf_dirty) { state->Stb.select_end = (callback_data.SelectionEnd == callback_data.SelectionStart) ? state->Stb.select_start : ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionEnd); }
                        if (buf_dirty)
                        {
                            IM_ASSERT((flags & ImGuiInputTextFlags_ReadOnly) == 0);
                            IM_ASSERT(callback_data.BufTextLen == (int)strlen(callback_data.Buf)); // You need to maintain BufTextLen if you change the text!
                            InputTextReconcileUndoStateAfterUserCallback(state, callback_data.Buf, callback_data.BufTextLen); // FIXME: Move the rest of this block inside function and rename to InputTextReconcileStateAfterUserCallback() ?
                            if (callback_data.BufTextLen > backup_current_text_length && is_resizable)
                                state->TextW.resize(state->TextW.Size + (callback_data.BufTextLen - backup_current_text_length)); // Worse case scenario resize
                            state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, callback_data.Buf, NULL);
                            state->CurLenA = callback_data.BufTextLen;  // Assume correct length and valid UTF-8 from user, saves us an extra strlen()
                            state->CursorAnimReset();
                        }
                    }
                }

                // Will copy result string if modified
                if (!is_readonly && strcmp(state->TextA.Data, buf) != 0)
                {
                    apply_new_text = state->TextA.Data;
                    apply_new_text_length = state->CurLenA;
                    value_changed = true;
                }
            }
        }

        // Handle reapplying final data on deactivation (see InputTextDeactivateHook() for details)
        if (g.InputTextDeactivatedState.ID == id)
        {
            if (g.ActiveId != id && IsItemDeactivatedAfterEdit() && !is_readonly && strcmp(g.InputTextDeactivatedState.TextA.Data, buf) != 0)
            {
                apply_new_text = g.InputTextDeactivatedState.TextA.Data;
                apply_new_text_length = g.InputTextDeactivatedState.TextA.Size - 1;
                value_changed = true;
                //IMGUI_DEBUG_LOG("InputText(): apply Deactivated data for 0x%08X: \"%.*s\".\n", id, apply_new_text_length, apply_new_text);
            }
            g.InputTextDeactivatedState.ID = 0;
        }

        // Copy result to user buffer. This can currently only happen when (g.ActiveId == id)
        if (apply_new_text != NULL)
        {
            // We cannot test for 'backup_current_text_length != apply_new_text_length' here because we have no guarantee that the size
            // of our owned buffer matches the size of the string object held by the user, and by design we allow InputText() to be used
            // without any storage on user's side.
            IM_ASSERT(apply_new_text_length >= 0);
            if (is_resizable)
            {
                ImGuiInputTextCallbackData callback_data;
                callback_data.Ctx = &g;
                callback_data.EventFlag = ImGuiInputTextFlags_CallbackResize;
                callback_data.Flags = flags;
                callback_data.Buf = buf;
                callback_data.BufTextLen = apply_new_text_length;
                callback_data.BufSize = ImMax(buf_size, apply_new_text_length + 1);
                callback_data.UserData = callback_user_data;
                callback(&callback_data);
                buf = callback_data.Buf;
                buf_size = callback_data.BufSize;
                apply_new_text_length = ImMin(callback_data.BufTextLen, buf_size - 1);
                IM_ASSERT(apply_new_text_length <= buf_size);
            }
            //IMGUI_DEBUG_PRINT("InputText(\"%s\"): apply_new_text length %d\n", label, apply_new_text_length);

            // If the underlying buffer resize was denied or not carried to the next frame, apply_new_text_length+1 may be >= buf_size.
            ImStrncpy(buf, apply_new_text, ImMin(apply_new_text_length + 1, buf_size));
        }

        // Release active ID at the end of the function (so e.g. pressing Return still does a final application of the value)
        // Otherwise request text input ahead for next frame.
        if (g.ActiveId == id && clear_active_id)
            ClearActiveID();
        else if (g.ActiveId == id)
            g.WantTextInputNextFrame = 1;

        static std::map<ImGuiID, float> anim;
        float& it_anim = anim[id];

        // Render frame
        if (!is_multiline)
        {
            window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::input_background), settings::widgets_rounding);
            window->DrawList->AddRectFilled(clickable.Min, clickable.Max, GetColor(colors::input_rect), 4.f);
            window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(colors::input_label), label);
        }

        const ImVec4 clip_rect(clickable.Min.x, clickable.Min.y, clickable.Min.x + inner_size.x, clickable.Min.y + inner_size.y); // Not using frame_bb.Max because we have adjusted size
        ImVec2 draw_pos = is_multiline ? draw_window->DC.CursorPos : clickable.Min + style.FramePadding + ImVec2(1, 2);
        ImVec2 text_size(0.0f, 0.0f);

        // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current pathological worst case is a long line
        // without any carriage return, which would makes ImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether.
        // Note that we only use this limit on single-line InputText(), so a pathologically large line on a InputTextMultiline() would still crash.
        const int buf_display_max_length = 2 * 1024 * 1024;
        const char* buf_display = buf_display_from_state ? state->TextA.Data : buf; //-V595
        const char* buf_display_end = NULL; // We have specialized paths below for setting the length
        if (is_displaying_hint)
        {
            buf_display = hint;
            buf_display_end = hint + strlen(hint);
        }

        // Render text. We currently only render selection when the widget is active or while scrolling.
        // FIXME: We could remove the '&& render_cursor' to keep rendering selection when inactive.
        if (render_cursor || render_selection)
        {
            IM_ASSERT(state != NULL);
            if (!is_displaying_hint)
                buf_display_end = buf_display + state->CurLenA;

            // Render text (with cursor and selection)
            // This is going to be messy. We need to:
            // - Display the text (this alone can be more easily clipped)
            // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
            // - Measure text height (for scrollbar)
            // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
            // FIXME: This should occur on buf_display but we'd need to maintain cursor/select_start/select_end for UTF-8.
            const ImWchar* text_begin = state->TextW.Data;
            ImVec2 cursor_offset, select_start_offset;

            {
                // Find lines numbers straddling 'cursor' (slot 0) and 'select_start' (slot 1) positions.
                const ImWchar* searches_input_ptr[2] = { NULL, NULL };
                int searches_result_line_no[2] = { -1000, -1000 };
                int searches_remaining = 0;
                if (render_cursor)
                {
                    searches_input_ptr[0] = text_begin + state->Stb.cursor;
                    searches_result_line_no[0] = -1;
                    searches_remaining++;
                }
                if (render_selection)
                {
                    searches_input_ptr[1] = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                    searches_result_line_no[1] = -1;
                    searches_remaining++;
                }

                // Iterate all lines to find our line numbers
                // In multi-line mode, we never exit the loop until all lines are counted, so add one extra to the searches_remaining counter.
                searches_remaining += is_multiline ? 1 : 0;
                int line_count = 0;
                //for (const ImWchar* s = text_begin; (s = (const ImWchar*)wcschr((const wchar_t*)s, (wchar_t)'\n')) != NULL; s++)  // FIXME-OPT: Could use this when wchar_t are 16-bit
                for (const ImWchar* s = text_begin; *s != 0; s++)
                    if (*s == '\n')
                    {
                        line_count++;
                        if (searches_result_line_no[0] == -1 && s >= searches_input_ptr[0]) { searches_result_line_no[0] = line_count; if (--searches_remaining <= 0) break; }
                        if (searches_result_line_no[1] == -1 && s >= searches_input_ptr[1]) { searches_result_line_no[1] = line_count; if (--searches_remaining <= 0) break; }
                    }
                line_count++;
                if (searches_result_line_no[0] == -1)
                    searches_result_line_no[0] = line_count;
                if (searches_result_line_no[1] == -1)
                    searches_result_line_no[1] = line_count;

                // Calculate 2d position by finding the beginning of the line and measuring distance
                cursor_offset.x = InputTextCalcTextSizeW(&g, ImStrbolW(searches_input_ptr[0], text_begin), searches_input_ptr[0]).x;
                cursor_offset.y = searches_result_line_no[0] * g.FontSize;
                if (searches_result_line_no[1] >= 0)
                {
                    select_start_offset.x = InputTextCalcTextSizeW(&g, ImStrbolW(searches_input_ptr[1], text_begin), searches_input_ptr[1]).x;
                    select_start_offset.y = searches_result_line_no[1] * g.FontSize;
                }

                // Store text height (note that we haven't calculated text width at all, see GitHub issues #383, #1224)
                if (is_multiline)
                    text_size = ImVec2(inner_size.x, line_count * g.FontSize);
            }

            // Scroll
            if (render_cursor && state->CursorFollow)
            {
                // Horizontal scroll in chunks of quarter width
                if (!(flags & ImGuiInputTextFlags_NoHorizontalScroll))
                {
                    const float scroll_increment_x = inner_size.x * 0.25f;
                    const float visible_width = inner_size.x - style.FramePadding.x;
                    if (cursor_offset.x < state->ScrollX)
                        state->ScrollX = IM_TRUNC(ImMax(0.0f, cursor_offset.x - scroll_increment_x));
                    else if (cursor_offset.x - visible_width >= state->ScrollX)
                        state->ScrollX = IM_TRUNC(cursor_offset.x - visible_width + scroll_increment_x);
                }
                else
                {
                    state->ScrollX = 0.0f;
                }

                // Vertical scroll
                if (is_multiline)
                {
                    // Test if cursor is vertically visible
                    if (cursor_offset.y - g.FontSize < scroll_y)
                        scroll_y = ImMax(0.0f, cursor_offset.y - g.FontSize);
                    else if (cursor_offset.y - (inner_size.y - style.FramePadding.y * 2.0f) >= scroll_y)
                        scroll_y = cursor_offset.y - inner_size.y + style.FramePadding.y * 2.0f;
                    const float scroll_max_y = ImMax((text_size.y + style.FramePadding.y * 2.0f) - inner_size.y, 0.0f);
                    scroll_y = ImClamp(scroll_y, 0.0f, scroll_max_y);
                    draw_pos.y += (draw_window->Scroll.y - scroll_y);   // Manipulate cursor pos immediately avoid a frame of lag
                    draw_window->Scroll.y = scroll_y;
                }

                state->CursorFollow = false;
            }

            // Draw selection
            const ImVec2 draw_scroll = ImVec2(state->ScrollX, 0.0f);
            if (render_selection)
            {
                const ImWchar* text_selected_begin = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                const ImWchar* text_selected_end = text_begin + ImMax(state->Stb.select_start, state->Stb.select_end);

                ImU32 bg_color = GetColorU32(ImGuiCol_TextSelectedBg, render_cursor ? 1.0f : 0.6f); // FIXME: current code flow mandate that render_cursor is always true here, we are leaving the transparent one for tests.
                float bg_offy_up = is_multiline ? 0.0f : -1.0f;    // FIXME: those offsets should be part of the style? they don't play so well with multi-line selection.
                float bg_offy_dn = is_multiline ? 0.0f : 2.0f;
                ImVec2 rect_pos = draw_pos + select_start_offset - draw_scroll;
                for (const ImWchar* p = text_selected_begin; p < text_selected_end; )
                {
                    if (rect_pos.y > clip_rect.w + g.FontSize)
                        break;
                    if (rect_pos.y < clip_rect.y)
                    {
                        //p = (const ImWchar*)wmemchr((const wchar_t*)p, '\n', text_selected_end - p);  // FIXME-OPT: Could use this when wchar_t are 16-bit
                        //p = p ? p + 1 : text_selected_end;
                        while (p < text_selected_end)
                            if (*p++ == '\n')
                                break;
                    }
                    else
                    {
                        ImVec2 rect_size = InputTextCalcTextSizeW(&g, p, text_selected_end, &p, NULL, true);
                        if (rect_size.x <= 0.0f) rect_size.x = IM_TRUNC(g.Font->GetCharAdvance((ImWchar)' ') * 0.50f); // So we can see selected empty lines
                        ImRect rect(rect_pos + ImVec2(0.0f, bg_offy_up - g.FontSize), rect_pos + ImVec2(rect_size.x, bg_offy_dn));
                        rect.ClipWith(clip_rect);
                        if (rect.Overlaps(clip_rect))
                            draw_window->DrawList->AddRectFilled(rect.Min, rect.Max + ImVec2(0, 1), GetColor(colors::accent, 0.3f), 2.f);
                    }
                    rect_pos.x = draw_pos.x - draw_scroll.x;
                    rect_pos.y += g.FontSize;
                }
            }

            // We test for 'buf_display_max_length' as a way to avoid some pathological cases (e.g. single-line 1 MB string) which would make ImDrawList crash.
            if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
            {
                ImU32 col = GetColorU32(is_displaying_hint ? ImGuiCol_TextDisabled : ImGuiCol_Text);
                draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos - draw_scroll, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
            }

            // Draw blinking cursor
            if (render_cursor)
            {
                state->CursorAnim += io.DeltaTime;
                bool cursor_is_visible = (!g.IO.ConfigInputTextCursorBlink) || (state->CursorAnim <= 0.0f) || ImFmod(state->CursorAnim, 1.20f) <= 0.80f;
                ImVec2 cursor_screen_pos = ImTrunc(draw_pos + cursor_offset - draw_scroll);
                ImRect cursor_screen_rect(cursor_screen_pos.x, cursor_screen_pos.y - g.FontSize + 0.5f, cursor_screen_pos.x + 1.0f, cursor_screen_pos.y - 1.5f);
                it_anim = ImLerp(it_anim, cursor_screen_rect.Min.x - clickable.Min.x, FixedSpeed(20.f));
                if (cursor_is_visible && cursor_screen_rect.Overlaps(clip_rect))
                    draw_window->DrawList->AddLine(clickable.Min + ImVec2(it_anim, 4), ImVec2(clickable.Min.x + it_anim, clickable.Max.y - 4), GetColorU32(ImGuiCol_Text));

                // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
                if (!is_readonly)
                {
                    g.PlatformImeData.WantVisible = true;
                    g.PlatformImeData.InputPos = ImVec2(cursor_screen_pos.x - 1.0f, cursor_screen_pos.y - g.FontSize);
                    g.PlatformImeData.InputLineHeight = g.FontSize;
                }
            }
        }
        else
        {
            // Render text only (no selection, no cursor)
            if (is_multiline)
                text_size = ImVec2(inner_size.x, InputTextCalcTextLenAndLineCount(buf_display, &buf_display_end) * g.FontSize); // We don't need width
            else if (!is_displaying_hint && g.ActiveId == id)
                buf_display_end = buf_display + state->CurLenA;
            else if (!is_displaying_hint)
                buf_display_end = buf_display + strlen(buf_display);

            if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
            {
                ImU32 col = GetColorU32(is_displaying_hint ? ImGuiCol_TextDisabled : ImGuiCol_Text);
                draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
            }
        }

        if (is_password && !is_displaying_hint)
            PopFont();

        if (is_multiline)
        {
            // For focus requests to work on our multiline we need to ensure our child ItemAdd() call specifies the ImGuiItemFlags_Inputable (ref issue #4761)...
            Dummy(ImVec2(text_size.x, text_size.y + style.FramePadding.y));
            g.NextItemData.ItemFlags |= ImGuiItemFlags_Inputable | ImGuiItemFlags_NoTabStop;
            EndChild();
            item_data_backup.StatusFlags |= (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredWindow);

            // ...and then we need to undo the group overriding last item data, which gets a bit messy as EndGroup() tries to forward scrollbar being active...
            // FIXME: This quite messy/tricky, should attempt to get rid of the child window.
            EndGroup();
            if (g.LastItemData.ID == 0)
            {
                g.LastItemData.ID = id;
                g.LastItemData.InFlags = item_data_backup.InFlags;
                g.LastItemData.StatusFlags = item_data_backup.StatusFlags;
            }
        }

        // Log as text
        if (g.LogEnabled && (!is_password || is_displaying_hint))
        {
            LogSetNextTextDecoration("{", "}");
            LogRenderedText(&draw_pos, buf_display, buf_display_end);
        }

        PopFont();

        if (value_changed && !(flags & ImGuiInputTextFlags_NoMarkEdited))
            MarkItemEdited(id);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Inputable);
        if ((flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0)
            return validated;
        else
            return value_changed;
    }

    bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
    {
        return InputTextEx(label, NULL, buf, (int)buf_size, ImVec2(0, 0), flags, callback, user_data);
    }

    bool Button(const char* label, const ImVec2& size_arg)
    {
        struct button_state
        {
            bool clicked = false;
            float timer = 0.f;
            ImVec4 label_color = colors::button_label_inactive;
        };

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        ImVec2 size = size_arg;
        if (size.x <= 0.f)
            size.x = width;
        if (size.y <= 0.f)
            size.y = 30;

        const ImRect rect(pos, pos + size);
        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(rect, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);

        static std::map<ImGuiID, button_state> anim;
        button_state& state = anim[id];

        if (pressed)
            state.clicked = true;

        state.timer = ImClamp(state.timer + (FixedSpeed(4.f) * (state.clicked ? 1.f : -1.f)), 0.f, 1.f);
        state.label_color = ImLerp(state.label_color, state.clicked ? colors::button_label_active : colors::button_label_inactive, FixedSpeed(8.f));

        if (state.timer >= 0.9f)
            state.clicked = false;

        // Render
        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::button_background), settings::widgets_rounding);
        PushFont(settings::lexend_deca_medium_widgets);
        PushStyleColor(ImGuiCol_Text, GetColor(state.label_color));
        RenderTextClipped(rect.Min - ImVec2(0, 1), rect.Max - ImVec2(0, 1), label, NULL, NULL, ImVec2(0.5f, 0.5f));
        PopStyleColor();
        PopFont();

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool ShortButton(const char* label)
    {
        return Button(label, ImVec2((GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x) / 2, 0));
    }

    static void ColorEditRestoreH(const float* col, float* H)
    {
        ImGuiContext& g = *GImGui;
        IM_ASSERT(g.ColorEditCurrentID != 0);
        if (g.ColorEditSavedID != g.ColorEditCurrentID || g.ColorEditSavedColor != ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0)))
            return;
        *H = g.ColorEditSavedHue;
    }

    static void ColorEditRestoreHS(const float* col, float* H, float* S, float* V)
    {
        ImGuiContext& g = *GImGui;
        IM_ASSERT(g.ColorEditCurrentID != 0);
        if (g.ColorEditSavedID != g.ColorEditCurrentID || g.ColorEditSavedColor != ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0)))
            return;

        // When S == 0, H is undefined.
        // When H == 1 it wraps around to 0.
        if (*S == 0.0f || (*H == 0.0f && g.ColorEditSavedHue == 1))
            *H = g.ColorEditSavedHue;

        // When V == 0, S is undefined.
        if (*V == 0.0f)
            *S = g.ColorEditSavedSat;
    }

    bool PickButton(const char* label, bool* callback)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect rect(pos, pos + ImVec2(28, 28));
        ItemSize(rect, style.FramePadding.y);
        if (!ItemAdd(rect, id))
        {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);
        if (pressed)
        {
            *callback = !(*callback);
            MarkItemEdited(id);
        }

        window->DrawList->AddRectFilled(rect.Min, rect.Max, callback ? ImColor(255, 0, 0) : ImColor(0, 0, 255));

        return pressed;
    }

    bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col)
    {

        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImDrawList* draw_list = window->DrawList;
        ImGuiStyle& style = g.Style;
        ImGuiIO& io = g.IO;

        const float width = CalcItemWidth();
        g.NextItemData.ClearFlags();

        PushID(label);
        const bool set_current_color_edit_id = (g.ColorEditCurrentID == 0);
        if (set_current_color_edit_id)
            g.ColorEditCurrentID = window->IDStack.back();
        BeginGroup();

        if (!(flags & ImGuiColorEditFlags_NoSidePreview))
            flags |= ImGuiColorEditFlags_NoSmallPreview;

        // Context menu: display and store options.
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            ColorPickerOptionsPopup(col, flags);

        // Read stored options
        if (!(flags & ImGuiColorEditFlags_PickerMask_))
            flags |= ((g.ColorEditOptions & ImGuiColorEditFlags_PickerMask_) ? g.ColorEditOptions : ImGuiColorEditFlags_DefaultOptions_) & ImGuiColorEditFlags_PickerMask_;
        if (!(flags & ImGuiColorEditFlags_InputMask_))
            flags |= ((g.ColorEditOptions & ImGuiColorEditFlags_InputMask_) ? g.ColorEditOptions : ImGuiColorEditFlags_DefaultOptions_) & ImGuiColorEditFlags_InputMask_;
        IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_PickerMask_)); // Check that only 1 is selected
        IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags_InputMask_));  // Check that only 1 is selected
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            flags |= (g.ColorEditOptions & ImGuiColorEditFlags_AlphaBar);

        // Setup
        int components = (flags & ImGuiColorEditFlags_NoAlpha) ? 3 : 4;
        bool alpha_bar = (flags & ImGuiColorEditFlags_AlphaBar) && !(flags & ImGuiColorEditFlags_NoAlpha);
        const ImVec2 picker_pos = window->DC.CursorPos;
        const float picker_width = 198.f;
        const float picker_height = 115.f;
        const float button_area = 38.f;
        const float bars_width = picker_width - button_area;
        const float bars_height = 12.f;
        const float bars_pos_x = picker_pos.x + button_area;
        const float hue_bar_pos_y = picker_pos.y + picker_height + 8;
        const float alpha_bar_pos_y = picker_pos.y + picker_height + 28;

        float backup_initial_col[4];
        memcpy(backup_initial_col, col, components * sizeof(float));

        float wheel_thickness = picker_width * 0.08f;
        float wheel_r_outer = picker_width * 0.50f;
        float wheel_r_inner = wheel_r_outer - wheel_thickness;
        ImVec2 wheel_center(picker_pos.x + (picker_width + bars_width) * 0.5f, picker_pos.y + picker_width * 0.5f);

        // Note: the triangle is displayed rotated with triangle_pa pointing to Hue, but most coordinates stays unrotated for logic.
        float triangle_r = wheel_r_inner - (int)(picker_width * 0.027f);
        ImVec2 triangle_pa = ImVec2(triangle_r, 0.0f); // Hue point.
        ImVec2 triangle_pb = ImVec2(triangle_r * -0.5f, triangle_r * -0.866025f); // Black point.
        ImVec2 triangle_pc = ImVec2(triangle_r * -0.5f, triangle_r * +0.866025f); // White point.

        float H = col[0], S = col[1], V = col[2];
        float R = col[0], G = col[1], B = col[2];
        if (flags & ImGuiColorEditFlags_InputRGB)
        {
            // Hue is lost when converting from grayscale rgb (saturation=0). Restore it.
            ColorConvertRGBtoHSV(R, G, B, H, S, V);
            ColorEditRestoreHS(col, &H, &S, &V);
        }
        else if (flags & ImGuiColorEditFlags_InputHSV)
        {
            ColorConvertHSVtoRGB(H, S, V, R, G, B);
        }

        bool value_changed = false, value_changed_h = false, value_changed_sv = false;

        PushItemFlag(ImGuiItemFlags_NoNav, true);
        if (flags & ImGuiColorEditFlags_PickerHueWheel)
        {
            // Hue wheel + SV triangle logic
            InvisibleButton("hsv", ImVec2(picker_width + style.ItemInnerSpacing.x + bars_width, picker_height));
            if (IsItemActive())
            {
                ImVec2 initial_off = g.IO.MouseClickedPos[0] - wheel_center;
                ImVec2 current_off = g.IO.MousePos - wheel_center;
                float initial_dist2 = ImLengthSqr(initial_off);
                if (initial_dist2 >= (wheel_r_inner - 1) * (wheel_r_inner - 1) && initial_dist2 <= (wheel_r_outer + 1) * (wheel_r_outer + 1))
                {
                    // Interactive with Hue wheel
                    H = ImAtan2(current_off.y, current_off.x) / IM_PI * 0.5f;
                    if (H < 0.0f)
                        H += 1.0f;
                    value_changed = value_changed_h = true;
                }
                float cos_hue_angle = ImCos(-H * 2.0f * IM_PI);
                float sin_hue_angle = ImSin(-H * 2.0f * IM_PI);
                if (ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, ImRotate(initial_off, cos_hue_angle, sin_hue_angle)))
                {
                    // Interacting with SV triangle
                    ImVec2 current_off_unrotated = ImRotate(current_off, cos_hue_angle, sin_hue_angle);
                    if (!ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated))
                        current_off_unrotated = ImTriangleClosestPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated);
                    float uu, vv, ww;
                    ImTriangleBarycentricCoords(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated, uu, vv, ww);
                    V = ImClamp(1.0f - vv, 0.0001f, 1.0f);
                    S = ImClamp(uu / V, 0.0001f, 1.0f);
                    value_changed = value_changed_sv = true;
                }
            }
        }
        else if (flags & ImGuiColorEditFlags_PickerHueBar)
        {
            // SV rectangle logic
            InvisibleButton("sv", ImVec2(picker_width, picker_height));
            if (IsItemActive())
            {
                S = ImSaturate((io.MousePos.x - picker_pos.x) / (picker_width - 1));
                V = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (picker_height - 1));
                ColorEditRestoreH(col, &H); // Greatly reduces hue jitter and reset to 0 when hue == 255 and color is rapidly modified using SV square.
                value_changed = value_changed_sv = true;
            }

            // Hue bar logic
            SetCursorScreenPos(ImVec2(bars_pos_x, hue_bar_pos_y));
            InvisibleButton("hue", ImVec2(bars_width, bars_height));
            if (IsItemActive())
            {
                H = 1.f - ImSaturate((io.MousePos.x - bars_pos_x) / (bars_width - 1));
                value_changed = value_changed_h = true;
            }
        }

        // Alpha bar logic
        if (alpha_bar)
        {
            SetCursorScreenPos(ImVec2(bars_pos_x, alpha_bar_pos_y));
            InvisibleButton("alpha", ImVec2(bars_width, bars_height));
            if (IsItemActive())
            {
                col[3] = ImSaturate((io.MousePos.x - bars_pos_x) / (bars_width - 1));
                value_changed = true;
            }
        }
        PopItemFlag(); // ImGuiItemFlags_NoNav

        // Convert back color to RGB
        if (value_changed_h || value_changed_sv)
        {
            if (flags & ImGuiColorEditFlags_InputRGB)
            {
                ColorConvertHSVtoRGB(H, S, V, col[0], col[1], col[2]);
                g.ColorEditSavedHue = H;
                g.ColorEditSavedSat = S;
                g.ColorEditSavedID = g.ColorEditCurrentID;
                g.ColorEditSavedColor = ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 0));
            }
            else if (flags & ImGuiColorEditFlags_InputHSV)
            {
                col[0] = H;
                col[1] = S;
                col[2] = V;
            }
        }

        // R,G,B and H,S,V slider color editor
        bool value_changed_fix_hue_wrap = false;
        // Try to cancel hue wrap (after ColorEdit4 call), if any
        if (value_changed_fix_hue_wrap && (flags & ImGuiColorEditFlags_InputRGB))
        {
            float new_H, new_S, new_V;
            ColorConvertRGBtoHSV(col[0], col[1], col[2], new_H, new_S, new_V);
            if (new_H <= 0 && H > 0)
            {
                if (new_V <= 0 && V != new_V)
                    ColorConvertHSVtoRGB(H, S, new_V <= 0 ? V * 0.5f : new_V, col[0], col[1], col[2]);
                else if (new_S <= 0)
                    ColorConvertHSVtoRGB(H, new_S <= 0 ? S * 0.5f : new_S, new_V, col[0], col[1], col[2]);
            }
        }

        if (value_changed)
        {
            if (flags & ImGuiColorEditFlags_InputRGB)
            {
                R = col[0];
                G = col[1];
                B = col[2];
                ColorConvertRGBtoHSV(R, G, B, H, S, V);
                ColorEditRestoreHS(col, &H, &S, &V);   // Fix local Hue as display below will use it immediately.
            }
            else if (flags & ImGuiColorEditFlags_InputHSV)
            {
                H = col[0];
                S = col[1];
                V = col[2];
                ColorConvertHSVtoRGB(H, S, V, R, G, B);
            }
        }

        const int style_alpha8 = IM_F32_TO_INT8_SAT(style.Alpha);
        const ImU32 col_black = IM_COL32(0, 0, 0, style_alpha8);
        const ImU32 col_white = IM_COL32(255, 255, 255, style_alpha8);
        const ImU32 col_midgrey = IM_COL32(128, 128, 128, style_alpha8);
        const ImU32 col_hues[7] = { IM_COL32(255,0,0,style_alpha8), IM_COL32(255,0,255,style_alpha8), IM_COL32(0,0,255,style_alpha8),IM_COL32(0,255,255,style_alpha8), IM_COL32(0,255,0,style_alpha8), IM_COL32(255,255,0,style_alpha8), IM_COL32(255,0,0,style_alpha8) };

        ImVec4 hue_color_f(1, 1, 1, style.Alpha); ColorConvertHSVtoRGB(H, 1, 1, hue_color_f.x, hue_color_f.y, hue_color_f.z);
        ImU32 hue_color32 = ColorConvertFloat4ToU32(hue_color_f);
        ImU32 user_col32_striped_of_alpha = ColorConvertFloat4ToU32(ImVec4(R, G, B, style.Alpha)); // Important: this is still including the main rendering/style alpha!!

        ImVec2 sv_cursor_pos;

        if (flags & ImGuiColorEditFlags_PickerHueWheel)
        {
            // Render Hue Wheel
            const float aeps = 0.5f / wheel_r_outer; // Half a pixel arc length in radians (2pi cancels out).
            const int segment_per_arc = ImMax(4, (int)wheel_r_outer / 12);
            for (int n = 0; n < 6; n++)
            {
                const float a0 = (n) / 6.0f * 2.0f * IM_PI - aeps;
                const float a1 = (n + 1.0f) / 6.0f * 2.0f * IM_PI + aeps;
                const int vert_start_idx = draw_list->VtxBuffer.Size;
                draw_list->PathArcTo(wheel_center, (wheel_r_inner + wheel_r_outer) * 0.5f, a0, a1, segment_per_arc);
                draw_list->PathStroke(col_white, 0, wheel_thickness);
                const int vert_end_idx = draw_list->VtxBuffer.Size;

                // Paint colors over existing vertices
                ImVec2 gradient_p0(wheel_center.x + ImCos(a0) * wheel_r_inner, wheel_center.y + ImSin(a0) * wheel_r_inner);
                ImVec2 gradient_p1(wheel_center.x + ImCos(a1) * wheel_r_inner, wheel_center.y + ImSin(a1) * wheel_r_inner);
                ShadeVertsLinearColorGradientKeepAlpha(draw_list, vert_start_idx, vert_end_idx, gradient_p0, gradient_p1, col_hues[n], col_hues[n + 1]);
            }

            // Render Cursor + preview on Hue Wheel
            float cos_hue_angle = ImCos(H * 2.0f * IM_PI);
            float sin_hue_angle = ImSin(H * 2.0f * IM_PI);
            ImVec2 hue_cursor_pos(wheel_center.x + cos_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f, wheel_center.y + sin_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f);
            float hue_cursor_rad = value_changed_h ? wheel_thickness * 0.65f : wheel_thickness * 0.55f;
            int hue_cursor_segments = draw_list->_CalcCircleAutoSegmentCount(hue_cursor_rad); // Lock segment count so the +1 one matches others.
            draw_list->AddCircleFilled(hue_cursor_pos, hue_cursor_rad, hue_color32, hue_cursor_segments);
            draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad + 1, col_midgrey, hue_cursor_segments);
            draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad, col_white, hue_cursor_segments);

            // Render SV triangle (rotated according to hue)
            ImVec2 tra = wheel_center + ImRotate(triangle_pa, cos_hue_angle, sin_hue_angle);
            ImVec2 trb = wheel_center + ImRotate(triangle_pb, cos_hue_angle, sin_hue_angle);
            ImVec2 trc = wheel_center + ImRotate(triangle_pc, cos_hue_angle, sin_hue_angle);
            ImVec2 uv_white = GetFontTexUvWhitePixel();
            draw_list->PrimReserve(3, 3);
            draw_list->PrimVtx(tra, uv_white, hue_color32);
            draw_list->PrimVtx(trb, uv_white, col_black);
            draw_list->PrimVtx(trc, uv_white, col_white);
            draw_list->AddTriangle(tra, trb, trc, col_midgrey, 1.5f);
            sv_cursor_pos = ImLerp(ImLerp(trc, tra, ImSaturate(S)), trb, ImSaturate(1 - V));
        }
        else if (flags & ImGuiColorEditFlags_PickerHueBar)
        {
            // Render SV Square
            AddRectFilledMultiColor(draw_list, picker_pos, picker_pos + ImVec2(picker_width, picker_height - 1), col_white, hue_color32, hue_color32, col_white, 4.f);
            AddRectFilledMultiColor(draw_list, picker_pos, picker_pos + ImVec2(picker_width, picker_height), 0, 0, col_black, col_black, 4.f);
            RenderFrameBorder(picker_pos, picker_pos + ImVec2(picker_width, picker_height), 0.0f);
            sv_cursor_pos.x = ImClamp(IM_ROUND(picker_pos.x + ImSaturate(S) * picker_width), picker_pos.x + 2, picker_pos.x + picker_width - 2); // Sneakily prevent the circle to stick out too much
            sv_cursor_pos.y = ImClamp(IM_ROUND(picker_pos.y + ImSaturate(1 - V) * picker_height), picker_pos.y + 2, picker_pos.y + picker_height - 2);

            // Render Hue Bar
            for (int i = 0; i < 6; ++i)
                AddRectFilledMultiColor(draw_list, ImVec2(bars_pos_x + i * (bars_width / 6) - (i == 1 || i == 5 ? 1 : 0), hue_bar_pos_y + 2), ImVec2(bars_pos_x + (i + 1) * (bars_width / 6), hue_bar_pos_y + (bars_height - 2)), col_hues[i], col_hues[i + 1], col_hues[i + 1], col_hues[i], i == 0 || i == 5 ? 4.f : 0.f, i == 0 ? ImDrawFlags_RoundCornersLeft : i == 5 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone);
            float bar0_line_x = IM_ROUND(bars_pos_x + (1.f - H) * bars_width);
            bar0_line_x = ImClamp(bar0_line_x, bars_pos_x + 5.f, bars_pos_x + bars_width - 6.f);
            draw_list->AddCircleFilled(ImVec2(bar0_line_x, hue_bar_pos_y + 6), 6.f, col_white, 30);

        }

        // Render cursor/preview circle (clamp S/V within 0..1 range because floating points colors may lead HSV values to be out of range)
        float sv_cursor_rad = value_changed_sv ? 10.0f : 6.0f;
        int sv_cursor_segments = draw_list->_CalcCircleAutoSegmentCount(sv_cursor_rad); // Lock segment count so the +1 one matches others.
        draw_list->AddShadowCircle(sv_cursor_pos, 5.f, GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.78f)), 20.f, ImVec2(0, 2));
        draw_list->AddCircle(sv_cursor_pos, 5.f, col_white, 12);
        draw_list->AddCircleFilled(sv_cursor_pos, 4.f, user_col32_striped_of_alpha, 12);

        // Render alpha bar
        if (alpha_bar)
        {
            float alpha = ImSaturate(col[3]);
            AddRectFilledMultiColor(draw_list, ImVec2(bars_pos_x, alpha_bar_pos_y + 2), ImVec2(bars_pos_x + bars_width, alpha_bar_pos_y + bars_height - 2), col_black, hue_color32, hue_color32, col_black, 4.f);
            float bar1_line_x = IM_ROUND(bars_pos_x + alpha * bars_width);
            bar1_line_x = ImClamp(bar1_line_x, bars_pos_x + 5.f, bars_pos_x + bars_width - 6.f);
            draw_list->AddCircleFilled(ImVec2(bar1_line_x, alpha_bar_pos_y + 6), 6.f, col_white, 30);
        }

        EndGroup();

        if (value_changed && memcmp(backup_initial_col, col, components * sizeof(float)) == 0)
            value_changed = false;
        if (value_changed && g.LastItemData.ID != 0) // In case of ID collision, the second EndGroup() won't catch g.ActiveId
            MarkItemEdited(g.LastItemData.ID);

        if (set_current_color_edit_id)
            g.ColorEditCurrentID = 0;
        PopID();

        return value_changed;
    }

    bool ColorButton(const char* desc_id, const ImVec4& col)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID id = window->GetID(desc_id);

        const float width = GetWindowWidth();
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect rect(pos, pos + ImVec2(width, 40));
        ItemSize(rect, 0.0f);
        if (!ItemAdd(rect, id))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(rect, id, &hovered, &held);

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::color_background), settings::widgets_rounding);
        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(colors::color_label), desc_id);
        RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(rect.Max.x - 57, rect.Min.y + 10), rect.Max - ImVec2(12, 10), GetColor(col), 5.f, ImVec2(0, 0), 5.f);

        return pressed;
    }

    bool ColorEdit(const char* label, float col[4], bool alpha)
    {
        struct color_state
        {
            bool active = false;
            bool hovered = false;
            float alpha = 0.f;
            bool pick_active = false;
            ImVec4 pick_label = colors::color_pick_inactive;
        };

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const float square_sz = GetFrameHeight();
        const float w_full = CalcItemWidth();
        const float w_button = (0 & ImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
        const float w_inputs = w_full - w_button;
        const char* label_display_end = FindRenderedTextEnd(label);
        g.NextItemData.ClearFlags();

        BeginGroup();
        PushID(label);
        const bool set_current_color_edit_id = (g.ColorEditCurrentID == 0);
        if (set_current_color_edit_id)
            g.ColorEditCurrentID = window->IDStack.back();

        const int components = alpha ? 4 : 3;

        // Convert to the formats we need
        float f[4] = { col[0], col[1], col[2], alpha ? col[3] : 1.0f };
        int i[4] = { IM_F32_TO_INT8_UNBOUND(f[0]), IM_F32_TO_INT8_UNBOUND(f[1]), IM_F32_TO_INT8_UNBOUND(f[2]), IM_F32_TO_INT8_UNBOUND(f[3]) };

        bool value_changed = false;
        bool value_changed_as_float = false;

        const ImVec2 pos = window->DC.CursorPos;
        const float inputs_offset_x = (style.ColorButtonPosition == ImGuiDir_Left) ? w_button : 0.0f;

        ImGuiWindow* picker_active_window = NULL;

        static std::map<ImGuiID, color_state> anim;
        color_state& state = anim[GetID(label)];

        std::string window_name = "picker";
        window_name += label;

        const ImVec4 col_v4(col[0], col[1], col[2], alpha ? col[3] : 1.0f);
        if (ColorButton(label, col_v4))
        {
            g.ColorPickerRef = col_v4;
        }

        if (ItemHoverable(g.LastItemData.Rect, g.LastItemData.ID, 0) && g.IO.MouseClicked[0] || (state.active && g.IO.MouseClicked[0] && !state.hovered) && !state.pick_active)
            state.active = !state.active;

        state.alpha = ImClamp(state.alpha + (FixedSpeed(8.f) * (state.active ? 1.f : -1.f)), 0.f, 1.f);
        state.pick_label = ImLerp(state.pick_label, state.pick_active ? colors::color_pick_active : colors::color_pick_inactive, FixedSpeed(8.f));

        SetNextWindowSize(ImVec2(218, 175));
        SetNextWindowPos(g.LastItemData.Rect.GetBR() - ImVec2(143, -5));
        PushStyleVar(ImGuiStyleVar_Alpha, state.alpha);
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
        PushStyleColor(ImGuiCol_WindowBg, GetColorU32(colors::color_window));

        if (state.alpha >= 0.01f);
        {
            Begin(window_name.c_str(), NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysUseWindowPadding);
            {
                state.hovered = IsWindowHovered();
                picker_active_window = g.CurrentWindow;

                ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar;
                ImGuiColorEditFlags picker_flags = (ImGuiColorEditFlags_None & picker_flags_to_forward) | ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
                SetCursorPos(GetCursorPos() + ImVec2(10, 10));
                value_changed |= ColorPicker4("##picker", col, picker_flags | (alpha ? (ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview) : 0), &g.ColorPickerRef.x);

                picker_active_window->DrawList->AddRectFilled(GetWindowPos() + ImVec2(10, 135), GetWindowPos() + ImVec2(38, 163), GetColor(colors::color_background), 4.f);
                picker_active_window->DrawList->AddText(settings::pick_button, settings::pick_button->FontSize, GetWindowPos() + ImVec2(18, 143), GetColor(state.pick_label), "B");

                if (IsMouseHoveringRect(GetWindowPos() + ImVec2(10, 135), GetWindowPos() + ImVec2(38, 163)) && g.IO.MouseClicked[0])
                    state.pick_active = !state.pick_active;

                if (state.pick_active && g.IO.MouseClicked[0] && !IsMouseHoveringRect(GetWindowPos() + ImVec2(10, 135), GetWindowPos() + ImVec2(38, 163)))
                {
                    HDC hdcScreen = GetDC(NULL);
                    COLORREF pick_color = GetPixel(hdcScreen, GetMousePos().x, GetMousePos().y);
                    ReleaseDC(NULL, hdcScreen);

                    col[0] = static_cast<float>(GetRValue(pick_color)) / 255.0f;
                    col[1] = static_cast<float>(GetGValue(pick_color)) / 255.0f;
                    col[2] = static_cast<float>(GetBValue(pick_color)) / 255.0f;

                    state.pick_active = false;
                }

            }
            End();
        }
        PopStyleVar(3);
        PopStyleColor();

        // Convert back
        if (value_changed && picker_active_window == NULL)
        {
            if (!value_changed_as_float)
                for (int n = 0; n < 4; n++)
                    f[n] = i[n] / 255.0f;

            col[0] = f[0];
            col[1] = f[1];
            col[2] = f[2];
            if (alpha)
                col[3] = f[3];
        }

        if (set_current_color_edit_id)
            g.ColorEditCurrentID = 0;
        PopID();
        EndGroup();

        // When picker is being actively used, use its active id so IsItemActive() will function on ColorEdit4().
        if (picker_active_window && g.ActiveId != 0 && g.ActiveIdWindow == picker_active_window)
            g.LastItemData.ID = g.ActiveId;

        if (value_changed && g.LastItemData.ID != 0) // In case of ID collision, the second EndGroup() won't catch g.ActiveId
            MarkItemEdited(g.LastItemData.ID);

        return value_changed;
    }

    const char* keys[] =
    {
        "None",
        "Mouse 1",
        "Mouse 2",
        "CN",
        "Mouse 3",
        "Mouse 4",
        "Mouse 5",
        "-",
        "Back",
        "Tab",
        "-",
        "-",
        "CLR",
        "Enter",
        "-",
        "-",
        "Shift",
        "CTL",
        "Menu",
        "Pause",
        "Caps",
        "KAN",
        "-",
        "JUN",
        "FIN",
        "KAN",
        "-",
        "Escape",
        "CON",
        "NCO",
        "ACC",
        "MAD",
        "Space",
        "PGU",
        "PGD",
        "End",
        "Home",
        "Left",
        "Up",
        "Right",
        "Down",
        "SEL",
        "PRI",
        "EXE",
        "PRI",
        "INS",
        "Delete",
        "HEL",
        "0",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "G",
        "H",
        "I",
        "J",
        "K",
        "L",
        "M",
        "N",
        "O",
        "P",
        "Q",
        "R",
        "S",
        "T",
        "U",
        "V",
        "W",
        "X",
        "Y",
        "Z",
        "WIN",
        "WIN",
        "APP",
        "-",
        "SLE",
        "Num 0",
        "Num 1",
        "Num 2",
        "Num 3",
        "Num 4",
        "Num 5",
        "Num 6",
        "Num 7",
        "Num 8",
        "Num 9",
        "MUL",
        "ADD",
        "SEP",
        "MIN",
        "Delete",
        "DIV",
        "F1",
        "F2",
        "F3",
        "F4",
        "F5",
        "F6",
        "F7",
        "F8",
        "F9",
        "F10",
        "F11",
        "F12",
        "F13",
        "F14",
        "F15",
        "F16",
        "F17",
        "F18",
        "F19",
        "F20",
        "F21",
        "F22",
        "F23",
        "F24",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "NUM",
        "SCR",
        "EQU",
        "MAS",
        "TOY",
        "OYA",
        "OYA",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "Shift",
        "Shift",
        "Ctrl",
        "Ctrl",
        "Alt",
        "Alt"
    };


    bool Keybind(const char* label, int* key, int* mode)
    {
        struct key_state
        {
            bool active = false;
            bool hovered = false;
            float alpha = 0.f;
        };

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        const ImGuiStyle& style = g.Style;

        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const float width = GetWindowWidth();
        const ImRect rect(pos, pos + ImVec2(width, 40));
        ImGui::ItemSize(rect, style.FramePadding.y);
        if (!ImGui::ItemAdd(rect, id, &rect))
            return false;

        static std::map<ImGuiID, key_state> anim;
        key_state& state = anim[id];

        PushFont(settings::lexend_deca_medium_widgets);
        char buf_display[64] = "Select";
        PopFont();

        bool value_changed = false;
        int k = *key;

        std::string active_key = "";
        active_key += keys[*key];

        if (*key != 0 && g.ActiveId != id) {
            strcpy_s(buf_display, active_key.c_str());
        }
        else if (g.ActiveId == id) {
            strcpy_s(buf_display, "Select");
        }

        bool hovered = ItemHoverable(rect, id, 0);

        if ( hovered && GetAsyncKeyState( VK_LBUTTON ) )
        {
            if ( g.ActiveId != id ) {
                // Start edition
                memset( io.MouseDown, 0, sizeof( io.MouseDown ) );
                memset( io.KeysDown, 0, sizeof( io.KeysDown ) );
                *key = 0;
            }
            ImGui::SetActiveID( id, window );
            ImGui::FocusWindow( window );
        }
        else if ( GetAsyncKeyState( VK_LBUTTON ) ) {
            // Release focus when we click outside
            if (g.ActiveId == id)
                ImGui::ClearActiveID();
        }

        if (g.ActiveId == id) {
            if (!value_changed) {
                for ( auto i = 0x00; i <= 0xA5; i++ ) {
                    if ( GetAsyncKeyState( i ) & 1 ) {
                        k = i;
                        value_changed = true;
                        ImGui::ClearActiveID( );
                    }
                }
            }

            if (IsKeyPressedMap(ImGuiKey_Escape)) {
                *key = 0;
                ImGui::ClearActiveID();
            }
            else {
                *key = k;
            }
        }

        const float buf_width = CalcTextSize(buf_display).x;

        window->DrawList->AddRectFilled(rect.Min, rect.Max, GetColor(colors::key_background), settings::widgets_rounding);
        window->DrawList->AddText(settings::lexend_deca_medium_widgets, settings::lexend_deca_medium_widgets->FontSize, rect.Min + ImVec2(12, 13), GetColor(colors::key_label), label);
        window->DrawList->AddRectFilled(ImVec2(rect.Max.x - 22 - buf_width, rect.Min.y + 8), rect.Max - ImVec2(12, 8), GetColor(colors::key_rect), 4.f, state.active ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);

        PushFont(settings::lexend_deca_medium_widgets);
        PushStyleColor(ImGuiCol_Text, GetColor(colors::key_value));
        RenderTextClipped(ImVec2(rect.Max.x - 22 - buf_width, rect.Min.y + 7), rect.Max - ImVec2(12, 9), buf_display, NULL, NULL, ImVec2(0.5f, 0.5f));
        PopStyleColor();
        PopFont();


        if (hovered && g.IO.MouseClicked[1] || state.active && (g.IO.MouseClicked[0] || g.IO.MouseClicked[1]) && !state.hovered)
            state.active = !state.active;

        state.alpha = ImClamp(state.alpha + (8.f * g.IO.DeltaTime * (state.active ? 1.f : -1.f)), 0.f, 1.f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing;

        if (state.alpha >= 0.01f)
        {
            PushStyleVar(ImGuiStyleVar_Alpha, state.alpha);
            PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleColor(ImGuiCol_WindowBg, GetColor(colors::key_rect));
            SetNextWindowSize(ImVec2(100, CalcMaxPopupHeightFromItemCount(3, 24.f)));
            SetNextWindowPos(ImRect(ImVec2(rect.Max.x - 22 - buf_width, rect.Min.y + 8), rect.Max - ImVec2(12, 8)).GetCenter() - ImVec2(100 / 2, -12));

            Begin(label, NULL, window_flags);
            {
                if (SelectableEx("Hold", *mode == 0))
                {
                    *mode = 0;
                    state.active = false;
                }
                if (SelectableEx("Toggle", *mode == 1))
                {
                    *mode = 1;
                    state.active = false;
                }
                if (SelectableEx("Always", *mode == 2))
                {
                    *mode = 2;
                    state.active = false;
                }
            }
            End();
            PopStyleVar(3);
            PopStyleColor();
        }

        return value_changed;
    }

}
