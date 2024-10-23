#include "combat.hpp"
#include <core/game/sdk.hpp>

auto powf_(float _X, float _Y) -> float { return (_mm_cvtss_f32(_mm_pow_ps(_mm_set_ss(_X), _mm_set_ss(_Y)))); };
auto GetCrossDistance(double x1, double y1, double x2, double y2) -> double { return sqrtf(powf((x2 - x1), 2) + powf_((y2 - y1), 2)); };
template<typename T> static inline T ImLerp(T a, T b, float t) { return (T)(a + (b - a) * t); }

auto SetMousePosition(FVector2D vTarget) -> void
{
	std::srand(static_cast<unsigned>(std::time(nullptr)));
	int iMaxMistakePx = 0;

	if (atx::settings.bHumanization)
		iMaxMistakePx = atx::settings.iMistakeSize;

	FVector2D vScreenCenter(atx::screen.fWidth / 2, atx::screen.fHeight / 2);
	FVector2D vDestination(0, 0);
	int iSmoothFactor = atx::settings.iSmooth + 1;

	if (vTarget.x != 0)
	{
		int xMistake = -iMaxMistakePx + (std::rand() % (2 * iMaxMistakePx + 1));
		vDestination.x = (vTarget.x > vScreenCenter.x) ? -(vScreenCenter.x - vTarget.x + xMistake) : (vTarget.x - vScreenCenter.x + xMistake);
		vDestination.x /= iSmoothFactor;
		vDestination.x = (vDestination.x + vScreenCenter.x > atx::screen.fWidth) ? 0 : vDestination.x;
	}

	if (vTarget.y != 0)
	{
		int yMistake = -iMaxMistakePx + (std::rand() % (2 * iMaxMistakePx + 1));
		vDestination.y = (vTarget.y > vScreenCenter.y) ? -(vScreenCenter.y - vTarget.y + yMistake) : (vTarget.y - vScreenCenter.y + yMistake);
		vDestination.y /= iSmoothFactor;
		vDestination.y = (vDestination.y + vScreenCenter.y > atx::screen.fHeight) ? 0 : vDestination.y;
	}

	if (atx::settings.bHumanization)
	{
		int iSleepTime = 1 + (std::rand() % 3);
		std::this_thread::sleep_for(std::chrono::milliseconds(iSleepTime));
	}

	atx::interception.SetCursorPosition(vDestination.x, vDestination.y);
}

FVector2D vTargetScreen;
FVector vTargetBone;
__int64 pTargetBoneMap = 0;

inline bool bCanClick = false;
std::chrono::steady_clock::time_point tCachedTime;
bool bHasRecentlyClicked = false;
int iTimeSince;
std::chrono::steady_clock::time_point tpBeginning;
std::chrono::steady_clock::time_point tpEnding;
auto atx::combat_c::TriggerBot() -> void
{
	if (atx::settings.bTriggerbot)
	{
		auto tCurrentTime = std::chrono::steady_clock::now();
		auto fTimeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tCurrentTime - tCachedTime).count();

		if (fTimeElapsed > 250)
			bCanClick = true;

		int iBoneIndex = 0;
		switch (atx::settings.iHitBox)
		{
		case 0:
			iBoneIndex = 109;
			break;
		case 1:
			iBoneIndex = 67;
			break;
		case 2:
			iBoneIndex = 7;
			break;
		case 3:
			iBoneIndex = 2;
			break;
		case 4:
			iBoneIndex = 67;
			break;
		}

		float fDistance = Camera::Location.Distance(Cached::TargetPawn->GetRootComponent()->GetRelativeLocation()) / 100.f;
		bool bIsKeyDown = atx::settings.bIgnoreKeybind ? GetAsyncKeyState(atx::settings.iTriggerbotKeybind) : true;
		if (Cached::TargetPawn && (fDistance <= 100) && Cached::PlayerController->IsTargetUnderReticle(Cached::TargetPawn, pTargetBoneMap, iBoneIndex) && bIsKeyDown)
		{
			if (atx::settings.bShotgunOnly)
			{
				if (Cached::FortWeaponType == EFortWeaponType::Shotgun)
				{
					if (bHasRecentlyClicked)
					{
						tpBeginning = std::chrono::steady_clock::now();
						bHasRecentlyClicked = false;
					}

					tpEnding = std::chrono::steady_clock::now();
					iTimeSince = std::chrono::duration_cast<std::chrono::milliseconds>(tpEnding - tpBeginning).count();

					if (iTimeSince >= atx::settings.iCustomDelay)
					{
						if (bCanClick)
						{
							atx::interception.LeftClick();
							bHasRecentlyClicked = true;
							bCanClick = false;
							tCachedTime = std::chrono::steady_clock::now();
						}
					}
				}
			}
			else
			{
				if (bHasRecentlyClicked)
				{
					tpBeginning = std::chrono::steady_clock::now();
					bHasRecentlyClicked = false;
				}

				tpEnding = std::chrono::steady_clock::now();
				iTimeSince = std::chrono::duration_cast<std::chrono::milliseconds>(tpEnding - tpBeginning).count();

				if (iTimeSince >= atx::settings.iCustomDelay)
				{
					if (bCanClick)
					{
						atx::interception.LeftClick();
						bHasRecentlyClicked = true;
						bCanClick = false;
						tCachedTime = std::chrono::steady_clock::now();
					}
				}
			}
		}
	}
}

auto atx::combat_c::CombatThread() -> void
{
	for (;; )
	{
		int iBoneIDs[4] = { EBoneIndex::Head, EBoneIndex::Neck, EBoneIndex::Chest, EBoneIndex::Pelvis };
		vTargetScreen = { };
		vTargetBone = { };
		pTargetBoneMap = 0;

		auto tCachedTime = std::chrono::high_resolution_clock::now();

		for (;; )
		{
			auto tCurrentTime = std::chrono::high_resolution_clock::now();
			if (std::chrono::duration_cast<std::chrono::microseconds>(tCurrentTime - tCachedTime).count() < 5000) continue;
			tCachedTime = tCurrentTime;

			this->TriggerBot();

			if (Cached::Pawn)
			{
				Cached::TargetDistance = FLT_MAX;
				Cached::TargetPawn = NULL;

				for (auto& Actor : ActorList)
				{
					auto Pawn = Actor.Pawn;
					auto Mesh = Pawn->GetMesh();
					auto pBoneArray = Actor.pBoneArray;

					bool bIsVisible = false;

					if (atx::settings.bVisibleCheck)
						bIsVisible = Mesh->IsVisible();
					else
						bIsVisible = true;

					if (atx::settings.bVisibleCheck)
						if (!bIsVisible)
							continue;

					if (Pawn->IsDespawning())
						continue;

					if (Cached::TeamIndex == Actor.PlayerState->GetTeamIndex())
						continue;

					if (atx::settings.bAimbot)
					{
						FVector2D vHeadScreen = Cached::ViewState->WorldToScreen(Mesh->GetBoneMatrix(pBoneArray, 110));
						FVector2D vDistanceFromCrosshair = FVector2D(vHeadScreen.x - (atx::screen.fWidth / 2), vHeadScreen.y - (atx::screen.fHeight / 2));

						auto fDist = sqrtf(vDistanceFromCrosshair.x * vDistanceFromCrosshair.x + vDistanceFromCrosshair.y * vDistanceFromCrosshair.y);
						if (fDist < atx::settings.iFovRadius * 10 && fDist < Cached::TargetDistance)
						{
							Cached::TargetDistance = fDist;
							Cached::TargetPawn = Pawn;
							pTargetBoneMap = pBoneArray;
						}
					}
				}

				if (Cached::TargetPawn)
				{
					switch (atx::settings.iHitBox)
					{
					case 0:
						vTargetBone = Cached::TargetPawn->GetMesh()->GetBoneMatrix(pTargetBoneMap, 110);
						vTargetScreen = Cached::ViewState->WorldToScreen(vTargetBone);
						break;
					case 1:
						vTargetBone = Cached::TargetPawn->GetMesh()->GetBoneMatrix(pTargetBoneMap, 67);
						vTargetScreen = Cached::ViewState->WorldToScreen(vTargetBone);
						break;
					case 2:
						vTargetBone = Cached::TargetPawn->GetMesh()->GetBoneMatrix(pTargetBoneMap, 7);
						vTargetScreen = Cached::ViewState->WorldToScreen(vTargetBone);
						break;
					case 3:
						vTargetBone = Cached::TargetPawn->GetMesh()->GetBoneMatrix(pTargetBoneMap, 2);
						vTargetScreen = Cached::ViewState->WorldToScreen(vTargetBone);
						break;
					case 4:
						for (int i = 0; i < 3; i++)
						{
							auto vBoneWorld = Cached::TargetPawn->GetMesh()->GetBoneMatrix(pTargetBoneMap, iBoneIDs[i]);
							auto vBoneScreen = Cached::ViewState->WorldToScreen(vBoneWorld);
							if (vBoneScreen.Distance(FVector2D(atx::screen.fWidth / 2, atx::screen.fHeight / 2)) < vTargetScreen.Distance(FVector2D(atx::screen.fWidth / 2, atx::screen.fHeight / 2)))
							{
								vTargetBone = vBoneWorld;
								vTargetScreen = vBoneScreen;
							}
						}
						break;
					}
				}
			}

			if (Cached::TargetPawn && Cached::Pawn && GetAsyncKeyState(atx::settings.iAimbotKeybind) && !vTargetScreen.IsZero() && (GetCrossDistance(vTargetScreen.x, vTargetScreen.y, (atx::screen.fWidth / 2), (atx::screen.fHeight / 2) <= atx::settings.iFovRadius)))
			{
				auto CurrentWeapon = Cached::Pawn->GetCurrentWeapon();
				auto fTargetDistance = Camera::Location.Distance(vTargetBone);
				auto fProjectileSpeed = CurrentWeapon->GetProjectileSpeed();
				auto fProjectileGravity = CurrentWeapon->GetProjectileGravity();

				if (Cached::FortWeaponType != (EFortWeaponType::Melee || EFortWeaponType::Unarmed))
				{
					if (fProjectileSpeed && fProjectileGravity && CurrentWeapon && atx::settings.bPrediction)
					{
						auto pTargetRoot = atx::memory.read<uintptr_t>((uintptr_t)Cached::TargetPawn + Offset::RootComponent);
						auto vTargetVelocity = atx::memory.read<FVector>(pTargetRoot + Offset::ComponentVelocity);
						vTargetBone = vTargetBone.Predict(vTargetBone, vTargetVelocity, fTargetDistance, fProjectileSpeed, fProjectileGravity);
						vTargetScreen = Cached::ViewState->WorldToScreen(vTargetBone);
					}
				}

				if (!vTargetBone.IsZero())
				{
					SetMousePosition(vTargetScreen);
				}
			}
			else
			{
				vTargetBone = FVector(0, 0, 0);
				vTargetScreen = FVector2D(0, 0);
				Cached::TargetPawn = NULL;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
}