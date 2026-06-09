#pragma once
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_BackendTypes.h"

#include "Profiling/Zenith_Profiling.h"

// Forward declarations
struct Flux_Texture;
struct Flux_Buffer;
struct Flux_RenderAttachment;

// Single source of truth for all command types.
// To add a new command: add one X() entry here and define the command class below.
// The enum and IterateCommands dispatch are auto-generated from this list.
#define FLUX_COMMAND_LIST(X) \
	X(SET_PIPELINE,                Flux_CommandSetPipeline) \
	X(SET_VERTEX_BUFFER,           Flux_CommandSetVertexBuffer) \
	X(SET_INDEX_BUFFER,            Flux_CommandSetIndexBuffer) \
	X(BIND_SRV,                    Flux_CommandBindSRV) \
	X(BIND_SRV_BUFFER,             Flux_CommandBindSRV_Buffer) \
	X(BIND_UAV_TEXTURE,            Flux_CommandBindUAV_Texture) \
	X(BIND_UAV_BUFFER,             Flux_CommandBindUAV_Buffer) \
	X(BIND_CBV,                    Flux_CommandBindCBV) \
	X(USE_UNBOUNDED_TEXTURES,      Flux_CommandUseUnboundedTextures) \
	X(BIND_DRAW_CONSTANTS,         Flux_CommandBindDrawConstants) \
	X(DRAW,                        Flux_CommandDraw) \
	X(DRAW_INDEXED,                Flux_CommandDrawIndexed) \
	X(DRAW_INDEXED_INDIRECT,       Flux_CommandDrawIndexedIndirect) \
	X(DRAW_INDEXED_INDIRECT_COUNT, Flux_CommandDrawIndexedIndirectCount) \
	X(BIND_COMPUTE_PIPELINE,       Flux_CommandBindComputePipeline) \
	X(DISPATCH,                    Flux_CommandDispatch) \
	X(SET_DEPTH_BIAS,              Flux_CommandSetDepthBias) \
	X(RENDER_IMGUI,                Flux_CommandRenderImGui)

enum Flux_CommandType
{
	#define FLUX_X_ENUM(Name, Class) FLUX_COMMANDTYPE__##Name,
	FLUX_COMMAND_LIST(FLUX_X_ENUM)
	#undef FLUX_X_ENUM
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

class Flux_CommandBindDrawConstants
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_DRAW_CONSTANTS;

	Flux_CommandBindDrawConstants(const void* pData, u_int uSize, const Flux_BindingSlot& xSlot = {})
	: m_uSize(uSize)
	, m_xSlot(xSlot)
	{
		Zenith_Assert(uSize <= uMAX_SIZE, "Scratch UBO payload too big (%u > %u)", uSize, uMAX_SIZE);
		// Runtime guard for release builds - prevent buffer overflow
		if (uSize > uMAX_SIZE)
		{
			m_uSize = 0;
			return;
		}
		memcpy(m_acData, pData, uSize);
	}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		// TODO: elide second memcpy by having AddCommand record directly into the
		// worker's scratch-partition slot. Requires verifying pass-to-worker pinning
		// holds across record -> execute.
		pxCmdBuf->BindDrawConstants(m_acData, m_uSize, m_xSlot);
	}
private:
	// Inline storage cap for deferred command payload.
	// NOT related to Vulkan's maxPushConstantsSize — this path binds a dynamic UBO, not push constants.
	// Sized to comfortably fit DP's per-frame fog CBV (60 holes × 16 B
	// + 32 B header ≈ 1 KiB) plus headroom for future per-pass payloads.
	static constexpr u_int uMAX_SIZE = 2048;

	u_int8 m_acData[uMAX_SIZE];
	u_int m_uSize;
	Flux_BindingSlot m_xSlot;
};

class Flux_CommandSetPipeline
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_PIPELINE;

	Flux_CommandSetPipeline(Flux_Pipeline* pxPipeline);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->SetPipeline(m_pxPipeline);
	}
	Flux_Pipeline* m_pxPipeline;
};

// Command list entry for SetVertexBuffer. Accepts either a static or
// dynamic vertex buffer via overload. The two constructors erase the source
// type into a (payload, resolver) pair so operator() can execute without a
// runtime branch; the resolver defers GetBuffer() to execution time, which
// matters for Flux_DynamicVertexBuffer (GetBuffer() returns the current
// frame's buffer, and the frame index may have advanced between command
// recording and command execution).
class Flux_CommandSetVertexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_VERTEX_BUFFER;

	Flux_CommandSetVertexBuffer(const Flux_VertexBuffer* const pxVertexBuffer, const u_int uBindPoint = 0);
	Flux_CommandSetVertexBuffer(const Flux_DynamicVertexBuffer* const pxDynamicVertexBuffer, const u_int uBindPoint = 0);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		m_pfnDispatch(pxCmdBuf, m_pvSource, m_uBindPoint);
	}

	using DispatchFn = void(*)(Flux_CommandBuffer*, const void*, u_int);

	const void* m_pvSource;
	DispatchFn m_pfnDispatch;
	const u_int m_uBindPoint;
};

class Flux_CommandSetIndexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_INDEX_BUFFER;

	Flux_CommandSetIndexBuffer(const Flux_IndexBuffer* const pxIndexBuffer);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->SetIndexBuffer(*m_pxIndexBuffer);
	}

	const Flux_IndexBuffer* m_pxIndexBuffer;
};
class Flux_CommandBindCBV
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_CBV;

	Flux_CommandBindCBV(const Flux_ConstantBufferView* pxCBV, const Flux_BindingSlot& xSlot);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindCBV(m_pxCBV, m_xSlot);
	}

	const Flux_ConstantBufferView* m_pxCBV;
	Flux_BindingSlot m_xSlot;
};

class Flux_CommandBindSRV
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_SRV;

	Flux_CommandBindSRV(const Flux_ShaderResourceView* const pxSRV, const Flux_BindingSlot& xSlot, Flux_Sampler* pxSampler = nullptr);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindSRV(m_pxSRV, m_xSlot, m_pxSampler);
	}
	const Flux_ShaderResourceView* m_pxSRV;
	Flux_BindingSlot m_xSlot;
	Flux_Sampler* m_pxSampler;
};

class Flux_CommandBindUAV_Texture
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_UAV_TEXTURE;

	Flux_CommandBindUAV_Texture(const Flux_UnorderedAccessView_Texture* const pxUAV, const Flux_BindingSlot& xSlot);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindUAV_Texture(m_pxUAV, m_xSlot);
	}
	const Flux_UnorderedAccessView_Texture* m_pxUAV;
	Flux_BindingSlot m_xSlot;
};

class Flux_CommandBindUAV_Buffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_UAV_BUFFER;

	Flux_CommandBindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* const pxUAV, const Flux_BindingSlot& xSlot);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindUAV_Buffer(m_pxUAV, m_xSlot);
	}
	const Flux_UnorderedAccessView_Buffer* m_pxUAV;
	Flux_BindingSlot m_xSlot;
};

// Bind a read-only structured-buffer SSBO (StructuredBuffer<T> in Slang). The
// underlying Vulkan descriptor is the same eStorageBuffer write that
// Flux_CommandBindUAV_Buffer emits — the distinct command type lets the
// render-graph access path treat the bind as a read so a missing
// RESOURCE_ACCESS_READ_BUFFER_SRV declaration trips the bind-time assertion
// instead of silently passing as a write. The view is stored by value so the
// command is self-contained even if the source Flux_ReadWriteBuffer's GetSRV()
// reference is rebound between record and execute.
class Flux_CommandBindSRV_Buffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_SRV_BUFFER;

	Flux_CommandBindSRV_Buffer(const Flux_ShaderResourceView_Buffer& xSRV, const Flux_BindingSlot& xSlot);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindSRV_Buffer(m_xSRV, m_xSlot);
	}
	Flux_ShaderResourceView_Buffer m_xSRV;
	Flux_BindingSlot               m_xSlot;
};

class Flux_CommandDraw
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DRAW;

	Flux_CommandDraw(u_int uNumVerts) : m_uNumVerts(uNumVerts) {}

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

class Flux_CommandDrawIndexedIndirect
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DRAW_INDEXED_INDIRECT;

	Flux_CommandDrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, u_int uDrawCount, u_int uOffset = 0, u_int uStride = 20);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->DrawIndexedIndirect(m_pxIndirectBuffer, m_uDrawCount, m_uOffset, m_uStride);
	}

	const Flux_IndirectBuffer* m_pxIndirectBuffer;
	u_int m_uDrawCount;
	u_int m_uOffset;
	u_int m_uStride;
};

class Flux_CommandDrawIndexedIndirectCount
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__DRAW_INDEXED_INDIRECT_COUNT;

	Flux_CommandDrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, u_int uMaxDrawCount, u_int uIndirectOffset = 0, u_int uCountOffset = 0, u_int uStride = 20);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->DrawIndexedIndirectCount(m_pxIndirectBuffer, m_pxCountBuffer, m_uMaxDrawCount, m_uIndirectOffset, m_uCountOffset, m_uStride);
	}

	const Flux_IndirectBuffer* m_pxIndirectBuffer;
	const Flux_IndirectBuffer* m_pxCountBuffer;
	u_int m_uMaxDrawCount;
	u_int m_uIndirectOffset;
	u_int m_uCountOffset;
	u_int m_uStride;
};

// ========== COMPUTE COMMANDS ==========

class Flux_CommandBindComputePipeline
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_COMPUTE_PIPELINE;

	Flux_CommandBindComputePipeline(Flux_Pipeline* pxPipeline);
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->BindComputePipeline(m_pxPipeline);
	}
	Flux_Pipeline* m_pxPipeline;
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

// Synchronisation is the render graph's responsibility — declare reads and
// writes via Flux_RenderGraph::Read / Write and the graph emits the
// necessary transitions outside the pass's command list. There is no
// supported way for a pass to emit a barrier from inside its own recording
// (it wouldn't be visible to the graph's tracker and would cause sync drift).

class Flux_CommandSetDepthBias
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_DEPTH_BIAS;

	Flux_CommandSetDepthBias(float fConstant, float fSlope, float fClamp = 0.0f)
		: m_fConstant(fConstant), m_fSlope(fSlope), m_fClamp(fClamp) {}

	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->SetDepthBias(m_fConstant, m_fSlope, m_fClamp);
	}
private:
	float m_fConstant;
	float m_fSlope;
	float m_fClamp;
};

class Flux_CommandRenderImGui
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__RENDER_IMGUI;

	Flux_CommandRenderImGui() = default;

	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		pxCmdBuf->RenderImGui();
	}
};

class Flux_CommandList
{
public:
	// No default ctor — every command list must be constructed with a name and
	// (optionally) an initial capacity. Render-graph passes that need a member-
	// default-initialised command list should heap-own the instance instead.
	Flux_CommandList() = delete;
	Flux_CommandList(const Flux_CommandList&) = delete;
	Flux_CommandList& operator=(const Flux_CommandList&) = delete;
	Flux_CommandList(Flux_CommandList&&) = delete;
	Flux_CommandList& operator=(Flux_CommandList&&) = delete;

	// Explicit initial capacity lets heavy passes pre-size to avoid the
	// cold-start realloc spike. Name pointer must outlive the command list.
	explicit Flux_CommandList(const char* szName, u_int uInitialCapacity = uDEFAULT_INITIAL_SIZE)
	: m_pcData(static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uInitialCapacity)))
	, m_uCursor(0)
	, m_uCapacity(uInitialCapacity)
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
			u_int uNewCapacity = m_uCapacity * 2;
			u_int8* pNewData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(m_pcData, uNewCapacity));
			if (pNewData == nullptr)
			{
				Zenith_Assert(false, "Command list reallocation failed for %s - out of memory", m_szName);
				Zenith_Error(LOG_CATEGORY_RENDERER, "Command list reallocation failed for %s", m_szName);
				return; // Skip command rather than crash in release
			}
			m_pcData = pNewData;
			m_uCapacity = uNewCapacity;
		}

		*reinterpret_cast<Flux_CommandType*>(&m_pcData[m_uCursor]) = CommandType_T::m_eType;
		new (reinterpret_cast<CommandType_T*>(&m_pcData[m_uCursor + sizeof(Flux_CommandType)])) CommandType_T(std::forward<Args>(xArgs)...);
		m_uCursor += sizeof(CommandType_T) + sizeof(Flux_CommandType);
		m_uCommandCount++;
	}

	void IterateCommands(Flux_CommandBuffer* pxCmdBuf) const
	{
		g_xEngine.Profiling().BeginProfile(ZENITH_PROFILE_INDEX__FLUX_ITERATE_COMMANDS);
#ifdef ZENITH_FLUX_PROFILING
		pxCmdBuf->BeginDebugMarker(m_szName);
#endif
		u_int uCursor = 0;
		while(uCursor < m_uCursor)
		{
			const Flux_CommandType eCmd = *reinterpret_cast<Flux_CommandType*>(&m_pcData[uCursor]);
			switch(eCmd)
			{
				#define FLUX_X_DISPATCH(Name, Class) \
					case FLUX_COMMANDTYPE__##Name: \
						(*reinterpret_cast<Class*>(&m_pcData[uCursor + sizeof(Flux_CommandType)]))(pxCmdBuf); \
						uCursor += sizeof(Class) + sizeof(Flux_CommandType); \
						break;
				FLUX_COMMAND_LIST(FLUX_X_DISPATCH)
				#undef FLUX_X_DISPATCH

				default:
					Zenith_Assert(false, "Unhandled command");
			}
		}
#ifdef ZENITH_FLUX_PROFILING
		pxCmdBuf->EndDebugMarker();
#endif
		g_xEngine.Profiling().EndProfile(ZENITH_PROFILE_INDEX__FLUX_ITERATE_COMMANDS);
	}

	// Reset no longer carries clear state — that lives on the render-graph pass.
	void Reset()
	{
		m_uCursor = 0;
		m_uCommandCount = 0;
	}

	u_int GetCommandCount() const
	{
		return m_uCommandCount;
	}

	const char* GetName() const
	{
		return m_szName;
	}

private:
	static constexpr u_int uDEFAULT_INITIAL_SIZE = 4096;
	u_int8* m_pcData = nullptr;
	u_int m_uCursor = 0;
	u_int m_uCapacity = 0;
	u_int m_uCommandCount = 0;
	const char* m_szName = nullptr;
};
