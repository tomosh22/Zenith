#pragma once

#include <string>

// ============================================================================
// Zenith_Tools_GlbImport -- the .glb import path, for binary glTF that the
// Assimp route cannot read.
//
// ★★ THIS IS NOT A SECOND MESH IMPORTER FOR ITS OWN SAKE. Zenith_Tools_MeshExport
// already walks the assets tree and hands .gltf / .fbx / .obj to Assimp, and that
// stays the route for anything Assimp can open. What it cannot open is a file
// carrying EXT_meshopt_compression -- which is what gltfpack, and therefore most
// generated-asset tooling, emits by default. Assimp rejects the whole file on the
// extension's fallback buffer before reading a vertex, so there is no "import it
// worse" fallback to lean on. See Zenith_Tools_MeshoptDecode.h.
//
// ★ THE OUTPUT SHAPE IS THE POINT. Import writes the SAME per-model file set a
// procedural generator writes, using the same basenames:
//
//     <Name>.zmesh  <Name>_albedo.ztxtr  <Name>_normal.ztxtr
//     <Name>_rm.ztxtr  <Name>_ao.ztxtr  <Name>.zmtrl  <Name>.zmodel
//
// so dropping <Name>.glb into an existing asset folder REPLACES that model for
// every consumer without a single call site changing. Nothing needs to know
// whether a model was generated or imported. Zenithmon's ArtBrief.md calls that
// the condition for a hand-made asset to count as delivered, and it is also what
// keeps the generated fallback meaningful: delete the .glb, and the next boot
// regenerates the model exactly as before.
//
// ★ CHANNEL CONVENTIONS ARE CARRIED ACROSS, NOT ASSUMED. glTF stores
// roughness in G and metallic in B of one texture, which is precisely what
// Flux's SampleRoughnessMetallic reads, so that map is passed through with no
// swizzle. Base colour is already sRGB-encoded in its source file and goes to
// BC1 unchanged (BC1 has no sRGB variant -- the engine's convention is encoded
// bytes sampled as UNORM). Normal maps go to BC5, which keeps R and G and lets
// the shader rebuild Z. An absent occlusion map is written as neutral white
// rather than skipped, because a partial bundle reads to a generator as "not
// baked yet" and gets overwritten.
// ============================================================================
namespace Zenith_Tools_GlbImport
{
	// What one import produced. The bounds are in the .glb's own units after any
	// node transform is baked in -- the caller usually wants them, since a
	// hand-made model's real size is not knowable until it has been read.
	struct GlbImportResult
	{
		bool  m_bSuccess = false;
		float m_afBoundsMin[3] = { 0.0f, 0.0f, 0.0f };
		float m_afBoundsMax[3] = { 0.0f, 0.0f, 0.0f };
		u_int m_uNumVerts = 0;
		u_int m_uNumIndices = 0;
		u_int m_uNumSubmeshes = 0;
		u_int m_uNumTexturesWritten = 0;
	};

	// Import szGlbPath, writing the bundle beside it. Every failure logs its own
	// reason and leaves m_bSuccess false; a half-written bundle is never reported
	// as a success.
	GlbImportResult ImportGlbFile(const std::string& strGlbPath);

	// Import every .glb under a directory tree. Missing directory is not an error
	// (the assets tree is gitignored, so CI can legitimately have none).
	void ImportGlbsInDirectory(const std::string& strDirectory);
}
