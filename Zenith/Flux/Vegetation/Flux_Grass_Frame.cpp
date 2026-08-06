#include "Zenith.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

#include "Core/Zenith_Engine.h"
#include "Core/FrameContext.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Flux/Flux_BackendTypes.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/RenderViews/Flux_RenderViews.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Profiling/Zenith_Profiling.h"

#include <bit>

//=============================================================================
// Flux_Grass — the per-frame half: render-graph declaration, the main-thread
// gather, and the four record callbacks. Lifecycle, GPU resources, map feeding
// and the CPU query surface live in Flux_Grass.cpp.
//
// The split between the two is the SINGLE-WRITER rule: GatherGrassFrame runs on
// the main thread and is the only writer of the per-frame state; the record
// callbacks run on workers and are pure readers of it.
//=============================================================================

namespace
{
	// Per-blade hash seed. Baked in rather than exposed: changing it re-rolls every
	// blade in every world, which is a content change, not a tuning knob.
	constexpr u_int uGRASS_PLACEMENT_SEED = 1337u;

	// Draw-time shaping constants. These are pose/anti-aliasing tuning shared by all
	// types, not per-type authoring, so they live with the draw constants rather than
	// in the type table.
	constexpr float fGRASS_MIN_PIXEL_WIDTH = 1.0f;    // a sub-pixel blade crawls as a dotted line under TAA
	constexpr float fGRASS_THICKEN_BLEND   = 1.0f;    // 0 = no min-pixel clamp, 1 = full clamp
	constexpr float fGRASS_NORMAL_BLEND_DISTANCE = 30.0f;   // metres over which a blade borrows its clump's normal
	constexpr float fGRASS_LOD_BLEND_BAND  = 0.25f;   // fraction of the HI radius the HI blade converges over
	constexpr float fGRASS_GLOSS_CUT_DISTANCE = 60.0f;   // past this the gloss streak is sub-pixel sparkle

	// Displacement DYNAMICS. The field's geometry (resolution, footprint, texel size,
	// push clamp) lives in Flux_GrassConfig because the snap is a pure function the
	// unit tests drive; only the tuning is here.
	//
	// e-folding time of the trail. Long enough that a walking mover leaves a visible
	// tail behind it, short enough that a scar closes within a second or two.
	constexpr float fGRASS_DISPLACEMENT_EFOLD_SECONDS = 0.5f;

	// The debug orbiter: one synthetic mover circling the camera's ground point, so
	// the field can be inspected with no game body to walk.
	constexpr float fGRASS_ORBIT_RADIUS = 3.0f;          // metres from the camera's ground point
	constexpr float fGRASS_ORBIT_PERIOD_SECONDS = 4.0f;
	constexpr float fGRASS_ORBIT_MOVER_RADIUS = 1.25f;
	constexpr float fGRASS_TAU = 6.28318530718f;

	// The graph's OnRecord / OnPrepare signatures are plain function pointers and
	// admit no capture, so every trampoline below has to recover the feature from the
	// singleton. One accessor rather than one lookup spelled out per trampoline.
	Flux_GrassImpl& GrassFeature() { return g_xEngine.Grass(); }

	// bWindEnabled false zeroes BOTH amplitude terms, not just the gust strength: the
	// detail bob is an independent per-vertex term the shader does not scale by
	// strength, so zeroing only the gust would leave every blade still moving with
	// wind switched off. Everything else — heading, frequencies and above all
	// m_fTime — rides through untouched, so the field keeps scrolling while it is
	// muted and re-enabling resumes it instead of snapping back to a stale phase.
	void FillWindBlock(Flux_GrassWindBlockGPU& xOut, const Flux_WindConstants& xWind, bool bWindEnabled)
	{
		xOut.m_xDirXZ_Strength_Time = Zenith_Maths::Vector4(
			xWind.m_xDirectionXZ.x, xWind.m_xDirectionXZ.y, bWindEnabled ? xWind.m_fStrength : 0.0f, xWind.m_fTime);
		// The seed rides as raw BITS through a float slot (the shader asuint()s it
		// back); a numeric conversion would round a large seed to a different value.
		xOut.m_xFreq_Scroll_Gust_Seed = Zenith_Maths::Vector4(
			xWind.m_fFrequency, xWind.m_fScrollSpeed, xWind.m_fGustSharpness, std::bit_cast<float>(xWind.m_uSeed));
		xOut.m_xDetailFreq_Speed_Amp = Zenith_Maths::Vector4(
			xWind.m_fDetailFrequency, xWind.m_fDetailSpeed, bWindEnabled ? xWind.m_fDetailAmplitude : 0.0f, 0.0f);
	}

	// Lattice units spanned by one tile of this LOD. Both LODs are uTILE_CELLS cells
	// across; a LO cell steps uLO_LATTICE_STRIDE HI nodes, so a LO tile spans twice
	// the HI-lattice range and lands on the (even,even) subset.
	int TileLatticeSpan(Flux_GrassTileLOD eLOD)
	{
		return static_cast<int>(Flux_GrassTileSize(eLOD) / Flux_GrassConfig::fHI_LATTICE_STEP + 0.5f);
	}
}

// Graph trampolines. Captureless by necessity (Flux_RenderGraph_OnRecordFunc is a
// plain function pointer), so each recovers the feature and forwards.
static void ExecuteGrassReset(Flux_CommandBuffer* pxCmdList, void*)
{
	GrassFeature().RecordReset(*pxCmdList);
}

static void ExecuteGrassPlacement(Flux_CommandBuffer* pxCmdList, void*)
{
	GrassFeature().RecordPlacement(*pxCmdList);
}

static void ExecuteGrassIndirectFixup(Flux_CommandBuffer* pxCmdList, void*)
{
	GrassFeature().RecordIndirectFixup(*pxCmdList);
}

static void ExecuteGrassGBuffer(Flux_CommandBuffer* pxCmdList, void*)
{
	GrassFeature().RecordGBuffer(*pxCmdList);
}

static void ExecuteGrassDisplacement(Flux_CommandBuffer* pxCmdList, void*)
{
	GrassFeature().RecordDisplacement(*pxCmdList);
}

//=============================================================================
// Render graph — five passes, UnifiedMesh-style edges.
//
// The dynamic (frame-indexed) buffers are NEVER declared: GetBuffer() returns a
// different physical buffer per frame in flight, so a pointer captured here would
// bind the wrong frame's buffer on every later frame. They are ordered by the
// DependsOn edges and by vkQueueSubmit's implicit host-write barrier.
//
// THE INTRA-FRAME DISPLACEMENT CONVENTION, stated once:
//
//   "Grass Displacement" runs LAST, AFTER placement, and the placement CS samples
//   the field the PREVIOUS frame produced.
//
// The obvious arrangement is the other way round — decay/splat early, sample the
// fresh field the same frame — and it does not survive the graph's cross-frame
// rules. A persistent image only keeps its cyclic barrier seed when its LAST
// access in the frame is a WRITE (Flux_RenderGraph_Compilation.cpp, the
// prime/un-seed pass after SeedCyclicImageState); a read-last image is un-seeded
// back to a per-frame UNDEFINED first touch, which is licensed to DISCARD the
// contents at the frame boundary. The trail map's whole point is that it survives
// that boundary, so the frame has to END on the write. Placement-then-displacement
// gets that for free; the cost is one frame of latency on a field whose e-folding
// time is half a second.
//
// Because the producer is therefore declared AFTER its consumer, the placement
// read is declared with ReadsPrevFrame — the same marker the TAA history uses, and
// for the same real reason, not to quiet a mis-ordered same-frame producer. The
// ordering itself comes from the explicit DependsOn on the displacement pass; the
// marker only exempts that one usage from ValidateProducerBeforeConsumer.
//=============================================================================

void Flux_GrassImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	if (!m_bGPUResourcesReady)
	{
		return;
	}

	Flux_PassHandle xResetPass = xGraph.AddPass("Grass Reset", ExecuteGrassReset);

	// The gather hangs on the placement pass, which is ALWAYS enabled and early-outs
	// internally. The graph skips the Prepare of a DISABLED pass, so gating the pass
	// on "is there grass this frame" would also gate the CPU work that decides it.
	Flux_PassHandle xPlacementPass = xGraph.AddPass("Grass Placement", ExecuteGrassPlacement)
		.Prepare([](void* pUserData) { GrassFeature().GatherGrassFrame(pUserData); })
		.DependsOn(xResetPass);

	Flux_PassHandle xFixupPass = xGraph.AddPass("Grass Indirect Fixup", ExecuteGrassIndirectFixup)
		.DependsOn(xPlacementPass);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_PassHandle xGBufferPass = xGraph.AddPass("Grass GBuffer", ExecuteGrassGBuffer)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV)
		.DependsOn(xFixupPass);

	// The optional velocity MRT changes the pass's ATTACHMENT COUNT, never whether
	// the pass exists — the pass set has to be invariant under the TAA toggle, and
	// the record-time pipeline pick reads the same latch this does.
	if (xGraphics.IsVelocityMRTActive())
	{
		xGraph.Write(xGBufferPass, xGraphics.GetVelocityAttachment(), RESOURCE_ACCESS_WRITE_RTV);
	}

	Flux_Buffer& xPool    = m_xBladePoolBuffer.GetBuffer();
	Flux_Buffer& xVisible = m_xVisibleIndexBuffer.GetBuffer();
	Flux_Buffer& xCounter = m_xBladeCounterBuffer.GetBuffer();
	Flux_Buffer& xArgs    = m_xIndirectArgsBuffer.GetBuffer();

	// Reset fully overwrites both.
	xGraph.WriteBuffer(xResetPass, xArgs,    RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xResetPass, xCounter, RESOURCE_ACCESS_WRITE_UAV);

	// Placement scatters into the pool + visible list and read-modify-writes both
	// cursors (InterlockedAdd), so those two are READWRITE and order after the reset.
	xGraph.WriteBuffer(xPlacementPass, xPool,    RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xPlacementPass, xVisible, RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xPlacementPass, xCounter, RESOURCE_ACCESS_READWRITE_UAV);
	xGraph.WriteBuffer(xPlacementPass, xArgs,    RESOURCE_ACCESS_READWRITE_UAV);

	// Fixup clamps the instance counts an over-budget frame overshot.
	xGraph.WriteBuffer(xFixupPass, xArgs, RESOURCE_ACCESS_READWRITE_UAV);

	// The draw reads the two SSBOs plus the args at the command-processor stage.
	xGraph.ReadBuffer(xGBufferPass, xPool,    RESOURCE_ACCESS_READ_BUFFER_SRV);
	xGraph.ReadBuffer(xGBufferPass, xVisible, RESOURCE_ACCESS_READ_BUFFER_SRV);
	xGraph.ReadBuffer(xGBufferPass, xArgs,    RESOURCE_ACCESS_READ_INDIRECT_ARG);

	// The trail field. UNCONDITIONAL — the pass exists whether or not displacement
	// (or grass at all) is enabled and early-outs internally, because a pass set that
	// came and went with an option would rebuild the graph rather than skip work in it.
	Flux_PassHandle xDisplacementPass = xGraph.AddPass("Grass Displacement", ExecuteGrassDisplacement)
		.DependsOn(xPlacementPass);

	// BOTH halves of the ping-pong get the SAME declaration on both passes, which is
	// what makes the per-frame role swap invisible to the graph: the compiled
	// barriers are identical under either parity, so the record is free to decide
	// which image is the source and which is the destination. Declaring one read and
	// one written would need a different graph on odd and even frames, and the graph
	// is compiled per BUILD, not per frame.
	for (u_int u = 0; u < 2u; u++)
	{
		// Read-modify-write: the pass loads the source through a storage-image view
		// and stores the destination through another. One access covers both roles.
		xGraph.Write(xDisplacementPass, m_axDisplacementMaps[u], RESOURCE_ACCESS_READWRITE_UAV);
		// ...and the placement CS samples last frame's result. See the convention note.
		xGraph.ReadPrevFrame(xPlacementPass, m_axDisplacementMaps[u], RESOURCE_ACCESS_READ_SRV);
	}
}

//=============================================================================
// Gather (main thread — the single writer of the per-frame state)
//=============================================================================

void Flux_GrassImpl::AdvanceWind(float fTimeSeconds)
{
	// The previous block is captured BEFORE the advance: it is the only
	// previous-frame state the velocity vertex stage reads, so it must describe the
	// pose the blade actually held last frame. On the first frame it equals the
	// current block, which reports zero motion rather than a jump.
	m_xPrevWind = m_xWind;

	m_xWind.m_fTime = fTimeSeconds;
	// The yaw is the stored truth; the direction vector is derived here so the debug
	// slider and a game-code SetWindDirection can never disagree.
	const float fYawRad = glm::radians(m_fWindYawDeg);
	m_xWind.m_xDirectionXZ = Zenith_Maths::Vector2(cosf(fYawRad), sinf(fYawRad));
}

float Flux_GrassImpl::ComputeBladeHeadroom() const
{
	// Blades stand ON the ground, so a tile AABB built from terrain height alone
	// clips the tallest type's tips and culls tiles whose grass is visible.
	float fTallest = 0.0f;
	for (u_int u = 0; u < m_xTypeTable.GetCount(); u++)
	{
		const Flux_GrassTypeParams& xType = m_xTypeTable.Get(u);
		fTallest = glm::max(fTallest, xType.m_fHeightMax * (1.0f + xType.m_fClumpHeightBoost));
	}
	return fTallest;
}

void Flux_GrassImpl::StageFrustumViewProjs(const Flux_RenderViewRegistry& xViews)
{
	// Slot 0 is the camera. NoJitter, NEVER the jittered view-proj: TAA's sub-pixel
	// jitter would shimmer both the tile set and the per-blade cull frame to frame.
	m_axFrustumViewProjs[0] = xViews.View(kuFluxViewSlotMain).m_xConstants.m_xViewProjMatNoJitter;

	// Slots 1-2 are cascades 0-1, read from the SAME registry payload the unified
	// cull extracts its per-view planes from, so a blade and the mesh beside it are
	// culled against the identical cascade frustum. A cascade never jitters —
	// UpdateShadowMatrices stages NoJitter equal to its ortho view-proj — so the
	// accessor is shared for uniformity, not because there is a jitter to strip.
	//
	// The count is what activates the cascade partitions, so it must reflect REAL
	// planes only: it stays at zero whenever casting is off, which is what stops the
	// placement CS generating into partitions no cascade will draw.
	m_uCascadeFrustaCount = 0u;
	if (IsShadowCastingEnabled())
	{
		for (u_int u = 0; u < uFLUX_GRASS_MAX_CASCADES; u++)
		{
			const u_int uSlot = kuFluxViewSlotShadowFirst + u;
			if (!xViews.IsViewActive(uSlot))
			{
				// Cascades activate as a block from slot 0 up, so the first inactive
				// one ends the run — taking a later one would mis-key the partitions.
				break;
			}
			m_axFrustumViewProjs[1u + u] = xViews.View(uSlot).m_xConstants.m_xViewProjMatNoJitter;
			m_uCascadeFrustaCount = u + 1u;
		}
	}

	// Every unfilled slot carries a COPY of the camera's. Nothing reads them (they are
	// outside the active-slot mask), but a duplicate degrades to "cull like the camera"
	// if a slot were ever enabled early, whereas uninitialised planes cull randomly.
	for (u_int u = m_uCascadeFrustaCount; u < uFLUX_GRASS_MAX_CASCADES; u++)
	{
		m_axFrustumViewProjs[1u + u] = m_axFrustumViewProjs[0];
	}
}

void Flux_GrassImpl::SelectTilesForFrame(const Zenith_Maths::Vector3& xCameraPos)
{
	if (m_bFreezeCulling && m_xTileList.m_uCount > 0u)
	{
		// Reuse the frozen schedule verbatim — including the stats derived from it,
		// which is the point: the debug var exists to hold a set still while it is
		// inspected from a moved camera.
		return;
	}

	// Camera + every live cascade, from the matrices this frame froze.
	Zenith_Frustum axFrusta[uFLUX_GRASS_FRUSTUM_COUNT];
	const u_int uFrustaCount = 1u + glm::min(m_uCascadeFrustaCount, uFLUX_GRASS_MAX_CASCADES);
	for (u_int u = 0; u < uFrustaCount; u++)
	{
		axFrusta[u].ExtractFromViewProjection(m_axFrustumViewProjs[u]);
	}

	Flux_GrassTileSelectParams xParams;
	xParams.m_xCameraPos = xCameraPos;
	xParams.m_fMaxDistance = m_fMaxDistance;
	xParams.m_fBladeHeadroom = ComputeBladeHeadroom();
	// Maps cover [0, worldSize] on both axes from the world origin.
	xParams.m_xExtents.m_fMinX = 0.0f;
	xParams.m_xExtents.m_fMinZ = 0.0f;
	xParams.m_xExtents.m_fMaxX = m_fMapWorldSize;
	xParams.m_xExtents.m_fMaxZ = m_fMapWorldSize;
	xParams.m_xHeights.m_pfMinY = m_afTileMinY.GetDataPointer();
	xParams.m_xHeights.m_pfMaxY = m_afTileMaxY.GetDataPointer();
	xParams.m_xHeights.m_uCellsX = m_uHeightGridCells;
	xParams.m_xHeights.m_uCellsZ = m_uHeightGridCells;
	xParams.m_xHeights.m_fCellSize = m_fHeightGridCellSize;
	xParams.m_xHeights.m_fFallbackMinY = m_fHeightBias;
	xParams.m_xHeights.m_fFallbackMaxY = m_fHeightBias + m_fHeightScale;
	// A UNION, never an intersection: a tile behind the camera still has to fill the
	// cascades it casts into, so a tile survives if ANY of these wants it. With
	// casting off the count is 1 and the selection is exactly what it was before
	// grass cast at all.
	xParams.m_xFrusta.m_pxFrusta = axFrusta;
	xParams.m_xFrusta.m_uCount = uFrustaCount;

	Flux_GrassSelectTiles(xParams, m_xTileList);

	m_uVisibleTileCount = m_xTileList.m_uCount;
	m_uTileCount = m_xTileList.m_uConsidered;
	// Lattice cells dispatched, not blades. Deterministic for fixed inputs because
	// the scheduler is pure and its output carries a total order.
	m_uScheduledInstanceCount = m_xTileList.m_uCount * uFLUX_GRASS_TILE_CELL_COUNT;
}

void Flux_GrassImpl::StageTileRecords(Flux_GrassTileGPU* paxOut) const
{
	for (u_int u = 0; u < m_xTileList.m_uCount; u++)
	{
		const Flux_GrassTile& xTile = m_xTileList.Get(u);
		const bool bLo = xTile.m_eLOD == Flux_GrassTileLOD::LO;
		// GLOBAL HI-lattice units, not metres: both LODs share one lattice->world
		// mapping, which is what makes a LO tile's nodes exactly the (even,even)
		// subset of the HI lattice and the HI->LO transition a fade.
		const int iSpan = TileLatticeSpan(xTile.m_eLOD);
		paxOut[u].m_iLatticeBaseX  = xTile.m_iTileX * iSpan;
		paxOut[u].m_iLatticeBaseZ  = xTile.m_iTileZ * iSpan;
		paxOut[u].m_uLatticeStride = bLo ? Flux_GrassConfig::uLO_LATTICE_STRIDE : 1u;
		paxOut[u].m_uIsLoLOD       = bLo ? 1u : 0u;
	}
}

void Flux_GrassImpl::StagePlacementConstants(Flux_GrassPlacementConstantsGPU& xOut,
	const Zenith_Maths::Vector3& xCameraPos) const
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();

	// Camera in slot 0, cascades 0-1 in slots 1-2 — the very matrices the tile
	// scheduler culled against this frame, so a tile kept for a cascade and the blades
	// inside it cannot disagree about which views want them. Slots without a live
	// cascade already hold a camera copy and are masked off.
	for (u_int uView = 0; uView < uFLUX_GRASS_FRUSTUM_COUNT; uView++)
	{
		Flux_InstanceCullingUtil::ExtractFrustumPlanes(m_axFrustumViewProjs[uView], &xOut.m_axFrustumPlanes[uView * 6u]);
	}

	FillWindBlock(xOut.m_xWind, m_xWind, xOpts.m_bGrassWindEnabled);
	xOut.m_xCameraPosWS_HiRadius = Zenith_Maths::Vector4(xCameraPos, Flux_GrassConfig::fHI_RADIUS);

	const float fInvWorld = m_fMapWorldSize > 0.0f ? 1.0f / m_fMapWorldSize : 0.0f;
	// Coverage scale/bias are the MAP's own contrast controls and stay neutral; the
	// global density scale rides the LoRadius block below, so it is applied once.
	xOut.m_xCoverageMapParams = Zenith_Maths::Vector4(m_fMapWorldSize, fInvWorld, 1.0f, 0.0f);
	xOut.m_xTypeMapParams = Zenith_Maths::Vector4(m_fMapWorldSize, fInvWorld,
		m_uTypeSize > 0u ? 1.0f / static_cast<float>(m_uTypeSize) : 0.0f,
		static_cast<float>(m_xTypeTable.GetCount()));
	xOut.m_xHeightMapParams = Zenith_Maths::Vector4(m_fMapWorldSize, fInvWorld, m_fHeightScale, m_fHeightBias);

	// Central-difference step of one height texel. A larger step smooths real slopes
	// away; a smaller one reads the same texel twice and reports a flat normal.
	const float fTexelMetres = m_uHeightSize > 0u ? m_fMapWorldSize / static_cast<float>(m_uHeightSize) : 1.0f;
	xOut.m_xSlopeStep_Pad = Zenith_Maths::Vector4(fTexelMetres, 0.0f, 0.0f, 0.0f);

	// The push scale is the ONLY route the displacement field has into a blade, so
	// disabling displacement zeroes it here rather than branching in the CS (a branch
	// there diverges a whole wave for a field that is usually inert).
	//
	// The origin is the PREVIOUS anchor, not this frame's: the map the placement CS
	// samples is the one the last frame's displacement pass wrote, and it is aligned
	// to the anchor that pass was given. Using this frame's would slide every blade's
	// lookup by the frame's scroll — a trail that lagged behind its own mover by
	// exactly the camera's motion.
	const float fPushScale = xOpts.m_bGrassDisplacementEnabled ? m_fDisplacementPushScale : 0.0f;
	const Zenith_Maths::Vector2 xDisplacementOrigin = Flux_GrassDisplacementOriginWS(m_xDisplacementAnchorPrev);
	xOut.m_xDisplacementParams = Zenith_Maths::Vector4(
		xDisplacementOrigin.x, xDisplacementOrigin.y,
		Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE, fPushScale);

	const float fMaxDistance = Flux_GrassClampMaxDistance(m_fMaxDistance);
	xOut.m_xLoRadius_MaxDist_Density_Pad = Zenith_Maths::Vector4(fMaxDistance, fMaxDistance, m_fDensityScale, 0.0f);

	xOut.m_xTileCount_Seed_ActiveSlots_PoolCap = Zenith_Maths::UVector4(
		m_xTileList.m_uCount, uGRASS_PLACEMENT_SEED, ComputeActiveSlotMask(), uFLUX_GRASS_BLADE_POOL_CAPACITY);
}

void Flux_GrassImpl::StageDrawConstants(Flux_GrassDrawConstantsGPU& xOut, const Zenith_Maths::Vector3& xCameraPos) const
{
	FillWindBlock(xOut.m_xWind, m_xWind, Zenith_GraphicsOptions::Get().m_bGrassWindEnabled);
	// The MAIN camera position in EVERY pass, cascades included: the shadow pose has
	// to be the pose the lit blade is holding, and both the class fade and the LOD
	// convergence key off distance to the camera, not to the light.
	xOut.m_xCameraPosWS_HiRadius = Zenith_Maths::Vector4(xCameraPos, Flux_GrassConfig::fHI_RADIUS);
	xOut.m_xMinPixelWidth_Thicken_NormalBlend_LODBand = Zenith_Maths::Vector4(
		fGRASS_MIN_PIXEL_WIDTH, fGRASS_THICKEN_BLEND, fGRASS_NORMAL_BLEND_DISTANCE, fGRASS_LOD_BLEND_BAND);
	xOut.m_xMaxDrawDistance_GlossCut_DebugMode_Pad = Zenith_Maths::Vector4(
		Flux_GrassClampMaxDistance(m_fMaxDistance), fGRASS_GLOSS_CUT_DISTANCE,
		std::bit_cast<float>(m_uDebugMode), 0.0f);
}

void Flux_GrassImpl::AdvanceDisplacementAnchor(const Zenith_Maths::Vector3& xCameraPos)
{
	// Flip FIRST. After it, m_uDisplacementWriteIndex names the map this frame FILLS
	// and the other names the map both the displacement pass and the placement CS
	// READ — so no pass ever samples the image it is writing, whatever the parity.
	m_uDisplacementWriteIndex ^= 1u;

	// Last frame's destination anchor is this frame's source anchor by definition:
	// they describe the same image, one frame apart.
	m_xDisplacementAnchorPrev = m_xDisplacementAnchorNext;
	m_xDisplacementAnchorNext = Flux_GrassSnapDisplacementAnchor(xCameraPos.x, xCameraPos.z);
}

void Flux_GrassImpl::SubmitDebugOrbitMover(const Zenith_Maths::Vector3& xCameraPos)
{
	if (!m_bDebugOrbitDisplacer)
	{
		return;
	}

	// Driven by the frame's STAGED wind time — the same clock the blades sway on, so
	// the orbiter cannot drift against the field it is pushing and a fixed-dt capture
	// replays the identical trail. A wall-clock read here would make the capture
	// machine-dependent.
	const float fPhase = (m_xWind.m_fTime / fGRASS_ORBIT_PERIOD_SECONDS) * fGRASS_TAU;

	Mover xMover;
	xMover.m_xPos = Zenith_Maths::Vector3(
		xCameraPos.x + cosf(fPhase) * fGRASS_ORBIT_RADIUS,
		xCameraPos.y,
		xCameraPos.z + sinf(fPhase) * fGRASS_ORBIT_RADIUS);
	xMover.m_fRadius = fGRASS_ORBIT_MOVER_RADIUS;
	xMover.m_fStrength = 1.0f;
	// Through the PUBLIC submit, so the orbiter answers to the same cap and overflow
	// counter a game body does rather than getting a private slot.
	SubmitMover(xMover);
}

void Flux_GrassImpl::StageMoverRecords(Flux_GrassMoverGPU* paxOut) const
{
	for (u_int u = 0; u < m_axMovers.GetSize(); u++)
	{
		const Mover& xMover = m_axMovers.Get(u);
		// WORLD XZ, not map-relative: the CS resolves each texel to a world position
		// anyway, and a map-relative record would have to be restaged the moment the
		// anchor scrolled — which is every frame the camera moves.
		paxOut[u].m_xPosXZ_Radius_Strength = Zenith_Maths::Vector4(
			xMover.m_xPos.x, xMover.m_xPos.z, xMover.m_fRadius, xMover.m_fStrength);
	}
}

void Flux_GrassImpl::StageDisplacementConstants(Flux_GrassDisplacementConstantsGPU& xOut, float fDeltaSeconds) const
{
	const Zenith_Maths::Vector2 xPrevOrigin = Flux_GrassDisplacementOriginWS(m_xDisplacementAnchorPrev);
	const Zenith_Maths::Vector2 xNextOrigin = Flux_GrassDisplacementOriginWS(m_xDisplacementAnchorNext);

	// An unproduced source is rejected by SCROLLING PAST IT rather than by a validity
	// flag in the shader: a whole map of scroll puts every source texel out of range,
	// so the field starts from exact zero. Multiplying an unproduced source by a zero
	// decay would not do — undefined RG16F bytes can decode as NaN, and NaN * 0 is
	// still NaN, which would then live in the field forever.
	const Zenith_Maths::Vector2 xShift = m_bDisplacementSourceValidThisFrame
		? Flux_GrassDisplacementTexelShift(m_xDisplacementAnchorPrev, m_xDisplacementAnchorNext)
		: Zenith_Maths::Vector2(static_cast<float>(Flux_GrassConfig::uDISPLACEMENT_RESOLUTION),
			static_cast<float>(Flux_GrassConfig::uDISPLACEMENT_RESOLUTION));

	xOut.m_xPrevOriginXZ_Size_Pad = Zenith_Maths::Vector4(
		xPrevOrigin.x, xPrevOrigin.y, Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE, 0.0f);
	xOut.m_xNextOriginXZ_Size_Pad = Zenith_Maths::Vector4(
		xNextOrigin.x, xNextOrigin.y, Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE, 0.0f);
	xOut.m_xDecay_Texel_ShiftXZ = Zenith_Maths::Vector4(
		Flux_GrassDisplacementDecay(fDeltaSeconds, fGRASS_DISPLACEMENT_EFOLD_SECONDS),
		Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE, xShift.x, xShift.y);
	xOut.m_xResolution_MoverCount = Zenith_Maths::UVector4(
		Flux_GrassConfig::uDISPLACEMENT_RESOLUTION, Flux_GrassConfig::uDISPLACEMENT_RESOLUTION,
		m_axMovers.GetSize(), 0u);
}

void Flux_GrassImpl::UploadFrameBuffers(const Zenith_Maths::Vector3& xCameraPos, float fDeltaSeconds)
{
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();

	// Tile records. Only the live prefix is uploaded; the tail is never read (the CS
	// bounds-guards on the tile count in the constants).
	if (m_xTileList.m_uCount > 0u)
	{
		Flux_GrassTileGPU axTiles[Flux_GrassConfig::uMAX_TILES];
		StageTileRecords(axTiles);
		xMem.UploadBufferData(m_xTileBuffer.GetBuffer().m_xVRAMHandle, axTiles,
			static_cast<size_t>(m_xTileList.m_uCount) * sizeof(Flux_GrassTileGPU));
	}

	// The whole 2 KB type block, every frame. Tracking a dirty flag across frames in
	// flight would need a per-copy refresh counter to be correct, which costs more
	// code than the upload it would save.
	Flux_GrassTypeParamsGPU axTypeParams[uFLUX_GRASS_MAX_TYPES];
	m_xTypeTable.ToGPU(axTypeParams);
	xMem.UploadBufferData(m_xTypeParamsBuffer.GetBuffer().m_xVRAMHandle, axTypeParams, sizeof(axTypeParams));

	Flux_GrassPlacementConstantsGPU xPlacement;
	StagePlacementConstants(xPlacement, xCameraPos);
	xMem.UploadBufferData(m_xPlacementConstantsBuffer.GetBuffer().m_xVRAMHandle, &xPlacement, sizeof(xPlacement));

	Flux_GrassDrawConstantsGPU xDraw;
	StageDrawConstants(xDraw, xCameraPos);
	xMem.UploadBufferData(m_xDrawConstantsBuffer.GetBuffer().m_xVRAMHandle, &xDraw, sizeof(xDraw));

	// The previous block is muted by the SAME flag as the current one: a prev pose
	// that still swayed against a muted current pose would report the mute itself as
	// blade motion and smear the velocity buffer for a frame.
	Flux_GrassWindBlockGPU xPrevWind;
	FillWindBlock(xPrevWind, m_xPrevWind, Zenith_GraphicsOptions::Get().m_bGrassWindEnabled);
	xMem.UploadBufferData(m_xPrevWindConstantsBuffer.GetBuffer().m_xVRAMHandle, &xPrevWind, sizeof(xPrevWind));

	// Movers: only the live prefix. The tail is never read — the CS bounds-guards on
	// the count in the constants and the count is staged from the same list.
	if (m_axMovers.GetSize() > 0u)
	{
		Flux_GrassMoverGPU axMovers[uFLUX_GRASS_MAX_MOVERS];
		StageMoverRecords(axMovers);
		xMem.UploadBufferData(m_xMoverBuffer.GetBuffer().m_xVRAMHandle, axMovers,
			static_cast<size_t>(m_axMovers.GetSize()) * sizeof(Flux_GrassMoverGPU));
	}

	Flux_GrassDisplacementConstantsGPU xDisplacement;
	StageDisplacementConstants(xDisplacement, fDeltaSeconds);
	xMem.UploadBufferData(m_xDisplacementConstantsBuffer.GetBuffer().m_xVRAMHandle, &xDisplacement, sizeof(xDisplacement));
}

void Flux_GrassImpl::GatherGrassFrame(void*)
{
	ZENITH_PROFILE_SCOPE("Grass Gather");

	m_uSubmittedDrawCount = 0u;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	// Frozen here so the worker-side record and the setup-time attachment count read
	// the same answer even if the latch flips mid-frame.
	m_bVelocityLatched = xGraphics.IsVelocityMRTActive();
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	m_bEnabledLatched = xOpts.m_bGrassEnabled;
	m_bDisplacementLatched = xOpts.m_bGrassDisplacementEnabled;

	// Wind advances whether or not there is grass this frame: the field is a global
	// atmosphere clock, and letting it stall would make the first frame after a
	// build (or after a re-enable) resume from a stale phase.
	const FrameContext& xFrame = g_xEngine.Frame();
	AdvanceWind(static_cast<float>(xFrame.GetTimePassed()));

	if (!m_bGPUResourcesReady || !m_bBuilt || !m_bEnabledLatched)
	{
		m_xTileList.Clear();
		m_uScheduledInstanceCount = 0u;
		m_uVisibleTileCount = 0u;
		m_uTileCount = 0u;
		// No frame was staged, so no cascade frustum is live — the count must say so
		// or the mask would advertise partitions this frame never filled.
		m_uCascadeFrustaCount = 0u;
		// Movers are consumed every frame whether or not there is grass to push. An
		// unbuilt world that kept them would let a submitter accumulate silently and
		// then splat a frame's worth of stale pushes the moment a map arrived.
		m_axMovers.Clear();
		// No dispatch runs on this path, so whatever the maps hold is stale or
		// undefined — the frame grass comes back has to start the field from zero.
		m_bDisplacementFieldValid = false;
		m_bDisplacementSourceValidThisFrame = false;
		return;
	}

	const Zenith_Maths::Vector3 xCameraPos = xGraphics.GetCameraPosition();

	// Before the upload, or the orbiter's push would land one frame late.
	SubmitDebugOrbitMover(xCameraPos);

	StageFrustumViewProjs(xGraphics.RenderViews());
	SelectTilesForFrame(xCameraPos);
	// Before the constants: BOTH the placement and displacement blocks are written
	// from the anchor pair this call establishes.
	AdvanceDisplacementAnchor(xCameraPos);
	// This frame's source is real iff a dispatch produced it last frame; and this
	// frame's dispatch is what makes the NEXT frame's source real. Two reads of one
	// fact a frame apart, so the record cannot see the answer meant for next frame.
	m_bDisplacementSourceValidThisFrame = m_bDisplacementFieldValid;
	m_bDisplacementFieldValid = m_bDisplacementLatched;
	UploadFrameBuffers(xCameraPos, xFrame.GetDt());

	// Cleared only AFTER the upload has consumed them, so a mover submitted during
	// this frame's update reaches the GPU exactly once and never twice.
	m_axMovers.Clear();

	// HI + LO. Recorded unconditionally when there is anything to place; a frame that
	// placed nothing still records them with instanceCount 0 (see RecordGBuffer).
	m_uSubmittedDrawCount = m_xTileList.m_uCount > 0u ? 2u : 0u;
}

//=============================================================================
// Record callbacks (workers — pure readers of the frozen gather state)
//=============================================================================

void Flux_GrassImpl::RecordReset(Flux_CommandBuffer& xCmdBuf)
{
	// Disabled grass emits NOTHING from any of the four callbacks — no dispatch and
	// no draw — while the passes, pipelines and buffers all stay exactly as they are.
	// Pass existence must never depend on an option: the graph keys its barriers by
	// object address, so a pass set that came and went with a toggle would rebuild
	// the graph rather than skip work in it.
	if (!m_bGPUResourcesReady || !m_bEnabledLatched)
	{
		return;
	}

	xCmdBuf.BindComputePipeline(&m_xResetPipeline);
	Flux_ShaderBinder xBinder(xCmdBuf);
	xBinder.BindUAV_Buffer(m_xResetShader, "IndirectArgs", &m_xIndirectArgsBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xResetShader, "BladeCounter", &m_xBladeCounterBuffer.GetUAV());
	// One thread per indirect slot, a single group.
	xCmdBuf.Dispatch(1u, 1u, 1u);
}

void Flux_GrassImpl::RecordPlacement(Flux_CommandBuffer& xCmdBuf)
{
	// Nothing scheduled: skip the DISPATCH, not the pass. On that path the reset HAS
	// run, so the indirect commands are validly zeroed and the draws are no-ops.
	if (!m_bGPUResourcesReady || !m_bEnabledLatched || m_xTileList.m_uCount == 0u)
	{
		return;
	}

	xCmdBuf.BindComputePipeline(&m_xPlacementPipeline);
	Flux_ShaderBinder xBinder(xCmdBuf);
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	xBinder.BindCBV(m_xPlacementShader, "GrassPlacementConstants", &m_xPlacementConstantsBuffer.GetCBV());
	xBinder.BindSRV_Buffer(m_xPlacementShader, "TypeParams", m_xTypeParamsBuffer.GetSRV());
	xBinder.BindSRV_Buffer(m_xPlacementShader, "Tiles", m_xTileBuffer.GetSRV());
	xBinder.BindSRV(m_xPlacementShader, "g_xHeightMap", &m_xHeightTexture.m_xSRV, &xGraphics.m_xClampSampler);
	xBinder.BindSRV(m_xPlacementShader, "g_xCoverageMap", &m_xCoverageTexture.m_xSRV, &xGraphics.m_xClampSampler);
	// POINT, never linear. The texel value IS a type index: a lerp between type 0 and
	// type 4 selects type 2, a type the author never placed there.
	xBinder.BindSRV(m_xPlacementShader, "g_xTypeMap", &m_xTypeTexture.m_xSRV, &xGraphics.m_xPointSampler);
	// The REAL trail map — the SOURCE half of the ping-pong, which is the one last
	// frame's displacement pass filled and the one this frame's placement constants
	// carry the anchor of. Otherwise the neutral 1x1: with displacement off the push
	// scale is already zero, but binding a live field the CS is told to ignore leaves
	// two things that have to agree; and before the first dispatch the source holds
	// undefined bytes that must not reach a blade at all.
	const bool bDisplacementLive = m_bDisplacementLatched && m_bDisplacementSourceValidThisFrame;
	const Flux_ShaderResourceView* pxDisplacementSRV = bDisplacementLive
		? &m_axDisplacementMaps[1u - m_uDisplacementWriteIndex].SRV()
		: &m_xDisplacementTexture.m_xSRV;
	// CLAMP, never repeat: a blade standing outside the 64 m footprint must read the
	// border value, not the field wrapped in from the opposite edge.
	xBinder.BindSRV(m_xPlacementShader, "g_xDisplacementMap", pxDisplacementSRV, &xGraphics.m_xClampSampler);
	xBinder.BindUAV_Buffer(m_xPlacementShader, "BladePool", &m_xBladePoolBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xPlacementShader, "BladeCounter", &m_xBladeCounterBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xPlacementShader, "VisibleIndices", &m_xVisibleIndexBuffer.GetUAV());
	xBinder.BindUAV_Buffer(m_xPlacementShader, "IndirectArgs", &m_xIndirectArgsBuffer.GetUAV());

	// TILE-MAJOR: one thread per lattice cell across every scheduled tile, matching
	// the CS's tid / kGRASS_TILE_CELL_COUNT decode.
	const u_int uThreads = m_xTileList.m_uCount * uFLUX_GRASS_TILE_CELL_COUNT;
	xCmdBuf.Dispatch((uThreads + uFLUX_GRASS_PLACEMENT_GROUP_SIZE - 1u) / uFLUX_GRASS_PLACEMENT_GROUP_SIZE, 1u, 1u);
}

void Flux_GrassImpl::RecordDisplacement(Flux_CommandBuffer& xCmdBuf)
{
	// Two different silences, and the difference is deliberate:
	//
	//   grass DISABLED  -> nothing at all, exactly like every other callback here.
	//   displacement disabled (grass on) -> the pass still exists and still emits
	//     nothing, which FREEZES the field rather than zeroing it. That costs nothing
	//     because the push scale is zero on that path, so no blade samples the frozen
	//     field; and m_bGrassDisplacementEnabled is set once at boot, so "re-enabled
	//     onto a stale trail" is not a live path. Spending a full-map dispatch every
	//     frame to decay a field nobody reads would be the worse trade.
	if (!m_bGPUResourcesReady || !m_bEnabledLatched || !m_bDisplacementLatched)
	{
		return;
	}

	xCmdBuf.BindComputePipeline(&m_xDisplacementPipeline);
	Flux_ShaderBinder xBinder(xCmdBuf);
	xBinder.BindCBV(m_xDisplacementShader, "GrassDisplacementConstants", &m_xDisplacementConstantsBuffer.GetCBV());
	xBinder.BindSRV_Buffer(m_xDisplacementShader, "Movers", m_xMoverBuffer.GetSRV());
	// Both halves as storage images. The source is LOADED at an integer texel offset
	// rather than sampled, which is what lets the graph declare a single access for
	// both roles — and is exact, because the anchor snap makes the offset integral.
	xBinder.BindUAV_Texture(m_xDisplacementShader, "g_xPrevDisplacementTex",
		&m_axDisplacementMaps[1u - m_uDisplacementWriteIndex].UAV(0u));
	xBinder.BindUAV_Texture(m_xDisplacementShader, "g_xNextDisplacementTex",
		&m_axDisplacementMaps[m_uDisplacementWriteIndex].UAV(0u));

	const u_int uGroups = (Flux_GrassConfig::uDISPLACEMENT_RESOLUTION + uFLUX_GRASS_DISPLACEMENT_GROUP_SIZE - 1u)
		/ uFLUX_GRASS_DISPLACEMENT_GROUP_SIZE;
	xCmdBuf.Dispatch(uGroups, uGroups, 1u);
}

void Flux_GrassImpl::RecordIndirectFixup(Flux_CommandBuffer& xCmdBuf)
{
	if (!m_bGPUResourcesReady || !m_bEnabledLatched)
	{
		return;
	}

	xCmdBuf.BindComputePipeline(&m_xFixupPipeline);
	Flux_ShaderBinder xBinder(xCmdBuf);
	xBinder.BindUAV_Buffer(m_xFixupShader, "IndirectArgs", &m_xIndirectArgsBuffer.GetUAV());
	xCmdBuf.Dispatch(1u, 1u, 1u);
}

void Flux_GrassImpl::RecordGBuffer(Flux_CommandBuffer& xCmdBuf)
{
	// Both indirect draws go with the reset that seeds their args: recording them
	// against args no reset touched this frame would redraw the last enabled frame's
	// blades out of a pool nothing refilled.
	if (!m_bGPUResourcesReady || !m_bEnabledLatched)
	{
		return;
	}

	// Same pure selector the pipeline set was built from, against the latch the
	// gather froze — which is the latch SetupRenderGraph used to decide the
	// attachment count, so the pipeline and the framebuffer cannot disagree.
	const Flux_GrassPipelineVariant eVariant = Flux_GrassSelectPipelineVariant(m_bVelocityLatched, false);
	const bool bVelocity = Flux_GrassPipelineVariantIsVelocity(eVariant);
	Flux_Shader& xShader = bVelocity ? m_xGBufferVelocityShader : m_xGBufferShader;

	xCmdBuf.SetPipeline(bVelocity ? &m_xGBufferVelocityPipeline : &m_xGBufferPipeline);
	// No vertex buffer at all — SV_VertexID delivers the fetched index value, which
	// IS the logical vertex id.
	xCmdBuf.SetIndexBuffer(m_xBladeIndexBuffer);

	Flux_ShaderBinder xBinder(xCmdBuf);
	// The blade shading core samples the vein / gloss / ramp tables out of the bindless
	// array, so this one is load-bearing rather than a layout formality. It was absent
	// and the pass still drew, because a Vulkan set binding survives a pipeline switch
	// when the layouts are prefix-compatible and Terrain / UnifiedMesh had already bound
	// set 2 earlier in the same worker command buffer. Relying on that makes correctness
	// depend on which OTHER features are enabled; bind it here.
	xCmdBuf.UseBindlessTextures(2);
	xBinder.BindCBV(xShader, "GrassDrawConstants", &m_xDrawConstantsBuffer.GetCBV());
	if (bVelocity)
	{
		xBinder.BindCBV(xShader, "GrassPrevWindConstants", &m_xPrevWindConstantsBuffer.GetCBV());
	}
	xBinder.BindSRV_Buffer(xShader, "BladePool", m_xBladePoolBuffer.GetSRV());
	xBinder.BindSRV_Buffer(xShader, "VisibleIndices", m_xVisibleIndexBuffer.GetSRV());
	xBinder.BindSRV_Buffer(xShader, "TypeParams", m_xTypeParamsBuffer.GetSRV());

	// Slot 0 = HI mesh, slot 1 = LO mesh. firstInstance (seeded by the reset CS)
	// carries each partition's base, so the vertex stage recovers its slice from
	// SV_StartInstanceLocation without a per-draw constant. Recorded unconditionally:
	// an empty frame draws instanceCount 0, which keeps the command stream identical
	// whether or not there is grass.
	xCmdBuf.DrawIndexedIndirect(&m_xIndirectArgsBuffer, 1u,
		uFLUX_GRASS_SLOT_CAMERA_HI * uFLUX_GRASS_INDIRECT_STRIDE, uFLUX_GRASS_INDIRECT_STRIDE);
	xCmdBuf.DrawIndexedIndirect(&m_xIndirectArgsBuffer, 1u,
		uFLUX_GRASS_SLOT_CAMERA_LO * uFLUX_GRASS_INDIRECT_STRIDE, uFLUX_GRASS_INDIRECT_STRIDE);
}

//=============================================================================
// Shadow caster (called from inside a cascade pass's record by a later phase)
//=============================================================================

void Flux_GrassImpl::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, u_int uCascade)
{
	// Only cascades 0-1 have an indirect slot and a visible-index partition at all.
	if (uCascade >= uFLUX_GRASS_MAX_CASCADES)
	{
		return;
	}
	// The enable latch is load-bearing here for the same reason it is in RecordGBuffer:
	// a disabled frame skips the reset, so the cascade slot's args are last enabled
	// frame's and would cast shadows from blades that are no longer drawn.
	if (!m_bGPUResourcesReady || !m_bBuilt || !m_bEnabledLatched || !IsShadowCastingEnabled())
	{
		return;
	}
	// The partition is only meaningful if the placement CS was told to fill it.
	const u_int uSlot = uFLUX_GRASS_SLOT_CASCADE_0 + uCascade;
	if ((ComputeActiveSlotMask() & (1u << uSlot)) == 0u)
	{
		return;
	}

	xCmdBuf.SetPipeline(&m_xShadowPipeline);
	xCmdBuf.SetIndexBuffer(m_xBladeIndexBuffer);

	Flux_ShaderBinder xBinder(xCmdBuf);
	// Set 2 is in EVERY spine pipeline's layout whether or not the shader samples it:
	// Common/Bindings.slang declares the BINDLESS block and Slang does not dead-strip a
	// declared ParameterBlock (that is what keeps the PASS block at space 3). The
	// depth-only caster never reads a texture — fsMain is empty — but Vulkan still
	// requires the set its layout names to be BOUND, and BindPersistentSpineSets covers
	// only GLOBAL/VIEW. Nothing else in a cascade is guaranteed to have bound it first:
	// UnifiedMesh::RenderToShadowMap early-outs with zero buckets BEFORE its own
	// UseBindlessTextures, so on a grass-only scene this draw is the first user.
	xCmdBuf.UseBindlessTextures(2);
	// The SAME draw constants as the lit pass. The projection differs and comes from
	// the cascade pass's own VIEW set; everything else must match or a swaying blade
	// casts a shadow it is not standing in.
	xBinder.BindCBV(m_xShadowShader, "GrassDrawConstants", &m_xDrawConstantsBuffer.GetCBV());
	xBinder.BindSRV_Buffer(m_xShadowShader, "BladePool", m_xBladePoolBuffer.GetSRV());
	xBinder.BindSRV_Buffer(m_xShadowShader, "VisibleIndices", m_xVisibleIndexBuffer.GetSRV());
	xBinder.BindSRV_Buffer(m_xShadowShader, "TypeParams", m_xTypeParamsBuffer.GetSRV());

	xCmdBuf.DrawIndexedIndirect(&m_xIndirectArgsBuffer, 1u,
		uSlot * uFLUX_GRASS_INDIRECT_STRIDE, uFLUX_GRASS_INDIRECT_STRIDE);
}
