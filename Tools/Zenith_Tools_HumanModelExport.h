#pragma once

#include <string>

// ============================================================================
// Zenith_Tools_HumanModelExport -- the ONE call site that turns an artist's
// humanoid .glb into a skinned model bundle on the shared StickFigure rig.
//
// Called from GenerateTestAssets(), immediately AFTER GenerateStickFigureAssets()
// -- because that is what writes the .zskel this binds to, and on a cold tree it
// does not exist before then.
//
// ★ THE .zmodel IS THE COMMIT MARKER. A multi-file bundle cannot be made atomic
// by a sequence of renames, so it is not pretended to be: the mesh, the textures
// and the material are PREREQUISITES, written first, and the .zmodel lands last
// via a rename. Until it does, the bundle does not exist as far as any consumer
// is concerned -- and on any failure it is deleted, so a half-built bundle can
// never be picked up by an existence-only check downstream.
//
// The whole in-memory result is validated BEFORE anything is published:
// skinning present, weights normalised, max bone index in range, bounds
// non-degenerate. That is the real protection; the marker is what makes a
// failure invisible rather than merely logged.
// ============================================================================
namespace Zenith_Tools_HumanModelExport
{
	struct ExportResult
	{
		bool  m_bAttempted = false;   // false when there was simply no source .glb
		bool  m_bSuccess = false;
		u_int m_uNumVerts = 0u;
		u_int m_uNumBones = 0u;
		std::string m_strFailureStage;
	};

	// Bind every declared humanoid under ENGINE_ASSETS_DIR. A missing source is
	// NOT a failure -- the asset tree is gitignored, so a fresh clone legitimately
	// has none, exactly as ImportGlbsInDirectory treats a missing directory.
	void ExportBoundHumanModels();

	// True for a .glb this exporter owns -- i.e. one under Meshes/Humans. The
	// generic .glb walk asks this before importing anything, so a humanoid is
	// never bound twice in one boot. See the definition for the incident.
	bool IsHumanoidSourcePath(const std::string& strPath);

	// The single model, for tests and for the one call above.
	ExportResult ExportBoundHumanModel(const std::string& strGlbPath);

	// engine:-prefixed ref for the bound male, or an empty string when the bundle
	// is absent. Games ask for THIS rather than hardcoding a path.
	std::string GetMaleModelRef();
}
