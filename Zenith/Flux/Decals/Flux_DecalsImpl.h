#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Zenith_TextureAsset;
class Flux_RenderGraph;

// Phase 9: state + behaviour for Decals subsystem.
class Flux_DecalsImpl
{
public:
	Flux_DecalsImpl() = default;
	~Flux_DecalsImpl() = default;

	Flux_DecalsImpl(const Flux_DecalsImpl&) = delete;
	Flux_DecalsImpl& operator=(const Flux_DecalsImpl&) = delete;

	void Initialise();
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void SpawnDecal(const Zenith_Maths::Vector3& xPosition,
	                const Zenith_Maths::Vector3& xNormal,
	                Zenith_TextureAsset*         pxTexture,
	                float                        fSize,
	                float                        fLifetime);

	bool IsInitialised() const { return m_bInitialised; }

	static constexpr u_int uMAX_DECALS = 64;

#ifdef ZENITH_TESTING
	struct TestSlotView
	{
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xNormal;
		float                 m_fRemainingLifetime;
		bool                  m_bActive;
	};
	u_int        GetActiveCountForTest();
	TestSlotView GetSlotForTest(u_int uSlotIndex);
	void         ResetForTest();
#endif

	bool                        m_bInitialised      = false;
	u_int                       m_uNextSlot         = 0;
	u_int                       m_uActiveDecalCount = 0;

	Flux_RenderGraph*           m_pxGraph = nullptr;
	Flux_TransientHandle        m_xNormalsCopyHandle;
	Flux_PassHandle             m_xNormalsCopyPass;
	Flux_PassHandle             m_xApplyPass;

	Flux_Shader                 m_xNormalsCopyShader;
	Flux_Shader                 m_xApplyShader;
	Flux_Pipeline               m_xNormalsCopyPipeline;
	Flux_Pipeline               m_xApplyPipeline;

	Flux_DynamicReadWriteBuffer m_xDecalBuffer;
	Flux_IndexBuffer            m_xDecalIndexBuffer;
};
