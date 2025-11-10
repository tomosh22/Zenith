#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"

class Flux_Material
{
public:
	Flux_Material()
		: m_xBaseColor(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}
	Flux_Material(Flux_Texture xDiffuse, Flux_Texture xNormal, Flux_Texture xRoughnessMetallic)
		: m_xDiffuse(xDiffuse)
		, m_xNormal(xNormal)
		, m_xRoughnessMetallic(xRoughnessMetallic)
		, m_xBaseColor(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}

	void Reset()
	{
		m_xDiffuse = Flux_Texture{};
		m_xNormal = Flux_Texture{};
		m_xRoughnessMetallic = Flux_Texture{};
		m_xOcclusion = Flux_Texture{};
		m_xEmissive = Flux_Texture{};
	}

	void SetDiffuse(const Flux_Texture& xTexture) { m_xDiffuse = xTexture; }
	void SetNormal(const Flux_Texture& xTexture) { m_xNormal = xTexture; }
	void SetRoughnessMetallic(const Flux_Texture& xTexture) { m_xRoughnessMetallic = xTexture; }
	void SetOcclusion(const Flux_Texture& xTexture) { m_xOcclusion = xTexture; }
	void SetEmissive(const Flux_Texture& xTexture) { m_xEmissive = xTexture; }
	void SetBaseColor(const Zenith_Maths::Vector4& xColor) { m_xBaseColor = xColor; }

	const Flux_Texture* GetDiffuse() const { return m_xDiffuse.m_xVRAMHandle.IsValid() ? &m_xDiffuse : GetBlankTexture(); }
	const Flux_Texture* GetNormal() const { return m_xNormal.m_xVRAMHandle.IsValid() ? &m_xNormal : GetBlankTexture(); }
	const Flux_Texture* GetRoughnessMetallic() const { return m_xRoughnessMetallic.m_xVRAMHandle.IsValid() ? &m_xRoughnessMetallic : GetBlankTexture(); }
	const Flux_Texture* GetOcclusion() const { return m_xOcclusion.m_xVRAMHandle.IsValid() ? &m_xOcclusion : GetBlankTexture(); }
	const Flux_Texture* GetEmissive() const { return m_xEmissive.m_xVRAMHandle.IsValid() ? &m_xEmissive : GetBlankTexture(); }
	const Zenith_Maths::Vector4& GetBaseColor() const { return m_xBaseColor; }

private:
	static const Flux_Texture* GetBlankTexture()
	{
		return &Flux_Graphics::s_xWhiteBlankTexture2D;
	}

	Flux_Texture m_xDiffuse;
	Flux_Texture m_xNormal;
	Flux_Texture m_xRoughnessMetallic;
	Flux_Texture m_xOcclusion;
	Flux_Texture m_xEmissive;
	Zenith_Maths::Vector4 m_xBaseColor;
};