#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_SceneGraphDebug.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Maths/Zenith_FrustumCulling.h"

namespace Zenith_SceneGraphDebug
{
	namespace
	{
		// Function-local-static state (the sanctioned cross-TU-singleton shape — a
		// module-scope static trips the convention lint). Holds the injected primitives
		// pointer, the overlay toggles (bound to debug variables), and the per-frame stats.
		struct State
		{
			Flux_PrimitivesImpl* m_pxPrimitives = nullptr;
			bool     m_bDrawAABBs   = false;   // master overlay toggle (Render/SceneGraph/Draw AABBs)
			bool     m_bDrawCulled  = true;    // also draw the culled (red) boxes
			uint32_t m_uTotalCount  = 0;
			uint32_t m_uVisibleCount = 0;
			uint32_t m_uCulledCount  = 0;
			uint32_t m_bSnapshotStale = 0;     // 1 == snapshot was stale this frame (read-only flag)
		};

		State& Get()
		{
			static State s_xState;
			return s_xState;
		}
	}

	void Install(Zenith_DebugVariables& xDebugVars, Flux_PrimitivesImpl& xPrimitives)
	{
		State& xState = Get();
		xState.m_pxPrimitives = &xPrimitives;

		xDebugVars.AddBoolean({ "Render", "SceneGraph", "Draw AABBs" }, xState.m_bDrawAABBs);
		xDebugVars.AddBoolean({ "Render", "SceneGraph", "Draw Culled (red)" }, xState.m_bDrawCulled);
		xDebugVars.AddUInt32_ReadOnly({ "Render", "SceneGraph", "Entities" }, xState.m_uTotalCount);
		xDebugVars.AddUInt32_ReadOnly({ "Render", "SceneGraph", "Camera Visible" }, xState.m_uVisibleCount);
		xDebugVars.AddUInt32_ReadOnly({ "Render", "SceneGraph", "Camera Culled" }, xState.m_uCulledCount);
		xDebugVars.AddUInt32_ReadOnly({ "Render", "SceneGraph", "Snapshot Stale" }, xState.m_bSnapshotStale);
	}

	void QueueOverlays(const Flux_RenderSceneSnapshot& xSnapshot, uint64_t uCurrentEpoch)
	{
		State& xState = Get();

		// Stale snapshot: mark pending and read NOTHING (the entries may reference freed
		// instances — though we only ever touch pointer-free fields, gating here is the
		// documented contract and keeps the stats honest).
		if (!xSnapshot.IsCurrent(uCurrentEpoch))
		{
			xState.m_bSnapshotStale = 1;
			xState.m_uTotalCount = xState.m_uVisibleCount = xState.m_uCulledCount = 0;
			return;
		}
		xState.m_bSnapshotStale = 0;

		// Mirror the consumers: when the camera frustum isn't valid this frame (no resolved
		// camera), nothing is culled — count everything visible, draw no red boxes.
		const bool bCull = xSnapshot.IsCameraFrustumValid();
		const Zenith_Frustum& xFrustum = xSnapshot.GetCameraFrustum();
		const Zenith_Vector<Flux_RenderSceneItem>& xItems = xSnapshot.Items();

		uint32_t uVisible = 0, uCulled = 0;
		const bool bDraw = xState.m_bDrawAABBs && xState.m_pxPrimitives;

		for (u_int u = 0; u < xItems.GetSize(); ++u)
		{
			const Flux_RenderSceneItem& xItem = xItems.Get(u);
			if (!xItem.m_xWorldAABB.IsValid()) { ++uVisible; continue; }   // no bounds -> never culled

			// Mirror the consumers' conservative cull decision (pointer-free: world AABB only).
			// Animated-skinned models are conservatively force-shown in this debug overlay (the
			// unified path culls them with an inflated bind-pose bound, which this pointer-free
			// world-AABB mirror doesn't replicate), so mark them VISIBLE (green) rather than
			// running the camera-cull test — but still DRAW their box (the bind-pose world AABB
			// from the snapshot), so skinned characters get an overlay like everything else.
			const bool bVisible = xItem.m_bAnimatedSkinned
				|| !bCull
				|| Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xItem.m_xWorldAABB);
			if (bVisible) ++uVisible; else ++uCulled;

			if (bDraw && (bVisible || xState.m_bDrawCulled))
			{
				const Zenith_Maths::Vector3 xColour = bVisible
					? Zenith_Maths::Vector3(0.2f, 1.0f, 0.2f)   // green: camera-visible
					: Zenith_Maths::Vector3(1.0f, 0.2f, 0.2f);  // red: frustum-culled
				xState.m_pxPrimitives->AddWireframeCube(xItem.m_xWorldAABB.GetCenter(), xItem.m_xWorldAABB.GetExtents(), xColour);
			}
		}

		xState.m_uTotalCount   = xItems.GetSize();
		xState.m_uVisibleCount = uVisible;
		xState.m_uCulledCount  = uCulled;
	}
}

#endif // ZENITH_TOOLS
