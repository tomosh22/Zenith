#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"

class Flux_Material
{
public:
	Flux_Material()
		: m_xDiffuse(&Flux_Graphics::s_xBlankTexture2D)
		, m_xNormal(&Flux_Graphics::s_xBlankTexture2D)
		, m_xRoughness(&Flux_Graphics::s_xBlankTexture2D)
		, m_xMetallic(&Flux_Graphics::s_xBlankTexture2D)
	{

	}
	Flux_Material(Flux_Texture* xDiffuse, Flux_Texture* xNormal, Flux_Texture* xRoughness, Flux_Texture* xMetallic)
		: m_xDiffuse(xDiffuse)
		, m_xNormal(xNormal)
		, m_xRoughness(xRoughness)
		, m_xMetallic(xMetallic)
	{

	}

	void SetDiffuse(Flux_Texture* xDiffuse) { m_xDiffuse = xDiffuse; }
	void SetNormal(Flux_Texture* xNormal) { m_xNormal = xNormal; }
	void SetRoughness(Flux_Texture* xRoughness) { m_xRoughness = xRoughness; }
	void SetMetallic(Flux_Texture* xMetallic) { m_xMetallic = xMetallic; }

	Flux_Texture* GetDiffuse() { return m_xDiffuse; }
	Flux_Texture* GetNormal() { return m_xNormal; }
	Flux_Texture* GetRoughness() { return m_xRoughness; }
	Flux_Texture* GetMetallic() { return m_xMetallic; }

private:
	Flux_Texture* m_xDiffuse;
	Flux_Texture* m_xNormal;
	Flux_Texture* m_xRoughness;
	Flux_Texture* m_xMetallic;
};