#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"   // Flux_SkinnedInstanceIdRegistry (stable skinned bucket key)
#include "Flux/Flux_RefcountDiffRegistry.h"   // shared refcount-diff sync machinery
#include <cstddef>   // offsetof, size_t
#include <utility>   // std::move (Flux_BonePaletteHistory ping-pong)

// ============================================================================
// Flux compute-skinning core (Stage 5 — unified GPU-driven opaque-mesh pipeline).
//
// The unified path draws STATIC geometry: the cull/draw/shadow kernels read a
// per-object model matrix from the GPU-scene SSBO and apply it. Skeletal meshes
// are brought in by a COMPUTE PRE-PASS that skins each animated submesh-instance
// to OBJECT/LOCAL space and writes the result (in the static 72-byte vertex layout)
// into a shared skinned-vertex arena. The unified static VS then consumes the arena
// UNCHANGED — it already does `worldPos = model * localPos` and re-normalises the
// incoming normal/tangent, so the pre-pass outputs the RAW bone-weighted vectors
// (standard linear-blend skinning: accumulate weighted bone-transformed position +
// TBN, deferring normalisation to the VS).
//
// This header is the PURE CPU mirror of Flux_UnifiedMesh_Skinning.slang (added in
// Stage 5d): Flux_SkinVertexCPU is a line-by-line transliteration of the shader's
// skinning loop, so a headless golden test pins the GPU path. No GPU access, no
// renderer boot — the whole surface is headless-unit-testable (tests in
// Flux_Skinning.Tests.inl, hosted in an already-linked TU per the dead-strip idiom).
//
// LAYOUT RULE: Flux_SkinInputVertex / Flux_SkinOutputVertex are byte-identical to the
// skinned (104B) / static (72B) interleaved vertex layouts built by
// Flux_MeshInstance::CreateSkinnedFromAsset / CreateFromAsset, and to the Slang
// structured-buffer mirrors the compute shader reads/writes. All members are 4-byte
// aligned (glm default gentypes) so there is no scalar-packing surprise.
// ============================================================================

// 0xFFFFFFFF terminates a vertex's bone-influence list (most verts use 1-2 of 4).
inline constexpr u_int uFLUX_SKIN_BONE_SENTINEL = 0xFFFFFFFFu;

// Conservative radius multiplier applied to a skinned submesh's BIND-POSE bounding
// sphere before frustum culling. Animation can push verts outside the bind-pose
// extent; inflating the cull sphere trades a little over-draw for no pop. (Animated
// meshes were never culled at all before Stage 5, so this is the chosen safety net.)
inline constexpr float fFLUX_SKIN_BOUNDS_INFLATION = 2.0f;

// ---- vertex layouts (byte mirrors of the GPU interleaved formats) -----------
// Skinned INPUT (104B): pos(12)+uv(8)+normal(12)+tangent(12)+bitangent(12)+color(16)
//                       +boneIndices uint4(16)+boneWeights float4(16).
struct Flux_SkinInputVertex
{
	Zenith_Maths::Vector3 m_xPosition;     // offset  0
	Zenith_Maths::Vector2 m_xUV;           // offset 12
	Zenith_Maths::Vector3 m_xNormal;       // offset 20
	Zenith_Maths::Vector3 m_xTangent;      // offset 32
	Zenith_Maths::Vector3 m_xBitangent;    // offset 44
	Zenith_Maths::Vector4 m_xColor;        // offset 56
	u_int                 m_auBoneIDs[4];  // offset 72
	Zenith_Maths::Vector4 m_xBoneWeights;  // offset 88
};
static_assert(sizeof(Flux_SkinInputVertex) == 104, "Flux_SkinInputVertex must be 104 bytes (skinned vertex stride)");
static_assert(offsetof(Flux_SkinInputVertex, m_xUV)          == 12,  "skinned vertex UV at offset 12");
static_assert(offsetof(Flux_SkinInputVertex, m_xNormal)      == 20,  "skinned vertex normal at offset 20");
static_assert(offsetof(Flux_SkinInputVertex, m_xTangent)     == 32,  "skinned vertex tangent at offset 32");
static_assert(offsetof(Flux_SkinInputVertex, m_xBitangent)   == 44,  "skinned vertex bitangent at offset 44");
static_assert(offsetof(Flux_SkinInputVertex, m_xColor)       == 56,  "skinned vertex color at offset 56");
static_assert(offsetof(Flux_SkinInputVertex, m_auBoneIDs)    == 72,  "skinned vertex boneIndices at offset 72");
static_assert(offsetof(Flux_SkinInputVertex, m_xBoneWeights) == 88,  "skinned vertex boneWeights at offset 88");

// Static OUTPUT (72B): pos(12)+uv(8)+normal(12)+tangent(12)+bitangent(12)+color(16).
struct Flux_SkinOutputVertex
{
	Zenith_Maths::Vector3 m_xPosition;     // offset  0
	Zenith_Maths::Vector2 m_xUV;           // offset 12
	Zenith_Maths::Vector3 m_xNormal;       // offset 20
	Zenith_Maths::Vector3 m_xTangent;      // offset 32
	Zenith_Maths::Vector3 m_xBitangent;    // offset 44
	Zenith_Maths::Vector4 m_xColor;        // offset 56
};
static_assert(sizeof(Flux_SkinOutputVertex) == 72, "Flux_SkinOutputVertex must be 72 bytes (static vertex stride)");
static_assert(offsetof(Flux_SkinOutputVertex, m_xUV)        == 12, "static vertex UV at offset 12");
static_assert(offsetof(Flux_SkinOutputVertex, m_xNormal)    == 20, "static vertex normal at offset 20");
static_assert(offsetof(Flux_SkinOutputVertex, m_xTangent)   == 32, "static vertex tangent at offset 32");
static_assert(offsetof(Flux_SkinOutputVertex, m_xBitangent) == 44, "static vertex bitangent at offset 44");
static_assert(offsetof(Flux_SkinOutputVertex, m_xColor)     == 56, "static vertex color at offset 56");

// ---- skin-job descriptor (gather builds these; the compute pass consumes them) ----
// Byte-mirror of SkinJob in Common/SceneObjects.slang (16B, all-uint — no vec3 trap).
// One per animated submesh-instance.
struct Flux_GPUSkinJob
{
	u_int m_uBindPoseVertBase = 0u;  // base vertex of this job's mesh in the shared bind-pose pool
	u_int m_uOutVertBase      = 0u;  // base vertex of this job's slice in the skinned arena
	u_int m_uVertCount        = 0u;  // vertex count
	u_int m_uBonePaletteBase  = 0u;  // base matrix of this job's skeleton in the bone palette
};
static_assert(sizeof(Flux_GPUSkinJob) == 16, "Flux_GPUSkinJob must be 16 bytes (mirrors SkinJob in Common/SceneObjects.slang)");
static_assert(offsetof(Flux_GPUSkinJob, m_uOutVertBase)     == 4,  "SkinJob outVertBase at offset 4");
static_assert(offsetof(Flux_GPUSkinJob, m_uVertCount)       == 8,  "SkinJob vertCount at offset 8");
static_assert(offsetof(Flux_GPUSkinJob, m_uBonePaletteBase) == 12, "SkinJob bonePaletteBase at offset 12");

// ---- the skinning kernel (pure CPU mirror of Flux_UnifiedMesh_Skinning.slang) --
// Skins one vertex to OBJECT/LOCAL space: accumulate (boneMatrix * vtx) * weight over
// up to 4 influences. boneMatrix = pxBonePalette[uBonePaletteBase + boneId], where the
// palette holds every live skeleton's skinning matrices (modelSpace * inverseBindPose)
// concatenated and uBonePaletteBase is this instance's block base. Mirrors the legacy
// accumulation EXACTLY (no normalisation — the unified VS re-normalises the result):
//   - 0xFFFFFFFF sentinel terminates the influence list early.
//   - a defensive bounds-guard skips an influence whose palette index is out of range
//     (corrupt data only; mirrors the cull shader's numObjects guard). UV / colour are
//     passthrough (skinning does not touch them).
inline Flux_SkinOutputVertex Flux_SkinVertexCPU(const Flux_SkinInputVertex& xIn,
	const Zenith_Maths::Matrix4* pxBonePalette, u_int uBonePaletteBase, u_int uPaletteCount)
{
	Flux_SkinOutputVertex xOut;
	xOut.m_xUV    = xIn.m_xUV;
	xOut.m_xColor = xIn.m_xColor;

	Zenith_Maths::Vector4 xFinalPosition(0.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xFinalNormal(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xFinalTangent(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xFinalBitangent(0.0f, 0.0f, 0.0f);

	for (u_int u = 0; u < 4u; ++u)
	{
		const u_int uBoneId = xIn.m_auBoneIDs[u];
		if (uBoneId == uFLUX_SKIN_BONE_SENTINEL)
		{
			break;   // no more influences for this vertex
		}
		const u_int uPaletteIndex = uBonePaletteBase + uBoneId;
		if (uPaletteIndex >= uPaletteCount)
		{
			continue;   // defensive: skip an out-of-range influence (corrupt data)
		}

		const Zenith_Maths::Matrix4& xBone = pxBonePalette[uPaletteIndex];
		const Zenith_Maths::Matrix3 xBone3(xBone);
		const float fWeight = xIn.m_xBoneWeights[u];

		xFinalPosition  += (xBone * Zenith_Maths::Vector4(xIn.m_xPosition, 1.0f)) * fWeight;
		xFinalNormal    += (xBone3 * xIn.m_xNormal)    * fWeight;
		xFinalTangent   += (xBone3 * xIn.m_xTangent)   * fWeight;
		xFinalBitangent += (xBone3 * xIn.m_xBitangent) * fWeight;
	}

	xOut.m_xPosition  = Zenith_Maths::Vector3(xFinalPosition.x, xFinalPosition.y, xFinalPosition.z);
	xOut.m_xNormal    = xFinalNormal;
	xOut.m_xTangent   = xFinalTangent;
	xOut.m_xBitangent = xFinalBitangent;
	return xOut;
}

// ---- raw-word skinning (the compute shader's exact memory access) -----------
// Per-vertex word strides: the bind-pose input is the tightly-packed 104-byte skinned
// vertex (26 u32 words); the output is the tightly-packed 72-byte static vertex (18 words).
inline constexpr u_int uFLUX_SKIN_INPUT_WORDS  = 26u;  // 104 / 4
inline constexpr u_int uFLUX_SKIN_OUTPUT_WORDS = 18u;  //  72 / 4

namespace Flux_SkinDetail
{
	inline float WordToFloat(u_int uWord) { float f; memcpy(&f, &uWord, sizeof(f)); return f; }
	inline u_int FloatToWord(float fVal)  { u_int w; memcpy(&w, &fVal, sizeof(w)); return w; }
}

// Skins one vertex straight out of / into FLAT WORD ARRAYS, byte-identical to how the
// compute shader (Flux_UnifiedMesh_Skinning.slang) must access the buffers. The shader
// reads the packed VB / writes the packed arena as raw word buffers ON PURPOSE: a Slang
// StructuredBuffer<float3> 16-byte-aligns each vec3 (std430), which would NOT match the
// 12-byte-packed vertex layout — so both sides index flat words with these offsets.
// This indirection is what the headless test pins (offsets vs the C++ struct layout); the
// skinning MATH is already pinned by Flux_SkinVertexCPU, which this delegates to.
//   input word offsets  (104B): pos@0 uv@3 normal@5 tangent@8 bitangent@11 color@14 boneIDs@18 weights@22
//   output word offsets ( 72B): pos@0 uv@3 normal@5 tangent@8 bitangent@11 color@14
inline void Flux_SkinVertexRaw(
	const u_int* pInputWords, u_int uInVertexIndex,
	const Zenith_Maths::Matrix4* pxBonePalette, u_int uBonePaletteBase, u_int uPaletteCount,
	u_int* pOutputWords, u_int uOutVertexIndex)
{
	using namespace Flux_SkinDetail;
	const u_int* p = pInputWords + (size_t)uInVertexIndex * uFLUX_SKIN_INPUT_WORDS;

	Flux_SkinInputVertex xIn;
	xIn.m_xPosition  = Zenith_Maths::Vector3(WordToFloat(p[0]),  WordToFloat(p[1]),  WordToFloat(p[2]));
	xIn.m_xUV        = Zenith_Maths::Vector2(WordToFloat(p[3]),  WordToFloat(p[4]));
	xIn.m_xNormal    = Zenith_Maths::Vector3(WordToFloat(p[5]),  WordToFloat(p[6]),  WordToFloat(p[7]));
	xIn.m_xTangent   = Zenith_Maths::Vector3(WordToFloat(p[8]),  WordToFloat(p[9]),  WordToFloat(p[10]));
	xIn.m_xBitangent = Zenith_Maths::Vector3(WordToFloat(p[11]), WordToFloat(p[12]), WordToFloat(p[13]));
	xIn.m_xColor     = Zenith_Maths::Vector4(WordToFloat(p[14]), WordToFloat(p[15]), WordToFloat(p[16]), WordToFloat(p[17]));
	xIn.m_auBoneIDs[0] = p[18]; xIn.m_auBoneIDs[1] = p[19]; xIn.m_auBoneIDs[2] = p[20]; xIn.m_auBoneIDs[3] = p[21];
	xIn.m_xBoneWeights = Zenith_Maths::Vector4(WordToFloat(p[22]), WordToFloat(p[23]), WordToFloat(p[24]), WordToFloat(p[25]));

	const Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, pxBonePalette, uBonePaletteBase, uPaletteCount);

	u_int* o = pOutputWords + (size_t)uOutVertexIndex * uFLUX_SKIN_OUTPUT_WORDS;
	o[0]  = FloatToWord(xOut.m_xPosition.x);  o[1]  = FloatToWord(xOut.m_xPosition.y);  o[2]  = FloatToWord(xOut.m_xPosition.z);
	o[3]  = FloatToWord(xOut.m_xUV.x);        o[4]  = FloatToWord(xOut.m_xUV.y);
	o[5]  = FloatToWord(xOut.m_xNormal.x);    o[6]  = FloatToWord(xOut.m_xNormal.y);    o[7]  = FloatToWord(xOut.m_xNormal.z);
	o[8]  = FloatToWord(xOut.m_xTangent.x);   o[9]  = FloatToWord(xOut.m_xTangent.y);   o[10] = FloatToWord(xOut.m_xTangent.z);
	o[11] = FloatToWord(xOut.m_xBitangent.x); o[12] = FloatToWord(xOut.m_xBitangent.y); o[13] = FloatToWord(xOut.m_xBitangent.z);
	o[14] = FloatToWord(xOut.m_xColor.x);     o[15] = FloatToWord(xOut.m_xColor.y);     o[16] = FloatToWord(xOut.m_xColor.z); o[17] = FloatToWord(xOut.m_xColor.w);
}

// ---- previous-pose positions-only raw skinning (TAA velocity motion vectors) --
// The velocity path runs a SECOND skinning dispatch (Flux_UnifiedMesh_SkinningPrev.slang)
// that reuses the current frame's skin-jobs UNCHANGED (same bind-pose bases, out bases and
// per-instance palette base) but with the PREVIOUS-frame bone palette, and writes ONLY the
// skinned OBJECT-space POSITION into a compact prev arena (3 words / vertex). The velocity
// VS reads it as the vertex's prior-frame local position to encode uvPrev. Positions-only
// because a motion vector needs only the reprojected position, so the prev arena is 1/6 the
// size of the full 18-word arena.
//
// Pure CPU mirror of that shader: it delegates the bone-weighted accumulation to
// Flux_SkinVertexCPU (the SAME pinned math the full skinner uses) and writes just the
// position as 3 raw words at (uOutVertexIndex * uFLUX_SKIN_PREV_WORDS). The velocity VS
// addresses the GLOBAL arena index = (this skinned draw's uOutVertBase + SV_VertexID),
// because the indirect draw's vertexOffset is 0 and the arena slice base is applied via the
// vertex-buffer bind offset — so SV_VertexID is the submesh-LOCAL index (pinned by the
// stride test below, and empirically by Flux_UnifiedMesh_Reset.slang writing vertexOffset=0).
inline constexpr u_int uFLUX_SKIN_PREV_WORDS = 3u;   // positions only (12B / 3 words per vertex)

inline void Flux_SkinPrevPositionRaw(
	const u_int* pInputWords, u_int uInVertexIndex,
	const Zenith_Maths::Matrix4* pxPrevBonePalette, u_int uBonePaletteBase, u_int uPaletteCount,
	u_int* pPrevArenaWords, u_int uOutVertexIndex)
{
	using namespace Flux_SkinDetail;
	const u_int* p = pInputWords + (size_t)uInVertexIndex * uFLUX_SKIN_INPUT_WORDS;

	Flux_SkinInputVertex xIn;
	xIn.m_xPosition  = Zenith_Maths::Vector3(WordToFloat(p[0]),  WordToFloat(p[1]),  WordToFloat(p[2]));
	xIn.m_xUV        = Zenith_Maths::Vector2(WordToFloat(p[3]),  WordToFloat(p[4]));
	xIn.m_xNormal    = Zenith_Maths::Vector3(WordToFloat(p[5]),  WordToFloat(p[6]),  WordToFloat(p[7]));
	xIn.m_xTangent   = Zenith_Maths::Vector3(WordToFloat(p[8]),  WordToFloat(p[9]),  WordToFloat(p[10]));
	xIn.m_xBitangent = Zenith_Maths::Vector3(WordToFloat(p[11]), WordToFloat(p[12]), WordToFloat(p[13]));
	xIn.m_xColor     = Zenith_Maths::Vector4(WordToFloat(p[14]), WordToFloat(p[15]), WordToFloat(p[16]), WordToFloat(p[17]));
	xIn.m_auBoneIDs[0] = p[18]; xIn.m_auBoneIDs[1] = p[19]; xIn.m_auBoneIDs[2] = p[20]; xIn.m_auBoneIDs[3] = p[21];
	xIn.m_xBoneWeights = Zenith_Maths::Vector4(WordToFloat(p[22]), WordToFloat(p[23]), WordToFloat(p[24]), WordToFloat(p[25]));

	// Same accumulation as the full skinner — only the position is written to the prev arena.
	const Flux_SkinOutputVertex xOut = Flux_SkinVertexCPU(xIn, pxPrevBonePalette, uBonePaletteBase, uPaletteCount);

	u_int* o = pPrevArenaWords + (size_t)uOutVertexIndex * uFLUX_SKIN_PREV_WORDS;
	o[0] = FloatToWord(xOut.m_xPosition.x);
	o[1] = FloatToWord(xOut.m_xPosition.y);
	o[2] = FloatToWord(xOut.m_xPosition.z);
}

// ---- cull-bounds inflation for skinned submeshes ----------------------------
// Scale a (center.xyz, radius.w) bounding sphere's radius by fFactor, leaving the
// centre put. The unified cull transforms centre by the model matrix and scales the
// radius by the matrix's max axis scale, so inflating the LOCAL radius here keeps the
// extra margin under any object transform.
inline Zenith_Maths::Vector4 Flux_InflateBoundsSphere(const Zenith_Maths::Vector4& xSphere, float fFactor)
{
	return Zenith_Maths::Vector4(xSphere.x, xSphere.y, xSphere.z, xSphere.w * fFactor);
}

// ---- bone-palette builder (gather-side; dedups skeletons into one SSBO) ------
// The skinned draw path packs EVERY live skeleton's skinning matrices into one shared
// bone-palette SSBO, each skeleton occupying a fixed uBonesPerSkeleton (MAX_BONES) block.
// Multiple submesh-instances of the same skeleton share ONE block (dedup by an opaque
// per-frame skeleton id — the Flux_SkeletonInstance pointer in practice). GetOrAddSkeleton
// returns the block's base MATRIX index, which the gather stores in the instance's
// Flux_GPUSceneObject.m_uBonePaletteRef and its skin-job. On first sight of a skeleton
// (bNewlyAdded) the caller fills [base, base+numBones) of Matrices() from the skeleton's
// GetSkinningMatrices(); unused tail matrices in the block stay identity. Pure CPU (no GPU);
// skeleton count per frame is small so a linear dedup scan is fine. Begin() reuses storage.
class Flux_BonePaletteBuilder
{
public:
	void Begin(u_int uBonesPerSkeleton)
	{
		m_uBonesPerSkeleton = uBonesPerSkeleton;
		m_auSeenIds.Clear();
		m_auSeenBases.Clear();
		m_xMatrices.Clear();
	}

	// Returns the base matrix index of this skeleton's block; appends an identity-filled
	// block (bNewlyAdded=true) the first time the id is seen, else returns the existing base.
	u_int GetOrAddSkeleton(u_int64 ulSkeletonId, bool& bNewlyAdded)
	{
		for (u_int u = 0; u < m_auSeenIds.GetSize(); ++u)
		{
			if (m_auSeenIds.Get(u) == ulSkeletonId)
			{
				bNewlyAdded = false;
				return m_auSeenBases.Get(u);
			}
		}
		const u_int uBase = m_xMatrices.GetSize();
		m_auSeenIds.PushBack(ulSkeletonId);
		m_auSeenBases.PushBack(uBase);
		for (u_int u = 0; u < m_uBonesPerSkeleton; ++u)
		{
			m_xMatrices.PushBack(Zenith_Maths::Matrix4(1.0f));   // identity; caller overwrites the live bones
		}
		bNewlyAdded = true;
		return uBase;
	}

	Zenith_Vector<Zenith_Maths::Matrix4>&       Matrices()       { return m_xMatrices; }
	const Zenith_Vector<Zenith_Maths::Matrix4>& Matrices() const { return m_xMatrices; }
	u_int GetSkeletonCount()    const { return m_auSeenIds.GetSize(); }
	u_int GetMatrixCount()      const { return m_xMatrices.GetSize(); }
	u_int GetBonesPerSkeleton() const { return m_uBonesPerSkeleton; }

private:
	u_int m_uBonesPerSkeleton = 0u;
	Zenith_Vector<u_int64> m_auSeenIds;    // distinct skeleton ids this frame
	Zenith_Vector<u_int>   m_auSeenBases;  // parallel: each id's base matrix index
	Zenith_Vector<Zenith_Maths::Matrix4> m_xMatrices;  // concatenated MAX_BONES blocks
};

// ---- previous-frame bone-palette history (skinned motion vectors) -----------
// The velocity path runs a SECOND, positions-only skinning dispatch using the PREVIOUS
// frame's bone palette, so a skinned vertex's prior-frame world position is available to
// encode uvPrev. That dispatch reuses the CURRENT frame's skin-jobs UNCHANGED (same
// Flux_GPUSkinJob.m_uBonePaletteBase), so the prev palette must be laid out at the SAME
// per-skeleton bases as the current palette. This pure store rebuilds that prev palette
// each frame: for every skeleton block the current Flux_BonePaletteBuilder placed at base B
// with matrices M, SubmitSkeleton emits the SAME skeleton's matrices from LAST frame at base
// B — or M itself the first frame a skeleton is seen (prev == current => zero surface
// velocity, absorbed by the resolve clamp). Keyed by the OPAQUE skeleton id (the
// Flux_SkeletonInstance pointer bits), NOT the base, so a skeleton that moves to a different
// block (an eviction re-pack) still reprojects against its own history.
//
// Pure CPU (no GPU); headless-testable (tests in Flux_Skinning.Tests.inl).
class Flux_BonePaletteHistory
{
public:
	// Start assembling this frame's prev palette. uBonesPerSkeleton must match the current
	// Flux_BonePaletteBuilder's block size (so bases line up).
	void BeginFrame(u_int uBonesPerSkeleton)
	{
		m_uBonesPerSkeleton = uBonesPerSkeleton;
		m_xPrevPalette.Clear();
		m_xThisFrameById.Clear();
	}

	// Emit skeleton ulSkeletonId (placed at matrix base uBase in the CURRENT palette, with
	// current matrices pxCurrent[0..uNumBones)) into the prev palette. Writes LAST frame's
	// matrices for this id into [uBase, uBase+uNumBones); a missing history copies pxCurrent
	// (prev == current on first sight). The block tail [uBase+uNumBones, uBase+bonesPerSkeleton)
	// stays identity, matching Flux_BonePaletteBuilder. Also records pxCurrent as next frame's
	// history for this id.
	void SubmitSkeleton(u_int64 ulSkeletonId, u_int uBase,
		const Zenith_Maths::Matrix4* pxCurrent, u_int uNumBones)
	{
		const u_int uBlockEnd = uBase + m_uBonesPerSkeleton;
		if (m_xPrevPalette.GetSize() < uBlockEnd)
		{
			m_xPrevPalette.Resize(uBlockEnd, Zenith_Maths::Matrix4(1.0f));   // identity-fill the new tail
		}

		const Zenith_Vector<Zenith_Maths::Matrix4>* pxHistory = m_xPrevById.TryGet(ulSkeletonId);
		const bool bHaveHistory = (pxHistory != nullptr) && (pxHistory->GetSize() >= uNumBones);
		for (u_int u = 0; u < uNumBones; ++u)
		{
			m_xPrevPalette.Get(uBase + u) = bHaveHistory ? pxHistory->Get(u) : pxCurrent[u];
		}

		// Record this frame's matrices as next frame's history for this id.
		Zenith_Vector<Zenith_Maths::Matrix4> xCur;
		for (u_int u = 0; u < uNumBones; ++u) { xCur.PushBack(pxCurrent[u]); }
		m_xThisFrameById.Insert(ulSkeletonId, std::move(xCur));
	}

	// The assembled previous-frame palette, laid out identically to the current palette.
	const Zenith_Vector<Zenith_Maths::Matrix4>& PrevPalette() const { return m_xPrevPalette; }

	// End of frame: this frame's recorded matrices become next frame's history; skeletons not
	// seen this frame drop out (their next appearance starts fresh at prev == current).
	void EndFrame()
	{
		m_xPrevById = std::move(m_xThisFrameById);
		m_xThisFrameById.Clear();
	}

	u_int GetHistoryCount() const { return m_xPrevById.GetSize(); }

private:
	u_int m_uBonesPerSkeleton = 0u;
	Zenith_Vector<Zenith_Maths::Matrix4>                          m_xPrevPalette;    // assembled this frame (same bases as current)
	Zenith_HashMap<u_int64, Zenith_Vector<Zenith_Maths::Matrix4>> m_xPrevById;       // last frame's matrices per skeleton id
	Zenith_HashMap<u_int64, Zenith_Vector<Zenith_Maths::Matrix4>> m_xThisFrameById;  // this frame's matrices per skeleton id
};

// ---- golden hash over a skinned-vertex run (characterisation tests) ---------
// FNV-1a byte feed. Deterministic for a fixed input (the skinning math is pure FP),
// so two runs match; not a stable cross-compiler constant (do NOT hardcode it).
inline u_int64 Flux_HashSkinnedForTest(const Flux_SkinOutputVertex* pxVerts, u_int uCount)
{
	u_int64 uHash = 0xcbf29ce484222325ull;
	auto Bytes = [&uHash](const void* p, size_t n)
	{
		const u_int8* pb = static_cast<const u_int8*>(p);
		for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
	};
	Bytes(&uCount, sizeof(uCount));
	for (u_int u = 0; u < uCount; ++u)
	{
		Bytes(&pxVerts[u], sizeof(Flux_SkinOutputVertex));
	}
	return uHash;
}

// ---- bind-pose pool upload gating (persistent pool) -------------------------
// The bind-pose pool holds STATIC vertices. It changes only when a new distinct skinned
// mesh appears (grow) or an eviction re-packs it (shrink + rebase) — both bump the
// renderer's pool GENERATION counter. Its GPU copy is frame-indexed (MAX_FRAMES_IN_FLIGHT
// physical buffers), so after ANY change we must re-upload for MAX_FRAMES_IN_FLIGHT
// consecutive frames (refreshing every physical copy) and then skip uploads in steady
// state. This pure state machine drives that decision; the renderer holds uUploadedGen +
// uDirtyFrames across frames. Returns true iff the caller should upload the whole pool.
// (Generation, not word count, so a re-pack that SHRINKS the pool still re-uploads.)
inline bool Flux_BindPosePoolShouldUpload(u_int uPoolGeneration, u_int uMaxFramesInFlight,
	u_int& uUploadedGeneration, u_int& uDirtyFrames)
{
	if (uPoolGeneration != uUploadedGeneration)
	{
		uDirtyFrames        = uMaxFramesInFlight;   // re-fill every frame-indexed physical copy
		uUploadedGeneration = uPoolGeneration;
	}
	if (uDirtyFrames > 0u)
	{
		--uDirtyFrames;
		return true;
	}
	return false;
}

// ============================================================================
// Stable skinned-instance id allocator (optimization (iii)).
//
// Each skinned submesh-INSTANCE is skinned to its own arena slice, so each is its own
// indirect draw needing a per-instance bucket. The bucket key's meshGeometryId used to
// be `uFLUX_GPUSCENE_SKINNED_MESH_BIT | <per-frame iteration index>` — UNSTABLE frame to
// frame, which made the bucket registry's topology/refcount-diff signal meaningless for
// skinned buckets (and blocked Stage-6 batching). This allocator hands out a STABLE small
// id per distinct (skeletonInstance, meshAsset, submeshSlot) identity, reused across
// frames and recycled when an instance stops being drawn — the same refcount-diff shape
// as Flux_MeshGeometryRegistry (BeginSync -> Reference -> EndSync), id-only (no GPU).
//
// The id seeds the skinned bucket key (the caller ORs in uFLUX_GPUSCENE_SKINNED_MESH_BIT),
// so it must stay below that 0x80000000 high bit — asserted at allocation.
// ============================================================================
struct Flux_SkinnedInstanceKey
{
	const void* m_pvSkeleton   = nullptr;  // Flux_SkeletonInstance* (per-entity, stable across frames)
	const void* m_pvMeshAsset  = nullptr;  // Zenith_MeshAsset* (the skinned submesh source)
	u_int       m_uSubmeshSlot = 0u;       // submesh index within the model (disambiguates same-skeleton submeshes)

	bool operator==(const Flux_SkinnedInstanceKey& xOther) const
	{
		return m_pvSkeleton == xOther.m_pvSkeleton
		    && m_pvMeshAsset == xOther.m_pvMeshAsset
		    && m_uSubmeshSlot == xOther.m_uSubmeshSlot;
	}
};

// FNV-1a over the two identity pointers + the submesh slot.
template<>
struct Zenith_Hash<Flux_SkinnedInstanceKey>
{
	u_int64 operator()(const Flux_SkinnedInstanceKey& xKey) const noexcept
	{
		u_int64 uHash = 0xcbf29ce484222325ull;
		auto Bytes = [&uHash](const void* p, size_t n)
		{
			const u_int8* pb = static_cast<const u_int8*>(p);
			for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
		};
		Bytes(&xKey.m_pvSkeleton,   sizeof(xKey.m_pvSkeleton));
		Bytes(&xKey.m_pvMeshAsset,  sizeof(xKey.m_pvMeshAsset));
		Bytes(&xKey.m_uSubmeshSlot, sizeof(xKey.m_uSubmeshSlot));
		return uHash;
	}
};

// Collision-FREE stable id allocator (a raw hash could merge two distinct poses into one
// bucket -> visible corruption). One hashmap lookup per skinned instance per frame. Thin
// instantiation of Flux_RefcountDiffRegistry<Key> (no payload, id-only): the sync + recycle
// machinery is the base's; this class only enforces the 31-bit id ceiling.
class Flux_SkinnedInstanceIdRegistry : public Flux_RefcountDiffRegistry<Flux_SkinnedInstanceKey>
{
public:
	// STABLE id for this identity (allocates on first sight). NOT OR-ed with the skinned-mesh
	// bit — that is the caller's job. The id stays < 0x80000000 (asserted). BeginSync / EndSync /
	// GetLiveCount / TryGetId are inherited.
	u_int Reference(const Flux_SkinnedInstanceKey& xKey)
	{
		const u_int uSlot = Flux_RefcountDiffRegistry<Flux_SkinnedInstanceKey>::Reference(xKey);
		Zenith_Assert(uSlot < 0x80000000u, "skinned-instance id overflowed the 31-bit space (high bit reserved for the skinned-mesh tag)");
		return uSlot;
	}
};

// ============================================================================
// Stable skinned-POSE store — folded onto the refcount-diff base.
//
// One cached bind-pose entry per DISTINCT skinned mesh asset: the 104B interleaved
// bind-pose vertices (compute-skinning input, uFLUX_SKIN_INPUT_WORDS/vert) + the shared
// mesh instance (IB/counts). Lifetime is the refcount-diff sync — Reference(asset) on each
// drawn skinned submesh; an entry not referenced a whole sync is evicted (its GPU IB/VB
// deferred-freed by the provider) and the persistent bind-pose POOL re-packed. This registry
// OWNS that pool: each entry's words are concatenated once on create (m_uPoolVertBase
// recorded) and the pool is re-packed wholesale when anything retires (the cold path: a
// character/scene unload). The pool generation bumps on grow/repack -> the gather re-uploads.
//
// EVICTION ORDERING (asymmetric, on purpose): the evict + repack must finish BEFORE the
// per-frame walk builds skin-jobs (they capture pool bases), so the registry is driven by
// BeginFrameEvictingPrevious() at frame start (evict the PRIOR sync's unreferenced poses,
// then open this sync) rather than an EndSync() at frame end.
// ============================================================================
class Flux_MeshInstance;   // entry holds a shared mesh instance by pointer (IB/counts)

struct Flux_SkinnedPoseEntry
{
	Zenith_Vector<u_int> m_auBindPoseWords;        // 104B-per-vertex interleaved (uFLUX_SKIN_INPUT_WORDS/vert)
	Flux_MeshInstance*   m_pxMesh        = nullptr; // CreateSkinnedFromAsset -> IB + index count + bounds
	const void*          m_pvSourceAsset = nullptr; // Zenith_MeshAsset* (the skinned submesh source)
	u_int                m_uNumVerts     = 0u;
	u_int                m_uPoolVertBase = 0u;       // base VERTEX in the persistent bind-pose pool (re-assigned on a repack)
};

struct Flux_SkinnedPoseKey
{
	const void* m_pvAsset = nullptr;   // Zenith_MeshAsset* (one cached pose per distinct skinned mesh)

	bool operator==(const Flux_SkinnedPoseKey& xOther) const { return m_pvAsset == xOther.m_pvAsset; }
};

// FNV-1a over the asset pointer bits.
template<>
struct Zenith_Hash<Flux_SkinnedPoseKey>
{
	u_int64 operator()(const Flux_SkinnedPoseKey& xKey) const noexcept
	{
		u_int64 uHash = 0xcbf29ce484222325ull;
		const u_int8* pb = reinterpret_cast<const u_int8*>(&xKey.m_pvAsset);
		for (size_t i = 0; i < sizeof(xKey.m_pvAsset); ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
		return uHash;
	}
};

class Flux_SkinnedPoseRegistry : public Flux_RefcountDiffRegistry<Flux_SkinnedPoseKey, Flux_SkinnedPoseEntry*>
{
public:
	// Reference the cached pose for a skinned mesh asset (builds on first sight via the provider,
	// appending its bind-pose words to the owned pool). Returns nullptr on build failure / null asset.
	Flux_SkinnedPoseEntry* Reference(const void* pvMeshAsset)
	{
		if (pvMeshAsset == nullptr) { return nullptr; }
		Flux_SkinnedPoseKey xKey;
		xKey.m_pvAsset = pvMeshAsset;
		bool bCreated = false;
		const u_int uSlot = Flux_RefcountDiffRegistry<Flux_SkinnedPoseKey, Flux_SkinnedPoseEntry*>::Reference(xKey, &bCreated);
		if (uSlot == uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT) { return nullptr; }
		Flux_SkinnedPoseEntry** ppEntry = TryGetPayloadMutable(uSlot);
		Flux_SkinnedPoseEntry* pxEntry = ppEntry ? *ppEntry : nullptr;
		if (bCreated && pxEntry != nullptr)
		{
			// First sight: concatenate this mesh's static bind-pose words into the persistent pool
			// (grow-only between repacks); the skin-job reads m_uPoolVertBase directly.
			pxEntry->m_uPoolVertBase = static_cast<u_int>(m_auBindPosePoolWords.GetSize() / uFLUX_SKIN_INPUT_WORDS);
			AppendWords(pxEntry->m_auBindPoseWords);
			++m_uPoolGeneration;   // pool grew -> GPU re-upload next gather
		}
		return pxEntry;
	}

	// Frame boundary. Evicts poses unreferenced LAST sync (freeing their GPU IB/VB via the provider
	// + re-packing the pool) and opens THIS sync. Asymmetric on purpose — see the class comment.
	void BeginFrameEvictingPrevious()
	{
		EndSync();     // retire prior-sync's unreferenced poses (refcounts still hold last sync) + repack
		BeginSync();   // reset refcounts for this sync
	}

	// EndSync = base retire (provider-destroys each unreferenced entry) + a pool re-pack iff anything
	// retired (survivors' bases shift -> bump the generation so the gather re-uploads).
	void EndSync()
	{
		Flux_RefcountDiffRegistry<Flux_SkinnedPoseKey, Flux_SkinnedPoseEntry*>::EndSync();
		if (WasAnyRetiredThisSync())
		{
			RepackPool();
		}
	}

	// Free EVERY cached pose (GPU IB/VB + heap entry) — renderer shutdown, device still up.
	void Shutdown()
	{
		BeginSync();   // mark all unreferenced
		EndSync();     // -> provider-destroys them all + repacks the pool to empty
	}

	const Zenith_Vector<u_int>& GetPoolWords()      const { return m_auBindPosePoolWords; }
	u_int                       GetPoolGeneration() const { return m_uPoolGeneration; }

private:
	void AppendWords(const Zenith_Vector<u_int>& auWords)
	{
		for (u_int w = 0; w < auWords.GetSize(); ++w) { m_auBindPosePoolWords.PushBack(auWords.Get(w)); }
	}

	// Wholesale re-pack from the live entries (cold path). Re-assigns each survivor's
	// m_uPoolVertBase and bumps the generation so the gather re-uploads.
	void RepackPool()
	{
		m_auBindPosePoolWords.Clear();
		for (u_int u = 0; u < GetSlotCount(); ++u)
		{
			Flux_SkinnedPoseEntry** ppEntry = TryGetPayloadMutable(u);
			Flux_SkinnedPoseEntry* pxEntry = ppEntry ? *ppEntry : nullptr;
			if (pxEntry == nullptr) { continue; }
			pxEntry->m_uPoolVertBase = static_cast<u_int>(m_auBindPosePoolWords.GetSize() / uFLUX_SKIN_INPUT_WORDS);
			AppendWords(pxEntry->m_auBindPoseWords);
		}
		++m_uPoolGeneration;
	}

	Zenith_Vector<u_int> m_auBindPosePoolWords;   // persistent grow-only bind-pose pool
	u_int                m_uPoolGeneration = 0u;  // bumped on append / repack -> gates the GPU re-upload
};

// Production provider: build = Flux_MeshInstance::CreateSkinnedFromAsset + bind-pose interleave,
// destroy = Flux_MeshInstance::Destroy + delete. GPU-touching -> wired by the renderer
// (LateInitialise) and exercised windowed, never headless. Defined in Flux.cpp.
Flux_SkinnedPoseRegistry::Provider Flux_MakeRealSkinnedPoseProvider();
