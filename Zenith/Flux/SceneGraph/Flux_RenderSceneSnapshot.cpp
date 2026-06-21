#include "Zenith.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"

void Flux_RenderSceneSnapshot::Rebuild(Zenith_SceneSnapshotFillFn pfnFill, uint64_t uEpoch)
{
	m_xItems.Clear();
	if (pfnFill)
	{
		// The EC fill fn pushes one Flux_RenderSceneItem per renderable into our item
		// vector; it never names this snapshot type (no EC->Flux-snapshot edge).
		pfnFill(m_xItems);
	}
	m_uBuiltEpoch = uEpoch;
	++m_uGeneration;
	// The owner re-stamps the camera frustum (only when the camera is valid) right after
	// this; until then consumers must NOT cull (clear the valid flag here).
	m_bCameraFrustumValid = false;
}

void Flux_RenderSceneSnapshot::Reset()
{
	m_xItems.Clear();
	// Epoch 0 == "never built": any IsCurrent(currentEpoch) (currentEpoch >= 1) is then
	// false until the next authoritative rebuild, so a consumer can't read post-teardown
	// entries as current.
	m_uBuiltEpoch = 0;
	m_bCameraFrustumValid = false;
	// Generation is monotonic (never reset) so a packet built against a pre-reset
	// generation can never alias a post-reset rebuild.
	++m_uGeneration;
}
