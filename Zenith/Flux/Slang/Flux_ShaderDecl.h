#pragma once

// ---------------------------------------------------------------------------
// Flux_ShaderDecl
//
// Static, backend-independent description of ONE shader program. A decl is
// declared next to the render feature that owns it, in
// Flux/<Feature>/Flux_<Feature>_Shaders.h, and listed in that header's
// apxALL[] array. Ownership is STRUCTURAL: a program belongs to exactly the
// feature whose apxALL lists it — there is no central registry row and no
// subsystem->feature ownership table. Flux_ShaderCatalog gathers every
// feature's apxALL into the flat program index that FluxCompiler walks and
// that Flux_FeatureRegistry validates parity against.
//
// Post-W1.3 this is a pure-data, dependency-free leaf (only const char*):
// include it from a per-feature _Shaders.h with zero risk of a layering cycle.
// ---------------------------------------------------------------------------
struct Flux_ShaderDecl
{
	const char* m_szName          = nullptr;  // friendly/debug name, unique across the catalog (e.g. "TexturedQuad")
	const char* m_szModuleName    = nullptr;  // ISession::loadModule path, no extension, '/'-separated (e.g. "Quads/Flux_TexturedQuad")
	const char* m_szVertexEntry   = nullptr;  // null unless this is a graphics program
	const char* m_szFragmentEntry = nullptr;  // null unless this is a graphics program
	const char* m_szComputeEntry  = nullptr;  // null unless this is a compute program
	const char* m_szTargetProfile = "spirv_1_3";
	const char* m_szSubsystem     = nullptr;  // generated-header grouping ONLY (decoupled from the owning feature)
	const char* m_szArtifactRoot  = nullptr;  // null => SHADER_SOURCE_ROOT; set for per-game decls (W2)
};
