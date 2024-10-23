#include <dependencies/framework/imgui.h>

namespace atx
{
	class settings_c
	{
	public:
		bool bAimbot = true;
		
		bool bPrediction = true;
		bool bVisibleCheck = true;
		bool IgnoreDowned = true;
		bool bRenderFOV = true;
		int iFovRadius = 15;
		int iSmooth = 5;
		int iHitBox = 1;
		int iAimbotKeybind = VK_LCONTROL;

		bool bTriggerbot = true;
		bool bShotgunOnly = true;
		bool bIgnoreKeybind = false;
		int iCustomDelay = 100;
		int iMaxDistance = 15;
		int iTriggerbotKeybind = VK_LCONTROL;

		bool bHumanization = true;
		int iMistakeSize = 10;
		int iMistakeCorrection = 10;

		bool bUsername = true;
		bool bDistance = true;
		bool bHeldWeapon = true;
		bool bSkeletons = false;
		bool bBox = true;
		int iBoxType = 0;
		bool bOffScreenIndicator = true;
		bool bBoxOutline = true;
		bool bTextOutline = true;
		int iRenderDistance = 280;

		ImColor iVisibleColor = ImColor( 227, 154, 150 );
		ImColor iInvisibleColor = ImColor( 255, 255, 255 );
	};
	inline settings_c settings;
}