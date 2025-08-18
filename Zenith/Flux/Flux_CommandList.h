#pragma once

using u_int = unsigned int;
using u_int8 = unsigned char;
class Flux_VertexBuffer;
class Flux_IndexBuffer;

#include "Zenith_PlatformGraphics_Include.h"

enum Flux_CommandType
{
	FLUX_COMMANDTYPE__SET_PIPELINE,
	FLUX_COMMANDTYPE__SET_VERTEX_BUFFER,
	FLUX_COMMANDTYPE__SET_INDEX_BUFFER,

	FLUX_COMMANDTYPE__BEGIN_BIND,
	FLUX_COMMANDTYPE__BIND_TEXTURE,
	FLUX_COMMANDTYPE__BIND_BUFFER,

	FLUX_COMMANDTYPE__DRAW,
	FLUX_COMMANDTYPE__DRAW_INDEXED,

	FLUX_COMMANDTYPE__COUNT,
};

class Flux_CommandSetPipeline
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_PIPELINE;

	Flux_CommandSetPipeline(Flux_Pipeline* pxPipeline) : m_pxPipeline(pxPipeline) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		printf("Set Pipeline: %x\n", m_pxPipeline);

		pxCmdBuf->SetPipeline(m_pxPipeline);
	}
	Flux_Pipeline* m_pxPipeline;

};

class Flux_CommandSetVertexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_VERTEX_BUFFER;

	Flux_CommandSetVertexBuffer(Flux_VertexBuffer* const pxVertexBuffer) : m_pxVertexBuffer(pxVertexBuffer) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		printf("Set Vertex Buffer: %x\n", m_pxVertexBuffer);

		pxCmdBuf->SetVertexBuffer(*m_pxVertexBuffer);
	}

	Flux_VertexBuffer* m_pxVertexBuffer;
};

class Flux_CommandSetIndexBuffer
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__SET_INDEX_BUFFER;

	Flux_CommandSetIndexBuffer(Flux_IndexBuffer* const pxIndexBuffer) : m_pxIndexBuffer(pxIndexBuffer) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		printf("Set Index Buffer: %x\n", m_pxIndexBuffer);

		pxCmdBuf->SetIndexBuffer(*m_pxIndexBuffer);
	}

	Flux_IndexBuffer* m_pxIndexBuffer;
};
class Flux_CommandBeginBind
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BEGIN_BIND;

	Flux_CommandBeginBind(const u_int uIndex) : m_uIndex(uIndex) {}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		printf("Begin Bind: %u\n", m_uIndex);

		pxCmdBuf->BeginBind(m_uIndex);
	}
	u_int m_uIndex;
};
class Flux_CommandBindTexture
{
public:
	static constexpr Flux_CommandType m_eType = FLUX_COMMANDTYPE__BIND_TEXTURE;

	Flux_CommandBindTexture(Flux_Texture* const pxTexture, const u_int uBindPoint)
		: m_pxTexture(pxTexture)
		, m_uBindPoint(uBindPoint)
	{}
	void operator()(Flux_CommandBuffer* pxCmdBuf)
	{
		printf("Bind Texture: %x\n", m_pxTexture);

		pxCmdBuf->BindTexture(m_pxTexture, m_uBindPoint);
	}
	Flux_Texture* m_pxTexture;
	const u_int m_uBindPoint;
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
		printf("Bind Buffer: %x\n", m_pxBuffer);

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
		printf("Draw\n");

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
		printf("Draw Indexed\n");

		pxCmdBuf->DrawIndexed(m_uNumIndices, m_uNumInstances, m_uVertexOffset, m_uIndexOffset, m_uInstanceOffset);
	}

	u_int m_uNumIndices;
	u_int m_uNumInstances;
	u_int m_uVertexOffset;
	u_int m_uIndexOffset;
	u_int m_uInstanceOffset;
};

class Flux_CommandList
{
public:
	Flux_CommandList()
	: m_pcData(static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uINITIAL_SIZE)))
	, m_uCursor(0)
	, m_uCapacity(uINITIAL_SIZE)
	{}

	template<typename CommandType_T, typename... Args>
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

	void IterateCommands(Flux_CommandBuffer* pxCmdBuf)
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

				HANDLE_COMMAND(FLUX_COMMANDTYPE__DRAW, Flux_CommandDraw);
				HANDLE_COMMAND(FLUX_COMMANDTYPE__DRAW_INDEXED, Flux_CommandDrawIndexed);
				default:
					Zenith_Assert(false, "Unhandled command");
			}
		}
	}

	void Reset()
	{
		m_uCursor = 0;
	}

private:
	static constexpr u_int uINITIAL_SIZE = 32;
	u_int8* m_pcData;
	u_int m_uCursor = 0;
	u_int m_uCapacity = 0;
};
