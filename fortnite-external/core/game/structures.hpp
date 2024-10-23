#include <Windows.h>
#include <iostream>
#include <xmmintrin.h>
#include <immintrin.h>
#include <cmath>
#include <d3d9.h>
#include <vector>
#include <array>
#include <thread>
#include <chrono>

#include <utilities/settings/settings.hpp>

#include <dependencies/framework/imgui.h>
#include <dependencies/communications/communications.hpp>
#include <dependencies/ida.hpp>

#include <utilities/interface/interface.hpp>

#define CURRENT_CLASS reinterpret_cast<std::uintptr_t>( this )

enum Offset
{
	GameState = 0x160,
	GameInstance = 0x1d8,
	LocalPlayers = 0x38,
	PlayerController = 0x30,
	AcknowledgedPawn = 0x338,

	LocationUnderReticle = 0x2698,
	Mesh = 0x310,
	PlayerState = 0x2b0,
	RootComponent = 0x198,
	CurrentWeapon = 0x9d8,
	CurrentVehicle = 0x2998,
	bIsDying = 0x758,
	TeamIndex = 0x1201,   //0x11d
	NameStructure = 0xA68,
	BuildingState = 0x1e51,

	BoneArray = 0x570,
	BoneCached = 0x5b8,

	RelativeLocation = 0x120,
	RelativeRotation = 0x138,
	ComponentVelocity = 0x168,

	WeaponData = 0x510,
	LastFireTime = 0xe4c,
	ProjectileSpeed = 0x1d08,
	ProjectileGravity = 0x1d0c,
	WeaponCoreAnimation = 0x1300,
	ItemName = 0x40,

	PlayerArray = 0x2a8,
	PawnPrivate = 0x308
};

template< typename t >
class TArray
{
public:

	TArray() : tData(), iCount(), iMaxCount() { }
	TArray(t* data, int count, int max_count) :
		tData(tData), iCount(iCount), iMaxCount(iMaxCount) { }

public:

	auto Get(int idx) -> t
	{
		return atx::memory.read< t >(reinterpret_cast<__int64>(this->tData) + (idx * sizeof(t)));
	}

	auto Size() -> std::uint32_t
	{
		return this->iCount;
	}

	bool IsValid()
	{
		return this->iCount != 0;
	}

	t* tData;
	int iCount;
	int iMaxCount;
};

class FVector2D
{
public:
	double x;
	double y;

	inline bool IsZero() {
		return (x == 0) && (y == 0);
	}
	inline double Distance(FVector2D v)
	{
		return double(sqrtf(powf(v.x - x, 2.0) + powf(v.y - y, 2.0)));
	}
	std::string ToString() const
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
	}
	auto ToImVec2() -> ImVec2
	{
		return { (float)this->x, (float)this->y };
	}
	auto OnScreen() -> bool
	{
		return (this->x > 0 && this->x < atx::screen.fWidth && this->y > 0 && this->y < atx::screen.fHeight);
	}
};

class FVector
{
public:
	FVector() : x(0.f), y(0.f), z(0.f)
	{

	}

	FVector(double _x, double _y, double _z) : x(_x), y(_y), z(_z)
	{

	}
	~FVector()
	{

	}

	double x;
	double y;
	double z;

	std::string ToString() const
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
	}

	inline double Dot(FVector v)
	{
		return x * v.x + y * v.y + z * v.z;
	}

	inline auto IsZero() -> bool
	{
		if (x == 0 && y == 0 && z == 0)
			return true;
		else
			return false;
	}

	inline FVector CalculateViewPoint(const FVector& dst)
	{
		FVector angle = FVector();
		FVector delta = FVector((this->x - dst.x), (this->y - dst.y), (this->z - dst.z));

		double hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

		angle.x = atan(delta.z / hyp) * (180.0f / M_PI);
		angle.y = atan(delta.y / delta.x) * (180.0f / M_PI);
		angle.z = 0;

		if (delta.x >= 0.0)
			angle.y += 180.0f;

		return angle;
	}

	FVector Predict(FVector Target, FVector Velocity, float DistanceToTarget, float GetProjectileSpeed, float ProjectileGravity)
	{
		float fHorizontalTime = DistanceToTarget / GetProjectileSpeed;
		float fVerticalTime = DistanceToTarget / GetProjectileSpeed;

		Target.x += Velocity.x * fHorizontalTime;
		Target.y += Velocity.y * fHorizontalTime;
		Target.z += Velocity.z * fVerticalTime +
			abs(-980.f * ProjectileGravity) * 0.5f * (fVerticalTime * fVerticalTime);

		return Target;
	}

	inline double Distance(FVector v)
	{
		return double(sqrtf(powf(v.x - x, 2.0) + powf(v.y - y, 2.0) + powf(v.z - z, 2.0)));
	}

	inline double Length()
	{
		return sqrt(x * x + y * y + z * z);
	}

	inline auto Equals(FVector v) -> bool
	{
		if (x == v.x && y == v.y && z == v.z)
			return true;
		else
			return false;
	}

	FVector operator/(double v) const
	{
		return FVector(x / v, y / v, z / v);
	}

	FVector operator+(FVector v)
	{
		return FVector(x + v.x, y + v.y, z + v.z);
	}

	FVector operator-(FVector v)
	{
		return FVector(x - v.x, y - v.y, z - v.z);
	}

	FVector operator*(double number) const
	{
		return FVector(x * number, y * number, z * number);
	}

	FVector& operator+=(const FVector& OtherVector)
	{
		x += OtherVector.x;
		y += OtherVector.y;
		z += OtherVector.z;
		return *this;
	}

	FVector& operator-=(const FVector& OtherVector)
	{
		x -= OtherVector.x;
		y -= OtherVector.y;
		z -= OtherVector.z;
		return *this;
	}
};

inline double DegreesToRadians(double dDegrees)
{
	return dDegrees * static_cast<double>(M_PI) / 180.0f;
}

inline double RadiansToDegrees(double dRadians)
{
	return dRadians * (180.0 / M_PI);
}

struct FQuat
{
	double x;
	double y;
	double z;
	double w;
};

struct FPlane : FVector
{
	double W;

	FPlane() : W(0) { }
	FPlane(double W) : W(W) { }
};

class FMatrix
{
public:
	double m[4][4];
	FPlane XPlane, YPlane, ZPlane, WPlane;

	FMatrix() : XPlane(), YPlane(), ZPlane(), WPlane() { }
	FMatrix(FPlane XPlane, FPlane YPlane, FPlane ZPlane, FPlane WPlane)
		: XPlane(XPlane), YPlane(YPlane), ZPlane(ZPlane), WPlane(WPlane) { }
};

struct FTransform
{
	FPlane  pRot;
	FVector vTranslation;
	char    cPad[8];
	FVector vScale;

	auto ToMatrixWithScale() -> D3DMATRIX
	{
		D3DMATRIX m;
		m._41 = vTranslation.x;
		m._42 = vTranslation.y;
		m._43 = vTranslation.z;

		double x2 = pRot.x + pRot.x;
		double y2 = pRot.y + pRot.y;
		double z2 = pRot.z + pRot.z;

		double xx2 = pRot.x * x2;
		double yy2 = pRot.y * y2;
		double zz2 = pRot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * vScale.x;
		m._22 = (1.0f - (xx2 + zz2)) * vScale.y;
		m._33 = (1.0f - (xx2 + yy2)) * vScale.z;

		double yz2 = pRot.y * z2;
		double wx2 = pRot.W * x2;
		m._32 = (yz2 - wx2) * vScale.z;
		m._23 = (yz2 + wx2) * vScale.y;

		double xy2 = pRot.x * y2;
		double wz2 = pRot.W * z2;
		m._21 = (xy2 - wz2) * vScale.y;
		m._12 = (xy2 + wz2) * vScale.x;

		double xz2 = pRot.x * z2;
		double wy2 = pRot.W * y2;
		m._31 = (xz2 + wy2) * vScale.z;
		m._13 = (xz2 - wy2) * vScale.x;

		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;

		return m;
	}
};

inline auto MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2) -> D3DMATRIX
{
	D3DMATRIX pOut;
	pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
	pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
	pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
	pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
	pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
	pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
	pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
	pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
	pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
	pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
	pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
	pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
	pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
	pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
	pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
	pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;

	return pOut;
}

inline auto Matrix(FVector rotation) -> D3DMATRIX
{
	float radPitch = (rotation.x * float(M_PI) / 180.f);
	float radYaw = (rotation.y * float(M_PI) / 180.f);
	float radRoll = (rotation.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = 0;
	matrix.m[3][1] = 0;
	matrix.m[3][2] = 0;
	matrix.m[3][3] = 1.f;

	return matrix;
}

enum EBoneIndex : int {
	Head = 110,
	Neck = 67,
	Chest = 66,
	Pelvis = 2,
	LShoulder = 9,
	LElbow = 10,
	LHand = 11,
	RShoulder = 38,
	RElbow = 39,
	RHand = 40,
	LHip = 71,
	LKnee = 72,
	LFoot = 75,
	RHip = 78,
	RKnee = 79,
	RFoot = 82,
	Root = 0
};

enum EFortWeaponCoreAnimation : uint8_t
{
	EFortWeaponCoreAnimation__Melee = 0,
	EFortWeaponCoreAnimation__Pistol = 1,
	EFortWeaponCoreAnimation__Shotgun = 2,
	EFortWeaponCoreAnimation__PaperBlueprint = 3,
	EFortWeaponCoreAnimation__Rifle = 4,
	EFortWeaponCoreAnimation__MeleeOneHand = 5,
	EFortWeaponCoreAnimation__MachinePistol = 6,
	EFortWeaponCoreAnimation__RocketLauncher = 7,
	EFortWeaponCoreAnimation__GrenadeLauncher = 8,
	EFortWeaponCoreAnimation__GoingCommando = 9,
	EFortWeaponCoreAnimation__AssaultRifle = 10,
	EFortWeaponCoreAnimation__TacticalShotgun = 11,
	EFortWeaponCoreAnimation__SniperRifle = 12,
	EFortWeaponCoreAnimation__TrapPlacement = 13,
	EFortWeaponCoreAnimation__ShoulderLauncher = 14,
	EFortWeaponCoreAnimation__AbilityDecoTool = 15,
	EFortWeaponCoreAnimation__Crossbow = 16,
	EFortWeaponCoreAnimation__C4 = 17,
	EFortWeaponCoreAnimation__RemoteControl = 18,
	EFortWeaponCoreAnimation__DualWield = 19,
	EFortWeaponCoreAnimation__AR_BullPup = 20,
	EFortWeaponCoreAnimation__AR_ForwardGrip = 21,
	EFortWeaponCoreAnimation__MedPackPaddles = 22,
	EFortWeaponCoreAnimation__SMG_P90 = 23,
	EFortWeaponCoreAnimation__AR_DrumGun = 24,
	EFortWeaponCoreAnimation__Consumable_Small = 25,
	EFortWeaponCoreAnimation__Consumable_Large = 26,
	EFortWeaponCoreAnimation__Balloon = 27,
	EFortWeaponCoreAnimation__MountedTurret = 28,
	EFortWeaponCoreAnimation__CreativeTool = 29,
	EFortWeaponCoreAnimation__ExplosiveBow = 30,
	EFortWeaponCoreAnimation__AshtonIndigo = 31,
	EFortWeaponCoreAnimation__AshtonChicago = 32,
	EFortWeaponCoreAnimation__MeleeDualWield = 33,
	EFortWeaponCoreAnimation__Unarmed = 34
};

enum EFortWeaponType : uint8_t
{
	Melee = 0,
	Rifle = 1,
	Shotgun = 2,
	Smg = 3,
	Pistol = 4,
	Sniper = 5,
	Unarmed = 6
};

constexpr std::array<std::pair<EBoneIndex, EBoneIndex>, 13> eBonePairs =
{
	{ { EBoneIndex::Neck, EBoneIndex::Pelvis },
	{ EBoneIndex::LShoulder, EBoneIndex::LElbow },
	{ EBoneIndex::LElbow, EBoneIndex::LHand },
	{ EBoneIndex::RShoulder, EBoneIndex::RElbow },
	{ EBoneIndex::RElbow, EBoneIndex::RHand },
	{ EBoneIndex::Pelvis, EBoneIndex::LHip },
	{ EBoneIndex::Pelvis, EBoneIndex::RHip },
	{ EBoneIndex::LHip, EBoneIndex::LKnee },
	{ EBoneIndex::LKnee, EBoneIndex::LFoot },
	{ EBoneIndex::RHip, EBoneIndex::RKnee },
	{ EBoneIndex::RKnee, EBoneIndex::RFoot },
	{ EBoneIndex::LShoulder, EBoneIndex::RShoulder } }
};

struct FBounds
{
	float top, bottom, left, right;
};

class FTracer
{
public:
	FVector vHit3D;
	FVector vOrigin3D;
	ImColor iColor;
	std::chrono::steady_clock::time_point tStartTime;
};

