#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include <cstddef>   // offsetof, size_t

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
