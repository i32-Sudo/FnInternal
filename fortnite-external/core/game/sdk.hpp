#include "structures.hpp"

namespace Camera
{
	inline FVector Location;
	inline FVector Rotation;
	inline float Fov;
}

class USceneViewState
{
public:
	auto UpdateCamera() -> void
	{
		auto mProjection = atx::memory.read<FMatrix>(CURRENT_CLASS + 0x900);

		Camera::Rotation.x = RadiansToDegrees(std::asin(mProjection.ZPlane.W));
		Camera::Rotation.y = RadiansToDegrees(std::atan2(mProjection.YPlane.W, mProjection.XPlane.W));
		Camera::Rotation.z = 0.0;

		Camera::Location.x = mProjection.m[3][0];
		Camera::Location.y = mProjection.m[3][1];
		Camera::Location.z = mProjection.m[3][2];

		float FieldOfView = atanf(1 / atx::memory.read<double>(CURRENT_CLASS + 0x700)) * 2;
		Camera::Fov = (FieldOfView) * (180.f / M_PI);
	}

	auto WorldToScreen(FVector WorldLocation) -> FVector2D
	{
		auto mMatrix = Matrix(Camera::Rotation);
		auto XAxis = FVector(mMatrix.m[0][0], mMatrix.m[0][1], mMatrix.m[0][2]);
		auto YAxis = FVector(mMatrix.m[1][0], mMatrix.m[1][1], mMatrix.m[1][2]);
		auto ZAxis = FVector(mMatrix.m[2][0], mMatrix.m[2][1], mMatrix.m[2][2]);

		auto vDeltaCoordinates = WorldLocation - Camera::Location;
		auto tTransform = FVector(vDeltaCoordinates.Dot(YAxis), vDeltaCoordinates.Dot(ZAxis), vDeltaCoordinates.Dot(XAxis));

		if (tTransform.z < 0.1)
			tTransform.z = 0.1;

		FVector2D vScreen;
		vScreen.x = (atx::screen.fWidth / 2) + tTransform.x * ((atx::screen.fWidth / 2) / tan(Camera::Fov * 3.14159265358979323846 / 360)) / tTransform.z;
		vScreen.y = (atx::screen.fHeight / 2) - tTransform.y * ((atx::screen.fWidth / 2) / tan(Camera::Fov * 3.14159265358979323846 / 360)) / tTransform.z;
		return vScreen;
	}
};

namespace Cached
{
	inline USceneViewState* ViewState;
}

class UFortWeaponItemDefinition
{
public:
	auto GetDisplayName() -> std::string
	{
		auto pDisplayName = atx::memory.read<uintptr_t>(CURRENT_CLASS + Offset::ItemName);
		if (!pDisplayName)
			return _s("No Item");
		auto iWeaponLength = atx::memory.read<uint32_t>(pDisplayName + 0x30);
		wchar_t* wcWeaponName = new wchar_t[uint64_t(iWeaponLength) + 1];
		if (wcWeaponName)
			atx::memory.read_array(atx::memory.read<uintptr_t>(pDisplayName + 0x28), (uint8_t*)wcWeaponName, iWeaponLength * sizeof(wchar_t));
		std::wstring m_WeaponWString(wcWeaponName);
		auto sDisplayString = std::string(m_WeaponWString.begin(), m_WeaponWString.end());
		delete[] wcWeaponName;
		return sDisplayString;
	}
};

class AFortWeapon
{
public:
	auto GetProjectileSpeed() -> float
	{
		return atx::memory.read<float>(CURRENT_CLASS + Offset::ProjectileSpeed);
	}
	auto GetProjectileGravity() -> float
	{
		return atx::memory.read<float>(CURRENT_CLASS + Offset::ProjectileGravity);
	}
	auto GetWeaponData() -> UFortWeaponItemDefinition*
	{
		return atx::memory.read< UFortWeaponItemDefinition*>(CURRENT_CLASS + Offset::WeaponData);
	}
	auto GetWeaponAnimation() -> uint8_t
	{
		return atx::memory.read<uint8_t>(CURRENT_CLASS + Offset::WeaponCoreAnimation);
	}
	auto SimplifyWeaponType(uint8_t WeaponCoreAnim) -> EFortWeaponType
	{
		switch (WeaponCoreAnim)
		{
		case EFortWeaponCoreAnimation__Melee:
			return EFortWeaponType::Melee;
			break;
		case EFortWeaponCoreAnimation__Pistol:
			return EFortWeaponType::Pistol;
			break;
		case EFortWeaponCoreAnimation__Shotgun:
			return EFortWeaponType::Shotgun;
			break;
		case EFortWeaponCoreAnimation__Rifle:
			return EFortWeaponType::Rifle;
			break;
		case EFortWeaponCoreAnimation__MeleeOneHand:
			return EFortWeaponType::Melee;
			break;
		case EFortWeaponCoreAnimation__MachinePistol:
			return EFortWeaponType::Smg;
			break;
		case EFortWeaponCoreAnimation__AssaultRifle:
			return EFortWeaponType::Rifle;
			break;
		case EFortWeaponCoreAnimation__TacticalShotgun:
			return EFortWeaponType::Shotgun;
			break;
		case EFortWeaponCoreAnimation__SniperRifle:
			return EFortWeaponType::Sniper;
			break;
		case EFortWeaponCoreAnimation__Crossbow:
			return EFortWeaponType::Sniper;
			break;
		case EFortWeaponCoreAnimation__DualWield:
			return EFortWeaponType::Pistol;
			break;
		case EFortWeaponCoreAnimation__SMG_P90:
			return EFortWeaponType::Smg;
			break;
		case EFortWeaponCoreAnimation__AR_DrumGun:
			return EFortWeaponType::Rifle;
			break;
		default:
			return EFortWeaponType::Unarmed;
			break;
		}
	}
};

class USceneComponent
{
public:
	auto GetRelativeLocation() -> FVector
	{
		return atx::memory.read<FVector>(CURRENT_CLASS + Offset::RelativeLocation);
	}
	auto SetPosition(FVector vPosition) -> void
	{
		atx::memory.write<FVector>(CURRENT_CLASS + 0x1e0, vPosition);
	}
};

class USkeletalMeshComponent
{
public:
	auto GetBoneArray() -> TArray<std::uintptr_t>
	{
		return atx::memory.read<TArray<std::uintptr_t>>(CURRENT_CLASS + Offset::BoneArray + (atx::memory.read<int>(CURRENT_CLASS + (Offset::BoneCached)) * 0x10));
	}
	auto GetBoneMatrix(__int64 pBoneArray, int iIndex) -> FVector
	{
		auto BoneTransform = atx::memory.read<FTransform>(pBoneArray + 0x60 * iIndex);
		auto ComponentToWorld = atx::memory.read<FTransform>(CURRENT_CLASS + 0x1c0);
		auto Matrix = MatrixMultiplication(BoneTransform.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());
		return FVector(Matrix._41, Matrix._42, Matrix._43);
	}
	auto IsVisible() -> bool
	{
		auto fLastSubmitTime = atx::memory.read<float>(CURRENT_CLASS + 0x2E0);
		auto fLastRenderTime = atx::memory.read<float>(CURRENT_CLASS + 0x2E8);
		return (fLastRenderTime + 0.06f >= fLastSubmitTime);
	}
	auto GetActorBounds(FVector2D vHead, FVector2D vRoot, float fSize) -> FBounds
	{
		FBounds bBounds;
		bBounds.top = vHead.y;
		bBounds.bottom = vRoot.y;
		bBounds.left = (vHead.x - (fSize / 2));
		bBounds.right = vRoot.x + (fSize / 2);
		return bBounds;
	}
};

class AFortPlayerState
{
public:

	auto GetTeamIndex() -> int
	{
		return atx::memory.read<int>(CURRENT_CLASS + Offset::TeamIndex);
	}
	auto GetPlayerName() -> std::string
	{
		auto fString = atx::memory.read<__int64>(CURRENT_CLASS + Offset::NameStructure);
		auto iLength = atx::memory.read<int>(fString + 16);

		auto v6 = (__int64)iLength;
		if (!v6)
			return std::string(_("Bot"));

		auto fText = atx::memory.read<__int64>(fString + 8);

		wchar_t* wcBuffer = new wchar_t[iLength];
		atx::memory.read_array(fText, (uint8_t*)wcBuffer, iLength * sizeof(wchar_t));

		char v21;
		int v22;
		int i;
		int v25;
		_WORD* v23;

		v21 = v6 - 1;
		if (!(_DWORD)v6)
			v21 = 0;
		v22 = 0;
		v23 = (_WORD*)wcBuffer;
		for (i = (v21) & 3; ; *v23++ += i & 7)
		{
			v25 = v6 - 1;
			if (!(_DWORD)v6)
				v25 = 0;
			if (v22 >= v25)
				break;
			i += 3;
			++v22;
		}

		std::wstring wsUsername{ wcBuffer };
		delete[] wcBuffer;
		return std::string(wsUsername.begin(), wsUsername.end());
	}

	auto GetHabaneroComponent() -> uintptr_t
	{
		return atx::memory.read<uintptr_t>(CURRENT_CLASS + 0x9a8);
	}
};

class APawn
{
public:
	auto GetCurrentWeapon() -> AFortWeapon*
	{
		return atx::memory.read<AFortWeapon*>(CURRENT_CLASS + Offset::CurrentWeapon);
	}
	auto GetCurrentVehicle() -> APawn*
	{
		return atx::memory.read<APawn*>(CURRENT_CLASS + Offset::CurrentVehicle);
	}
	auto GetRootComponent() -> USceneComponent*
	{
		return atx::memory.read<USceneComponent*>(CURRENT_CLASS + Offset::RootComponent);
	}
	auto GetMesh() -> USkeletalMeshComponent*
	{
		return atx::memory.read<USkeletalMeshComponent*>(CURRENT_CLASS + Offset::Mesh);
	}
	auto GetPlayerState() -> AFortPlayerState*
	{
		return atx::memory.read< AFortPlayerState*>(CURRENT_CLASS + Offset::PlayerState);
	}
	auto IsDespawning() -> bool
	{
		return (atx::memory.read<char>(CURRENT_CLASS + Offset::bIsDying) >> 3);
	}
};

class APlayerState : public AFortPlayerState
{
public:
	auto GetPawnPrivate() -> APawn*
	{
		return atx::memory.read<APawn*>(CURRENT_CLASS + Offset::PawnPrivate);
	}
};

class APlayerController
{
public:
	auto GetPawn() -> APawn*
	{
		return atx::memory.read<APawn*>(CURRENT_CLASS + Offset::AcknowledgedPawn);
	}
	auto IsTargetUnderReticle(APawn* Target, __int64 pBoneMap, int BoneIndex) -> bool
	{
		auto vLocationUnderReticle = this->GetLocationUnderReticle();
		auto vTargetBone = Target->GetMesh()->GetBoneMatrix(pBoneMap, BoneIndex);

		if (vLocationUnderReticle.x >= vTargetBone.x - 20 && vLocationUnderReticle.x <= vTargetBone.x + 20 &&
			vLocationUnderReticle.y >= vTargetBone.y - 20 && vLocationUnderReticle.y <= vTargetBone.y + 20 &&
			vLocationUnderReticle.z >= vTargetBone.z - 20 && vLocationUnderReticle.z <= vTargetBone.z + 20)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	auto GetLocationUnderReticle() -> FVector
	{
		return atx::memory.read<FVector>(CURRENT_CLASS + Offset::LocationUnderReticle);
	}
};

class ULocalPlayer
{
public:
	auto GetPlayerController() -> APlayerController*
	{
		return atx::memory.read<APlayerController*>(CURRENT_CLASS + Offset::PlayerController);
	}
	auto GetViewState() -> USceneViewState*
	{
		TArray<USceneViewState*> ViewState = atx::memory.read<TArray<USceneViewState*>>(CURRENT_CLASS + 0xd0);
		return ViewState.Get(1);
	}
};

class UGameInstance
{
public:
	auto GetLocalPlayer() -> ULocalPlayer*
	{
		return atx::memory.read<ULocalPlayer*>(atx::memory.read<uintptr_t>(CURRENT_CLASS + Offset::LocalPlayers));
	}
};

class AGameStateBase
{
public:
	auto GetPlayerArray() -> TArray<APlayerState*>
	{
		return atx::memory.read<TArray<APlayerState*>>(CURRENT_CLASS + Offset::PlayerArray);
	}
};

class UWorld
{
public:
	static auto Get() -> UWorld*
	{
		//return atx::memory.read<UWorld*>(atx::memory.text_section ); 
	
	    //atx::memory.get_text_section();

		// try now
		
		return (UWorld*)atx::memory.text_section;
	
	}
	auto GetGameState() -> AGameStateBase*
	{
		return atx::memory.read<AGameStateBase*>(CURRENT_CLASS + Offset::GameState); 
	}
	auto GetGameInstance() -> UGameInstance*
	{
		return atx::memory.read<UGameInstance*>(CURRENT_CLASS + Offset::GameInstance); 
	}
};

namespace Cached
{
	inline UWorld* GWorld;
	inline AGameStateBase* GameState;
	inline APlayerController* PlayerController;
	inline APawn* Pawn;
	inline int TeamIndex;

	inline APawn* TargetPawn;
	inline float TargetDistance;

	inline EFortWeaponType FortWeaponType;
}

class Actor
{
public:
	APawn* Pawn;
	APlayerState* PlayerState;
	USkeletalMeshComponent* Mesh;
	std::uintptr_t pBoneArray;
	std::string Username;
};
inline std::vector<Actor> ActorList;
inline std::vector<Actor> TemporaryActorList;

