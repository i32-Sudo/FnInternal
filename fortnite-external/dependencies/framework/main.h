#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_custom.h"
#include <imgui_freetype.h>
#include <D3DX11tex.h>
#pragma comment(lib, "D3DX11.lib")
#include <d3d11.h>
#include <tchar.h>

#include "image.h"
#include "font.h"

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

DWORD win_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
DWORD picker_flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview;

bool scroll_active = true;

namespace widget_var
{
	const char* user = "PAST OWL";
	const char* UID = "UID: 21095";
	const char* status = "[MODERATOR]";

	static int select = 0;
	const char* items[2]{ "Default", "Relatation" };

	static bool body_active[5] = { true, true, true, false };
	const char* body_select[5] = { "Head", "Chest", "Body", "Legs" };

	int page = 0, sub_page = 0;

	int x_pos0 = (1920 / 2), x_pos1 = (1080 / 2);

	float x_pos2 = (0.5f / 2), x_pos3 = (0.5f / 2);

	int knob_int = 50;
	float knob_float = 0.75;

	bool enemy_list = true;
	bool show_binds = true;

	float matrix_speed = 0.850f;

	static float hue_color[4] = { 0 / 255.f, 255 / 255.f, 255 / 255.f, 1.f };

	static float shot_color[4] = { 0 / 255.f, 255 / 255.f, 209 / 255.f, 1.f };
	static float shot_color1[4] = { 0 / 255.f, 255 / 255.f, 209 / 255.f, 1.f };

	static float color[4] = { 0 / 255.f, 255 / 255.f, 209 / 255.f, 255 / 255.f };
	static float bg_color[4] = { 0 / 255.f, 0 / 255.f, 0 / 255.f, 160 / 255.f };

	bool enable_aim = true;
	bool silent = true;
	int fov = 90;
	bool override_resolver = true;
	bool auto_stop = false;
	bool d_tap = true;

	bool bg_blur = true;

	float map_x = (0.5f / 2), map_y = (0.5f / 2);


	float sl_float = 0.f;

	char search[64] = {};

	int smooth = 0;

	float hit_chance = 45.f;
	float misses = 45.f;

	int key, m;
}

namespace preview
{
	bool show_preview = false;
	bool show_nick = false;
	bool show_weapon = false;
	bool show_bomb = false;
	bool show_scop = false;
	bool show_money = false;
	bool show_smoke = false;

	float preview_alpha = 0.f;

	static float box_color[4] = { 0.5f, 0.3f, 0.2f, 1.f };
	static float nick_color[4] = { 0.3f, 1.f, 1.f, 1.f };

	static int hp_side = 0;
	const char* hp_item[2] { "Left side", "Right side" };

	int namepos_x = 121, namepos_y = 22;
	int weaponpos_x = 131, weaponpos_y = 490;
	int bombpos_x = 47, bombpos_y = 86;
	int scoppos_x = 47, scoppos_y = 65;
	int moneypos_x = 202, moneypos_y = 66;
	int smokepos_x = 40, smokepos_y = 475;

	static float hp_position, name_alpha, weapon_alpha, bomb_alpha, scop_alpha, money_alpha, smoke_alpha;
}

namespace texture
{
	ID3D11ShaderResourceView* background = nullptr;
	ID3D11ShaderResourceView* background_blurred = nullptr;

	ID3D11ShaderResourceView* circle_user = nullptr;
	ID3D11ShaderResourceView* esp_preview = nullptr;

}

namespace font
{
	ImFont* inter_bold_widget = nullptr;
	ImFont* inter_black_name = nullptr;
	ImFont* inter_black_widget = nullptr;
	ImFont* inter_semibold_widget = nullptr;

	inline ImFont* icomoon[14];
}