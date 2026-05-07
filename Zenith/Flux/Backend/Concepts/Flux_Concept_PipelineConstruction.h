#pragma once

#include "Flux/Flux.h"

// Concept group: pipeline / shader / root signature construction.
//
// Four small concepts — each tied to one backend type. They are grouped in
// this header because they cooperate (Shader produces reflection;
// RootSigBuilder consumes reflection; PipelineBuilder consumes the spec
// that references the shader; ComputePipelineBuilder cooperates with all
// three).

// ---- Shader: initialise from a shader program ID (resolved against the
// Slang registry — graphics or compute is determined by the registry entry's
// populated entry-point fields), expose linked reflection. The const-
// correctness of GetReflection matters because the binder caches a
// reflection pointer.
template <typename T>
concept FluxBackendShader = requires(
	T& xShader,
	const T& xCShader,
	FluxShaderProgram eProgram)
{
	{ xShader.Initialise(eProgram)                                             } -> std::same_as<void>;
	{ xCShader.GetReflection()                                                 } -> std::same_as<const Flux_ShaderReflection&>;
};

// ---- Graphics pipeline builder: one entry point that compiles a complete
// graphics pipeline from a Flux_PipelineSpecification. The spec is a POD
// engine-typed struct; the backend reads it and produces the native PSO.
template <typename T>
concept FluxBackendPipelineBuilder = requires(
	Flux_Pipeline& xPipeline,
	const Flux_PipelineSpecification& xSpec)
{
	{ T::FromSpecification(xPipeline, xSpec)                                   } -> std::same_as<void>;
};

// ---- Compute pipeline builder: BuildFromShader is the engine-facing
// helper that hides the WithShader/WithLayout/Build chain plus the root-sig
// assignment. Engine code never touches the public m_xRootSig member or the
// vk::PipelineLayout inside Flux_RootSig — backends must ensure
// BuildFromShader populates the equivalent state.
template <typename T>
concept FluxBackendComputePipelineBuilder = requires(
	Flux_Pipeline& xPipeline,
	const Flux_Shader& xShader,
	const Flux_RootSig& xRootSig)
{
	{ T::BuildFromShader(xPipeline, xShader, xRootSig)                         } -> std::same_as<void>;
};

// ---- Root signature builder: one entry point that populates a Flux_RootSig
// from a shader's reflection. Engine code calls this after the shader has
// been loaded; the resulting root sig is then handed to ComputePipelineBuilder
// or referenced by a graphics PipelineSpecification.
template <typename T>
concept FluxBackendRootSigBuilder = requires(
	Flux_RootSig& xRootSig,
	const Flux_ShaderReflection& xReflection)
{
	{ T::FromReflection(xRootSig, xReflection)                                 } -> std::same_as<void>;
};
