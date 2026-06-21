#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

#include <string>

struct Flux_SlangProgramDesc;
class Flux_FeatureRegistry; // ValidateFeatureParity argument

// ---------------------------------------------------------------------------
// Flux_ShaderCatalog
//
// The flat program index, DERIVED from the per-feature apxALL arrays (one decl
// per program, owned by exactly one feature) plus a small explicit
// apxUnownedEnginePrograms[] for engine programs no engine feature rebuilds.
// FluxCompiler walks this to compile every program; codegen reads each decl's
// fields directly. Replaces the former Flux_ShaderRegistry + s_axRegistry[].
//
// Ownership/parity is asserted by Flux_ShaderCatalog::ValidateFeatureParity
// (added in W1.2) against Flux_FeatureRegistry; Validate() replaces the old
// static_assert on the row table.
// ---------------------------------------------------------------------------
namespace Flux_ShaderCatalog
{
	u_int GetProgramCount();
	const Flux_ShaderDecl& GetProgramByIndex(u_int uIndex);

	// Decl-keyed API — the post-flip surface (W1.3 onward). Callers hold a
	// const Flux_ShaderDecl& straight from their feature's _Shaders.h.
	void DescribeProgram(const Flux_ShaderDecl& xDecl, Flux_SlangProgramDesc& xDescOut);

	// On-disk artifact stem: `<module>.<entry>` for graphics stages, `<module>`
	// for compute. Caller appends `.spv` / `.spv.refl`.
	std::string GetVertexArtifactStem(const Flux_ShaderDecl& xDecl);
	std::string GetFragmentArtifactStem(const Flux_ShaderDecl& xDecl);
	std::string GetComputeArtifactStem(const Flux_ShaderDecl& xDecl);

	// Structural integrity of every catalog decl: required fields present, a
	// valid graphics(vertex&&fragment, no compute) XOR compute(compute only)
	// shape, no duplicate name, no duplicate module+entry stem. Replaces the
	// old kRegistryCount==COUNT static_assert. FluxCompiler aborts on failure;
	// runtime asserts. Returns false (with strErrOut set) on the first problem.
	bool Validate(std::string& strErrOut);

	// Cross-check the catalog against the registered features: the catalog decl set
	// must EXACTLY equal (union of every registered engine-feature's apxALL) plus
	// the explicit apxUnownedEnginePrograms — every catalog decl owned by exactly
	// one feature or explicitly unowned; nothing in both; none missing. This is the
	// guard that makes "new feature = 2 central lines" safe: forget the catalog
	// include OR the RegisterFeature line and this fails loudly. Backend-independent
	// (called right after RegisterDefaultFeatures in engine boot; also by
	// FluxCompiler against a CreateDefaultSnapshotForValidation snapshot). Returns
	// false (strErrOut set) on the first mismatch.
	bool ValidateFeatureParity(const Flux_FeatureRegistry& xRegistry, std::string& strErrOut);

	// True iff this TU was compiled with ZENITH_TOOLS — i.e. the catalog holds the
	// FULL shader set (incl. the tools-only Gizmos/MaterialPreview programs). The
	// canonical FluxCompiler (built Tools=True) uses this to gate codegen + the
	// destructive artifact prune; a Tools=False FluxCompiler compiles its reduced
	// set but must NOT regenerate headers or delete tools-only artifacts.
	bool IsCanonicalToolsBuild();
}
