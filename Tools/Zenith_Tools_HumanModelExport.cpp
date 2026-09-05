#include "Zenith.h"

#include "Zenith_Tools_HumanModelExport.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_HumanProportions.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_SkinDeform.h"
#include "Core/Zenith_ProjectHooks.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Zenith_Tools_GlbImport.h"
#include "Zenith_Tools_HumanSkinBind.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Zenith_Tools_HumanModelExport
{
namespace
{
	// ★ THE "engine:" PREFIX IS LOAD-BEARING. Zenith_AssetRegistry::NormalizeAssetPath
	// converts an ABSOLUTE path into a prefixed one and leaves a bare RELATIVE path
	// exactly as it found it -- so a model whose skeleton ref is
	// "Meshes/StickFigure/StickFigure.zskel" loads, renders, and reports NO SKELETON.
	// The mesh still says HasSkinning (that only asks whether the string is empty),
	// the bundle passes every completeness check, and the character stands in its
	// bind pose while the animator logs "ModelComponent reports no skeleton" at info
	// level. Which is exactly what it did.
	const char* szSKELETON_REF = "engine:Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
	// The tree this exporter owns. IsHumanoidSourcePath is what makes it a routing
	// rule rather than a convention, and the generic .glb walk reads it.
	const char* szHUMANS_DIR = "Meshes/Humans/";
	const char* szMALE_DIR = "Meshes/Humans/Male/";
	const char* szMALE_NAME = "Male";

	//--------------------------------------------------------------------------
	// How close a source has to be to the rig it binds to.
	//
	// ★★ ONE SHARED SKELETON MEANS FIXED JOINT POSITIONS, and that is a real
	// constraint on what may be imported, not a formality. A mesh whose knee is
	// 15cm from the rig's knee bone will bend at its thigh no matter how good the
	// weight solve is. So the landmarks are measured and CHECKED, and a source that
	// is not close enough is refused BY NAME -- "shoulder is 0.71 of height, the rig
	// wants 0.77" is something a person can act on, where a silently badly-rigged
	// character is not.
	//
	// ★ THE TOLERANCE IS A FRACTION OF TOTAL HEIGHT, which is what makes it
	// comparable across a 2.6-unit rig and a 0.98-unit source. 4% of height is about
	// 7cm on a real adult -- generous enough for ordinary build variation, tight
	// enough that a child or a stylised long-limbed character is caught rather than
	// shipped bending in the wrong place.
	//--------------------------------------------------------------------------
	constexpr float fLANDMARK_TOLERANCE_FRAC = 0.04f;

	//--------------------------------------------------------------------------
	// Publication
	//--------------------------------------------------------------------------
	bool PublishFile(const std::string& strTmp, const std::string& strFinal)
	{
		// std::filesystem::rename REPLACES an existing file on Windows (it is
		// MoveFileEx(..., MOVEFILE_REPLACE_EXISTING) underneath), which is not
		// what POSIX rename semantics would lead you to expect for a directory
		// entry that already exists.
		std::error_code xEc;
		std::filesystem::rename(strTmp, strFinal, xEc);
		if (xEc)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: could not publish %s -> %s (%s)",
				strTmp.c_str(), strFinal.c_str(), xEc.message().c_str());
			return false;
		}
		return true;
	}

	void RemoveIfPresent(const std::string& strPath)
	{
		std::error_code xEc;
		std::filesystem::remove(strPath, xEc);
	}

	// Everything a consumer would be entitled to assume, checked BEFORE anything
	// reaches disk. "Does the file exist" is not a validity check -- that is the
	// mistake the LampPost incident was made of.
	bool ValidateBoundMesh(const Zenith_MeshAsset& xMesh, u_int uNumBones, std::string& strWhyOut)
	{
		if (xMesh.GetNumVerts() == 0u || xMesh.GetNumIndices() == 0u)
		{
			strWhyOut = "empty mesh"; return false;
		}
		if (xMesh.m_xBoneIndices.GetSize() != xMesh.GetNumVerts() ||
			xMesh.m_xBoneWeights.GetSize() != xMesh.GetNumVerts())
		{
			strWhyOut = "no skinning data"; return false;
		}
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			const glm::vec4& xW = xMesh.m_xBoneWeights.Get(v);
			const float fSum = xW.x + xW.y + xW.z + xW.w;
			if (fSum < 1.0f - 1.0e-4f || fSum > 1.0f + 1.0e-4f)
			{
				strWhyOut = "vertex " + std::to_string(v) + " weights sum to " + std::to_string(fSum);
				return false;
			}
			const glm::uvec4& xI = xMesh.m_xBoneIndices.Get(v);
			for (int k = 0; k < 4; ++k)
			{
				if (xI[k] >= uNumBones)
				{
					strWhyOut = "vertex " + std::to_string(v) + " references bone " + std::to_string(xI[k]) +
						" of " + std::to_string(uNumBones);
					return false;
				}
			}
		}
		const Zenith_Maths::Vector3 xExtent = xMesh.GetBoundsMax() - xMesh.GetBoundsMin();
		if (xExtent.x <= 1.0e-4f || xExtent.y <= 1.0e-4f || xExtent.z <= 1.0e-4f)
		{
			strWhyOut = "degenerate bounds"; return false;
		}
		return true;
	}
}

//==============================================================================

ExportResult ExportBoundHumanModel(const std::string& strGlbPath)
{
	ExportResult xResult;

	const std::filesystem::path xPath(strGlbPath);
	const std::string strBaseName = (xPath.parent_path() / xPath.stem()).string();
	const std::string strModelName = xPath.stem().string();
	const std::string strMeshPath = strBaseName + ZENITH_MESH_ASSET_EXT;
	const std::string strModelPath = strBaseName + ZENITH_MODEL_EXT;

	std::error_code xEc;
	if (!std::filesystem::exists(xPath, xEc))
	{
		// ★ A MISSING SOURCE IS NOT A FAILURE. The asset tree is gitignored, so a
		// fresh clone legitimately has no art -- exactly the case
		// ImportGlbsInDirectory handles for a missing directory. Log, delete
		// nothing, return.
		Zenith_Log(LOG_CATEGORY_TOOLS, "HUMAN_BIND: no source at %s, skipping", strGlbPath.c_str());
		return xResult;
	}
	xResult.m_bAttempted = true;

	// ★ THE STALE BUNDLE GOES NOW, not at publish time. Once there is a source to
	// bind, whatever .zmodel is sitting there describes a previous run and a
	// consumer cannot tell it apart from a fresh one -- so every failure path
	// below leaves NO marker rather than an old one. The bundle is regenerable
	// bake output under the Assets gitignore, so losing it costs a re-boot.
	RemoveIfPresent(strModelPath);

	//--- The rig it binds to. It has to be on disk already; GenerateStickFigureAssets
	//    runs immediately before this for exactly that reason.
	const std::string strSkeletonPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
	Zenith_SkeletonAsset* pxShipped = Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(strSkeletonPath);
	if (pxShipped == nullptr || pxShipped->GetNumBones() == 0u)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: could not load the shipped rig at %s", strSkeletonPath.c_str());
		xResult.m_strFailureStage = "skeleton";
		return xResult;
	}

	//--- Load, normalise, measure.
	Zenith_MeshAsset xMesh;
	Zenith_Tools_GlbImport::GlbImportResult xLoad;
	if (!Zenith_Tools_GlbImport::LoadGlbMesh(strGlbPath, xMesh, xLoad))
	{
		xResult.m_strFailureStage = "load";
		return xResult;
	}

	// ★★ THE ORIENTATION IS MEASURED, NOT DECLARED. This used to be a hand-written
	// 'yaw' line in a committed .zbind sidecar beside the art, and a sidecar is
	// exactly the wrong shape for it: getting the sign wrong ships a character 180
	// degrees round, which no screenshot pass reliably catches (at head-thumbnail
	// size the back of a head reads as a face -- that is how the first version of
	// this model shipped backwards), and every future artist mesh would need a
	// human to author one correctly before it could be imported at all.
	//
	// Both halves are decidable from the geometry. See DetectAndNormaliseIntoRigSpace.
	float fYawApplied = 0.0f;
	if (!Zenith_Tools_HumanSkinBind::DetectAndNormaliseIntoRigSpace(xMesh, fYawApplied))
	{
		xResult.m_strFailureStage = "normalise";
		return xResult;
	}

	Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(xMesh);
	Zenith_HumanLandmarks xLandmarks;
	if (!Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xLandmarks))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: landmark scan failed on %s", strGlbPath.c_str());
		xResult.m_strFailureStage = "measure";
		return xResult;
	}

	Zenith_LogHumanLandmarks(strModelName.c_str(), xLandmarks);

	const Zenith_Tools_HumanSkinBind::ArmSanity xArm = Zenith_Tools_HumanSkinBind::CheckArmChain(xLandmarks);
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"HUMAN_BIND: arm sanity - hand %.4f of height, upper arm %.3fx legacy, forearm %.3fx legacy",
		xArm.m_fHandLengthFrac, xArm.m_fUpperArmRatio, xArm.m_fForearmRatio);
	if (!xArm.m_bValid)
	{
		// ★ REFUSE RATHER THAN SHIP A BAD RIG. A landmark that caught a mitten
		// instead of a wrist produces a comically long hand and nothing else in
		// the build would notice.
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: arm chain fails its sanity bounds - %s", xArm.m_strReason.c_str());
		xResult.m_strFailureStage = "armsanity";
		return xResult;
	}

	//--- Does this body actually fit the rig? A shared skeleton has fixed joints,
	//    so this is the question that decides whether the bind can be any good.
	std::string strMismatch;
	if (!Zenith_Tools_HumanSkinBind::CheckLandmarksAgainstRig(xLandmarks,
		Zenith_HumanProportionsRealistic(), fLANDMARK_TOLERANCE_FRAC, strMismatch))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS,
			"HUMAN_BIND: %s does not fit the shared rig - %s. Re-proportion the source, or give this "
			"character its own rig.", strModelName.c_str(), strMismatch.c_str());
		xResult.m_strFailureStage = "proportions";
		return xResult;
	}

	//--- The solve, straight against the shipped rig.
	//
	// ★★ THERE IS NO FITTED INTERMEDIATE RIG ANY MORE, and there must not be. The
	// weights this produces are bone INDICES, and they ship alongside the shipped
	// rig's inverse bind matrices -- so solving against a rig fitted to this mesh's
	// own measurements and then binding to a different one deforms the mesh at rest
	// by exactly the difference between them. Solving against the rig the mesh will
	// actually be skinned by is the only version of this that is self-consistent,
	// and the check above is what makes it safe: a body the shipped rig does not
	// describe is refused rather than quietly mis-rigged.
	Zenith_Tools_HumanSkinBind::SolveReport xSolve;
	if (!Zenith_Tools_HumanSkinBind::SolveHumanSkinWeights(xMesh, *pxShipped, xLandmarks, xSolve) || !xSolve.m_bValid)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: weight solve failed (%u vertices no bone claimed)",
			xSolve.m_uFallbackCount);
		xResult.m_strFailureStage = "solve";
		return xResult;
	}

	// ★★ THE MESH IS NEVER RE-POSED. It ships in the pose the artist made it in,
	// bound to the engine's ONE humanoid rig, because that rig is T-POSED -- see
	// Zenith_HumanArmBindRotation for why, and for what the procedural humans gave
	// up to make it so. Nothing between LoadGlbMesh and here has moved a vertex
	// except the rigid rotate/scale/translate of normalisation.
	//
	// This is the whole reason the shoulders are right. Transferring a T-posed mesh
	// into an arms-down bind pose BAKES a 90-degree shoulder rotation through the
	// skin weights, and no weighting can make it clean: a vertex at lever r from
	// the joint, in a blend band of width W, shears by about r*(pi/2)/W, so holding
	// stretch under 25% needs W > 6r -- a band six times wider than its own
	// distance from the joint, which cannot exist. Measured on this exact asset:
	// 13% of shoulder edges past 25%, single edges past EIGHTFOLD, permanently,
	// reading as lumpy padded shoulders.
	// ★ AFTER THE VERTICES STOP MOVING, and not before. The normalise and the
	// rebind both move them; tangents derived before either would describe a mesh
	// that no longer exists, and a wrong tangent renders as noise rather than as
	// an error.
	xMesh.GenerateTangents();
	xMesh.ComputeBounds();
	xMesh.SetSkeletonPath(szSKELETON_REF);

	std::string strWhy;
	if (!ValidateBoundMesh(xMesh, pxShipped->GetNumBones(), strWhy))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: refusing to publish - %s", strWhy.c_str());
		xResult.m_strFailureStage = "validate";
		return xResult;
	}

	//--- Publish. The .zmodel is the marker, so it goes LAST (it was removed the
	//    moment this bind was attempted; see above).
	const std::string strMeshTmp = strMeshPath + ".tmp";
	const std::string strModelTmp = strModelPath + ".tmp";
	RemoveIfPresent(strMeshTmp);
	RemoveIfPresent(strModelTmp);

	auto Abort = [&](const char* szStage)
	{
		RemoveIfPresent(strModelPath);
		RemoveIfPresent(strMeshTmp);
		RemoveIfPresent(strModelTmp);
		Zenith_Error(LOG_CATEGORY_TOOLS, "HUMAN_BIND: publication aborted at stage '%s'; no .zmodel left behind", szStage);
		xResult.m_strFailureStage = szStage;
	};

	xMesh.Export(strMeshTmp.c_str());
	if (!std::filesystem::exists(strMeshTmp, xEc) || !PublishFile(strMeshTmp, strMeshPath))
	{
		Abort("mesh");
		return xResult;
	}

	// Textures and the .zmtrl go to their final names: the material file EMBEDS
	// the texture paths, so staging them under temporary names would publish a
	// material pointing at files that no longer exist. They are prerequisites,
	// and the marker below is what makes them invisible until they are all there.
	Zenith_Vector<std::string> xMaterialRefs;
	u_int uTextures = 0u;
	if (!Zenith_Tools_GlbImport::ExportGlbMaterials(strGlbPath, strBaseName, xMaterialRefs, uTextures))
	{
		Abort("materials");
		return xResult;
	}

	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModelHandle = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModelHandle.GetDirect();
		pxModel->SetName(strModelName);
		pxModel->SetSkeletonPath(std::string(szSKELETON_REF));
		pxModel->AddMeshByPath(strMeshPath, xMaterialRefs);
		pxModel->Export(strModelTmp.c_str());
	}
	if (!std::filesystem::exists(strModelTmp, xEc) || !PublishFile(strModelTmp, strModelPath))
	{
		Abort("model");
		return xResult;
	}

	xResult.m_bSuccess = true;
	xResult.m_uNumVerts = xMesh.GetNumVerts();
	xResult.m_uNumBones = pxShipped->GetNumBones();
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"HUMAN_BIND: published %s - %u verts skinned to %u bones on the SHARED rig (mesh UNDEFORMED), %u texture(s)",
		strModelPath.c_str(), xResult.m_uNumVerts, xResult.m_uNumBones, uTextures);
	return xResult;
}

bool IsHumanoidSourcePath(const std::string& strPath)
{
	// ★★ THE ROUTING IS A DIRECTORY, NOT A SIDECAR FILE. A .glb under Meshes/Humans
	// belongs to this exporter; every other .glb belongs to the generic walk. That
	// replaces a committed .zbind whose ONLY surviving job was to say exactly this,
	// and it is what makes the pipeline additive: dropping a new T-posed humanoid
	// into Meshes/Humans/<Name>/<Name>.glb is the entire import procedure.
	//
	// ★ THE HAZARD IT GUARDS IS REAL AND HAS COST THIS REPO ONCE. ExportAllMeshes
	// runs the generic walk at Zenith_Engine.cpp:558; GenerateTestAssets calls this
	// exporter at :571. Without the skip, the generic path writes a STATIC bundle
	// first and this one overwrites it in the same boot -- and if this one then
	// fails, an existence-only check downstream picks up the static bundle and
	// animates nothing. That is exactly the incident ZM_Tests_PropBake.cpp was
	// written for (ZM_BakeProp silently overwrote the imported LampPost on the same
	// paths in the same boot: 6623 verts to 72, suite green throughout).
	//
	// Compared with '/' separators on both sides because the walk hands us native
	// paths and a Windows backslash would match nothing.
	std::string strNormalised = strPath;
	for (char& c : strNormalised) { if (c == '\\') { c = '/'; } }
	return strNormalised.find(szHUMANS_DIR) != std::string::npos;
}

void ExportBoundHumanModels()
{
	// ★ EVERY .glb UNDER A HUMANS TREE, not a hardcoded Male. The rig is shared,
	// the binder takes no per-asset declaration, and the orientation is measured --
	// so there is nothing left for a new humanoid to configure, and nothing left
	// for this function to know about it beyond where to look.
	//
	// ★★ BOTH ASSET ROOTS, and that is not optional. IsHumanoidSourcePath makes the
	// generic .glb walk stand aside for Meshes/Humans in whichever tree it is
	// walking, and ExportAllMeshes walks the GAME tree as well as the engine one.
	// Scanning only the engine root here would leave a game's own humanoid skipped
	// by the generic importer and picked up by nobody -- present on disk, silently
	// absent from the build, with no error anywhere.
	const std::string astrRoots[] = {
		std::string(ENGINE_ASSETS_DIR) + szHUMANS_DIR,
		std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/" + szHUMANS_DIR,
	};

	for (const std::string& strRoot : astrRoots)
	{
		std::error_code xEc;
		if (!std::filesystem::exists(strRoot, xEc) || xEc)
		{
			// The asset tree is gitignored, so a fresh clone legitimately has no art,
			// and a game with no humanoids of its own is the ordinary case.
			Zenith_Log(LOG_CATEGORY_TOOLS, "HUMAN_BIND: no humans directory at %s, skipping", strRoot.c_str());
			continue;
		}

		for (const auto& xEntry : std::filesystem::recursive_directory_iterator(strRoot, xEc))
		{
			if (!xEntry.is_regular_file(xEc)) { continue; }
			if (xEntry.path().extension() != ".glb") { continue; }
			(void)ExportBoundHumanModel(xEntry.path().string());
		}
	}
}

std::string GetMaleModelRef()
{
	const std::string strPath = std::string(ENGINE_ASSETS_DIR) + szMALE_DIR + szMALE_NAME + ZENITH_MODEL_EXT;
	std::error_code xEc;
	if (!std::filesystem::exists(strPath, xEc)) { return std::string(); }
	return std::string("engine:") + szMALE_DIR + szMALE_NAME + ZENITH_MODEL_EXT;
}

}   // namespace Zenith_Tools_HumanModelExport

#ifdef ZENITH_TOOLS
#include "Zenith_Tools_HumanModelExport.Tests.inl"
#endif
