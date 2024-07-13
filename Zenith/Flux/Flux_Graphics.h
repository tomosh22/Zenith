#pragma once
#include "Flux/Flux.h"

class Flux_Graphics
{
public:
	Flux_Graphics() = delete;
	~Flux_Graphics() = delete;

	static void Initialise();

	static struct Flux_TargetSetup s_xFinalRenderTarget;

	static Flux_Sampler s_xDefaultSampler;

	static class Flux_MeshGeometry s_xQuadMesh;
	static class Flux_VertexBuffer s_xQuadVertexBuffer;
	static class Flux_IndexBuffer s_xQuadIndexBuffer;
};