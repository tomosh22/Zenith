#pragma once

#include "Collections/Zenith_Vector.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_CommandList.h"

struct Flux_SurfaceInfo
{
	TextureFormat m_eFormat = TEXTURE_FORMAT_NONE;
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	u_int m_uDepth = 0;
	u_int m_uNumMips = 0;
	u_int m_uNumLayers = 0;
	u_int m_uMemoryFlags = MEMORY_FLAGS__NONE;
};

// View structures for Direct3D-style resource views
struct Flux_ShaderResourceView {
	vk::ImageView m_xImageView = VK_NULL_HANDLE;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_UnorderedAccessView {
	vk::ImageView m_xImageView = VK_NULL_HANDLE;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderTargetView {
	vk::ImageView m_xImageView = VK_NULL_HANDLE;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_DepthStencilView {
	vk::ImageView m_xImageView = VK_NULL_HANDLE;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_ConstantBufferView {
	vk::DescriptorBufferInfo m_xBufferInfo;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderAttachment {
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

	// Views for different usage patterns
	Flux_ShaderResourceView m_pxSRV;  // For reading in shaders
	Flux_UnorderedAccessView m_pxUAV; // For compute shader read/write
	Flux_RenderTargetView m_pxRTV;     // For rendering (color attachments)
	Flux_DepthStencilView m_pxDSV;     // For depth/stencil attachments
};

struct Flux_Texture
{
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

	Flux_ShaderResourceView m_xSRV;
};

struct Flux_Buffer
{
	Flux_VRAMHandle m_xVRAMHandle;
	u_int64 m_ulSize = 0;
	
	Flux_ConstantBufferView m_xCBV;
};


class Flux_RenderAttachmentBuilder {
public:
	Flux_RenderAttachmentBuilder() = default;
	TextureFormat m_eFormat = TEXTURE_FORMAT_NONE;
	u_int m_uMemoryFlags = MEMORY_FLAGS__NONE;

	uint32_t m_uWidth;
	uint32_t m_uHeight;

	void BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName);
	void BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName);
};

struct Flux_TargetSetup {
	Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS];

	//#TO not owned by this
	Flux_RenderAttachment* m_pxDepthStencil = nullptr;

	std::string m_strName;

	void AssignDepthStencil(Flux_RenderAttachment* pxDS);

	const uint32_t GetNumColourAttachments();

	bool operator==(const Flux_TargetSetup& xOther) const
	{
		for (u_int u = 0; u < FLUX_MAX_TARGETS; u++)
		{
			if (m_axColourAttachments[u].m_xVRAMHandle.AsUInt() != xOther.m_axColourAttachments[u].m_xVRAMHandle.AsUInt())
			{
				return false;
			}
		}
		return m_pxDepthStencil == xOther.m_pxDepthStencil;
	}

	bool operator!=(const Flux_TargetSetup& xOther) const
	{
		return !(*this == xOther);
	}
};

class Flux
{
public:
	Flux() = delete;
	~Flux() = delete;
	static void EarlyInitialise();
	static void LateInitialise();

	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }

	static void SubmitCommandList(const Flux_CommandList* pxCmdList, const Flux_TargetSetup& xTargetSetup, RenderOrder eOrder)
	{
		static Zenith_Mutex ls_xMutex;
		ls_xMutex.Lock();
		s_xPendingCommandLists[eOrder].PushBack({pxCmdList, xTargetSetup});
		ls_xMutex.Unlock();
		}

	static void AddResChangeCallback(void(*pfnCallback)()) { s_xResChangeCallbacks.push_back(pfnCallback); }
	static void OnResChange();
private:
	friend class Flux_PlatformAPI;

	static uint32_t s_uFrameCounter;
	static std::vector<void(*)()> s_xResChangeCallbacks;
	static Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>> s_xPendingCommandLists[RENDER_ORDER_MAX];
};

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
	TextureFormat m_eDepthStencilFormat;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	struct Flux_TargetSetup* m_pxTargetSetup;
	LoadAction m_eColourLoadAction;
	StoreAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	StoreAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;

	bool m_bDepthBias = false;
	float m_fDepthBiasConstant = 0.0f;
	float m_fDepthBiasSlope = 0.0f;
	float m_fDepthBiasClamp = 0.0f;
};
