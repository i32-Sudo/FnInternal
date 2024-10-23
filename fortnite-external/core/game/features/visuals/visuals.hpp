#include <dependencies/framework/imgui.h>

namespace atx
{
	class visuals_c
	{
	public:
		auto AddArrowIndicator( ImVec2 vScreenPosition ) -> void;
		auto ActorLoop( ) -> void;
	};
	inline visuals_c visuals;
}