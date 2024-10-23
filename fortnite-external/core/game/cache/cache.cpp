#include "cache.hpp"
#include <core/game/sdk.hpp>
#include <iostream>

FVector vCachedLastFiredLocation = {};
uintptr_t fWeaponCached = 0;
bool bHasBeenCached = 0;
auto atx::cache_c::Data() -> void
{
    for (;; )
    {
        Cached::GWorld = UWorld::Get();

        if (!Cached::GWorld)
            continue;

        Cached::GameState = Cached::GWorld->GetGameState();
        if (!Cached::GameState)
            continue;

        auto GameInstance = Cached::GWorld->GetGameInstance();
        auto LocalPlayer = GameInstance->GetLocalPlayer();

        Cached::PlayerController = LocalPlayer->GetPlayerController();
        Cached::ViewState = LocalPlayer->GetViewState();

        Cached::Pawn = Cached::PlayerController->GetPawn();

        auto CurrentWeapon = Cached::Pawn->GetCurrentWeapon();
        if (CurrentWeapon)
        {
            auto iWeaponAnimation = CurrentWeapon->GetWeaponAnimation();
            Cached::FortWeaponType = CurrentWeapon->SimplifyWeaponType(iWeaponAnimation);
            if (!bHasBeenCached)
            {
                fWeaponCached = std::uintptr_t(CurrentWeapon);
                bHasBeenCached = true;
            }
        }


        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

auto atx::cache_c::Entities() -> void
{
    for (;; )
    {
        // here something wrong bro
        auto ActorArray = Cached::GameState->GetPlayerArray();
        // atx::Interface.Print(_("</> Player Count: "), ActorArray.iCount);
       //  atx::Interface.Print(_("GameState: "), Cached::GameState);
        for (int i = 0; i < ActorArray.iCount; ++i)
        {
            auto PlayerState = ActorArray.Get(i);
            auto Pawn = PlayerState->GetPawnPrivate();
            auto Mesh = Pawn->GetMesh();

            if (!Pawn)
                continue;
               // atx::Interface.Print(_("</> Pawn: "), Pawn);

            if (!Mesh)
                continue;
              //atx::Interface.Print(_("</> Mesh: "), Mesh);

            if (Pawn == Cached::Pawn)
            {
                Cached::TeamIndex = PlayerState->GetTeamIndex();
                 //  atx::Interface.Print(_("</> Local Pawn @ "), i);
                continue;
            }

            auto aBoneArray = atx::memory.read<TArray<std::uintptr_t>>((uintptr_t)Mesh + 0x570 + (atx::memory.read<int>((uintptr_t)Mesh + (0x600)) * 0x10));
            if (!aBoneArray.tData)
                continue;

           //  atx::Interface.Print(_("</> Bone Array: "), aBoneArray.tData);


            Actor CachedActor{ Pawn, PlayerState, Mesh, reinterpret_cast<std::uintptr_t>(aBoneArray.tData), PlayerState->GetPlayerName() };
            TemporaryActorList.insert(TemporaryActorList.end(), CachedActor);
        }

        ActorList.swap(TemporaryActorList);
        TemporaryActorList.clear();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}