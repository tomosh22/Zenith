#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_HashMap.h"
#include <utility>   // std::move

// ============================================================================
// Flux_PrevTransformCache — per-entity previous-frame model-matrix store (PURE).
//
// Static + foliage motion vectors reproject each opaque pixel by transforming its
// world position with the PREVIOUS frame's view-proj; for a moving object that also
// needs the object's previous-frame MODEL matrix. Rather than grow the 80-byte,
// static_assert-locked Flux_GPUSceneObject, the velocity path uploads a parallel
// StructuredBuffer<float4x4> of previous transforms, index-locked to the current
// object list. This double-buffered cache is the CPU source of that buffer.
//
// Keyed on the packed entity id (Flux_ModelInstance::m_ulEntityIDPacked). A MISS —
// a newly-spawned entity, or id 0 ("no stable id") — yields prev == current at the
// call site, i.e. one frame of camera-only velocity for that object (sub-texel,
// invisible, and absorbed by the resolve clamp).
//
// Headless-testable; no GPU. Tests in Flux_VelocityHistory.Tests.inl.
// ============================================================================
class Flux_PrevTransformCache
{
public:
	// Advance to a new frame: the matrices recorded LAST frame become this frame's
	// prev-lookup source. Call ONCE at the start of the GPU-scene build, before any
	// RecordCurrent. Reuses the old prev-map's buckets as the new current map so there
	// is no per-frame reallocation in steady state.
	void BeginFrame()
	{
		Zenith_HashMap<u_int64, Zenith_Maths::Matrix4> xRecycled = std::move(m_xPrev);
		m_xPrev = std::move(m_xCur);
		m_xCur  = std::move(xRecycled);
		m_xCur.Clear();
	}

	// The previous-frame model matrix for this entity. Returns false (=> caller uses the
	// current matrix as prev => camera-only velocity this frame) if the entity was not
	// recorded last frame, or if ulEntityId == 0 (no stable id).
	bool TryGetPrev(u_int64 ulEntityId, Zenith_Maths::Matrix4& xOut) const
	{
		if (ulEntityId == 0u) { return false; }
		const Zenith_Maths::Matrix4* pxMat = m_xPrev.TryGet(ulEntityId);
		if (pxMat == nullptr) { return false; }
		xOut = *pxMat;
		return true;
	}

	// Record this entity's current-frame model matrix (becomes prev next frame). id 0 ignored.
	void RecordCurrent(u_int64 ulEntityId, const Zenith_Maths::Matrix4& xMatrix)
	{
		if (ulEntityId == 0u) { return; }
		m_xCur.Insert(ulEntityId, xMatrix);
	}

	u_int GetPrevCount()    const { return m_xPrev.GetSize(); }
	u_int GetCurrentCount() const { return m_xCur.GetSize(); }

private:
	Zenith_HashMap<u_int64, Zenith_Maths::Matrix4> m_xPrev;   // last frame's transforms (lookup source)
	Zenith_HashMap<u_int64, Zenith_Maths::Matrix4> m_xCur;    // this frame's transforms (becomes prev on BeginFrame)
};
