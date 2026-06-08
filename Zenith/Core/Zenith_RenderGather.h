#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// ---------------------------------------------------------------------------
// Render-gather boundary (Wave 3: Flux<-EC reverse-edge decoupling).
//
// Renderer features must not reach into EntityComponent (no
// #include "EntityComponent/Components/Zenith_*Component.h" in Flux). Instead,
// the EC side GATHERS the per-feature render inputs into a renderer-neutral POD
// list (defined here, in the Core base layer that both Flux and EC already
// depend on) and publishes it through a function-pointer the renderer calls.
// The renderer consumes only this neutral data + the function pointer — never
// the component types. The EC-side gatherer is the only place that names the
// components, which is the natural (forward) direction.
// ---------------------------------------------------------------------------

// Renderer-neutral light category (mirrors LIGHT_TYPE; mapped by the gatherer so
// the renderer never needs the EC LIGHT_TYPE enum).
enum Zenith_LightRenderType
{
	ZENITH_LIGHT_RENDER_POINT,
	ZENITH_LIGHT_RENDER_SPOT,
	ZENITH_LIGHT_RENDER_DIRECTIONAL,
};

// One light's render inputs, extracted from a Zenith_LightComponent (+ its
// entity transform) by the EC-side gatherer. Pure data — Maths only.
struct Zenith_LightRenderData
{
	Zenith_LightRenderType m_eType            = ZENITH_LIGHT_RENDER_POINT;
	Zenith_Maths::Vector3  m_xColor           = Zenith_Maths::Vector3(0.0f);
	float                  m_fIntensity       = 0.0f;
	Zenith_Maths::Vector3  m_xWorldPosition   = Zenith_Maths::Vector3(0.0f);
	float                  m_fRange           = 0.0f;
	Zenith_Maths::Vector3  m_xWorldDirection  = Zenith_Maths::Vector3(0.0f);
	float                  m_fSpotInnerAngle  = 0.0f;
	float                  m_fSpotOuterAngle  = 0.0f;
	u_int                  m_uEntityIndex     = 0; // for diagnostics only
};

// The EC side sets this to its gatherer; Flux_DynamicLights calls it each frame.
// Defined (and pointed at the gatherer) in the EC TU that owns the light gather,
// so the linker pulls that TU in to resolve this symbol.
using Zenith_LightGatherFn = void (*)(Zenith_Vector<Zenith_LightRenderData>& xOut);
extern Zenith_LightGatherFn g_pfnZenithLightGather;

// The main camera's render inputs, resolved + extracted EC-side so Flux_Graphics
// never names Zenith_CameraComponent / Zenith_CameraResolve. m_bValid is false when
// there is no main camera (the renderer then keeps its previous-frame / default state).
struct Zenith_CameraRenderData
{
	bool                  m_bValid       = false;
	Zenith_Maths::Matrix4 m_xViewMatrix  = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Matrix4 m_xProjMatrix  = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector4 m_xPositionPad  = Zenith_Maths::Vector4(0.0f); // matches Flux frame-constants Vector4
	float                 m_fNearPlane   = 0.1f;
	float                 m_fFarPlane    = 1000.0f;
	float                 m_fFOV         = 1.0472f;
	float                 m_fAspectRatio = 1.7778f;
};

using Zenith_CameraGatherFn = void (*)(Zenith_CameraRenderData& xOut);
extern Zenith_CameraGatherFn g_pfnZenithCameraGather;
