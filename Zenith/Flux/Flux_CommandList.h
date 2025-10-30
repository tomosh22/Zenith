#pragma once
#include "Zenith_PlatformGraphics_Include.h"

enum Flux_CommandType
{
	FLUX_COMMANDTYPE__SET_PIPELINE,
	FLUX_COMMANDTYPE__SET_VERTEX_BUFFER,
	FLUX_COMMANDTYPE__SET_INDEX_BUFFER,

	FLUX_COMMANDTYPE__BEGIN_BIND,
	FLUX_COMMANDTYPE__BIND_TEXTURE,
	FLUX_COMMANDTYPE__BIND_BUFFER,

	FLUX_COMMANDTYPE__USE_UNBOUNDED_TEXTURES,

	FLUX_COMMANDTYPE__PUSH_CONSTANT,

	FLUX_COMMANDTYPE__DRAW,
	FLUX_COMMANDTYPE__DRAW_INDEXED,

	// Compute commands
	FLUX_COMMANDTYPE__BIND_COMPUTE_PIPELINE,
	FLUX_COMMANDTYPE__BIND_STORAGE_IMAGE,
	FLUX_COMMANDTYPE__DISPATCH,

	FLUX_COMMANDTYPE__COUNT,
};

template<typename T>
concept IsCommand = requires(T t, Flux_CommandBuffer* pxCmdBuf)
{
	{ t.m_eType } -> std::same_as<const Flux_CommandType&>;
	t(pxCmdBuf);
};

class Flux_CommandUseUnboundedTextures
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__USE_UNBOUNDED_TEXTURES;

	Flux_CommandUseUnboundedTextures(u_int uBindPoint)
	: m_uBindPoint(uBindPoint)
	{
	}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->UseBindlessTextures(m_uBindPoint);
	}
private:
	u_int m_uBindPoint;
};

class Flux_CommandPushConstant
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__PUSH_CONSTANT;

	Flux_CommandPushConstant(void* pData, u_int uSize)
	: m_uSize(uSize)
	{
		Zenith_Assert(uSize < uMAX_SIZE, "Push constant too big");
		memcpy(m_acData, pData, uSize);
	}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->PushConstant(m_acData, m_uSize);
	}
private:
	//#TO minimum that Vulkan requires for push constants
	static constexpr u_int uMAX_SIZE = 128;

	u_int8 m_acData[uMAX_SIZE];
	u_int m_uSize;
};

class Flux_CommandSetPipeline
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_PIPELINE;

	Flux_CommandSetPipeline(Flux_Pipeline* pxPipeline) : m_pxPipeline(pxPipeline) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->SetPipeline(m_pxPipeline);
	}
	Flux_Pipeline* m_pxPipeline;
};

//#TO_TODO: I might vomit... this needs changing to avoid the branch
class Flux_CommandSetVertexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_VERTEX_BUFFER;

	Flux_CommandSetVertexBuffer(const Flux_VertexBuffer* const pxVertexBuffer, const u_int uBindPoint = 0)
	: m_pxVertexBuffer(pxVertexBuffer)
	, m_pxDynamicVertexBuffer(nullptr)
	, m_uBindPoint(uBindPoint)
	{}
	Flux_CommandSetVertexBuffer(const Flux_DynamicVertexBuffer* const pxDynamicVertexBuffer, const u_int uBindPoint = 0)
	: m_pxVertexBuffer(nullptr)
	, m_pxDynamicVertexBuffer(pxDynamicVertexBuffer)
	, m_uBindPoint(uBindPoint)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		if(m_pxVertexBuffer)
		{
			pxCmdBuf->SetVertexBuffer(*m_pxVertexBuffer, m_uBindPoint);
		}
		else
		{
		Zenith_Assert(m_pxDynamicVertexBuffer, "Missing vertex buffer");
			pxCmdBuf->SetVertexBuffer(*m_pxDynamicVertexBuffer, m_uBindPoint);
		}
	}

	const Flux_VertexBuffer* m_pxVertexBuffer;
	const Flux_DynamicVertexBuffer* m_pxDynamicVertexBuffer;
	const u_int m_uBindPoint;
};

class Flux_CommandSetIndexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_INDEX_BUFFER;

	Flux_CommandSetIndexBuffer(const Flux_IndexBuffer* const pxIndexBuffer) : m_pxIndexBuffer(pxIndexBuffer) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->SetIndexBuffer(*m_pxIndexBuffer);
	}

	const Flux_IndexBuffer* m_pxIndexBuffer;
};
class Flux_CommandBeginBind
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BEGIN_BIND;

	Flux_CommandBeginBind(const u_int uIndex) : m_uIndex(uIndex) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BeginBind(m_uIndex);
	}
	u_int m_uIndex;
};
class Flux_CommandBindTexture
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_TEXTURE;

	Flux_CommandBindTexture(Flux_Texture* const pxTexture, const u_int uBindPoint, Flux_Sampler* pxSampler = nullptr)
		: m_pxTexture(pxTexture)
		, m_uBindPoint(uBindPoint)
		, m_pxSampler(pxSampler)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindTexture(m_pxTexture, m_uBindPoint, m_pxSampler);
	}
	Flux_Texture* m_pxTexture;
	const u_int m_uBindPoint;
	Flux_Sampler* m_pxSampler;
};
class Flux_CommandBindBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_BUFFER;

	Flux_CommandBindBuffer(Flux_Buffer* const pxBuffer, const u_int uBindPoint)
		: m_pxBuffer(pxBuffer)
		, m_uBindPoint(uBindPoint)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindBuffer(m_pxBuffer, m_uBindPoint);
	}
	Flux_Buffer* m_pxBuffer;
	const u_int m_uBindPoint;
};
class Flux_CommandDraw
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DRAW;

	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->Draw(m_uNumVerts);
	}

	u_int m_uNumVerts;
};
class Flux_CommandDrawIndexed
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DRAW_INDEXED;

	Flux_CommandDrawIndexed(const u_int uNumIndices, const u_int uNumInstances = 1, const u_int uVertexOffset = 0, const u_int uIndexOffset = 0, const u_int uInstanceOffset = 0)
		: m_uNumIndices(uNumIndices)
		, m_uNumInstances(uNumInstances)
		, m_uVertexOffset(uVertexOffset)
		, m_uIndexOffset(uIndexOffset)
		, m_uInstanceOffset(uInstanceOffset)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->DrawIndexed(m_uNumIndices, m_uNumInstances, m_uVertexOffset, m_uIndexOffset, m_uInstanceOffset);
	}

	u_int m_uNumIndices;
	u_int m_uNumInstances;
	u_int m_uVertexOffset;
	u_int m_uIndexOffset;
	u_int m_uInstanceOffset;
};

// ========== COMPUTE COMMANDS ==========

class Flux_CommandBindComputePipeline
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_COMPUTE_PIPELINE;

	Flux_CommandBindComputePipeline(Flux_Pipeline* pxPipeline) : m_pxPipeline(pxPipeline) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindComputePipeline(m_pxPipeline);
	}
	Flux_Pipeline* m_pxPipeline;
};

class Flux_CommandBindStorageImage
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_STORAGE_IMAGE;

	Flux_CommandBindStorageImage(Flux_Texture* pxTexture, u_int uBindPoint)
		: m_pxTexture(pxTexture)
		, m_uBindPoint(uBindPoint)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindStorageImage(m_pxTexture, m_uBindPoint);
	}
	Flux_Texture* m_pxTexture;
	u_int m_uBindPoint;
};

class Flux_CommandDispatch
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DISPATCH;

	Flux_CommandDispatch(u_int uGroupCountX, u_int uGroupCountY, u_int uGroupCountZ)
		: m_uGroupCountX(uGroupCountX)
		, m_uGroupCountY(uGroupCountY)
		, m_uGroupCountZ(uGroupCountZ)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->Dispatch(m_uGroupCountX, m_uGroupCountY, m_uGroupCountZ);
	}
	u_int m_uGroupCountX;
	u_int m_uGroupCountY;
	u_int m_uGroupCountZ;
};

class Flux_CommandList
{
public:
	Flux_CommandList() = delete;

	Flux_CommandList(const char* szName)
	: m_pcData(static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uINITIAL_SIZE)))
	, m_uCursor(0)
	, m_uCapacity(uINITIAL_SIZE)
	, m_szName(szName)
	{}

	~Flux_CommandList()
	{
		Zenith_MemoryManagement::Deallocate(m_pcData);
	}

	template<IsCommand CommandType_T, typename... Args>
	void AddCommand(Args... xArgs)
	{
		while (m_uCursor + sizeof(CommandType_T) + sizeof(Flux_CommandType) > m_uCapacity)
		{
			m_uCapacity *= 2;
			m_pcData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(m_pcData, m_uCapacity));
		}

		*reinterpret_cast<Flux_CommandType*>(&m_pcData[m_uCursor]) = CommandType_T::m_eType;
		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		new (reinterpret_cast<CommandType_T*>(&m_pcData[m_uCursor + sizeof(Flux_CommandType)])) CommandType_T(std::forward<Args>(xArgs)...);
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
		m_uCursor += sizeof(CommandType_T) + sizeof(Flux_CommandType);
	}

	void IterateCommands(Flux_CommandBuffer* pxCmdBuf) const
	{
		u_int uCursor = 0;
		while(uCursor < m_uCursor)
		{
			const Flux_CommandType eCmd = *reinterpret_cast<Flux_CommandType*>(&m_pcData[uCursor]);
			switch(eCmd)
			{
				#define HANDLE_COMMAND(Enum, Class)\
					case Enum:\
						(*reinterpret_cast<Class*>(&m_pcData[uCursor + sizeof(Flux_CommandType)]))(pxCmdBuf);\
						uCursor += sizeof(Class) + sizeof(Flux_CommandType);\
						break

				HANDLE_COMMAND(FLUX_COMMANDTYPE__SET_PIPELINE, Flux_CommandSetPipeline);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__SET_VERTEX_BUFFER, Flux_CommandSetVertexBuffer);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__SET_INDEX_BUFFER, Flux_CommandSetIndexBuffer);

				HANDLE_COMMAND(FLUX_COMMANDTYPE__BEGIN_BIND, Flux_CommandBeginBind);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__BIND_TEXTURE, Flux_CommandBindTexture);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__BIND_BUFFER, Flux_CommandBindBuffer);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__PUSH_CONSTANT, Flux_CommandPushConstant);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__USE_UNBOUNDED_TEXTURES, Flux_CommandUseUnboundedTextures);

				HANDLE_COMMAND(FLUX_COMMANDTYPE__DRAW, Flux_CommandDraw);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__DRAW_INDEXED, Flux_CommandDrawIndexed);
				
				HANDLE_COMMAND(FLUX_COMMANDTYPE__BIND_COMPUTE_PIPELINE, Flux_CommandBindComputePipeline);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__BIND_STORAGE_IMAGE, Flux_CommandBindStorageImage);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__DISPATCH, Flux_CommandDispatch);
				
				default:
					Zenith_Assert(false, "Unhandled command");
			}
		}
	}

	void Reset(const bool bClearTargets)
	{
		m_uCursor = 0;
		m_bClearTargets = bClearTargets;
	}

	bool RequiresClear() const
	{
		return m_bClearTargets;
	}

private:
	static constexpr u_int uINITIAL_SIZE = 32;
	u_int8* m_pcData;
	u_int m_uCursor = 0;
	u_int m_uCapacity = 0;
	const char* m_szName = nullptr;

	bool m_bClearTargets;
};
