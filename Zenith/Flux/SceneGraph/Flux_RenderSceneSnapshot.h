#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
// Flux_RenderSceneItem + the fill-fn typedef (Zenith_SceneSnapshotFillFn /
// g_pfnZenithSceneSnapshotFill) live in Flux_ModelInstance.h — the header the EC fill
// site already includes — so the EC side never includes THIS header (no new EC->Flux edge).
#include "Flux/Flux_ModelInstance.h"

// The central object of the scene-graph design: an UNCULLLED master list of every
// renderable entity, rebuilt EXACTLY ONCE per frame by its owner (Flux_RendererImpl)
// before any render-pass Prepare runs. It replaces the 2-3 independent per-consumer
// ECS scans; each geometry consumer derives its own filtered draw packet from it.
//
// Carries a lifetime EPOCH (stamped at Rebuild from the scene system's render-mutation
// counter) so the tools debug panel can detect a stale snapshot — one whose entries may
// reference a just-freed Flux_ModelInstance* — BEFORE dereferencing, and a GENERATION
// counter (bumped every Rebuild) that Phase 3's per-consumer packets guard against.
//
// Owns no g_xEngine reach: the epoch and the fill fn are passed in, so the type stays a
// pure data/algorithm object injected at the composition root.
class Flux_RenderSceneSnapshot
{
public:
	Flux_RenderSceneSnapshot() = default;
	~Flux_RenderSceneSnapshot() = default;

	Flux_RenderSceneSnapshot(const Flux_RenderSceneSnapshot&) = delete;
	Flux_RenderSceneSnapshot& operator=(const Flux_RenderSceneSnapshot&) = delete;

	// Clear, run the EC fill fn, and stamp the lifetime epoch (passed in by the owner).
	// Bumps the generation. Main-thread only (the owner calls it before render tasks open).
	void Rebuild(Zenith_SceneSnapshotFillFn pfnFill, uint64_t uEpoch);

	// Drop all entries (the Flux_ModelInstance* pointers are non-owning) so no stale
	// pointer survives a renderer teardown/reinit. Generation keeps climbing.
	void Reset();

	// True iff the snapshot was last built for this render-mutation epoch — i.e. no
	// model/scene create/destroy has happened since. Guarantees STRUCTURAL/pointer
	// validity only (not transform/material freshness — those are reflected by the
	// next authoritative rebuild).
	bool IsCurrent(uint64_t uEpoch) const { return m_uBuiltEpoch == uEpoch; }
	uint64_t GetBuiltEpoch() const { return m_uBuiltEpoch; }

	// Bumped each Rebuild — Phase 3 per-consumer draw packets rebuild when it changes.
	uint32_t GetGeneration() const { return m_uGeneration; }

	// Phase 3: the camera frustum for THIS frame, set by the owner right after Rebuild
	// (from the ratchet-excluded composition root, so the geometry consumers can
	// camera-cull against it WITHOUT reaching g_xEngine.FluxGraphics() themselves). The
	// owner sets it ONLY when the main camera resolved this frame; Rebuild clears the valid
	// flag, so consumers skip culling (treat everything visible) against a stale/identity
	// matrix until a real camera exists (e.g. the first boot frame before the camera entity).
	void SetCameraFrustum(const Zenith_Maths::Matrix4& xViewProj)
	{
		m_xCameraFrustum.ExtractFromViewProjection(xViewProj);
		m_bCameraFrustumValid = true;
	}
	const Zenith_Frustum& GetCameraFrustum() const { return m_xCameraFrustum; }
	bool IsCameraFrustumValid() const { return m_bCameraFrustumValid; }

	// Read API (render consumers iterate; filter with their own predicate).
	const Zenith_Vector<Flux_RenderSceneItem>& Items() const { return m_xItems; }
	uint32_t GetNumItems() const { return m_xItems.GetSize(); }

private:
	Zenith_Vector<Flux_RenderSceneItem> m_xItems;
	uint64_t m_uBuiltEpoch = 0;
	uint32_t m_uGeneration = 0;
	Zenith_Frustum m_xCameraFrustum;
	bool m_bCameraFrustumValid = false;   // false until SetCameraFrustum runs this frame (cleared each Rebuild)

	friend class Zenith_UnitTests;
};
