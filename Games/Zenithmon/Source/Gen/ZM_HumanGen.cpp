#include "Zenith.h"

// ============================================================================
// ZM_HumanGen -- the S4 human generator driver. See the header for the
// architecture + determinism contract. This TU owns: human -> recipe resolution,
// the per-domain seed derivation, the shared 16-bone skeleton emit, the
// SC3 appearance driver, the full-bundle driver, the byte-identity + hash +
// validation machinery, the golden clip metadata, the two asset-path schemes,
// and (tools only, SC5) the disk bake stubs. The SC2 mesh loft lives in
// ZM_HumanMesh.cpp; the SC3 appearance implementation lives in
// ZM_HumanAppearance.cpp.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"

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
	// height never lifts the feet off the floor. The skeleton remains fixed, hence
	// the intentionally narrow 0.97..1.03 spread.
	float ZM_HumanBuildHeightScale(ZM_HUMAN_BUILD eBuild)
	{
		switch (eBuild)
		{
		case ZM_HUMAN_BUILD_SLIGHT:  return 0.98f;
		case ZM_HUMAN_BUILD_AVERAGE: return 1.00f;
		case ZM_HUMAN_BUILD_STOCKY:  return 0.97f;
		case ZM_HUMAN_BUILD_TALL:    return 1.03f;
		default:                     return 1.00f;
		}
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
// Disk bake (TOOLS ONLY) -- the shared rig + 9 clips baked ONCE, then each model's
// mesh/albedo/.zmtrl/.zmodel binding the shared refs. Mirrors ZM_BakeCreature; the
// two new ZM_GenCommon bridges (ZM_GenBakeSkeleton / ZM_GenBakeMeshWithShared-
// Skeleton) keep the shared-rig-vs-per-model split explicit.
// ============================================================================
#ifdef ZENITH_TOOLS

// The .zmtrl / .zmodel bundle writers reach the asset layer directly (tools-only,
// so these headers never touch the pure generation API compiled in every config).
#include "AssetHandling/Zenith_MaterialAsset.h"   // Zenith_MaterialAsset, TextureHandle (BASE_COLOR = albedo)
#include "AssetHandling/Zenith_ModelAsset.h"      // Zenith_ModelAsset (mesh+skeleton+material bundle)
#include "AssetHandling/Zenith_AssetRegistry.h"   // Zenith_AssetRegistry::Create<> owning-handle + ResolvePath
#include "Flux/MeshAnimation/Flux_AnimationClip.h"   // Flux_AnimationClip authoring + Export
#include "Collections/Zenith_Vector.h"            // Zenith_Vector<std::string> material list
#include <filesystem>
#include <string>

// Bake the ONE shared rig + 9 clips (game:Humans/Shared/Human.zskel +
// Human_<Clip>.zanim x9). The skeleton bake creates the Humans/Shared/ folder
// FIRST (Flux_AnimationClip::Export creates NO directories), so the 9 clip Exports
// that follow land in the already-created folder.
bool ZM_BakeHumanShared()
{
	// The shared 16-bone rig -- the SAME canonical emit every model's mesh binds.
	ZM_GenMesh xRig;
	ZM_AppendSharedHumanBones(xRig);

	char acSkelRef[512];
	if (!ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_SKELETON, acSkelRef, sizeof(acSkelRef)))
	{
		return false;
	}
	const std::string strSkelFs = Zenith_AssetRegistry::ResolvePath(std::string(acSkelRef));
	bool bOk = ZM_GenBakeSkeleton(xRig, strSkelFs.c_str());   // create_directories(Humans/Shared/)

	std::error_code xEc;
	for (u_int c = 0; c < static_cast<u_int>(ZM_HUMAN_CLIP_COUNT); ++c)
	{
		Flux_AnimationClip xClip;
		ZM_BuildHumanClip(static_cast<ZM_HUMAN_ANIM_CLIP>(c), xClip);

		char acClipRef[512];
		if (!ZM_HumanSharedAssetPath(
				static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + c),
				acClipRef, sizeof(acClipRef)))
		{
			return false;
		}
		const std::string strClipFs = Zenith_AssetRegistry::ResolvePath(std::string(acClipRef));
		xClip.Export(strClipFs);   // void; creates no directories (folder already made by skel bake)
		bOk &= std::filesystem::exists(std::filesystem::path(strClipFs), xEc);
	}
	return bOk;
}

// Bake ONE model's mesh + placeholder albedo + .zmtrl + .zmodel. Binds the SHARED
// .zskel + SHARED 9-clip set by ref; writes NO per-model skeleton. Mesh + albedo
// bakes create the Humans/<Name>/ folder FIRST (SaveToFile + model Export create no
// directories), so the material + model writes that follow land in it.
bool ZM_BakeHuman(ZM_HUMAN_ID eId)
{
	ZM_Human xHuman;
	ZM_BuildHuman(eId, xHuman);   // mesh (16 shared bones) + placeholder albedo

	char acMeshRef[512], acAlbedoRef[512], acMatRef[512], acModelRef[512], acSkelRef[512];
	bool bOk = true;
	bOk &= ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_MESH,     acMeshRef,   sizeof(acMeshRef));
	bOk &= ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_ALBEDO,   acAlbedoRef, sizeof(acAlbedoRef));
	bOk &= ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_MATERIAL, acMatRef,    sizeof(acMatRef));
	bOk &= ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_MODEL,    acModelRef,  sizeof(acModelRef));
	bOk &= ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_SKELETON, acSkelRef, sizeof(acSkelRef));
	if (!bOk)
	{
		return false;   // a path overflowed; do not bake a partial bundle
	}

	const std::string strMeshFs   = Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));
	const std::string strAlbedoFs = Zenith_AssetRegistry::ResolvePath(std::string(acAlbedoRef));
	const std::string strMatFs    = Zenith_AssetRegistry::ResolvePath(std::string(acMatRef));
	const std::string strModelFs  = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	// Mesh (.zmesh) binding the SHARED skeleton ref; NO per-model .zskel. Albedo (BC1).
	bOk &= ZM_GenBakeMeshWithSharedSkeleton(xHuman.m_xMesh, strMeshFs.c_str(), acSkelRef);
	bOk &= ZM_SynthBakeAlbedoBC1(xHuman.m_xAlbedo, strAlbedoFs.c_str());

	const std::string strName = ZM_GetHumanName(eId);

	// Material (.zmtrl v5): baked albedo in the BASE_COLOR slot, matte dielectric.
	// Create<>()+GetDirect() keeps the asset alive across SaveToFile (never a stack
	// object). The albedo is passed as a "game:" ref (stored as a path, NOT loaded now).
	{
		Zenith_AssetHandle<Zenith_MaterialAsset> xMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat = xMat.GetDirect();
		pxMat->SetName(strName);
		pxMat->SetDiffuseTexture(TextureHandle(std::string(acAlbedoRef)));
		pxMat->SetRoughness(0.8f);
		pxMat->SetMetallic(0.0f);
		pxMat->SaveToFile(strMatFs);
	}

	// Model (.zmodel v2): the single-submesh human mesh + the SHARED skeleton ref +
	// the SHARED 9-clip set (IDLE..FAINT) + exactly ONE material (single-submesh mesh).
	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModel.GetDirect();
		pxModel->SetName(strName);
		pxModel->SetSkeletonPath(std::string(acSkelRef));               // SHARED .zskel
		for (u_int c = 0; c < static_cast<u_int>(ZM_HUMAN_CLIP_COUNT); ++c)
		{
			char acClipRef[512];
			ZM_HumanSharedAssetPath(
				static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + c),
				acClipRef, sizeof(acClipRef));
			pxModel->AddAnimationPath(std::string(acClipRef));          // SHARED .zanim x9
		}
		Zenith_Vector<std::string> xMats;
		xMats.PushBack(std::string(acMatRef));
		pxModel->AddMeshByPath(std::string(acMeshRef), xMats);
		pxModel->Export(strModelFs.c_str());
	}

	// SaveToFile always returns true and Export is void, so exists() is the real IO
	// signal (mirrors ZM_BakeCreature): AND both new artifacts into bOk.
	std::error_code xEc;
	bOk &= std::filesystem::exists(std::filesystem::path(strMatFs),   xEc);
	bOk &= std::filesystem::exists(std::filesystem::path(strModelFs), xEc);
	return bOk;
}

bool ZM_BakeAllHumans()
{
	bool bOk = ZM_BakeHumanShared();
	const u_int uCount = static_cast<u_int>(ZM_HUMAN_COUNT);
	for (u_int u = 0; u < uCount; ++u)
	{
		bOk &= ZM_BakeHuman(static_cast<ZM_HUMAN_ID>(u));
	}
	return bOk;
}
#endif   // ZENITH_TOOLS
