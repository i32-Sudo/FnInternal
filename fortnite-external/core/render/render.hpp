#include <Windows.h>
#include <iostream>

namespace atx
{
	class render_c
	{
	public:
		auto Initialize( ) -> bool;
		auto RenderMenu( ) -> void;
		auto RenderThread( ) -> void;
	};
	inline render_c render;
}