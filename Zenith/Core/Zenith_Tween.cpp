#include "Zenith.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/easing.hpp>

#include "Core/Zenith_Tween.h"

float Zenith_ApplyEasing(Zenith_EasingType eType, float fT)
{
	fT = glm::clamp(fT, 0.0f, 1.0f);

	switch (eType)
	{
	case EASING_LINEAR:          return fT;
	case EASING_QUAD_IN:         return glm::quadraticEaseIn(fT);
	case EASING_QUAD_OUT:        return glm::quadraticEaseOut(fT);
	case EASING_QUAD_IN_OUT:     return glm::quadraticEaseInOut(fT);
	case EASING_CUBIC_IN:        return glm::cubicEaseIn(fT);
	case EASING_CUBIC_OUT:       return glm::cubicEaseOut(fT);
	case EASING_CUBIC_IN_OUT:    return glm::cubicEaseInOut(fT);
	case EASING_ELASTIC_IN:      return glm::elasticEaseIn(fT);
	case EASING_ELASTIC_OUT:     return glm::elasticEaseOut(fT);
	case EASING_ELASTIC_IN_OUT:  return glm::elasticEaseInOut(fT);
	case EASING_BOUNCE_IN:       return glm::bounceEaseIn(fT);
	case EASING_BOUNCE_OUT:      return glm::bounceEaseOut(fT);
	case EASING_BOUNCE_IN_OUT:   return glm::bounceEaseInOut(fT);
	case EASING_BACK_IN:         return glm::backEaseIn(fT);
	case EASING_BACK_OUT:        return glm::backEaseOut(fT);
	case EASING_BACK_IN_OUT:     return glm::backEaseInOut(fT);
	case EASING_SINE_IN:         return glm::sineEaseIn(fT);
	case EASING_SINE_OUT:        return glm::sineEaseOut(fT);
	case EASING_SINE_IN_OUT:     return glm::sineEaseInOut(fT);
	default:                     return fT;
	}
}

const char* Zenith_GetEasingTypeName(Zenith_EasingType eType)
{
	switch (eType)
	{
	case EASING_LINEAR:          return "Linear";
	case EASING_QUAD_IN:         return "Quad In";
	case EASING_QUAD_OUT:        return "Quad Out";
	case EASING_QUAD_IN_OUT:     return "Quad In/Out";
	case EASING_CUBIC_IN:        return "Cubic In";
	case EASING_CUBIC_OUT:       return "Cubic Out";
	case EASING_CUBIC_IN_OUT:    return "Cubic In/Out";
	case EASING_ELASTIC_IN:      return "Elastic In";
	case EASING_ELASTIC_OUT:     return "Elastic Out";
	case EASING_ELASTIC_IN_OUT:  return "Elastic In/Out";
	case EASING_BOUNCE_IN:       return "Bounce In";
	case EASING_BOUNCE_OUT:      return "Bounce Out";
	case EASING_BOUNCE_IN_OUT:   return "Bounce In/Out";
	case EASING_BACK_IN:         return "Back In";
	case EASING_BACK_OUT:        return "Back Out";
	case EASING_BACK_IN_OUT:     return "Back In/Out";
	case EASING_SINE_IN:         return "Sine In";
	case EASING_SINE_OUT:        return "Sine Out";
	case EASING_SINE_IN_OUT:     return "Sine In/Out";
	default:                     return "Unknown";
	}
}

float Zenith_TweenInstance::GetNormalizedTime() const
{
	if (m_fDuration <= 0.0f)
		return 1.0f;

	float fActiveTime = m_fElapsed - m_fDelay;
	if (fActiveTime < 0.0f)
		return 0.0f;

	float fRawT = glm::clamp(fActiveTime / m_fDuration, 0.0f, 1.0f);
	return Zenith_ApplyEasing(m_eEasing, fRawT);
}
