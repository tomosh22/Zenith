#pragma once

#include "Maths/Zenith_Maths.h"

namespace Zenith_UI {

struct UIStyle
{
	// Fill
	Zenith_Maths::Vector4 m_xFillColor = {1.0f, 1.0f, 1.0f, 1.0f};
	Zenith_Maths::Vector4 m_xGradientBottomColor = {-1, -1, -1, -1}; // -1 sentinel = no gradient

	// Border
	Zenith_Maths::Vector4 m_xBorderColor = {0.0f, 0.0f, 0.0f, 0.0f};
	float m_fBorderThickness = 0.0f;

	// Corners
	float m_fCornerRadius = 0.0f;

	// Shadow
	bool m_bShadowEnabled = false;
	Zenith_Maths::Vector4 m_xShadowColor = {0.0f, 0.0f, 0.0f, 0.4f};
	Zenith_Maths::Vector2 m_xShadowOffset = {4.0f, 4.0f};
	float m_fShadowSpread = 4.0f;

	static UIStyle Lerp(const UIStyle& xA, const UIStyle& xB, float fT)
	{
		UIStyle xResult;
		xResult.m_xFillColor = glm::mix(xA.m_xFillColor, xB.m_xFillColor, fT);

		// Gradient: lerp if either has gradient enabled
		bool bAHasGradient = xA.m_xGradientBottomColor.x >= 0.0f;
		bool bBHasGradient = xB.m_xGradientBottomColor.x >= 0.0f;
		if (bAHasGradient || bBHasGradient)
		{
			Zenith_Maths::Vector4 xGradA = bAHasGradient ? xA.m_xGradientBottomColor : xA.m_xFillColor;
			Zenith_Maths::Vector4 xGradB = bBHasGradient ? xB.m_xGradientBottomColor : xB.m_xFillColor;
			xResult.m_xGradientBottomColor = glm::mix(xGradA, xGradB, fT);
		}
		else
		{
			xResult.m_xGradientBottomColor = {-1, -1, -1, -1};
		}

		xResult.m_xBorderColor = glm::mix(xA.m_xBorderColor, xB.m_xBorderColor, fT);
		xResult.m_fBorderThickness = glm::mix(xA.m_fBorderThickness, xB.m_fBorderThickness, fT);
		xResult.m_fCornerRadius = glm::mix(xA.m_fCornerRadius, xB.m_fCornerRadius, fT);

		xResult.m_bShadowEnabled = (fT < 0.5f) ? xA.m_bShadowEnabled : xB.m_bShadowEnabled;
		xResult.m_xShadowColor = glm::mix(xA.m_xShadowColor, xB.m_xShadowColor, fT);
		xResult.m_xShadowOffset = glm::mix(xA.m_xShadowOffset, xB.m_xShadowOffset, fT);
		xResult.m_fShadowSpread = glm::mix(xA.m_fShadowSpread, xB.m_fShadowSpread, fT);

		return xResult;
	}
};

} // namespace Zenith_UI
