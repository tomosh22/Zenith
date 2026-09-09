#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"

// ============================================================================
// Flux_MaterialPreviewController unit tests — the pure preview-orbit math
// ported from the retired offscreen MaterialPreview renderer: spherical orbit
// camera position, light-direction construction, ViewConstants fill (matrices
// + inverses + dims + near/far) and the pitch/zoom input clamps. Headless (no
// device); golden values pin the exact construction, not just invariants.
// ============================================================================

namespace
{
	// Element-wise mat4 compare (glm is column-major: m[col][row]).
	void MatPreview_AssertMatricesNear(const Zenith_Maths::Matrix4& xA, const Zenith_Maths::Matrix4& xB, const char* szWhat)
	{
		for (int iCol = 0; iCol < 4; iCol++)
		{
			for (int iRow = 0; iRow < 4; iRow++)
			{
				ZENITH_ASSERT_EQ_FLOAT(xA[iCol][iRow], xB[iCol][iRow], 1e-4f, "matrix elements must match");
			}
		}
		(void)szWhat;
	}
}

ZENITH_TEST(MaterialPreviewOrbit, CameraPosAtZeroYawPitchIsOnPlusZ)
{
	// (yaw=0, pitch=0, dist=D) sits at (0, 0, D) looking down -Z at the origin.
	const Zenith_Maths::Vector3 xPos = Flux_PreviewOrbitCameraPos(0.0f, 0.0f, 1.6f);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 0.0f, 1e-6f, "x is 0 at zero yaw/pitch");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 0.0f, 1e-6f, "y is 0 at zero pitch");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 1.6f, 1e-6f, "z is the orbit distance");
}

ZENITH_TEST(MaterialPreviewOrbit, CameraPosSpotValues)
{
	// yaw=pi/2 swings the camera onto +X.
	const Zenith_Maths::Vector3 xOnX = Flux_PreviewOrbitCameraPos(3.14159265f * 0.5f, 0.0f, 3.0f);
	ZENITH_ASSERT_EQ_FLOAT(xOnX.x, 3.0f, 1e-4f, "yaw=pi/2 lands on +X");
	ZENITH_ASSERT_EQ_FLOAT(xOnX.y, 0.0f, 1e-4f, "pitch 0 keeps y 0");
	ZENITH_ASSERT_EQ_FLOAT(xOnX.z, 0.0f, 1e-4f, "yaw=pi/2 leaves z 0");

	// Golden numeric spot: yaw=1.0, pitch=0.5, dist=2.0 (hardcoded expected
	// values pin the exact spherical construction / axis assignment).
	const Zenith_Maths::Vector3 xPos = Flux_PreviewOrbitCameraPos(1.0f, 0.5f, 2.0f);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 1.4769205f, 1e-3f, "x = d*cos(pitch)*sin(yaw)");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 0.9588511f, 1e-3f, "y = d*sin(pitch)");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 0.9483198f, 1e-3f, "z = d*cos(pitch)*cos(yaw)");
}

ZENITH_TEST(MaterialPreviewOrbit, LightDirConstructionAndUnitLength)
{
	// (yaw=0, pitch=0): the light sits on +Z shining INTO the scene -> (0,0,-1).
	const Zenith_Maths::Vector3 xDown = Flux_PreviewLightDir(0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDown.x,  0.0f, 1e-6f, "zero yaw/pitch light dir x");
	ZENITH_ASSERT_EQ_FLOAT(xDown.y,  0.0f, 1e-6f, "zero yaw/pitch light dir y");
	ZENITH_ASSERT_EQ_FLOAT(xDown.z, -1.0f, 1e-6f, "light points INTO the scene (-Z)");

	// Golden spot at the controller's default light angles (0.8, 0.7): matches
	// the old -(cos*sin, sin, cos*cos) construction, and is unit length.
	const Zenith_Maths::Vector3 xDir = Flux_PreviewLightDir(0.8f, 0.7f);
	ZENITH_ASSERT_EQ_FLOAT(xDir.x, -0.5486642f, 1e-3f, "-cos(pitch)*sin(yaw)");
	ZENITH_ASSERT_EQ_FLOAT(xDir.y, -0.6442177f, 1e-3f, "-sin(pitch)");
	ZENITH_ASSERT_EQ_FLOAT(xDir.z, -0.5328705f, 1e-3f, "-cos(pitch)*cos(yaw)");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xDir), 1.0f, 1e-4f, "light dir is normalized");
}

ZENITH_TEST(MaterialPreviewOrbit, ViewConstantsFill)
{
	Flux_ViewConstants xVC;
	Flux_PreviewBuildViewConstants(0.0f, 0.0f, 2.0f, xVC);

	// Fixed 512^2 target + the preview near/far.
	ZENITH_ASSERT_TRUE(xVC.m_xScreenDims.x == kuFLUX_PREVIEW_VIEW_SIZE && xVC.m_xScreenDims.y == kuFLUX_PREVIEW_VIEW_SIZE, "screen dims are 512^2");
	ZENITH_ASSERT_EQ_FLOAT(xVC.m_xRcpScreenDims.x, 1.0f / 512.0f, 1e-8f, "rcp dims x");
	ZENITH_ASSERT_EQ_FLOAT(xVC.m_xRcpScreenDims.y, 1.0f / 512.0f, 1e-8f, "rcp dims y");
	ZENITH_ASSERT_EQ_FLOAT(xVC.m_xCameraNearFar.x, 0.05f, 1e-6f, "near 0.05");
	ZENITH_ASSERT_EQ_FLOAT(xVC.m_xCameraNearFar.y, 50.0f, 1e-6f, "far 50");

	// Camera position rides in camPos (w = 0, like the old upload).
	ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(xVC.m_xCamPos_Pad), Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f), 1e-5f, "camPos = orbit position");
	ZENITH_ASSERT_EQ_FLOAT(xVC.m_xCamPos_Pad.w, 0.0f, 1e-6f, "camPos pad w is 0");

	// Vulkan Y-flip applied to the projection.
	ZENITH_ASSERT_TRUE(xVC.m_xProjMat[1][1] < 0.0f, "proj[1][1] is Y-flipped for Vulkan");

	// viewProj really is proj*view, and each inverse actually inverts.
	MatPreview_AssertMatricesNear(xVC.m_xViewProjMat, xVC.m_xProjMat * xVC.m_xViewMat, "viewProj == proj*view");
	const Zenith_Maths::Matrix4 xIdentity(1.0f);
	MatPreview_AssertMatricesNear(xVC.m_xInvViewMat * xVC.m_xViewMat, xIdentity, "invView*view == I");
	MatPreview_AssertMatricesNear(xVC.m_xInvProjMat * xVC.m_xProjMat, xIdentity, "invProj*proj == I");
	MatPreview_AssertMatricesNear(xVC.m_xInvViewProjMat * xVC.m_xViewProjMat, xIdentity, "invViewProj*viewProj == I");

	// The builder leaves flags/slot 0 (deterministic value-init) — the
	// controller stages those per frame.
	ZENITH_ASSERT_TRUE(xVC.m_uViewFlags == 0u && xVC.m_uViewSlot == 0u, "builder leaves flags/slot zero");
}

ZENITH_TEST(MaterialPreviewOrbit, PitchAndDistanceClamps)
{
	// Pitch clamp (+-1.5 rad) — same bound for camera and light orbits.
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewClampPitch( 2.0f),  1.5f, 1e-6f, "pitch clamps at +1.5");
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewClampPitch(-2.0f), -1.5f, 1e-6f, "pitch clamps at -1.5");
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewClampPitch( 0.3f),  0.3f, 1e-6f, "in-range pitch untouched");

	// Zoom: dist' = clamp(dist - delta*0.15, 0.7, 6.0).
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewApplyZoom(1.6f,  100.0f), 0.7f,  1e-6f, "zoom-in clamps at 0.7");
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewApplyZoom(1.6f, -100.0f), 6.0f,  1e-6f, "zoom-out clamps at 6.0");
	ZENITH_ASSERT_EQ_FLOAT(Flux_PreviewApplyZoom(1.6f,    1.0f), 1.45f, 1e-6f, "nominal wheel step is -0.15/unit");
}

// ============================================================================
// The MATERIAL preview slot's arbitration (Flux_PreviewSlotArbiter).
//
// ★ RELOCATED HERE BY D5, from Zenith_AnimationPreviewSession.Tests.inl. It was
// parked there because the arbitration used to have two claimants on opposite
// sides of a layer boundary, and only the Editor side could see both. It does
// not any more: D3 moved the animation editor onto a preview slot of its OWN, so
// the material slot's claimants are this controller's liveness window and the
// --preview-test-view diagnostic — both of them right here.
//
// The half that could NOT come along is "a staged animation session claims
// nothing", which is a fact about the SESSION. Flux may not include Editor, so it
// stays in Zenith_AnimationPreviewSession.Tests.inl.
//
// No force-link anchor is needed for this TU: Flux_MaterialPreviewController is a
// registered tools feature reached through Zenith_Engine::MaterialPreview(), so
// live symbols hold the .obj in — which the five orbit units above already
// demonstrate, since they count in the pinned baseline today.
//
// THE RISING EDGE IS WHAT THE COVERAGE IS FOR: SetActive(true) runs every frame
// the material panel is visible, and a claim per frame would make the material
// editor impossible to dispossess — last-opened-wins would quietly become
// last-drawn-wins.
//
// Headless and device-free: Update() is never called (that needs procedural
// meshes, a material table and the submission seam), only the claim/release
// edges, which are pure static state.
// ============================================================================
ZENITH_TEST(MaterialPreview, TheArbiterClaimsOnTheRisingEdgeOnly)
{
	// Process-level state, so it is reset at BOTH ends: a unit that left the slot
	// claimed would hand its claim to the next one.
	Flux_PreviewSlotArbiter::ResetForTesting();

	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "the reset leaves nobody owning it");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and no owner name");

	Flux_MaterialPreviewController xMaterialPreview;
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "the material editor opening claims the slot");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName() == "Material Editor", "under its own name");

	// The second claimant: a bare address standing in for whatever claims the
	// material slot next (the --preview-test-view diagnostic claims exactly like
	// this, under its own name). The arbiter stores an IDENTITY it never
	// dereferences, so a local is a complete claimant.
	const int iOtherClaimant = 0;

	// ★ ACTIVE AND DISPOSSESSED ARE STILL DIFFERENT THINGS. The DP automation
	// asserts IsActive() stays true for the whole time the panel is open, so
	// arbitration must not touch the liveness flag.
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::Claim(&iOtherClaimant, "Preview Test View"), "the owner changed");
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "the material editor is dispossessed");
	ZENITH_ASSERT_TRUE(xMaterialPreview.IsActive(), "but its panel is still open");
	ZENITH_ASSERT_TRUE(xMaterialPreview.GetPreviewSlotOwnerName() == "Preview Test View", "and it is told who took it");

	// THE RISING EDGE IS THE ONLY EDGE. These are the per-frame liveness refreshes.
	xMaterialPreview.SetActive(true);
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "a liveness refresh is not a claim");

	// A dispossessed claimant closing must not free somebody else's slot.
	xMaterialPreview.SetActive(false);
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == &iOtherClaimant, "a dispossessed close frees nothing");

	// ...and re-opening IS a rising edge, so it wins.
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "reopening claims again");

	// The falling edge of the OWNER releases.
	xMaterialPreview.SetActive(false);
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "the owner's close frees the slot");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and clears the owner name");

	// The reset is what every fixture in the suite leans on.
	Flux_PreviewSlotArbiter::Claim(&iOtherClaimant, "Leftover");
	Flux_PreviewSlotArbiter::ResetForTesting();
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "ResetForTesting clears the owner");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and the name with it");
}
