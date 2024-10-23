#include "core.hpp"

bool isProcessOpen(const std::wstring& processName) {
	while (true) {
		auto iPid = atx::memory.get_process_pid(processName);

		if (iPid != 0) {
			atx::Interface.Print(_("(!) Process found\n"));
			return true;
		}
	}
}


auto atx::core_c::Initialize( ) -> bool
{
	PrepareForUIAccess( );

	if ( !atx::screen.GetScreenSize( ) )
	{
		atx::Interface.Print( _( "(!) failed to get screen size.\n" ) );
		return false;
	}

	if ( !atx::memory.initialize_handle( ) )
	{
		atx::Interface.Print( _( "(!) failed to establish a connection with the kernel module.\n" ) );
		return false;
	}

	if ( !atx::interception.Initialize( ) )
	{
		atx::Interface.Print( _( "(!) failed to establish a connection with the mouse driver.\n" ) );
		return false;
	}
	bool processOpen = isProcessOpen(static_cast<std::wstring>(_(L"FortniteClient-Win64-Shipping.exe")));
	if (!processOpen) {
		atx::Interface.Print(_("(!) failed to find the target process\n"));
		return false;
	}

	MessageBoxA(0, _("Press [OK] In Lobby."), _("Alert"), MB_OK);


	auto iPid = atx::memory.get_process_pid( static_cast< std::wstring >( _( L"FortniteClient-Win64-Shipping.exe" ) ) );
	if ( !atx::memory.attach( iPid ) )
	{
		atx::Interface.Print( _( "(!) failed to find the target process.\n" ) );
		return false;
	}
	atx::Interface.Print( _( "(+) process pid: " ), iPid );

	if ( !atx::memory.get_image_base( nullptr ) )
	{
		atx::Interface.Print( _( "(!) failed to find the target's base address.\n" ) );
		return false;
	}
	atx::Interface.Print( _( "(+) base address: " ), atx::memory.image_base );

	if ( !atx::memory.get_cr3( atx::memory.image_base ) )
	{
		atx::Interface.Print( _( "(!) failed to resolve process cr3.\n" ) );
		return false;
	}
	atx::Interface.Print( _( "(+) cr3: " ), atx::memory.dtb );

	if ( !atx::memory.get_text_section( ) )
	{
		atx::Interface.Print( _( "(!) failed to get .text section.\n" ) );
		return false;
	}
	atx::Interface.Print( _( "(+) .text section: " ), atx::memory.text_section );


	std::thread( [ & ]( ) { atx::cache.Data( ); } ).detach( );
	std::thread( [ & ]( ) { atx::cache.Entities( ); } ).detach( );
	std::thread( [ & ]( ) { atx::combat.CombatThread( ); } ).detach( );

	if ( !atx::render.Initialize( ) )
	{
		atx::Interface.Print( _( "(!) failed to initialize dx11.\n" ) );
		return false;
	}
	else
		atx::render.RenderThread( );

	std::cin.get( );
}