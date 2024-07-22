#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"

class Flux_Material
{
public:
	Flux_Material()
		: m_pxDiffuse(&Flux_Graphics::s_xBlankTexture2D)
		, m_pxNormal(&Flux_Graphics::s_xBlankTexture2D)
		, m_pxRoughness(&Flux_Graphics::s_xBlankTexture2D)
		, m_pxMetallic(&Flux_Graphics::s_xBlankTexture2D)
	{

	}
	Flux_Material(Flux_Texture* pxDiffuse, Flux_Texture* pxNormal, Flux_Texture* pxRoughness, Flux_Texture* pxMetallic)
		: m_pxDiffuse(pxDiffuse)
		, m_pxNormal(pxNormal)
		, m_pxRoughness(pxRoughness)
		, m_pxMetallic(pxMetallic)
	{

	}

	void SetDiffuse(Flux_Texture* pxDiffuse) { m_pxDiffuse = pxDiffuse; }
	void SetNormal(Flux_Texture* pxNormal) { m_pxNormal = pxNormal; }
	void SetRoughness(Flux_Texture* pxRoughness) { m_pxRoughness = pxRoughness; }
	void SetMetallic(Flux_Texture* pxMetallic) { m_pxMetallic = pxMetallic; }

	Flux_Texture* GetDiffuse() { return m_pxDiffuse; }
	Flux_Texture* GetNormal() { return m_pxNormal; }
	Flux_Texture* GetRoughness() { return m_pxRoughness; }
	Flux_Texture* GetMetallic() { return m_pxMetallic; }

private:
	Flux_Texture* m_pxDiffuse;
	Flux_Texture* m_pxNormal;
	Flux_Texture* m_pxRoughness;
	Flux_Texture* m_pxMetallic;
};