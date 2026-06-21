#pragma once
#include "Maths/Zenith_Maths.h"
#include <cstdint>

// FPS gun pickup/drop testbed for RenderTest.
//
// Several procedurally-built guns spawn lying on the floor of the spawn platform
// (the CenterPlatform). The player walks up to one, presses E to pick it up, can
// fire it, and presses E again to put it back down. Built on the same engine
// pieces the tennis racket testbed introduced:
//
//   * Zenith_AttachmentComponent binds the picked-up gun to the player's RightHand
//     bone (AttachToBone on pickup, Detach on drop) — the gun rides the hand.
//   * The arm IK chains place the hands correctly ON the gun: the RIGHT arm is
//     IK-driven to a body-anchored hold pose (and its end-effector orientation
//     squares the gun barrel forward); for a TWO-handed gun the LEFT (support)
//     arm is IK-driven onto the gun's foregrip. A pistol uses only the right hand.
//
// Why the right arm is IK'd rather than left to the aim animation: the StickFigure
// arms reach ~0.7 m and the shoulders are 0.6 m apart, so a gun held forward from
// the right hand puts its foregrip well outside the left arm's reach. Anchoring the
// grip near the chest centerline (RightArm IK) brings the foregrip back inside the
// support arm's reach, so both hands genuinely meet the gun. See RenderTest_Guns.cpp
// for the per-type anchor/foregrip numbers and the reachability budget.
//
// The four gun meshes are baked OFFLINE (tools) into CPU Zenith_MeshAssets + a
// bundling .zmodel each; the authored scene LoadModels them and the runtime
// pickup/drop bind happens via the serialized Zenith_AttachmentComponent.
// (Previously each mesh was built as a runtime GPU Flux_MeshGeometry and spawned
// post scene-load — that path is gone.)

namespace RenderTest_Guns
{
	enum class GunType : uint32_t
	{
		Pistol,     // one-handed
		SMG,        // two-handed, short
		Rifle,      // two-handed, long
		Shotgun,    // two-handed, pump
		COUNT
	};

	// Per-type description. Positions are in the player's MODEL space (hold anchor,
	// where the RIGHT hand / gun grip is IK-driven to) or the GUN's own local frame
	// (foregrip / muzzle). The gun mesh is built grip-at-origin with +Z = barrel
	// forward, +Y = up, so the attachment mount is identity and the right-hand
	// end-effector IK orientation alone aims the gun.
	struct GunSpec
	{
		GunType m_eType = GunType::Pistol;
		const char* m_szName = "Pistol";
		bool m_bTwoHanded = false;

		// Right-hand grip hold anchor, player model space. The RightArm IK drives
		// the wrist here so the gun (attached to RightHand) sits at a reachable,
		// believable hold.
		Zenith_Maths::Vector3 m_xHoldAnchorModel = Zenith_Maths::Vector3(0.12f, 1.18f, 0.52f);

		// Desired gun orientation in model space, Euler degrees (pitch about X, yaw
		// about Y, roll about Z). (0,0,0) => barrel points model +Z (the figure's
		// forward). Drives the right-hand end-effector IK rotation.
		Zenith_Maths::Vector3 m_xAimEulerDeg = Zenith_Maths::Vector3(0.0f);

		// Support (left) hand grip point in GUN-local space — two-handed only.
		Zenith_Maths::Vector3 m_xForegripLocal = Zenith_Maths::Vector3(0.0f, -0.03f, 0.16f);

		// Muzzle point in GUN-local space (shots / muzzle flash originate here).
		Zenith_Maths::Vector3 m_xMuzzleLocal = Zenith_Maths::Vector3(0.0f, 0.0f, 0.30f);

		uint32_t m_uMagSize = 12;
		uint32_t m_uReserve = 48;
		float m_fFireInterval = 0.18f;   // seconds between shots
	};

	// The canonical spec for a gun type (anchors, ammo, foregrip, muzzle).
	const GunSpec& GetSpec(GunType eType);

	// Number of pickup-prompt-visible spawned guns is GunType::COUNT (one of each).
	// Distance (m) within which the player can pick a gun up off the floor.
	constexpr float fPICKUP_RADIUS = 2.2f;
}

// Tools-only: build each gun's mesh as a CPU Zenith_MeshAsset and export it + a
// bundling .zmodel (referencing the shared vertex-colour material passed in) to
// disk, so the authored scene can LoadModel them. Loops over all GunType values.
// Overwrites every tools run. CPU-only (no GPU upload) — headless/
// --skip-tool-exports safe.
#ifdef ZENITH_TOOLS
void RenderTest_ExportGunAssets(const char* szVtxColorMaterialPath);
#endif

// Deterministic on-disk path of the exported .zmodel for a gun type (stable
// static storage — safe to pass straight to AddStep_LoadModel). Used by both the
// export (write target) and the authoring (load reference).
const char* RenderTest_GunModelPath(RenderTest_Guns::GunType eType);

// Parse --rendertest-gun-showcase[=...] + --gun-* into RenderTest_GunTuning.
// Called by the RenderTest bootstrap component in OnAwake (previously inlined in
// the now-deleted runtime spawn).
void RenderTest_ParseGunCLI();

// Release the file-scope asset handles BEFORE Zenith_AssetRegistry shutdown
// (mirrors RenderTest_JetpackShutdown). Call from Project_Shutdown.
void RenderTest_GunsShutdown();

// Optional showcase / tuning support. --rendertest-gun-showcase[=pistol|smg|rifle|
// shotgun] auto-equips the player with the named gun on spawn and parks a photo
// camera in front so the held pose + hand IK can be screenshotted. The
// --gun-* float knobs live-override the held gun's hold anchor / aim / foregrip so
// the pose can be calibrated from screenshots without recompiling. Parsed in the
// gun spawn; consumed by the player component.
namespace RenderTest_GunTuning
{
	// True when --rendertest-gun-showcase was passed.
	inline bool  s_bShowcaseActive = false;
	// Which gun type to auto-equip (default Pistol).
	inline uint32_t s_uShowcaseType = 0;

	// Live pose overrides (NaN = "unset, use the spec value"). Applied by the
	// player when holding a gun, on top of the type spec.
	inline float s_fAnchorX = 1e30f;
	inline float s_fAnchorY = 1e30f;
	inline float s_fAnchorZ = 1e30f;
	inline float s_fAimPitchDeg = 1e30f;
	inline float s_fAimYawDeg   = 1e30f;
	inline float s_fAimRollDeg  = 1e30f;
	inline float s_fForegripY   = 1e30f;
	inline float s_fForegripZ   = 1e30f;

	inline bool IsSet(float f) { return f < 1e29f; }
}
