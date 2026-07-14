#include "Zenith.h"

// ============================================================================
// ZM_HumanGen -- the S4 human generator driver. See the header for the
// architecture + determinism contract. This TU owns: human -> recipe resolution,
// the per-domain seed derivation, the shared 16-bone skeleton emit, the minimal
// SC1 body loft, the placeholder albedo, the full-bundle driver, the byte-
// identity + hash + validation machinery, the golden clip metadata, the two
// asset-path schemes, and (tools only, SC5) the disk bake stubs.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanGen.h"

#include <cstdio>    // snprintf
#include <cstring>   // memcmp, memcpy, strlen

namespace
{
	// FNV-1a constants (byte-identical to ZM_GenHashName / the terrain seed hash).
	constexpr u_int uZM_FNV_OFFSET = 2166136261u;
	constexpr u_int uZM_FNV_PRIME  = 16777619u;

	// Fold a raw byte range into a running FNV-1a hash.
	u_int ZM_FnvAccum(u_int uHash, const void* pData, size_t uBytes)
	{
		const u_int8* pByte = static_cast<const u_int8*>(pData);
		for (size_t i = 0; i < uBytes; ++i)
		{
			uHash ^= pByte[i];
			uHash *= uZM_FNV_PRIME;
		}
		return uHash;
	}

	// Fold a whole SoA buffer's bytes into a running FNV hash.
	template <typename T>
	u_int ZM_FnvAccumBuffer(u_int uHash, const Zenith_Vector<T>& xVec)
	{
		if (xVec.GetSize() == 0u) { return uHash; }
		return ZM_FnvAccum(uHash, xVec.GetDataPointer(),
			static_cast<size_t>(xVec.GetSize()) * sizeof(T));
	}

	// Byte-exact compare of one SoA buffer pair (sizes first, then memcmp).
	template <typename T>
	bool ZM_BufferBytesEqual(const Zenith_Vector<T>& xA, const Zenith_Vector<T>& xB)
	{
		if (xA.GetSize() != xB.GetSize()) { return false; }
		if (xA.GetSize() == 0u)           { return true;  }
		return memcmp(xA.GetDataPointer(), xB.GetDataPointer(),
			static_cast<size_t>(xA.GetSize()) * sizeof(T)) == 0;
	}

	// The shared-skeleton bone indices, in ZM_AppendSharedHumanBones emit order.
	// The mesh loft binds rings to these; the order is the frozen shared contract.
	enum
	{
		HB_ROOT = 0, HB_SPINE, HB_NECK, HB_HEAD,
		HB_LUARM, HB_LLARM, HB_LHAND,
		HB_RUARM, HB_RLARM, HB_RHAND,
		HB_LULEG, HB_LLLEG, HB_LFOOT,
		HB_RULEG, HB_RLLEG, HB_RFOOT
	};

	// Per-build height factor -- applied about the grounded feet (y=0), so scaling
	// height never lifts the feet off the floor. Modest spread (0.92..1.15).
	float ZM_HumanBuildHeightScale(ZM_HUMAN_BUILD eBuild)
	{
		switch (eBuild)
		{
		case ZM_HUMAN_BUILD_SLIGHT:  return 0.95f;
		case ZM_HUMAN_BUILD_AVERAGE: return 1.00f;
		case ZM_HUMAN_BUILD_STOCKY:  return 0.92f;
		case ZM_HUMAN_BUILD_TALL:    return 1.15f;
		default:                     return 1.00f;
		}
	}

	// Per-build width factor -- scales the loft ring extents + limb offsets.
	float ZM_HumanBuildWidthScale(ZM_HUMAN_BUILD eBuild)
	{
		switch (eBuild)
		{
		case ZM_HUMAN_BUILD_SLIGHT:  return 0.85f;
		case ZM_HUMAN_BUILD_AVERAGE: return 1.00f;
		case ZM_HUMAN_BUILD_STOCKY:  return 1.25f;
		case ZM_HUMAN_BUILD_TALL:    return 1.00f;
		default:                     return 1.00f;
		}
	}

	// Skin-tone -> flat linear colour (SC1 placeholder albedo). SC3 replaces this
	// with the real synthesised body texture.
	Zenith_Maths::Vector3 ZM_HumanSkinColour(ZM_HUMAN_SKIN_TONE eTone)
	{
		switch (eTone)
		{
		case ZM_HUMAN_SKIN_PALE:  return Zenith_Maths::Vector3(0.94f, 0.82f, 0.74f);
		case ZM_HUMAN_SKIN_FAIR:  return Zenith_Maths::Vector3(0.88f, 0.72f, 0.60f);
		case ZM_HUMAN_SKIN_TAN:   return Zenith_Maths::Vector3(0.76f, 0.58f, 0.44f);
		case ZM_HUMAN_SKIN_BROWN: return Zenith_Maths::Vector3(0.54f, 0.38f, 0.28f);
		case ZM_HUMAN_SKIN_DARK:  return Zenith_Maths::Vector3(0.34f, 0.24f, 0.18f);
		default:                  return Zenith_Maths::Vector3(0.80f, 0.66f, 0.55f);
		}
	}

	// Sweep one authored ring table into a closed (both ends capped) tube. Segment
	// count + light subdivision are fixed; caps are proven-outward by the shipped
	// loft (ZM_Gen/Loft_WindingOutward), which uses this exact AppendPart+cap path.
	void ZM_AppendHumanTube(ZM_GenMesh& xMesh, const ZM_LoftRing* pxRings, u_int uNumRings)
	{
		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = pxRings;
		xPart.m_uNumRings = uNumRings;
		xPart.m_uSegs     = 8u;
		xPart.m_bCapStart = true;
		xPart.m_bCapEnd   = true;
		xPart.m_uSubdiv   = 2u;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}

	// A 3-ring descending limb (top -> mid -> end), 2-bone blended across the mid
	// joint. All coordinates are pre-scaled by the caller (width/height factors).
	void ZM_AppendHumanLimb(ZM_GenMesh& xMesh, float fX, float fYTop, float fYBot,
		float fRTop, float fRBot, u_int uBoneUp, u_int uBoneLo, u_int uBoneEnd)
	{
		const float fYMid = 0.5f * (fYTop + fYBot);
		const float fRMid = 0.5f * (fRTop + fRBot);

		ZM_LoftRing axRings[3];
		axRings[0] = ZM_LoftRing{};
		axRings[0].m_fCx = fX; axRings[0].m_fY = fYTop; axRings[0].m_fRx = fRTop; axRings[0].m_fRz = fRTop;
		axRings[0].m_uBoneA = uBoneUp;  axRings[0].m_uBoneB = uBoneUp;  axRings[0].m_fBlendB = 0.0f;

		axRings[1] = ZM_LoftRing{};
		axRings[1].m_fCx = fX; axRings[1].m_fY = fYMid; axRings[1].m_fRx = fRMid; axRings[1].m_fRz = fRMid;
		axRings[1].m_uBoneA = uBoneUp;  axRings[1].m_uBoneB = uBoneLo;  axRings[1].m_fBlendB = 0.5f;

		axRings[2] = ZM_LoftRing{};
		axRings[2].m_fCx = fX; axRings[2].m_fY = fYBot; axRings[2].m_fRx = fRBot; axRings[2].m_fRz = fRBot;
		axRings[2].m_uBoneA = uBoneEnd; axRings[2].m_uBoneB = uBoneEnd; axRings[2].m_fBlendB = 0.0f;

		ZM_AppendHumanTube(xMesh, axRings, 3u);
	}

	// The SC1 placeholder body texture: a flat skin-tone fill.
	ZM_GenImage ZM_BuildHumanAlbedo(const ZM_HumanRecipe& xRecipe)
	{
		ZM_GenImage xImg(uZM_HUMAN_ALBEDO_RESOLUTION, uZM_HUMAN_ALBEDO_RESOLUTION);
		ZM_SynthFillSolid(xImg, ZM_HumanSkinColour(xRecipe.m_eSkinTone));
		return xImg;
	}

	// Per-kind per-model file basename pattern (embeds the human name).
	const char* ZM_HumanBasenameFmt(ZM_HUMAN_ASSET_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_HUMAN_ASSET_MESH:     return "%s.zmesh";
		case ZM_HUMAN_ASSET_ALBEDO:   return "%s_albedo.ztxtr";
		case ZM_HUMAN_ASSET_MATERIAL: return "%s.zmtrl";
		case ZM_HUMAN_ASSET_MODEL:    return "%s.zmodel";
		default:
			Zenith_Assert(false, "ZM_HumanBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}
}

// ============================================================================
// Golden clip metadata (literal-pinned).
// ============================================================================
const char* ZM_HumanClipName(ZM_HUMAN_ANIM_CLIP eClip)
{
	switch (eClip)
	{
	case ZM_HUMAN_CLIP_IDLE:  return "Idle";
	case ZM_HUMAN_CLIP_WALK:  return "Walk";
	case ZM_HUMAN_CLIP_RUN:   return "Run";
	case ZM_HUMAN_CLIP_TALK:  return "Talk";
	case ZM_HUMAN_CLIP_WAVE:  return "Wave";
	case ZM_HUMAN_CLIP_POINT: return "Point";
	case ZM_HUMAN_CLIP_CHEER: return "Cheer";
	case ZM_HUMAN_CLIP_HURT:  return "Hurt";
	case ZM_HUMAN_CLIP_FAINT: return "Faint";
	default:
		Zenith_Assert(false, "ZM_HumanClipName: bad clip %u", (u_int)eClip);
		return "Idle";
	}
}

float ZM_HumanClipDurationSeconds(ZM_HUMAN_ANIM_CLIP eClip)
{
	switch (eClip)
	{
	case ZM_HUMAN_CLIP_IDLE:  return 2.0f;
	case ZM_HUMAN_CLIP_WALK:  return 1.0f;
	case ZM_HUMAN_CLIP_RUN:   return 0.7f;
	case ZM_HUMAN_CLIP_TALK:  return 1.6f;
	case ZM_HUMAN_CLIP_WAVE:  return 1.0f;
	case ZM_HUMAN_CLIP_POINT: return 0.8f;
	case ZM_HUMAN_CLIP_CHEER: return 1.2f;
	case ZM_HUMAN_CLIP_HURT:  return 0.4f;
	case ZM_HUMAN_CLIP_FAINT: return 1.2f;
	default:
		Zenith_Assert(false, "ZM_HumanClipDurationSeconds: bad clip %u", (u_int)eClip);
		return 1.0f;
	}
}

bool ZM_HumanClipLooping(ZM_HUMAN_ANIM_CLIP eClip)
{
	switch (eClip)
	{
	case ZM_HUMAN_CLIP_IDLE:
	case ZM_HUMAN_CLIP_WALK:
	case ZM_HUMAN_CLIP_RUN:
	case ZM_HUMAN_CLIP_TALK:  return true;
	case ZM_HUMAN_CLIP_WAVE:
	case ZM_HUMAN_CLIP_POINT:
	case ZM_HUMAN_CLIP_CHEER:
	case ZM_HUMAN_CLIP_HURT:
	case ZM_HUMAN_CLIP_FAINT: return false;
	default:
		Zenith_Assert(false, "ZM_HumanClipLooping: bad clip %u", (u_int)eClip);
		return false;
	}
}

void ZM_BuildHumanClip(ZM_HUMAN_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	// SC4: author the shared clip's rotation channels against the shared skeleton
	// here. SC1 is a declared placeholder (never invoked by the SC1 gate), so the
	// body is intentionally empty -- the real curves land in SC4.
	(void)eClip;
	(void)xOut;
}

// ============================================================================
// Recipe resolution.
// ============================================================================
ZM_HumanRecipe ZM_ResolveHumanRecipe(ZM_HUMAN_ID eId)
{
	const ZM_HumanData& xData = ZM_GetHumanData(eId);

	ZM_HumanRecipe xRecipe;
	xRecipe.m_eId            = eId;
	// Family seed = name hash; distinct stems -> distinct synthetic seeds.
	xRecipe.m_uSyntheticSeed = ZM_GenHashName(xData.m_szName);

	// Per-domain PCG seeds -- the SOLE randomness source for every builder. Humans
	// have no evolution, so a fixed synthetic evo-stage feeds ZM_GenDeriveSeed.
	for (u_int d = 0; d < static_cast<u_int>(ZM_GEN_DOMAIN_COUNT); ++d)
	{
		xRecipe.m_aulDomainSeed[d] = ZM_GenDeriveSeed(xRecipe.m_uSyntheticSeed,
			static_cast<u_int>(eId), uZM_HUMAN_SYNTHETIC_EVO_STAGE, static_cast<ZM_GEN_DOMAIN>(d));
	}

	// Variety axes (drive the mesh loft + texture, NOT the shared skeleton).
	xRecipe.m_eBuild       = xData.m_eBuild;
	xRecipe.m_fHeightScale = ZM_HumanBuildHeightScale(xData.m_eBuild);
	xRecipe.m_eSkinTone    = xData.m_eSkinTone;
	xRecipe.m_uHairStyle   = xData.m_uHairStyle;
	xRecipe.m_eHairColour  = xData.m_eHairColour;
	xRecipe.m_eOutfit      = xData.m_eOutfit;
	xRecipe.m_eAttachment  = xData.m_eAttachment;

	return xRecipe;
}

// ============================================================================
// Shared skeleton -- THE canonical bone emit (16 bones, StickFigure core names,
// identity bind-local rotation on EVERY bone, feet grounded near world y=0).
// ============================================================================
void ZM_AppendSharedHumanBones(ZM_GenMesh& xMesh)
{
	const Zenith_Maths::Quat    xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root lifted to hip height (world y=1.0) so the leg chain descends to y=0.
	ZM_GenAddBone(xMesh, "Root",          -1,       Zenith_Maths::Vector3( 0.00f,  1.0f, 0.0f), xIdentity, xUnitScale);   // 0
	ZM_GenAddBone(xMesh, "Spine",         HB_ROOT,  Zenith_Maths::Vector3( 0.00f,  0.5f, 0.0f), xIdentity, xUnitScale);   // 1
	ZM_GenAddBone(xMesh, "Neck",          HB_SPINE, Zenith_Maths::Vector3( 0.00f,  0.7f, 0.0f), xIdentity, xUnitScale);   // 2
	ZM_GenAddBone(xMesh, "Head",          HB_NECK,  Zenith_Maths::Vector3( 0.00f,  0.2f, 0.0f), xIdentity, xUnitScale);   // 3

	ZM_GenAddBone(xMesh, "LeftUpperArm",  HB_SPINE, Zenith_Maths::Vector3(-0.30f,  0.6f, 0.0f), xIdentity, xUnitScale);   // 4
	ZM_GenAddBone(xMesh, "LeftLowerArm",  HB_LUARM, Zenith_Maths::Vector3( 0.00f, -0.4f, 0.0f), xIdentity, xUnitScale);   // 5
	ZM_GenAddBone(xMesh, "LeftHand",      HB_LLARM, Zenith_Maths::Vector3( 0.00f, -0.3f, 0.0f), xIdentity, xUnitScale);   // 6

	ZM_GenAddBone(xMesh, "RightUpperArm", HB_SPINE, Zenith_Maths::Vector3( 0.30f,  0.6f, 0.0f), xIdentity, xUnitScale);   // 7
	ZM_GenAddBone(xMesh, "RightLowerArm", HB_RUARM, Zenith_Maths::Vector3( 0.00f, -0.4f, 0.0f), xIdentity, xUnitScale);   // 8
	ZM_GenAddBone(xMesh, "RightHand",     HB_RLARM, Zenith_Maths::Vector3( 0.00f, -0.3f, 0.0f), xIdentity, xUnitScale);   // 9

	ZM_GenAddBone(xMesh, "LeftUpperLeg",  HB_ROOT,  Zenith_Maths::Vector3(-0.15f,  0.0f, 0.0f), xIdentity, xUnitScale);   // 10
	ZM_GenAddBone(xMesh, "LeftLowerLeg",  HB_LULEG, Zenith_Maths::Vector3( 0.00f, -0.5f, 0.0f), xIdentity, xUnitScale);   // 11
	ZM_GenAddBone(xMesh, "LeftFoot",      HB_LLLEG, Zenith_Maths::Vector3( 0.00f, -0.5f, 0.0f), xIdentity, xUnitScale);   // 12

	ZM_GenAddBone(xMesh, "RightUpperLeg", HB_ROOT,  Zenith_Maths::Vector3( 0.15f,  0.0f, 0.0f), xIdentity, xUnitScale);   // 13
	ZM_GenAddBone(xMesh, "RightLowerLeg", HB_RULEG, Zenith_Maths::Vector3( 0.00f, -0.5f, 0.0f), xIdentity, xUnitScale);   // 14
	ZM_GenAddBone(xMesh, "RightFoot",     HB_RLLEG, Zenith_Maths::Vector3( 0.00f, -0.5f, 0.0f), xIdentity, xUnitScale);   // 15
}

// ============================================================================
// Mesh builder (SC1 minimal humanoid: torso + head + 2 arms + 2 legs).
// ============================================================================
void ZM_BuildHumanMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	xMesh.Reset();
	ZM_AppendSharedHumanBones(xMesh);

	const float fW = ZM_HumanBuildWidthScale(xRecipe.m_eBuild);
	const float fH = xRecipe.m_fHeightScale;

	// Torso: pelvis -> upper chest, blending Spine -> Neck (ascending Y).
	{
		ZM_LoftRing axRings[4];
		axRings[0] = ZM_LoftRing{}; axRings[0].m_fY = 1.00f * fH; axRings[0].m_fRx = 0.24f * fW; axRings[0].m_fRz = 0.15f * fW; axRings[0].m_uBoneA = HB_SPINE; axRings[0].m_uBoneB = HB_SPINE; axRings[0].m_fBlendB = 0.0f;
		axRings[1] = ZM_LoftRing{}; axRings[1].m_fY = 1.50f * fH; axRings[1].m_fRx = 0.26f * fW; axRings[1].m_fRz = 0.16f * fW; axRings[1].m_uBoneA = HB_SPINE; axRings[1].m_uBoneB = HB_SPINE; axRings[1].m_fBlendB = 0.0f;
		axRings[2] = ZM_LoftRing{}; axRings[2].m_fY = 1.90f * fH; axRings[2].m_fRx = 0.22f * fW; axRings[2].m_fRz = 0.14f * fW; axRings[2].m_uBoneA = HB_SPINE; axRings[2].m_uBoneB = HB_NECK;  axRings[2].m_fBlendB = 0.40f;
		axRings[3] = ZM_LoftRing{}; axRings[3].m_fY = 2.20f * fH; axRings[3].m_fRx = 0.13f * fW; axRings[3].m_fRz = 0.11f * fW; axRings[3].m_uBoneA = HB_NECK;  axRings[3].m_uBoneB = HB_NECK;  axRings[3].m_fBlendB = 0.0f;
		ZM_AppendHumanTube(xMesh, axRings, 4u);
	}

	// Head: a small closed ovoid around the Head bone (ascending Y).
	{
		ZM_LoftRing axRings[3];
		axRings[0] = ZM_LoftRing{}; axRings[0].m_fY = 2.30f * fH; axRings[0].m_fRx = 0.09f * fW; axRings[0].m_fRz = 0.09f * fW; axRings[0].m_uBoneA = HB_HEAD; axRings[0].m_uBoneB = HB_HEAD; axRings[0].m_fBlendB = 0.0f;
		axRings[1] = ZM_LoftRing{}; axRings[1].m_fY = 2.42f * fH; axRings[1].m_fRx = 0.13f * fW; axRings[1].m_fRz = 0.13f * fW; axRings[1].m_uBoneA = HB_HEAD; axRings[1].m_uBoneB = HB_HEAD; axRings[1].m_fBlendB = 0.0f;
		axRings[2] = ZM_LoftRing{}; axRings[2].m_fY = 2.55f * fH; axRings[2].m_fRx = 0.09f * fW; axRings[2].m_fRz = 0.09f * fW; axRings[2].m_uBoneA = HB_HEAD; axRings[2].m_uBoneB = HB_HEAD; axRings[2].m_fBlendB = 0.0f;
		ZM_AppendHumanTube(xMesh, axRings, 3u);
	}

	// Arms (descending from the shoulder to the hand).
	ZM_AppendHumanLimb(xMesh, -0.30f * fW, 2.10f * fH, 1.40f * fH, 0.07f * fW, 0.05f * fW, HB_LUARM, HB_LLARM, HB_LHAND);
	ZM_AppendHumanLimb(xMesh,  0.30f * fW, 2.10f * fH, 1.40f * fH, 0.07f * fW, 0.05f * fW, HB_RUARM, HB_RLARM, HB_RHAND);

	// Legs (descending from the hip to the grounded foot).
	ZM_AppendHumanLimb(xMesh, -0.15f * fW, 1.00f * fH, 0.05f * fH, 0.10f * fW, 0.08f * fW, HB_LULEG, HB_LLLEG, HB_LFOOT);
	ZM_AppendHumanLimb(xMesh,  0.15f * fW, 1.00f * fH, 0.05f * fH, 0.10f * fW, 0.08f * fW, HB_RULEG, HB_RLLEG, HB_RFOOT);

	// THE finalise order (EmitRing already wrote analytic loft normals, so normals
	// are NOT regenerated): tangents from UVs, then renormalise the <=2-bone skin.
	ZM_GenGenerateTangents(xMesh);
	ZM_GenNormalizeSkinWeights(xMesh);
}

void ZM_BuildHuman(ZM_HUMAN_ID eId, ZM_Human& xOut)
{
	const ZM_HumanRecipe xRecipe = ZM_ResolveHumanRecipe(eId);

	xOut.m_eId = eId;
	ZM_BuildHumanMesh(xRecipe, xOut.m_xMesh);
	xOut.m_xAlbedo = ZM_BuildHumanAlbedo(xRecipe);
}

// ============================================================================
// Determinism helpers.
// ============================================================================
bool ZM_HumanMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB)
{
	return ZM_BufferBytesEqual(xA.m_xPositions,   xB.m_xPositions)
		&& ZM_BufferBytesEqual(xA.m_xNormals,     xB.m_xNormals)
		&& ZM_BufferBytesEqual(xA.m_xUVs,         xB.m_xUVs)
		&& ZM_BufferBytesEqual(xA.m_xTangents,    xB.m_xTangents)
		&& ZM_BufferBytesEqual(xA.m_xColors,      xB.m_xColors)
		&& ZM_BufferBytesEqual(xA.m_xIndices,     xB.m_xIndices)
		&& ZM_BufferBytesEqual(xA.m_xBoneIndices, xB.m_xBoneIndices)
		&& ZM_BufferBytesEqual(xA.m_xBoneWeights, xB.m_xBoneWeights)
		&& ZM_BufferBytesEqual(xA.m_xBones,       xB.m_xBones);
}

bool ZM_HumanBuildEqual(const ZM_Human& xA, const ZM_Human& xB)
{
	return ZM_HumanMeshEqual(xA.m_xMesh, xB.m_xMesh)
		&& xA.m_xAlbedo.Equals(xB.m_xAlbedo);
}

u_int ZM_HumanContentHash(const ZM_Human& xHuman)
{
	const ZM_GenMesh& xMesh = xHuman.m_xMesh;
	u_int uHash = uZM_FNV_OFFSET;
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xPositions);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xNormals);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xUVs);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xTangents);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xColors);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xIndices);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBoneIndices);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBoneWeights);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBones);

	// Fold the albedo content hash (already FNV over packed texels).
	uHash = ZM_GenHashCombine(uHash, xHuman.m_xAlbedo.ContentHash());
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_HumanValidation ZM_ValidateHuman(const ZM_Human& xHuman)
{
	ZM_HumanValidation xV;

	// --- Mesh structure (bone cap == the shared-human bone count) ---
	const ZM_GenMeshValidation xMesh = ZM_ValidateGenMesh(xHuman.m_xMesh, uZM_HUMAN_BONE_COUNT);
	xV.m_bWindingOutward   = xMesh.m_bWindingOutward;
	xV.m_bBoundsNonDegen   = xMesh.m_bBoundsNonDegen;
	xV.m_bWeightsSumToOne  = xMesh.m_bWeightsSumToOne;
	xV.m_bWeightsAtMostTwo = xMesh.m_bWeightsAtMostTwo;
	xV.m_bBonesWithinCap   = xMesh.m_bBonesWithinCap;
	xV.m_uFirstBadVertex   = xMesh.m_uFirstBadVertex;
	xV.m_uFirstBadTriangle = xMesh.m_uFirstBadTriangle;
	xV.m_bMeshValid = xV.m_bWindingOutward && xV.m_bBoundsNonDegen && xV.m_bWeightsSumToOne
		&& xV.m_bWeightsAtMostTwo && xV.m_bBonesWithinCap;

	// --- Skeleton topology (single root + parent-before-child) ---
	const u_int uNumBones = xHuman.m_xMesh.GetNumBones();
	u_int uRootCount = 0u;
	bool  bOrder = (uNumBones > 0u);
	for (u_int u = 0; u < uNumBones; ++u)
	{
		const ZM_GenBone& xBone = xHuman.m_xMesh.m_xBones.Get(u);
		if (xBone.m_iParent == -1)
		{
			++uRootCount;
		}
		else if (!(xBone.m_iParent < static_cast<int>(u)))
		{
			bOrder = false;
			if (xV.m_szFirstBad[0] == '\0')
			{
				size_t uLen = strlen(xBone.m_szName);
				if (uLen >= static_cast<size_t>(uZM_GEN_BONE_NAME_MAX))
				{
					uLen = static_cast<size_t>(uZM_GEN_BONE_NAME_MAX - 1u);
				}
				memcpy(xV.m_szFirstBad, xBone.m_szName, uLen);
				xV.m_szFirstBad[uLen] = '\0';
			}
		}
	}
	xV.m_bHasSingleRoot          = (uRootCount == 1u);
	xV.m_bParentsBeforeChildren  = bOrder;
	xV.m_bBoneCountMatchesShared = (uNumBones == uZM_HUMAN_BONE_COUNT);

	// --- Texture ---
	xV.m_bAlbedoNonEmpty = !xHuman.m_xAlbedo.IsEmpty();

	// --- Rollup ---
	xV.m_bAllValid = xV.m_bMeshValid
		&& xV.m_bHasSingleRoot && xV.m_bParentsBeforeChildren && xV.m_bBoneCountMatchesShared
		&& xV.m_bAlbedoNonEmpty;
	return xV;
}

// ============================================================================
// Asset-path schemes.
// ============================================================================
bool ZM_HumanAssetPath(ZM_HUMAN_ID eId, ZM_HUMAN_ASSET_KIND eKind, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_HumanAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szName = ZM_GetHumanName(eId);
	char acBase[128];
	const int iBase = snprintf(acBase, sizeof(acBase), ZM_HumanBasenameFmt(eKind), szName);
	if (iBase < 0 || static_cast<u_int>(iBase) >= sizeof(acBase))
	{
		return false;   // internal basename overflowed acBase -- overflow contract is TOTAL
	}

	const int iN = snprintf(szOut, uCap, "game:Humans/%s/%s", szName, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

bool ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_KIND eKind, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_HumanSharedAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	int iN = -1;
	if (eKind == ZM_HUMAN_SHARED_ASSET_SKELETON)
	{
		iN = snprintf(szOut, uCap, "game:Humans/Shared/Human.zskel");
	}
	else if (eKind >= ZM_HUMAN_SHARED_ASSET_ANIM_IDLE && eKind < ZM_HUMAN_SHARED_ASSET_KIND_COUNT)
	{
		// Clip suffix is contiguous with the clip enum (Idle..Faint).
		const ZM_HUMAN_ANIM_CLIP eClip =
			static_cast<ZM_HUMAN_ANIM_CLIP>(eKind - ZM_HUMAN_SHARED_ASSET_ANIM_IDLE);
		iN = snprintf(szOut, uCap, "game:Humans/Shared/Human_%s.zanim", ZM_HumanClipName(eClip));
	}
	else
	{
		Zenith_Assert(false, "ZM_HumanSharedAssetPath: bad kind %u", (u_int)eKind);
		return false;
	}
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

// ============================================================================
// Disk bake (TOOLS ONLY) -- bodies land in SC5.
// ============================================================================
#ifdef ZENITH_TOOLS
bool ZM_BakeHumanShared()
{
	// SC5: bake the shared Human.zskel + the 9 shared Human_<Clip>.zanim files ONCE.
	return false;
}

bool ZM_BakeHuman(ZM_HUMAN_ID eId)
{
	(void)eId;
	// SC5: bake this model's mesh + placeholder albedo + .zmtrl + .zmodel bundle
	// (referencing the shared skeleton + shared clip set).
	return false;
}

bool ZM_BakeAllHumans()
{
	// SC5: bake the shared rig once, then every model.
	return false;
}
#endif   // ZENITH_TOOLS
