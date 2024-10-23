#pragma once

#include "Interception.h"
#include <thread>

extern InterceptionContext context;
extern InterceptionDevice device;
extern InterceptionStroke stroke;
extern void NormalMouse();

namespace atx
{
	class interception_c
	{
	public:
		inline bool Initialize( );
		inline void SetCursorPosition( int x, int y );
		inline void LeftClick( );
	}; static interception_c interception;
}

inline InterceptionContext context;
inline InterceptionDevice device;
inline InterceptionStroke stroke;

inline void NormalMouse( ) {

	while ( interception_receive( context, device = interception_wait( context ), &stroke, 1 ) > 0 ) {
		if ( interception_is_mouse( device ) )
		{
			InterceptionMouseStroke &mstroke = *( InterceptionMouseStroke * )&stroke;
			interception_send( context, device, &stroke, 1 );
		}
	}
}
// interesting...
inline bool atx::interception_c::Initialize( ) {

	context = interception_create_context( );
	if ( !context )
		return false;

	interception_set_filter( context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_MOVE );

	device = interception_wait( context );
	if ( !device )
		return false;

	while ( interception_receive( context, device = interception_wait( context ), &stroke, 1 ) > 0 ) {
		if ( interception_is_mouse( device ) ) {
			InterceptionMouseStroke &mstroke = *( InterceptionMouseStroke * )&stroke;
			interception_send( context, device, &stroke, 1 );
			break;
		}
	}
	std::thread normal( NormalMouse );
	normal.detach( );

	return true;
}

inline void atx::interception_c::SetCursorPosition( int x, int y )
{
	InterceptionMouseStroke &mstroke = *( InterceptionMouseStroke * )&stroke;
	mstroke.flags = 0;
	mstroke.information = 0;
	mstroke.x = x;
	mstroke.y = y;
	interception_send( context, device, &stroke, 1 );
}

inline void atx::interception_c::LeftClick( ) {

	InterceptionMouseStroke &mstroke = *( InterceptionMouseStroke * )&stroke;
	mstroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN;
	interception_send( context, device, &stroke, 1 );
	mstroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
	interception_send( context, device, &stroke, 1 );
}