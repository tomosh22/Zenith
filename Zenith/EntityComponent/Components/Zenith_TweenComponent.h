#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Core/Zenith_Tween.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"

//=============================================================================
// Zenith_TweenComponent
// Lightweight property tween system for position, scale, rotation
//=============================================================================
class Zenith_TweenComponent
{
public:
	Zenith_TweenComponent(Zenith_Entity& xEntity);
	~Zenith_TweenComponent() = default;

	Zenith_TweenComponent(Zenith_TweenComponent&& xOther) noexcept;
	Zenith_TweenComponent& operator=(Zenith_TweenComponent&& xOther) noexcept;

	Zenith_TweenComponent(const Zenith_TweenComponent&) = delete;
	Zenith_TweenComponent& operator=(const Zenith_TweenComponent&) = delete;

	// ECS lifecycle
	void OnUpdate(float fDt);

	// Start a tween from current transform value to target
	void TweenPosition(const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);
	void TweenScale(const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);
	void TweenRotation(const Zenith_Maths::Vector3& xToEulerDegrees, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);

	// Start a tween with explicit from/to values
	void TweenPositionFromTo(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);
	void TweenScaleFromTo(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);

	// Configure the most recently added tween
	void SetOnComplete(Zenith_TweenCallback pfnCallback, void* pUserData = nullptr);
	void SetDelay(float fDelay);
	void SetLoop(bool bLoop, bool bPingPong = false);

	// Cancel all active tweens
	void CancelAll();

	// Cancel tweens on a specific property
	void CancelByProperty(Zenith_TweenProperty eProperty);

	// Query
	bool HasActiveTweens() const;
	uint32_t GetActiveTweenCount() const;

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Static helper: adds TweenComponent if needed, creates scale tween
	static void ScaleTo(Zenith_Entity& xEntity, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing = EASING_QUAD_OUT);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	Zenith_Entity m_xParentEntity;
	Zenith_Vector<Zenith_TweenInstance> m_xActiveTweens;
};
