#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"

class Flux_Material
{
public:
	Flux_Material()
		: m_uDiffuse(UINT32_MAX)
		, m_uNormal(UINT32_MAX)
		, m_uRoughnessMetallic(UINT32_MAX)
		, m_uOcclusion(UINT32_MAX)
		, m_uEmissive(UINT32_MAX)
	{
	}
	Flux_Material(uint32_t uDiffuse, uint32_t uNormal, uint32_t uRoughnessMetallic)
		: m_uDiffuse(uDiffuse)
		, m_uNormal(uNormal)
		, m_uRoughnessMetallic(uRoughnessMetallic)
		, m_uOcclusion(UINT32_MAX)
		, m_uEmissive(UINT32_MAX)
	{
	}

	void Reset()
	{
		m_uDiffuse = UINT32_MAX;
		m_uNormal = UINT32_MAX;
		m_uRoughnessMetallic = UINT32_MAX;
		m_uOcclusion = UINT32_MAX;
		m_uEmissive = UINT32_MAX;
	}

	void SetDiffuse(uint32_t uHandle) { m_uDiffuse = uHandle; }
	void SetNormal(uint32_t uHandle) { m_uNormal = uHandle; }
	void SetRoughnessMetallic(uint32_t uHandle) { m_uRoughnessMetallic = uHandle; }
	void SetOcclusion(uint32_t uHandle) { m_uOcclusion = uHandle; }
	void SetEmissive(uint32_t uHandle) { m_uEmissive = uHandle; }

	uint32_t GetDiffuse() const { return m_uDiffuse != UINT32_MAX ? m_uDiffuse : Flux_Graphics::s_xBlackBlankTexture2D.m_uVRAMHandle; }
	uint32_t GetNormal() const { return m_uNormal != UINT32_MAX ? m_uNormal : Flux_Graphics::s_xBlackBlankTexture2D.m_uVRAMHandle; }
	uint32_t GetRoughnessMetallic() const { return m_uRoughnessMetallic != UINT32_MAX ? m_uRoughnessMetallic : Flux_Graphics::s_xBlackBlankTexture2D.m_uVRAMHandle; }
	uint32_t GetOcclusion() const { return m_uOcclusion != UINT32_MAX ? m_uOcclusion : Flux_Graphics::s_xBlackBlankTexture2D.m_uVRAMHandle; }
	uint32_t GetEmissive() const { return m_uEmissive != UINT32_MAX ? m_uEmissive : Flux_Graphics::s_xBlackBlankTexture2D.m_uVRAMHandle; }

private:
	uint32_t m_uDiffuse = UINT32_MAX;
	uint32_t m_uNormal = UINT32_MAX;
	uint32_t m_uRoughnessMetallic = UINT32_MAX;
	uint32_t m_uOcclusion = UINT32_MAX;
	uint32_t m_uEmissive = UINT32_MAX;
};