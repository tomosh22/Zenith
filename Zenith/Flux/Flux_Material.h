#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"

class Flux_Material
{
public:
	Flux_Material()
		: m_pxDiffuse(nullptr)
		, m_pxNormal(nullptr)
		, m_pxRoughness(nullptr)
		, m_pxMetallic(nullptr)
		, m_pxOcclusion(nullptr)
		, m_pxEmissive(nullptr)
	{
	}
	Flux_Material(Flux_Texture* pxDiffuse, Flux_Texture* pxNormal, Flux_Texture* pxRoughness, Flux_Texture* pxMetallic)
		: m_pxDiffuse(pxDiffuse)
		, m_pxNormal(pxNormal)
		, m_pxRoughness(pxRoughness)
		, m_pxMetallic(pxMetallic)
		, m_pxOcclusion(nullptr)
		, m_pxEmissive(nullptr)
	{
	}

	void Reset()
	{
		m_pxDiffuse = nullptr;
		m_pxNormal = nullptr;
		m_pxRoughness = nullptr;
		m_pxMetallic = nullptr;
		m_pxOcclusion = nullptr;
		m_pxEmissive = nullptr;
	}

	void SetDiffuse(Flux_Texture* pxDiffuse) { m_pxDiffuse = pxDiffuse; }
	void SetNormal(Flux_Texture* pxNormal) { m_pxNormal = pxNormal; }
	void SetRoughness(Flux_Texture* pxRoughness) { m_pxRoughness = pxRoughness; }
	void SetMetallic(Flux_Texture* pxMetallic) { m_pxMetallic = pxMetallic; }
	void SetOcclusion(Flux_Texture* pxOcclusion) { m_pxOcclusion = pxOcclusion; }
	void SetEmissive(Flux_Texture* pxEmissive) { m_pxEmissive = pxEmissive; }

	Flux_Texture* GetDiffuse() const { return m_pxDiffuse ? m_pxDiffuse : Flux_Graphics::s_pxBlackBlankTexture2D; }
	Flux_Texture* GetNormal() const { return m_pxNormal ? m_pxNormal : Flux_Graphics::s_pxBlackBlankTexture2D; }
	Flux_Texture* GetRoughness() const { return m_pxRoughness ? m_pxRoughness : Flux_Graphics::s_pxBlackBlankTexture2D; }
	Flux_Texture* GetMetallic() const { return m_pxMetallic ? m_pxMetallic : Flux_Graphics::s_pxBlackBlankTexture2D; }
	Flux_Texture* GetOcclusion() const { return m_pxOcclusion ? m_pxOcclusion : Flux_Graphics::s_pxBlackBlankTexture2D; }
	Flux_Texture* GetEmissive() const { return m_pxEmissive ? m_pxEmissive : Flux_Graphics::s_pxBlackBlankTexture2D; }

private:
	//#TO not owned
	Flux_Texture* m_pxDiffuse = nullptr;
	Flux_Texture* m_pxNormal = nullptr;
	Flux_Texture* m_pxRoughness = nullptr;
	Flux_Texture* m_pxMetallic = nullptr;
	Flux_Texture* m_pxOcclusion = nullptr;
	Flux_Texture* m_pxEmissive = nullptr;
};