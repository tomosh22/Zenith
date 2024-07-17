#pragma once
#include "Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

class Flux_Graphics
{
public:
	Flux_Graphics() = delete;
	~Flux_Graphics() = delete;

	static void Initialise();
	static void InitialiseRenderTargets();

	static void UploadFrameConstants();

	static Flux_TargetSetup s_xFinalRenderTarget;

	static Flux_Sampler s_xDefaultSampler;

	static Flux_MeshGeometry s_xQuadMesh;
	static Flux_VertexBuffer s_xQuadVertexBuffer;
	static Flux_IndexBuffer s_xQuadIndexBuffer;

	static Flux_ConstantBuffer s_xFrameConstantsBuffer;
private:
	struct Zenith_FrameConstants
	{
		Zenith_Maths::Matrix4 m_xViewMat;
		Zenith_Maths::Matrix4 m_xProjMat;
		Zenith_Maths::Matrix4 m_xViewProjMat;
	};
};