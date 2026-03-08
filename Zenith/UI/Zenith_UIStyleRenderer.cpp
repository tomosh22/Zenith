#include "Zenith.h"
#include "UI/Zenith_UIStyleRenderer.h"
#include "UI/Zenith_UICanvas.h"

namespace Zenith_UI {

void UIStyleRenderer::RenderStyledRect(
	Zenith_UICanvas& xCanvas,
	const UIStyle& xStyle,
	const Zenith_Maths::Vector4& xBounds,
	float fAlpha)
{
	float fLeft = xBounds.x;
	float fTop = xBounds.y;
	float fRight = xBounds.z;
	float fBottom = xBounds.w;

	// 1. Shadow (back-most layer)
	if (xStyle.m_bShadowEnabled)
	{
		Zenith_Maths::Vector4 xShadowBounds = {
			fLeft + xStyle.m_xShadowOffset.x - xStyle.m_fShadowSpread,
			fTop + xStyle.m_xShadowOffset.y - xStyle.m_fShadowSpread,
			fRight + xStyle.m_xShadowOffset.x + xStyle.m_fShadowSpread,
			fBottom + xStyle.m_xShadowOffset.y + xStyle.m_fShadowSpread
		};

		Zenith_Maths::Vector4 xShadowColor = xStyle.m_xShadowColor;
		xShadowColor.a *= fAlpha;

		xCanvas.SubmitQuad(xShadowBounds, xShadowColor, 0,
			xStyle.m_fCornerRadius + xStyle.m_fShadowSpread);
	}

	// 2. Border (middle layer)
	if (xStyle.m_fBorderThickness > 0.0f && xStyle.m_xBorderColor.a > 0.0f)
	{
		Zenith_Maths::Vector4 xBorderColor = xStyle.m_xBorderColor;
		xBorderColor.a *= fAlpha;

		xCanvas.SubmitQuad(xBounds, xBorderColor, 0,
			xStyle.m_fCornerRadius);
	}

	// 3. Fill (front-most layer)
	{
		Zenith_Maths::Vector4 xFillBounds = {
			fLeft + xStyle.m_fBorderThickness,
			fTop + xStyle.m_fBorderThickness,
			fRight - xStyle.m_fBorderThickness,
			fBottom - xStyle.m_fBorderThickness
		};

		Zenith_Maths::Vector4 xFillColor = xStyle.m_xFillColor;
		xFillColor.a *= fAlpha;

		Zenith_Maths::Vector4 xGradientColor = xStyle.m_xGradientBottomColor;
		if (xGradientColor.x >= 0.0f)
		{
			xGradientColor.a *= fAlpha;
		}

		float fInnerRadius = xStyle.m_fCornerRadius > 0.0f
			? glm::max(xStyle.m_fCornerRadius - xStyle.m_fBorderThickness, 0.0f)
			: 0.0f;

		xCanvas.SubmitQuad(xFillBounds, xFillColor, 0,
			fInnerRadius, xGradientColor);
	}
}

} // namespace Zenith_UI
