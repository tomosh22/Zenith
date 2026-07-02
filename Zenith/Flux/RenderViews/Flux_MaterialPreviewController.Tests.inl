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
