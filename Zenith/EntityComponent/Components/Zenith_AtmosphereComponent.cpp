#include "Zenith.h"

#include "EntityComponent/Components/Zenith_AtmosphereComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace
{
	// v1 = Rayleigh/Mie scale + Mie-G. v2 adds the two scale heights, the
	// capture ground albedo, and the local blend-volume trio. v1 streams still
	// load (the new fields take their physical defaults, which reproduces v1
	// behaviour exactly), so committed scenes need no re-save.
	constexpr u_int uATMOSPHERE_COMPONENT_VERSION = 2u;
	constexpr float fMIE_G_MAX = 0.99f;
	// Scale heights are a physical thickness: allow a wide authoring range but
	// never zero (a zero scale height makes the density integral degenerate).
	constexpr float fMIN_SCALE_HEIGHT = 1.0f;
	constexpr float fMAX_SCALE_HEIGHT = 200000.0f;

	float ClampRange(float fValue, float fMin, float fMax)
	{
		if (fValue < fMin) return fMin;
		if (fValue > fMax) return fMax;
		return fValue;
	}
}

Zenith_AtmosphereComponent::Zenith_AtmosphereComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

void Zenith_AtmosphereComponent::SetRayleighScale(float fScale)
{
	m_fRayleighScale = (fScale < 0.0f) ? 0.0f : fScale;
}

void Zenith_AtmosphereComponent::SetMieScale(float fScale)
{
	m_fMieScale = (fScale < 0.0f) ? 0.0f : fScale;
}

void Zenith_AtmosphereComponent::SetMieG(float fG)
{
	m_fMieG = ClampRange(fG, 0.0f, fMIE_G_MAX);
}

void Zenith_AtmosphereComponent::SetRayleighScaleHeight(float fMetres)
{
	m_fRayleighScaleHeight = ClampRange(fMetres, fMIN_SCALE_HEIGHT, fMAX_SCALE_HEIGHT);
}

void Zenith_AtmosphereComponent::SetMieScaleHeight(float fMetres)
{
	m_fMieScaleHeight = ClampRange(fMetres, fMIN_SCALE_HEIGHT, fMAX_SCALE_HEIGHT);
}

void Zenith_AtmosphereComponent::SetGroundAlbedo(float fAlbedo)
{
	// A Lambertian albedo above 1 would emit more energy than it receives.
	m_fGroundAlbedo = ClampRange(fAlbedo, 0.0f, 1.0f);
}

void Zenith_AtmosphereComponent::SetBlendRadius(float fRadius)
{
	m_fBlendRadius = (fRadius < 0.0f) ? 0.0f : fRadius;
	// Keep the invariant the weight function relies on: a falloff band wider
	// than the sphere would make the volume weightless everywhere.
	if (m_fBlendFalloff >= m_fBlendRadius)
	{
		m_fBlendFalloff = (m_fBlendRadius > 0.0f) ? m_fBlendRadius * 0.5f : 0.0f;
	}
}

void Zenith_AtmosphereComponent::SetBlendFalloff(float fFalloff)
{
	const float fClamped = (fFalloff < 0.0f) ? 0.0f : fFalloff;
	m_fBlendFalloff = (m_fBlendRadius > 0.0f && fClamped >= m_fBlendRadius)
		? m_fBlendRadius * 0.5f
		: fClamped;
}

Zenith_AtmosphereMedium Zenith_AtmosphereComponent::GetMedium() const
{
	Zenith_AtmosphereMedium x;
	x.m_fRayleighScale       = m_fRayleighScale;
	x.m_fMieScale            = m_fMieScale;
	x.m_fMieG                = m_fMieG;
	x.m_fRayleighScaleHeight = m_fRayleighScaleHeight;
	x.m_fMieScaleHeight      = m_fMieScaleHeight;
	x.m_fGroundAlbedo        = m_fGroundAlbedo;
	return x;
}

void Zenith_AtmosphereComponent::SetMedium(const Zenith_AtmosphereMedium& xMedium)
{
	SetRayleighScale(xMedium.m_fRayleighScale);
	SetMieScale(xMedium.m_fMieScale);
	SetMieG(xMedium.m_fMieG);
	SetRayleighScaleHeight(xMedium.m_fRayleighScaleHeight);
	SetMieScaleHeight(xMedium.m_fMieScaleHeight);
	SetGroundAlbedo(xMedium.m_fGroundAlbedo);
}

void Zenith_AtmosphereComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uATMOSPHERE_COMPONENT_VERSION;
	xStream << m_fRayleighScale;
	xStream << m_fMieScale;
	xStream << m_fMieG;
	// --- v2 ---
	xStream << m_fRayleighScaleHeight;
	xStream << m_fMieScaleHeight;
	xStream << m_fGroundAlbedo;
	xStream << m_fBlendRadius;
	xStream << m_fBlendFalloff;
	xStream << m_fBlendPriority;
}

void Zenith_AtmosphereComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	Zenith_Assert(uVersion >= 1u && uVersion <= uATMOSPHERE_COMPONENT_VERSION,
		"Unsupported AtmosphereComponent version %u (expected 1..%u)", uVersion, uATMOSPHERE_COMPONENT_VERSION);

	xStream >> m_fRayleighScale;
	xStream >> m_fMieScale;
	xStream >> m_fMieG;
	SetRayleighScale(m_fRayleighScale);
	SetMieScale(m_fMieScale);
	SetMieG(m_fMieG);

	if (uVersion >= 2u)
	{
		xStream >> m_fRayleighScaleHeight;
		xStream >> m_fMieScaleHeight;
		xStream >> m_fGroundAlbedo;
		float fRadius = 0.0f, fFalloff = 0.0f;
		xStream >> fRadius;
		xStream >> fFalloff;
		xStream >> m_fBlendPriority;
		SetRayleighScaleHeight(m_fRayleighScaleHeight);
		SetMieScaleHeight(m_fMieScaleHeight);
		SetGroundAlbedo(m_fGroundAlbedo);
		// Radius first: SetBlendFalloff clamps against it.
		SetBlendRadius(fRadius);
		SetBlendFalloff(fFalloff);
	}
	else
	{
		// A v1 stream predates these fields. The physical defaults reproduce v1
		// behaviour byte-for-byte, and radius 0 keeps the entity a GLOBAL
		// atmosphere -- exactly what it was.
		m_fRayleighScaleHeight = Zenith_GetDefaultAtmosphereRayleighScaleHeight();
		m_fMieScaleHeight      = Zenith_GetDefaultAtmosphereMieScaleHeight();
		m_fGroundAlbedo        = Zenith_GetDefaultAtmosphereGroundAlbedo();
		m_fBlendRadius         = 0.0f;
		m_fBlendFalloff        = 0.0f;
		m_fBlendPriority       = 0.0f;
	}
}

#ifdef ZENITH_TOOLS
void Zenith_RenderEnvironmentAuthorityBanner(const Zenith_Entity& xEntity, bool bIsLocalVolume)
{
	const Zenith_EnvironmentAuthorityData& xResolved = Zenith_GetLastResolvedEnvironmentAuthority();

	if (bIsLocalVolume)
	{
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f),
			"Local blend volume - does not compete for environment authority.");
		if (xResolved.m_uBlendVolumesApplied > 0u)
		{
			ImGui::TextDisabled("%u volume(s) currently contributing (total weight %.2f).",
				xResolved.m_uBlendVolumesApplied, xResolved.m_fBlendWeightTotal);
		}
		ImGui::Separator();
		return;
	}

	if (xResolved.m_uEnvironmentEntityCount == 0u)
	{
		ImGui::TextDisabled("No environment resolved yet.");
		ImGui::Separator();
		return;
	}

	const Zenith_EntityID xID = xEntity.GetEntityID();
	const bool bIsWinner = xID.m_uIndex == xResolved.m_uEnvironmentEntityIndex
		&& xID.m_uGeneration == xResolved.m_uEnvironmentEntityGeneration;

	if (bIsWinner)
	{
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
			"Resolved environment authority%s.",
			xResolved.m_uEnvironmentEntityCount > 1u ? " (winner of a CONFLICT)" : "");
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
			"CONFLICT: entity %u wins the environment - THIS entity is IGNORED.",
			xResolved.m_uEnvironmentEntityIndex);
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
			"Delete it, or give it a Blend Radius > 0 to make it a local volume.");
	}
	if (xResolved.m_uEnvironmentEntityCount > 1u)
	{
		ImGui::TextDisabled("%u global environment entities (%u Suns, %u atmospheres). Rule: active scene, then lowest entity ID.",
			xResolved.m_uEnvironmentEntityCount, xResolved.m_uSunAuthoredCount, xResolved.m_uAtmosphereAuthoredCount);
	}
	ImGui::Separator();
}

void Zenith_AtmosphereComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	Zenith_RenderEnvironmentAuthorityBanner(m_xParentEntity, IsLocalBlendVolume());

	float fRayleigh = m_fRayleighScale;
	if (ImGui::DragFloat("Rayleigh Scale", &fRayleigh, 0.01f, 0.0f, 5.0f, "%.3f"))
	{
		SetRayleighScale(fRayleigh);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(? Help)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Density multiplier on Rayleigh (molecular)\nscattering. 1.0 = Earth default. Changes\ninvalidate transmittance LUT, sky-view LUT,\ndirect sun key and IBL convolution.");
	}

	float fMie = m_fMieScale;
	if (ImGui::DragFloat("Mie Scale", &fMie, 0.01f, 0.0f, 5.0f, "%.3f"))
	{
		SetMieScale(fMie);
	}

	float fG = m_fMieG;
	if (ImGui::SliderFloat("Mie Phase G", &fG, 0.0f, 0.99f, "%.3f"))
	{
		SetMieG(fG);
	}

	float fRayleighH = m_fRayleighScaleHeight;
	if (ImGui::DragFloat("Rayleigh Scale Height (m)", &fRayleighH, 50.0f, 1.0f, 40000.0f, "%.0f"))
	{
		SetRayleighScaleHeight(fRayleighH);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Molecular layer thickness. Earth = 8000 m.\nHigher deepens the zenith blue.");
	}

	float fMieH = m_fMieScaleHeight;
	if (ImGui::DragFloat("Mie Scale Height (m)", &fMieH, 10.0f, 1.0f, 20000.0f, "%.0f"))
	{
		SetMieScaleHeight(fMieH);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Aerosol layer thickness. Earth = 1200 m.\nLOWER presses haze down against the ground\n(humid valley, dust bowl).");
	}

	float fAlbedo = m_fGroundAlbedo;
	if (ImGui::SliderFloat("Capture Ground Albedo", &fAlbedo, 0.0f, 1.0f, "%.3f"))
	{
		SetGroundAlbedo(fAlbedo);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Virtual ground the IBL capture integrates.\n0.25 vegetated land, ~0.7 snow/desert,\n~0.05 basalt/ocean. Fills the cube's lower\nhemisphere -- this is the bounce light on\nvertical surfaces. Does not affect the\nvisible sky (real terrain supplies it there).");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Local blend volume");

	float fRadius = m_fBlendRadius;
	if (ImGui::DragFloat("Blend Radius", &fRadius, 1.0f, 0.0f, 100000.0f, "%.1f"))
	{
		SetBlendRadius(fRadius);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("0 = GLOBAL atmosphere (competes for the one\nenvironment authority; two globals conflict).\n> 0 = LOCAL volume centred on this entity:\nblends its medium over the global one while\nthe camera is inside. The Sun is never blended.");
	}

	if (IsLocalBlendVolume())
	{
		float fFalloff = m_fBlendFalloff;
		if (ImGui::DragFloat("Blend Falloff", &fFalloff, 0.5f, 0.0f, 100000.0f, "%.1f"))
		{
			SetBlendFalloff(fFalloff);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Fade band just inside the radius.\n0 = a hard edge (visible pop on crossing).");
		}
		ImGui::DragFloat("Blend Priority", &m_fBlendPriority, 0.1f, -1000.0f, 1000.0f, "%.2f");
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Overlapping volumes apply low priority FIRST,\nso higher priority wins. Ties break on the\nstable entity ID (never on query order).");
		}
	}

	ImGui::Separator();
	ImGui::TextWrapped(
		"Atmosphere model inputs only. The engine's single radiometric anchor\n"
		"(top-of-atmosphere solar irradiance) is NOT authored here -- it is\n"
		"policy, not a look knob. Exposure controls presentation brightness.\n"
		"Co-locate with a Zenith_SunComponent on this environment entity.");
}
#endif

#include "EntityComponent/Components/Zenith_AtmosphereComponent.Tests.inl"