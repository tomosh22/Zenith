#pragma once
#ifdef ZENITH_TOOLS
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Gen/ZM_AuthoredCreatureMotion.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "../../../../Tools/Zenith_Tools_GlbImport.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ZM_AuthoredCreature
{
inline std::string Signature(const std::filesystem::path& xPath)
{
	std::error_code ec;
	if (!std::filesystem::is_regular_file(xPath, ec) || ec) return {};
	const auto uSize = std::filesystem::file_size(xPath, ec); if (ec || !uSize) return {};
	const auto xTime = std::filesystem::last_write_time(xPath, ec); if (ec) return {};
	return std::to_string(uSize) + " " + std::to_string(xTime.time_since_epoch().count());
}
inline bool OutputExtension(const std::filesystem::path& p)
{
	const auto ext = p.extension().string();
	return ext == ".zmodel" || ext == ".zmesh" || ext == ".zskel" || ext == ".zanim" || ext == ".zmtrl" || ext == ".ztxtr";
}
inline bool Warm(const std::filesystem::path& xBase)
{
	std::ifstream in(xBase.string() + ".authored-v4"); std::string line;
	if (!std::getline(in, line) || line != "Zenith authored creature 4") return false;
	for (const char* suffix : {".glb", "_shiny.glb"})
		if (!std::getline(in, line) || line != Signature(xBase.string() + suffix) || line.empty()) return false;
	u_int count = 0, uClips = 0; bool bModel = false, bShiny = false, bMesh = false, bRig = false, bEnd = false;
	while (std::getline(in, line))
	{
		if (line == "END") { bEnd = in.peek() == std::char_traits<char>::eof(); break; }
		const auto separator = line.find('|'); if (separator == std::string::npos) return false;
		const std::filesystem::path name(line.substr(0, separator));
		if (name.has_parent_path() || !OutputExtension(name) || Signature(xBase.parent_path() / name) != line.substr(separator + 1)) return false;
		bModel |= name == xBase.filename().string() + ".zmodel";
		bShiny |= name == xBase.filename().string() + "_shiny.zmodel";
		bMesh |= name == xBase.filename().string() + ".zmesh";
		bRig |= name == xBase.filename().string() + ".zskel"; ++count;
		for (u_int c = 0; c < ZM_ANIM_CLIP_COUNT; ++c)
			if (name == xBase.filename().string() + "_" + ZM_CreatureClipName(static_cast<ZM_ANIM_CLIP>(c)) + ".zanim") uClips |= 1u << c;
	}
	return bEnd && bModel && bShiny && bMesh && bRig && count >= 10 && uClips == (1u << ZM_ANIM_CLIP_COUNT) - 1;
}
inline bool Publish(const std::filesystem::path& p)
{
	if (Signature(p.string() + ".tmp").empty()) return false;
	std::error_code ec; std::filesystem::remove(p, ec); if (ec) return false;
	std::filesystem::rename(p.string() + ".tmp", p, ec); return !ec;
}
template<class T> inline bool Same(const Zenith_Vector<T>& a, const Zenith_Vector<T>& b)
{
	if (a.GetSize() != b.GetSize()) return false;
	for (u_int i = 0; i < a.GetSize(); ++i) if (a.Get(i) != b.Get(i)) return false;
	return true;
}

// A source owns the bundle even when it is invalid or incomplete. Return value
// answers ownership; bSuccess answers import success. Never fall back on failure.
inline bool Try(ZM_SPECIES_ID eId, const std::string& strModelRef, bool bForce, bool& bSuccess)
{
	auto xBase = std::filesystem::path(Zenith_AssetRegistry::ResolvePath(strModelRef)); xBase.replace_extension();
	bSuccess = false;
	std::error_code ec;
	const bool bBase = std::filesystem::exists(xBase.string() + ".glb", ec);
	if (ec) return true;
	const bool bShiny = std::filesystem::exists(xBase.string() + "_shiny.glb", ec);
	if (ec) return true;
	if (!bBase && !bShiny) return false;
	if (!bForce && Warm(xBase)) { bSuccess = true; return true; }
	struct Markers
	{
		std::filesystem::path m_xBase; bool& m_bSuccess;
		~Markers() { if (!m_bSuccess) for (const char* suffix : {".zmodel", "_shiny.zmodel", ".authored-v4"}) { std::error_code ec; std::filesystem::remove(m_xBase.string() + suffix, ec); } }
	} xMarkers{xBase, bSuccess};
	for (const char* suffix : {".zmodel", "_shiny.zmodel", ".authored-v4"})
	{
		std::filesystem::remove(xBase.string() + suffix, ec);
		if (ec) return true;
	}
	if (!bBase || !bShiny) return true;
	ZM_GenMesh xTemplateMesh; ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(eId), xTemplateMesh);
	Zenith_SkeletonAsset xTopology;
	for (u_int b = 0; b < xTemplateMesh.GetNumBones(); ++b)
	{
		const auto& bone = xTemplateMesh.m_xBones.Get(b);
		xTopology.AddBone(bone.m_szName, bone.m_iParent, bone.m_xLocalPos, bone.m_xLocalRot, bone.m_xLocalScale);
	}
	xTopology.ComputeBindPoseMatrices();
	const auto strRig = Zenith_AssetRegistry::NormalizeAssetPath(xBase.string() + ".zskel");
	const auto strMesh = Zenith_AssetRegistry::NormalizeAssetPath(xBase.string() + ".zmesh");
	Zenith_SkeletonAsset xRig; Zenith_MeshAsset xMesh, xShinyMesh;
	Zenith_Tools_GlbImport::GlbImportResult xImport;
	if (!Zenith_Tools_GlbImport::LoadGlbMeshWithSkeletonTopology(xBase.string() + ".glb", xTopology, strRig, xRig, xMesh, xImport) ||
		!Zenith_Tools_GlbImport::LoadGlbMeshForSharedSkeleton(xBase.string() + "_shiny.glb", xRig, strRig, xShinyMesh, xImport)) return true;
	if (!Same(xMesh.m_xPositions, xShinyMesh.m_xPositions) || !Same(xMesh.m_xNormals, xShinyMesh.m_xNormals) ||
		!Same(xMesh.m_xUVs, xShinyMesh.m_xUVs) || !Same(xMesh.m_xIndices, xShinyMesh.m_xIndices) ||
		!Same(xMesh.m_xBoneIndices, xShinyMesh.m_xBoneIndices) || !Same(xMesh.m_xBoneWeights, xShinyMesh.m_xBoneWeights)) return true;
	xRig.Export((xBase.string() + ".zskel.tmp").c_str()); if (!Publish(xBase.string() + ".zskel")) return true;
	xMesh.GenerateTangents(); xMesh.ComputeBounds(); xMesh.Export((xBase.string() + ".zmesh.tmp").c_str());
	if (!Publish(xBase.string() + ".zmesh")) return true;
	Zenith_Vector<std::string> axMaterials[2]; u_int textures = 0;
	const auto xMaterialStart = std::filesystem::file_time_type::clock::now();
	if (!Zenith_Tools_GlbImport::ExportGlbMaterials(xBase.string() + ".glb", xBase.string(), axMaterials[0], textures) ||
		!Zenith_Tools_GlbImport::ExportGlbMaterials(xBase.string() + "_shiny.glb", xBase.string() + "_shiny", axMaterials[1], textures)) return true;
	if (axMaterials[0].GetSize() != axMaterials[1].GetSize()) return true;
	for (u_int v = 0; v < 2; ++v) for (u_int m = 0; m < axMaterials[v].GetSize(); ++m)
	{
		const auto prefix = xBase.string() + (v ? "_shiny" : "") + (m ? "_mat" + std::to_string(m) : "");
		for (const char* suffix : {".zmtrl", "_albedo.ztxtr", "_normal.ztxtr", "_rm.ztxtr", "_ao.ztxtr"})
		{
			const auto path = prefix + suffix;
			if (Signature(path).empty() || std::filesystem::last_write_time(path, ec) < xMaterialStart || ec) return true;
		}
	}
	Zenith_Vector<std::string> xClips;
	for (u_int c = 0; c < ZM_ANIM_CLIP_COUNT; ++c)
	{
		Flux_AnimationClip clip; ZM_BuildCreatureClipForSpecies(eId, static_cast<ZM_ANIM_CLIP>(c), clip);
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator it(clip.GetBoneChannels());
		for (; !it.Done(); it.Next())
			if (it.GetValue().HasPositionKeyframes() || it.GetValue().HasScaleKeyframes() || !xRig.HasBone(it.GetValue().GetBoneName())) return true;
		if ((eId == ZM_SPECIES_FERNFAWN || eId == ZM_SPECIES_THICKETBUCK || eId == ZM_SPECIES_SYLVASTAG) &&
			c == ZM_ANIM_CLIP_FAINT && !ZM_BuildGroveDeerFaintForArtistRig(eId, xRig, xMesh, clip)) return true;
		if ((eId == ZM_SPECIES_RUBBLET || eId == ZM_SPECIES_CAIRNGO || eId == ZM_SPECIES_MONOLODE) &&
			!ZM_FitStoneCreatureClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		if ((eId == ZM_SPECIES_SPORELING || eId == ZM_SPECIES_DUSKCAP || eId == ZM_SPECIES_FUNGROVE) &&
			!ZM_FitFungalCreatureClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		if ((eId == ZM_SPECIES_PUFFSEED || eId == ZM_SPECIES_DANDELIFT || eId == ZM_SPECIES_ZEPHYRBLOOM) &&
			!ZM_FitFungalCreatureClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip, true)) return true;
		if ((eId == ZM_SPECIES_FINLET || eId == ZM_SPECIES_MARLANCE || eId == ZM_SPECIES_TIDESABRE ||
			eId == ZM_SPECIES_MINNET || eId == ZM_SPECIES_SHOALFIN || eId == ZM_SPECIES_TORRENTFIN) &&
			!ZM_FitLancefishClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		if ((eId == ZM_SPECIES_PRISMITE || eId == ZM_SPECIES_GEOBORER || eId == ZM_SPECIES_CRYSTALLISK) &&
			!ZM_FitMineralBeetleClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		if ((eId == ZM_SPECIES_KINDLET || eId == ZM_SPECIES_SCORCHEL || eId == ZM_SPECIES_PYROCLAST) &&
			!ZM_FitEmberBipedClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		if ((eId == ZM_SPECIES_PIPWIT || eId == ZM_SPECIES_TRILLARK || eId == ZM_SPECIES_STRATAVIS) &&
			!ZM_FitLarkCreatureClip(xRig, xMesh, static_cast<ZM_ANIM_CLIP>(c), clip)) return true;
		clip.GetMetadata().m_strSkeletonPath = strRig;
		clip.GetMetadata().m_strPreviewModelPath = Zenith_AssetRegistry::NormalizeAssetPath(xBase.string() + ".zmodel");
		const auto path = xBase.string() + "_" + ZM_CreatureClipName(static_cast<ZM_ANIM_CLIP>(c)) + ".zanim";
		clip.Export(path + ".tmp"); if (!Publish(path)) return true;
		xClips.PushBack(Zenith_AssetRegistry::NormalizeAssetPath(path));
	}
	// Retain the canonical first-atlas alias used by the legacy family manifest.
	// The models use all imported material sections; this copy changes no binding.
	std::filesystem::copy_file(xBase.string() + "_shiny_albedo.ztxtr", xBase.string() + "_shiny.ztxtr.tmp", std::filesystem::copy_options::overwrite_existing, ec);
	if (ec || !Publish(xBase.string() + "_shiny.ztxtr")) return true;
	if (Signature(xBase.string() + "_icon.ztxtr").empty())
	{
		const auto recipe = ZM_ResolveCreatureRecipe(eId);
		const auto icon = ZM_BuildCreatureIcon(recipe, ZM_BuildCreatureAlbedo(recipe));
		if (!ZM_SynthBakeIconBC1(icon, (xBase.string() + "_icon.ztxtr.tmp").c_str()) || !Publish(xBase.string() + "_icon.ztxtr")) return true;
	}
	for (u_int v = 0; v < 2; ++v)
	{
		auto handle = Zenith_AssetRegistry::Create<Zenith_ModelAsset>(); auto* model = handle.GetDirect();
		model->SetName(std::string(ZM_GetSpeciesName(eId)) + (v ? "_shiny" : "")); model->SetSkeletonPath(strRig);
		model->AddMeshByPath(strMesh, axMaterials[v]);
		for (u_int c = 0; c < xClips.GetSize(); ++c) model->AddAnimationPath(xClips.Get(c));
		model->Export((xBase.string() + (v ? "_shiny.zmodel.tmp" : ".zmodel.tmp")).c_str());
	}
	if (!Publish(xBase.string() + ".zmodel") || !Publish(xBase.string() + "_shiny.zmodel")) return true;
	std::ofstream receipt(xBase.string() + ".authored-v4.tmp", std::ios::trunc);
	receipt << "Zenith authored creature 4\n" << Signature(xBase.string() + ".glb") << '\n' << Signature(xBase.string() + "_shiny.glb") << '\n';
	for (const auto& entry : std::filesystem::directory_iterator(xBase.parent_path(), ec))
		if (entry.is_regular_file(ec) && OutputExtension(entry.path()) && entry.path().filename().string().rfind(xBase.filename().string(), 0) == 0)
			receipt << entry.path().filename().string() << '|' << Signature(entry.path()) << '\n';
	receipt << "END\n";
	receipt.flush(); const bool bWritten = receipt.good() && !ec; receipt.close();
	if (!bWritten || !Publish(xBase.string() + ".authored-v4")) return true;
	bSuccess = Warm(xBase); return true;
}
}
#endif
