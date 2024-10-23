#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <dependencies/interception/DefsForinterp.h>

namespace atx
{
	class combat_c
	{
	public:
		auto TriggerBot( ) -> void;
		auto CombatThread( ) -> void;
	};
	inline combat_c combat;
}