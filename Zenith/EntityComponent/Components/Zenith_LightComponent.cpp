#include "Zenith.h"

#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_SunComponent.h"
#include "EntityComponent/Components/Zenith_AtmosphereComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#ifdef ZENITH_TOOLS
// The offset editor reads the sibling model's LOCAL bounds so an author can see
// the box a bulb has to sit inside. Both edges are TOOLS-only and panel-only.
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Maths/Zenith_FrustumCulling.h"   // Zenith_AABB
#endif
#include "ZenithECS/Zenith_ComponentMeta.h"

// Wave 3: EC-side render-gather (publishes neutral light data to the renderer so
// Flux_DynamicLights no longer #includes Zenith_LightComponent.h / TransformComponent.h).
#include "Core/Zenith_RenderGather.h"
#include "Core/Zenith_EnvironmentAuthority.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"

void Zenith_LightComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// LIGHT_TYPE is intentionally omitted — its DataStream `>>` path goes through
	// u_int+static_cast (see WriteToDataStream / ReadFromDataStream), which the
	// generic property-setter macro doesn't model. Variants that need a
	// different light type should override at the prefab level instead.
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_xColor,            "Color",            axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_fIntensity,        "Intensity",        axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_fRange,            "Range",            axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_fSpotInnerAngle,   "SpotInnerAngle",   axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_fSpotOuterAngle,   "SpotOuterAngle",   axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_bCastShadows,      "CastShadows",      axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_xLocalPositionOffset, "LocalPositionOffset", axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY(Zenith_LightComponent, m_xDirectionOffset,  "DirectionOffset",  axProperties);
}

// Serialization version history:
// Version 1: Initial implementation
static constexpr u_int uLIGHT_COMPONENT_VERSION = 1;

// Magic number constants for direction normalization
static constexpr float fDIRECTION_NORMALIZE_EPSILON = 0.0001f;

Zenith_LightComponent::Zenith_LightComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

// ============================================================================
// ★★★ THE OFFSET IS IN THE MODEL'S OWN SPACE, AND IT USED TO BE ADDED IN WORLD
// SPACE. The old body was `xPos += m_xLocalPositionOffset` -- a plain world-space
// add that ignored the entity's rotation and scale entirely. That is fine for
// the only thing it had ever been used for, which is nothing: no game code set
// it, so the defect could not show.
//
// It shows the moment a light shares an entity with a MODEL, which is what a
// lamp post needs -- the bulb is a point inside the lantern head, and "inside
// the lantern head" is a statement about the mesh, not about the world:
//
//   * ROTATION. Turn the lamp post a quarter turn and a world-space offset
//     leaves the bulb where it was, now floating beside the post. Every
//     interior prop's yaw is authored, and the outdoor table can carry one too.
//   * SCALE, which is the one that bites even with no rotation at all.
//     ZM_ComputePropFit scales an imported prop by whatever it takes to reach
//     its roster size -- 3.0060 for this lamp post, from a model 0.998 m tall.
//     A world-space offset would have to be typed POST-scale, so re-exporting
//     the asset at a different size silently moves the bulb out of the lantern
//     -- exactly the coupling ZM_PropFit.h exists to remove ("someone
//     re-exports the bed at a different scale and nothing in the game needs
//     editing").
//
// So the offset is transformed by the parent's rotation and scale, which makes
// it a point ON THE MODEL: measured once off the mesh in the mesh's own units,
// correct at every scale and every yaw thereafter.
// ============================================================================
Zenith_Maths::Vector3 Zenith_LightComponent::GetWorldPosition() const
{
	Zenith_Maths::Vector3 xPos(0.0f);
	if (!m_xParentEntity.IsValid())
	{
		return m_bUsePositionOffset ? m_xLocalPositionOffset : xPos;
	}

	Zenith_TransformComponent* pxTransform =
		m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	if (pxTransform == nullptr)
	{
		return m_bUsePositionOffset ? m_xLocalPositionOffset : xPos;
	}
	pxTransform->GetPosition(xPos);

	if (!m_bUsePositionOffset)
	{
		return xPos;
	}

	Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xScale(1.0f);
	pxTransform->GetRotation(xRotation);
	pxTransform->GetScale(xScale);

	// Scale first, then rotate, then translate -- the same order the transform
	// itself composes, so the offset lands exactly where the equivalent point on
	// the mesh does.
	return xPos + (xRotation * (m_xLocalPositionOffset * xScale));
}

Zenith_Maths::Vector3 Zenith_LightComponent::GetWorldDirection() const
{
	// When direction offset is enabled, treat m_xDirectionOffset as an absolute world direction
	// This is set via SetWorldDirection() for convenience
	if (m_bUseDirectionOffset)
	{
		// Safely normalize - return default direction if zero-length to prevent NaN
		float fLength = Zenith_Maths::Length(m_xDirectionOffset);
		if (fLength < fDIRECTION_NORMALIZE_EPSILON)
		{
			return Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Default forward
		}
		return m_xDirectionOffset / fLength;
	}

	// Default: use transform rotation applied to forward vector (-Z)
	Zenith_Maths::Vector3 xDir(0.0f, 0.0f, -1.0f);
	if (m_xParentEntity.IsValid())
	{
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Quat xRot;
			pxTransform->GetRotation(xRot);
			xDir = xRot * Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
		}
	}
	return xDir;
}

void Zenith_LightComponent::SetWorldDirection(const Zenith_Maths::Vector3& xWorldDir)
{
	m_bUseDirectionOffset = true;
	// Safely normalize - use default direction if zero-length to prevent NaN
	float fLength = Zenith_Maths::Length(xWorldDir);
	if (fLength < fDIRECTION_NORMALIZE_EPSILON)
	{
		m_xDirectionOffset = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Default forward
		return;
	}
	m_xDirectionOffset = xWorldDir / fLength;
}

// Spot light angle constants (in radians)
static constexpr float fSPOT_MIN_INNER_ANGLE = 0.0f;          // 0 degrees
static constexpr float fSPOT_MAX_INNER_ANGLE = 1.5533f;       // ~89 degrees
static constexpr float fSPOT_MIN_OUTER_ANGLE = 0.01f;         // ~0.5 degrees
static constexpr float fSPOT_MAX_OUTER_ANGLE = 1.5708f;       // PI/2 (90 degrees)
static constexpr float fSPOT_MIN_ANGLE_MARGIN = 0.01f;        // Minimum gap between inner and outer

// UI editor constants
static constexpr float fUI_MAX_INTENSITY_LUX = 150000.0f;     // Max lux for directional lights (direct sunlight)
static constexpr float fUI_MAX_INTENSITY_LUMENS = 10000000.0f; // Max lumens for point/spot lights (stadium lights)
static constexpr float fUI_INTENSITY_DRAG_SPEED_LUX = 100.0f;  // Drag speed for directional light intensity
static constexpr float fUI_INTENSITY_DRAG_SPEED_LUMENS = 10.0f; // Drag speed for point/spot light intensity

void Zenith_LightComponent::ValidateSpotAngles()
{
	// Clamp outer first (it's the primary constraint)
	m_fSpotOuterAngle = Zenith_Maths::Clamp(m_fSpotOuterAngle, fSPOT_MIN_OUTER_ANGLE, fSPOT_MAX_OUTER_ANGLE);

	// Then constrain inner to be less than outer with minimum margin
	float fMaxInner = m_fSpotOuterAngle - fSPOT_MIN_ANGLE_MARGIN;
	m_fSpotInnerAngle = Zenith_Maths::Clamp(m_fSpotInnerAngle, fSPOT_MIN_INNER_ANGLE, fMaxInner);
}

void Zenith_LightComponent::SetSpotInnerAngle(float fAngle)
{
	m_fSpotInnerAngle = fAngle;
	ValidateSpotAngles();
}

void Zenith_LightComponent::SetSpotOuterAngle(float fAngle)
{
	m_fSpotOuterAngle = fAngle;
	ValidateSpotAngles();
}

void Zenith_LightComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write version first for future compatibility
	xStream << uLIGHT_COMPONENT_VERSION;

	xStream << static_cast<u_int>(m_eLightType);
	xStream << m_xColor;
	xStream << m_fIntensity;
	xStream << m_fRange;
	xStream << m_fSpotInnerAngle;
	xStream << m_fSpotOuterAngle;
	xStream << m_bCastShadows;
	xStream << m_bUsePositionOffset;
	xStream << m_xLocalPositionOffset;
	xStream << m_bUseDirectionOffset;
	xStream << m_xDirectionOffset;
}

void Zenith_LightComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read version for compatibility handling
	u_int uVersion;
	xStream >> uVersion;

	// Version 1 data (current)
	if (uVersion >= 1)
	{
		u_int uType;
		xStream >> uType;
		m_eLightType = static_cast<LIGHT_TYPE>(uType);
		xStream >> m_xColor;
		xStream >> m_fIntensity;
		xStream >> m_fRange;
		m_fRange = std::max(m_fRange, 0.1f);  // Clamp legacy data to minimum
		xStream >> m_fSpotInnerAngle;
		xStream >> m_fSpotOuterAngle;
		xStream >> m_bCastShadows;
		xStream >> m_bUsePositionOffset;
		xStream >> m_xLocalPositionOffset;
		xStream >> m_bUseDirectionOffset;
		xStream >> m_xDirectionOffset;
	}

	// Future versions can add new data here:
	// if (uVersion >= 2) { xStream >> m_fNewField; }

	// Warn about unknown future versions (data may be ignored)
	if (uVersion > uLIGHT_COMPONENT_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ECS, "Warning: LightComponent version %u is newer than supported (%u), some data may be ignored",
			uVersion, uLIGHT_COMPONENT_VERSION);
	}
}

#ifdef ZENITH_TOOLS
void Zenith_LightComponent::RenderPropertiesPanel()
{

	const char* aszLightTypes[] = { "Point", "Spot", "Directional" };
	int iLightType = static_cast<int>(m_eLightType);
	if (ImGui::Combo("Light Type", &iLightType, aszLightTypes, LIGHT_TYPE_COUNT))
	{
		m_eLightType = static_cast<LIGHT_TYPE>(iLightType);
	}

	ImGui::Separator();

	// Color picker (linear RGB space)
	float afColor[3] = { m_xColor.x, m_xColor.y, m_xColor.z };
	if (ImGui::ColorEdit3("Color (Linear)", afColor))
	{
		m_xColor = { afColor[0], afColor[1], afColor[2] };
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Color must be in LINEAR RGB space (not sRGB)");
	}

	RenderLightIntensity();

	// Range (point/spot only) — max 10km to prevent extreme light volumes
	if (m_eLightType != LIGHT_TYPE_DIRECTIONAL)
	{
		float fRange = m_fRange;
		if (ImGui::DragFloat("Range", &fRange, 0.5f, 0.1f, 10000.0f, "%.1f m"))
		{
			SetRange(fRange);
		}
	}

	if (m_eLightType == LIGHT_TYPE_SPOT)
	{
		RenderSpotParameters();
	}

	ImGui::Separator();

	// Shadow toggle (reserved for future)
	ImGui::BeginDisabled();
	ImGui::Checkbox("Cast Shadows", &m_bCastShadows);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("(Not implemented)");

	ImGui::Separator();
	ImGui::Text("Transform Offsets");
	RenderTransformOffsets();
}

void Zenith_LightComponent::RenderLightIntensity()
{
	// Intensity uses different physical units per light type. Minimum 0 prevents
	// subtractive lighting; max prevents overflow.
	if (m_eLightType == LIGHT_TYPE_DIRECTIONAL)
	{
		if (ImGui::DragFloat("Intensity (lux)", &m_fIntensity, fUI_INTENSITY_DRAG_SPEED_LUX, 0.0f, fUI_MAX_INTENSITY_LUX, "%.0f"))
		{
			m_fIntensity = Zenith_Maths::Clamp(m_fIntensity, 0.0f, fUI_MAX_INTENSITY_LUX);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Illuminance in lux (lm/m^2)\nOvercast: ~1000, Cloudy: ~10000, Sun: ~100000");
		}
	}
	else
	{
		if (ImGui::DragFloat("Intensity (lm)", &m_fIntensity, fUI_INTENSITY_DRAG_SPEED_LUMENS, 0.0f, fUI_MAX_INTENSITY_LUMENS, "%.0f"))
		{
			m_fIntensity = Zenith_Maths::Clamp(m_fIntensity, 0.0f, fUI_MAX_INTENSITY_LUMENS);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Luminous power in lumens\n60W bulb: ~800, Studio light: ~5000");
		}
	}
}

void Zenith_LightComponent::RenderSpotParameters()
{
	ImGui::Separator();
	ImGui::Text("Spot Parameters");

	float fInnerDeg = glm::degrees(m_fSpotInnerAngle);
	float fOuterDeg = glm::degrees(m_fSpotOuterAngle);

	// Min 0 degrees matches SetSpotInnerAngle clamping behavior
	if (ImGui::SliderFloat("Inner Angle", &fInnerDeg, 0.0f, 89.0f, "%.1f deg"))
	{
		SetSpotInnerAngle(glm::radians(fInnerDeg));
	}
	// Min 1 degree to prevent degenerate cone
	if (ImGui::SliderFloat("Outer Angle", &fOuterDeg, 1.0f, 90.0f, "%.1f deg"))
	{
		SetSpotOuterAngle(glm::radians(fOuterDeg));
	}
}

// ★★ THE OFFSET EDITOR IS FOR PLACING A BULB INSIDE A MESH, and that is a harder
// job than a DragFloat3 alone supports. This panel used to be exactly that: three
// numbers, in world space, with no indication of where the light actually ended
// up or what range of values was even sensible. Putting a light inside a lamp
// post's lantern head through it meant typing a number, building, looking, and
// typing another.
//
// What it shows now, when the entity also owns a model:
//   * the offset in the MODEL's own units, which is what GetWorldPosition now
//     consumes -- so a value read off the mesh can be typed in directly and stays
//     correct at every scale and yaw the prop is authored at;
//   * the model's local bounds, so the author can see the box the bulb has to be
//     inside rather than guessing at the magnitude;
//   * the offset as a FRACTION of those bounds, which is how a lantern head is
//     actually described ("87% of the way up the post");
//   * the resolved WORLD position, which is what the renderer will use, and
//     whether the offset currently lands inside the model's bounds at all --
//     a light outside its own casing is the failure this panel exists to prevent;
//   * a one-click CENTRE OF BOUNDS, the sane starting point to drag from.
void Zenith_LightComponent::RenderTransformOffsets()
{
	ImGui::Checkbox("Use Position Offset", &m_bUsePositionOffset);
	if (m_bUsePositionOffset)
	{
		// The model this light shares an entity with, if any. A light on its OWN
		// entity (which is how every interior lamp in Zenithmon is authored) has
		// none, and the extra read-outs are simply omitted for it -- there is no
		// mesh for an offset to be relative TO.
		const Zenith_ModelComponent* pxModel = m_xParentEntity.IsValid()
			? m_xParentEntity.TryGetComponent<Zenith_ModelComponent>()
			: nullptr;
		const Flux_MeshInstance* pxInstance =
			(pxModel != nullptr && pxModel->GetNumMeshes() > 0u)
				? pxModel->GetMeshInstance(0u)
				: nullptr;

		float afPos[3] = { m_xLocalPositionOffset.x, m_xLocalPositionOffset.y,
			m_xLocalPositionOffset.z };
		// 1 mm per pixel: a bulb inside a lantern is placed to the millimetre, and
		// the old 0.1 step moved it 10 cm a pixel -- straight through the casing.
		if (ImGui::DragFloat3("Local Position Offset", afPos, 0.001f, 0.0f, 0.0f, "%.4f"))
		{
			m_xLocalPositionOffset = { afPos[0], afPos[1], afPos[2] };
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("In the MODEL's own units, before the entity's scale and\n"
				"rotation. Measure it off the mesh once and it stays correct\n"
				"if the asset is re-exported at another size.");
		}

		if (pxInstance != nullptr)
		{
			const Zenith_AABB& xLocal = pxInstance->GetLocalBounds();
			const Zenith_Maths::Vector3 xSize = xLocal.m_xMax - xLocal.m_xMin;
			const Zenith_Maths::Vector3 xCentre = (xLocal.m_xMin + xLocal.m_xMax) * 0.5f;

			ImGui::TextDisabled("model bounds  (%.4f, %.4f, %.4f) .. (%.4f, %.4f, %.4f)",
				xLocal.m_xMin.x, xLocal.m_xMin.y, xLocal.m_xMin.z,
				xLocal.m_xMax.x, xLocal.m_xMax.y, xLocal.m_xMax.z);

			// Where the offset sits inside those bounds, per axis, as a fraction.
			// A denominator of zero is a flat axis; report the centre for it rather
			// than a NaN the author has to interpret.
			const auto Fraction = [](float fValue, float fMin, float fExtent) -> float
			{
				return (fExtent > 1.0e-6f) ? ((fValue - fMin) / fExtent) : 0.5f;
			};
			ImGui::TextDisabled("as a fraction  (%.3f, %.3f, %.3f) of those bounds",
				Fraction(m_xLocalPositionOffset.x, xLocal.m_xMin.x, xSize.x),
				Fraction(m_xLocalPositionOffset.y, xLocal.m_xMin.y, xSize.y),
				Fraction(m_xLocalPositionOffset.z, xLocal.m_xMin.z, xSize.z));

			const bool bInside =
				m_xLocalPositionOffset.x >= xLocal.m_xMin.x && m_xLocalPositionOffset.x <= xLocal.m_xMax.x &&
				m_xLocalPositionOffset.y >= xLocal.m_xMin.y && m_xLocalPositionOffset.y <= xLocal.m_xMax.y &&
				m_xLocalPositionOffset.z >= xLocal.m_xMin.z && m_xLocalPositionOffset.z <= xLocal.m_xMax.z;
			if (bInside)
			{
				ImGui::TextDisabled("inside the model's bounds");
			}
			else
			{
				// Not an error -- a porch light hanging off a wall is legitimately
				// outside the wall's bounds -- but for a bulb it is the whole bug,
				// so it is said out loud rather than left to be noticed in a render.
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
					"OUTSIDE the model's bounds");
			}

			if (ImGui::Button("Centre of bounds"))
			{
				m_xLocalPositionOffset = xCentre;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Start from the middle of the mesh and drag out.");
			}
		}

		// What the renderer will actually use. The offset is the input; THIS is the
		// thing that was previously impossible to see without a build.
		const Zenith_Maths::Vector3 xWorld = GetWorldPosition();
		ImGui::TextDisabled("world position  (%.4f, %.4f, %.4f)",
			xWorld.x, xWorld.y, xWorld.z);
	}

	// Direction offset applies to spot/directional only (point lights are omnidirectional).
	if (m_eLightType == LIGHT_TYPE_POINT) return;

	ImGui::Checkbox("Use Direction Offset", &m_bUseDirectionOffset);
	if (!m_bUseDirectionOffset) return;

	Zenith_Maths::Vector3 xNormalized = GetWorldDirection();
	float afDir[3] = { xNormalized.x, xNormalized.y, xNormalized.z };
	if (ImGui::DragFloat3("Direction Offset", afDir, 0.01f))
	{
		SetWorldDirection({ afDir[0], afDir[1], afDir[2] });
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Absolute WORLD direction, unlike the position offset above,\n"
			"and automatically normalized.");
	}
}
#endif

// ---------------------------------------------------------------------------
// Wave 3: light render-gather. Extracts every Zenith_LightComponent (+ its
// entity transform) into the renderer-neutral Zenith_LightRenderData list that
// Flux_DynamicLights consumes. This is the EC->renderer (forward) direction;
// the renderer no longer reaches into EntityComponent. The frustum cull,
// intensity threshold, direction validation and GPU staging stay renderer-side
// (they need the camera frustum + Flux buffers) — this only does the typed
// query + raw field extraction the renderer previously did itself.
// ---------------------------------------------------------------------------
static Zenith_SceneSystem& LightingScenes() { return g_xEngine.Scenes(); }

static void Zenith_GatherLightsImpl(Zenith_Vector<Zenith_LightRenderData>& xOut)
{
	LightingScenes().QueryAllScenes<Zenith_LightComponent, Zenith_TransformComponent>()
		.ForEach([&xOut](Zenith_EntityID uID, Zenith_LightComponent& xLight, Zenith_TransformComponent&)
	{
		Zenith_LightRenderData xData;
		switch (xLight.GetLightType())
		{
		case LIGHT_TYPE_POINT:       xData.m_eType = ZENITH_LIGHT_RENDER_POINT;       break;
		case LIGHT_TYPE_SPOT:        xData.m_eType = ZENITH_LIGHT_RENDER_SPOT;        break;
		case LIGHT_TYPE_DIRECTIONAL: xData.m_eType = ZENITH_LIGHT_RENDER_DIRECTIONAL; break;
		default: return; // invalid type — skip (renderer previously asserted; here we just drop)
		}
		xData.m_xColor          = xLight.GetColor();
		xData.m_fIntensity      = xLight.GetIntensity();
		xData.m_xWorldPosition  = xLight.GetWorldPosition();
		xData.m_fRange          = xLight.GetRange();
		xData.m_xWorldDirection = xLight.GetWorldDirection();
		xData.m_fSpotInnerAngle = xLight.GetSpotInnerAngle();
		xData.m_fSpotOuterAngle = xLight.GetSpotOuterAngle();
		xData.m_uEntityIndex    = uID.m_uIndex;
		xOut.PushBack(xData);
	});
}

// Published to the renderer. Constant-initialised, so referencing g_pfnZenithLightGather
// from Flux_DynamicLights pulls this TU in (no static-init-order or dead-strip hazard).
Zenith_LightGatherFn g_pfnZenithLightGather = &Zenith_GatherLightsImpl;

// Environment authority (Sun geometry + atmosphere model) is resolved beside
// ordinary lights so the whole lighting-extraction boundary shares this TU's
// single, grandfathered scene-system reach. ONE authoritative environment
// entity is chosen first -- active scene wins, then lowest stable entity ID --
// and BOTH the Zenith_SunComponent and Zenith_AtmosphereComponent are gathered
// from that one source. A Sun from one loaded scene is therefore never mixed
// with an atmosphere from another; a partial rig (only one of the two on the
// winning entity) falls back to the physical/default value for the missing
// half. The concrete components stay data-only ECS types; the renderer
// consumes only the neutral Zenith_EnvironmentAuthorityData.
namespace
{
	uint64_t s_ulLastEnvironmentConflictSignature = 0u;
	Zenith_EnvironmentAuthorityData s_xLastResolvedEnvironment;
#ifdef ZENITH_TESTING
	u_int s_uEnvironmentConflictWarningCount = 0u;
#endif
#ifdef ZENITH_TOOLS
	bool s_bEnvironmentConflictAssertsEnabled = true;
#endif

	struct EnvironmentCandidate
	{
		Zenith_EntityID          m_xID;
		bool                     m_bInActiveScene  = false;
		bool                     m_bHasSun         = false;
		Zenith_Maths::Vector3    m_xSunDir         = Zenith_Maths::Vector3(0.0f);
		bool                     m_bHasAtmosphere  = false;
		Zenith_AtmosphereMedium  m_xMedium;
	};

	// A LOCAL atmosphere volume: never competes for authority, blends its medium
	// over the resolved global one by a camera-distance weight.
	struct EnvironmentBlendVolume
	{
		uint64_t                 m_ulPackedID = 0u;
		float                    m_fPriority  = 0.0f;
		float                    m_fWeight    = 0.0f;
		Zenith_AtmosphereMedium  m_xMedium;
	};

	// Ascending (priority, packed entity ID): lower priority applies FIRST so a
	// higher-priority volume wins where they overlap, and the tie-break is the
	// stable ID so the result never depends on ECS query order.
	bool BlendVolumePrecedes(const EnvironmentBlendVolume& xA, const EnvironmentBlendVolume& xB)
	{
		if (xA.m_fPriority != xB.m_fPriority) return xA.m_fPriority < xB.m_fPriority;
		return xA.m_ulPackedID < xB.m_ulPackedID;
	}

	bool EnvironmentPrecedes(const EnvironmentCandidate& xCand,
		u_int uWinnerIndex, u_int uWinnerGeneration, bool bWinnerInActive)
	{
		// Active-scene environment beats non-active; then lowest stable ID.
		if (xCand.m_bInActiveScene != bWinnerInActive)
		{
			return xCand.m_bInActiveScene;
		}
		return xCand.m_xID.m_uIndex < uWinnerIndex
			|| (xCand.m_xID.m_uIndex == uWinnerIndex && xCand.m_xID.m_uGeneration < uWinnerGeneration);
	}

	EnvironmentCandidate* FindOrCreateCandidate(Zenith_Vector<EnvironmentCandidate>& axCandidates,
		Zenith_EntityID xID, bool bInActiveScene)
	{
		const uint64_t ulPacked = xID.GetPacked();
		for (u_int u = 0u; u < axCandidates.GetSize(); u++)
		{
			EnvironmentCandidate& rC = axCandidates.Get(u);
			if (rC.m_xID.GetPacked() == ulPacked)
			{
				return &rC;
			}
		}
		axCandidates.EmplaceBack();
		EnvironmentCandidate& rNew = axCandidates.Get(axCandidates.GetSize() - 1u);
		rNew.m_xID = xID;
		rNew.m_bInActiveScene = bInActiveScene;
		return &rNew;
	}

	void GatherEnvironmentAuthority(Zenith_EnvironmentAuthorityData& xOut)
	{
		xOut = Zenith_EnvironmentAuthorityData();
		Zenith_SceneSystem& xScenes = LightingScenes();
		Zenith_SceneData* pxActiveScene = xScenes.GetActiveSceneData();

		Zenith_Vector<EnvironmentCandidate> axCandidates;

		xScenes.QueryAllScenes<Zenith_SunComponent>().ForEach(
			[&](Zenith_EntityID xID, Zenith_SunComponent& xSun)
			{
				const bool bInActive = pxActiveScene != nullptr
					&& xScenes.GetSceneDataForEntity(xID) == pxActiveScene;
				EnvironmentCandidate* pxC = FindOrCreateCandidate(axCandidates, xID, bInActive);
				pxC->m_bHasSun = true;
				pxC->m_xSunDir = xSun.GetWorldDirection();
			});

		// Blend volumes are collected in the SAME walk but kept out of the
		// candidate set: a local volume is an intentional regional override, not
		// a competitor for authority, so it must never win and never conflict.
		//
		// The blend position is the VIEW position (Unity's rule -- an environment
		// is what the viewer is standing in), taken from the neutral render
		// gather rather than by resolving Zenith_CameraComponent directly. That
		// is the same boundary Flux itself consumes, so the volume the renderer
		// blends is always the volume the rendered camera is inside, and this
		// gather keeps naming no camera type.
		Zenith_Vector<EnvironmentBlendVolume> axBlendVolumes;
		Zenith_Maths::Vector3 xBlendPosition(0.0f);
		bool bHaveBlendPosition = false;
		if (g_pfnZenithCameraGather != nullptr)
		{
			Zenith_CameraRenderData xCamera;
			g_pfnZenithCameraGather(xCamera);
			if (xCamera.m_bValid)
			{
				xBlendPosition = Zenith_Maths::Vector3(xCamera.m_xPositionPad);
				bHaveBlendPosition = true;
			}
		}

		// GLOBAL atmospheres: authority candidates, exactly as before.
		xScenes.QueryAllScenes<Zenith_AtmosphereComponent>().ForEach(
			[&](Zenith_EntityID xID, Zenith_AtmosphereComponent& xAtmo)
			{
				if (xAtmo.IsLocalBlendVolume())
				{
					return;   // handled by the volume walk below
				}
				const bool bInActive = pxActiveScene != nullptr
					&& xScenes.GetSceneDataForEntity(xID) == pxActiveScene;
				EnvironmentCandidate* pxC = FindOrCreateCandidate(axCandidates, xID, bInActive);
				pxC->m_bHasAtmosphere = true;
				pxC->m_xMedium = xAtmo.GetMedium();
			});

		// LOCAL volumes: a separate walk that also requires a transform (the
		// sphere needs a centre). With no camera resolved yet -- boot, or a
		// headless scene with none -- there is no position to weight against, so
		// no volume contributes and the global base resolves alone.
		if (bHaveBlendPosition)
		{
			xScenes.QueryAllScenes<Zenith_AtmosphereComponent, Zenith_TransformComponent>().ForEach(
				[&](Zenith_EntityID xID, Zenith_AtmosphereComponent& xAtmo, Zenith_TransformComponent& xTransform)
				{
					if (!xAtmo.IsLocalBlendVolume())
					{
						return;
					}
					Zenith_Maths::Vector3 xCentre;
					xTransform.GetPosition(xCentre);
					const float fWeight = Zenith_ComputeBlendVolumeWeight(
						glm::length(xBlendPosition - xCentre),
						xAtmo.GetBlendRadius(), xAtmo.GetBlendFalloff());
					if (fWeight <= 0.0f)
					{
						return;
					}
					EnvironmentBlendVolume xVolume;
					xVolume.m_ulPackedID = xID.GetPacked();
					xVolume.m_fPriority  = xAtmo.GetBlendPriority();
					xVolume.m_fWeight    = fWeight;
					xVolume.m_xMedium    = xAtmo.GetMedium();
					axBlendVolumes.PushBack(xVolume);
				});
		}

		// Choose the one authoritative environment entity.
		const EnvironmentCandidate* pxWinner = nullptr;
		for (u_int u = 0u; u < axCandidates.GetSize(); u++)
		{
			const EnvironmentCandidate& rC = axCandidates.Get(u);
			if (pxWinner == nullptr
				|| EnvironmentPrecedes(rC, pxWinner->m_xID.m_uIndex,
					pxWinner->m_xID.m_uGeneration, pxWinner->m_bInActiveScene))
			{
				pxWinner = &rC;
			}
		}

		// Diagnostics counts.
		for (u_int u = 0u; u < axCandidates.GetSize(); u++)
		{
			if (axCandidates.Get(u).m_bHasSun)         xOut.m_uSunAuthoredCount++;
			if (axCandidates.Get(u).m_bHasAtmosphere)  xOut.m_uAtmosphereAuthoredCount++;
		}
		xOut.m_uEnvironmentEntityCount = axCandidates.GetSize();

		if (pxWinner != nullptr)
		{
			if (pxWinner->m_bHasSun)
			{
				xOut.m_bSunAuthored = true;
				xOut.m_xSunDirection = pxWinner->m_xSunDir;
				xOut.m_bSunSourceIsInActiveScene = pxWinner->m_bInActiveScene;
			}
			if (pxWinner->m_bHasAtmosphere)
			{
				xOut.m_bAtmosphereAuthored = true;
			}
			xOut.m_uEnvironmentEntityIndex = pxWinner->m_xID.m_uIndex;
			xOut.m_uEnvironmentEntityGeneration = pxWinner->m_xID.m_uGeneration;
			xOut.m_bEnvironmentSourceIsInActiveScene = pxWinner->m_bInActiveScene;
		}

		// ---- Medium: the global base, then every local volume in order -------
		// The base is the winner's medium (or the physical defaults if the winner
		// authored no atmosphere). Each local volume then LERPS the accumulated
		// medium toward its own values by its weight. With no volumes this is the
		// single-winner rule verbatim, which is why existing scenes are unaffected.
		Zenith_AtmosphereMedium xMedium;
		if (pxWinner != nullptr && pxWinner->m_bHasAtmosphere)
		{
			xMedium = pxWinner->m_xMedium;
		}

		// Insertion sort by (priority, id) -- volume counts are tiny, and this is
		// what makes the composed result independent of ECS query order.
		for (u_int i = 1u; i < axBlendVolumes.GetSize(); i++)
		{
			const EnvironmentBlendVolume xKey = axBlendVolumes.Get(i);
			u_int j = i;
			while (j > 0u && BlendVolumePrecedes(xKey, axBlendVolumes.Get(j - 1u)))
			{
				axBlendVolumes.Get(j) = axBlendVolumes.Get(j - 1u);
				j--;
			}
			axBlendVolumes.Get(j) = xKey;
		}
		for (u_int u = 0u; u < axBlendVolumes.GetSize(); u++)
		{
			const EnvironmentBlendVolume& rV = axBlendVolumes.Get(u);
			xMedium = Zenith_BlendAtmosphereLayer(xMedium, rV.m_xMedium, rV.m_fWeight);
			xOut.m_fBlendWeightTotal += rV.m_fWeight;
		}
		xOut.m_uBlendVolumesApplied = axBlendVolumes.GetSize();
		if (axBlendVolumes.GetSize() > 0u)
		{
			// A local volume authored the medium the renderer will use, even when
			// no global environment entity exists.
			xOut.m_bAtmosphereAuthored = true;
		}

		xOut.m_fRayleighScale       = xMedium.m_fRayleighScale;
		xOut.m_fMieScale            = xMedium.m_fMieScale;
		xOut.m_fMieG                = xMedium.m_fMieG;
		xOut.m_fRayleighScaleHeight = xMedium.m_fRayleighScaleHeight;
		xOut.m_fMieScaleHeight      = xMedium.m_fMieScaleHeight;
		xOut.m_fGroundAlbedo        = xMedium.m_fGroundAlbedo;

		// Conflict warning: more than one environment entity is a conflict (e.g.
		// a Sun on one entity + an atmosphere on another -- the atmosphere is
		// ignored, since the world is defined by the one winning entity). Throttled
		// by a deterministic, query-order-independent signature so the warning
		// fires once per distinct conflict set, not every frame.
		if (xOut.m_uEnvironmentEntityCount > 1u && pxWinner != nullptr)
		{
			// Signature over EVERY candidate (id + Sun mask + Atmosphere mask +
			// active-scene membership), not just the winner: a LOSING candidate
			// gaining or losing a Sun/Atmosphere changes the counts the warning
			// reports and which authored halves are being dropped, so it must
			// produce a fresh warning. The helper is pure + query-order
			// independent (see Core/Zenith_EnvironmentAuthority.h).
			Zenith_Vector<Zenith_EnvironmentConflictCandidate> axSigCandidates;
			for (u_int u = 0u; u < axCandidates.GetSize(); u++)
			{
				const EnvironmentCandidate& rC = axCandidates.Get(u);
				Zenith_EnvironmentConflictCandidate xSig;
				xSig.m_ulPackedEntityID = rC.m_xID.GetPacked();
				xSig.m_bHasSun          = rC.m_bHasSun;
				xSig.m_bHasAtmosphere   = rC.m_bHasAtmosphere;
				xSig.m_bInActiveScene   = rC.m_bInActiveScene;
				axSigCandidates.PushBack(xSig);
			}
			const uint64_t ulSig = Zenith_ComputeEnvironmentConflictSignature(
				axSigCandidates.GetDataPointer(), axSigCandidates.GetSize(),
				pxWinner->m_xID.GetPacked(), pxActiveScene != nullptr);

			if (ulSig != s_ulLastEnvironmentConflictSignature)
			{
#ifdef ZENITH_TESTING
				s_uEnvironmentConflictWarningCount++;
#endif
				Zenith_Warning(LOG_CATEGORY_ECS,
					"Environment authority conflict: %u environment entities (%u Suns, %u atmospheres); "
					"entity %u wins (%s). Rule: active scene first, then lowest stable entity ID. "
					"Sun and atmosphere are resolved together from the one winning entity. "
					"To vary the atmosphere by region, give the extra entity a Blend Radius > 0 "
					"instead -- a local volume blends and never conflicts.",
					xOut.m_uEnvironmentEntityCount, xOut.m_uSunAuthoredCount, xOut.m_uAtmosphereAuthoredCount,
					xOut.m_uEnvironmentEntityIndex,
					xOut.m_bEnvironmentSourceIsInActiveScene ? "active scene" : "no active-scene environment");
				s_ulLastEnvironmentConflictSignature = ulSig;

#ifdef ZENITH_TOOLS
				// A tools build is an AUTHORING session, and this is silent data
				// loss: the losing entity's Sun and/or atmosphere is dropped on
				// the floor and the scene renders as though it were never
				// authored. Fail loudly here rather than let it ship. Runtime
				// builds keep the warning only -- a shipped game must not die on
				// a content bug the player cannot fix.
				Zenith_Assert(!s_bEnvironmentConflictAssertsEnabled,
					"Environment authority conflict: %u global environment entities. Exactly one entity may "
					"carry the scene Sun/Atmosphere. Entity %u wins; the others are IGNORED. Delete them, or "
					"set Blend Radius > 0 to turn them into local atmosphere volumes.",
					xOut.m_uEnvironmentEntityCount, xOut.m_uEnvironmentEntityIndex);
#endif
			}
		}
		else
		{
			s_ulLastEnvironmentConflictSignature = 0u;
		}

		// Cache for the TOOLS property-panel banner (diagnostics only; reading it
		// never re-gathers, so a panel cannot trip the assert above).
		s_xLastResolvedEnvironment = xOut;
	}
}

const Zenith_EnvironmentAuthorityData& Zenith_GetLastResolvedEnvironmentAuthority()
{
	return s_xLastResolvedEnvironment;
}

#ifdef ZENITH_TOOLS
Zenith_ScopedEnvironmentConflictAssertSuppression::Zenith_ScopedEnvironmentConflictAssertSuppression()
	: m_bPrevious(s_bEnvironmentConflictAssertsEnabled)
{
	s_bEnvironmentConflictAssertsEnabled = false;
}

Zenith_ScopedEnvironmentConflictAssertSuppression::~Zenith_ScopedEnvironmentConflictAssertSuppression()
{
	s_bEnvironmentConflictAssertsEnabled = m_bPrevious;
}
#endif

Zenith_EnvironmentAuthorityGatherFn g_pfnZenithEnvironmentAuthorityGather = &GatherEnvironmentAuthority;

#ifdef ZENITH_TESTING
uint64_t Zenith_GetLastEnvironmentConflictSignatureForTest()
{
	return s_ulLastEnvironmentConflictSignature;
}

u_int Zenith_GetEnvironmentConflictWarningCountForTest()
{
	return s_uEnvironmentConflictWarningCount;
}

void Zenith_ResetEnvironmentConflictThrottleForTest()
{
	s_ulLastEnvironmentConflictSignature = 0u;
	s_uEnvironmentConflictWarningCount = 0u;
}
#endif

#include "EntityComponent/Components/Zenith_LightComponent.Tests.inl"
#include "EntityComponent/Components/Zenith_EnvironmentAuthority.Tests.inl"
