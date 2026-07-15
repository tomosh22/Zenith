#include "Zenith.h"

// ============================================================================
// ZM_HumanAppearance -- SC3's deterministic human albedo painter and categorical
// hair/attachment silhouette appenders. ALBEDO owns exactly six up-front draws;
// appearance geometry owns no RNG and never changes the shared skeleton.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"

#include <cmath>   // ceilf, floorf, fabsf

namespace
{
	using Zenith_Maths::Vector3;
	using Zenith_Maths::Vector4;

	constexpr u_int uHAIR_STYLE_CROP      = 0u;
	constexpr u_int uHAIR_STYLE_BOB       = 1u;
	constexpr u_int uHAIR_STYLE_SWEPT     = 2u;
	constexpr u_int uHAIR_STYLE_BUN       = 3u;
	constexpr u_int uHAIR_STYLE_LONG_BACK = 4u;
	constexpr u_int uHAIR_STYLE_TOPKNOT   = 5u;
	static_assert(uHAIR_STYLE_TOPKNOT + 1u == uZM_HUMAN_HAIR_STYLE_COUNT,
		"human hair style order must remain Crop/Bob/Swept/Bun/Long-back/Topknot");

	constexpr u_int uHUMAN_BONE_SPINE = 1u;
	constexpr u_int uHUMAN_BONE_HEAD  = 3u;
	static_assert(uHUMAN_BONE_HEAD < uZM_HUMAN_BONE_COUNT,
		"appearance geometry must bind the frozen shared human skeleton");

	struct ZM_HumanAtlasRect
	{
		u_int m_uMinX = 0u;
		u_int m_uMinY = 0u;
		u_int m_uMaxX = 0u;
		u_int m_uMaxY = 0u;
	};

	struct ZM_HumanPaintContext
	{
		u_int m_uSkinNoiseSeed = 0u;
		u_int m_uHairNoiseSeed = 0u;
		u_int m_uOutfitNoiseSeed = 0u;
		float m_fSkinValue = 1.0f;
		float m_fHairValue = 1.0f;
		float m_fOutfitValue = 1.0f;
	};

	struct ZM_HumanOutfitPalette
	{
		Vector3 m_xPrimary = Vector3(0.0f);
		Vector3 m_xSecondary = Vector3(0.0f);
		Vector3 m_xAccent = Vector3(0.0f);
	};

	float ZM_HumanClamp01(float fValue)
	{
		if (fValue < 0.0f)
		{
			return 0.0f;
		}
		if (fValue > 1.0f)
		{
			return 1.0f;
		}
		return fValue;
	}

	Vector3 ZM_HumanClampColour(const Vector3& xColour)
	{
		return Vector3(
			ZM_HumanClamp01(xColour.x),
			ZM_HumanClamp01(xColour.y),
			ZM_HumanClamp01(xColour.z));
	}

	bool ZM_HumanInRange(float fValue, float fMin, float fMax)
	{
		return fValue >= fMin && fValue <= fMax;
	}

	ZM_HumanAtlasRect ZM_HumanIslandRect(const ZM_GenUVIsland& xIsland,
		u_int uWidth, u_int uHeight)
	{
		ZM_HumanAtlasRect xRect;
		xRect.m_uMinX = static_cast<u_int>(ceilf(xIsland.m_fU0 * static_cast<float>(uWidth)));
		xRect.m_uMinY = static_cast<u_int>(ceilf(xIsland.m_fV0 * static_cast<float>(uHeight)));
		xRect.m_uMaxX = static_cast<u_int>(floorf(xIsland.m_fU1 * static_cast<float>(uWidth))) - 1u;
		xRect.m_uMaxY = static_cast<u_int>(floorf(xIsland.m_fV1 * static_cast<float>(uHeight))) - 1u;
		Zenith_Assert(xRect.m_uMinX <= xRect.m_uMaxX && xRect.m_uMinY <= xRect.m_uMaxY,
			"human atlas island has an empty core rectangle");
		return xRect;
	}

	float ZM_HumanLocalCoord(u_int uValue, u_int uMin, u_int uMax)
	{
		return (uMax > uMin)
			? static_cast<float>(uValue - uMin) / static_cast<float>(uMax - uMin)
			: 0.0f;
	}

	template <typename TFn>
	void ZM_PaintHumanIsland(ZM_GenImage& xImage, const ZM_GenUVIsland& xIsland, TFn&& xFn)
	{
		const ZM_HumanAtlasRect xRect = ZM_HumanIslandRect(
			xIsland, xImage.GetWidth(), xImage.GetHeight());
		for (u_int uY = xRect.m_uMinY; uY <= xRect.m_uMaxY; ++uY)
		{
			const float fV = ZM_HumanLocalCoord(uY, xRect.m_uMinY, xRect.m_uMaxY);
			for (u_int uX = xRect.m_uMinX; uX <= xRect.m_uMaxX; ++uX)
			{
				const float fU = ZM_HumanLocalCoord(uX, xRect.m_uMinX, xRect.m_uMaxX);
				const Vector3 xColour = ZM_HumanClampColour(xFn(fU, fV));
				xImage.Set(uY, uX, Vector4(xColour, 1.0f));
			}
		}
	}

	void ZM_DilateHumanIsland(ZM_GenImage& xImage, const ZM_GenUVIsland& xIsland)
	{
		const ZM_HumanAtlasRect xRect = ZM_HumanIslandRect(
			xIsland, xImage.GetWidth(), xImage.GetHeight());
		Zenith_Assert(xRect.m_uMinX > 0u && xRect.m_uMinY > 0u
			&& xRect.m_uMaxX + 1u < xImage.GetWidth()
			&& xRect.m_uMaxY + 1u < xImage.GetHeight(),
			"human atlas island lacks its one-texel dilation gutter");

		for (u_int uY = xRect.m_uMinY - 1u; uY <= xRect.m_uMaxY + 1u; ++uY)
		{
			for (u_int uX = xRect.m_uMinX - 1u; uX <= xRect.m_uMaxX + 1u; ++uX)
			{
				const bool bInsideCore = uX >= xRect.m_uMinX && uX <= xRect.m_uMaxX
					&& uY >= xRect.m_uMinY && uY <= xRect.m_uMaxY;
				if (bInsideCore)
				{
					continue;
				}

				const u_int uSourceX = (uX < xRect.m_uMinX) ? xRect.m_uMinX
					: ((uX > xRect.m_uMaxX) ? xRect.m_uMaxX : uX);
				const u_int uSourceY = (uY < xRect.m_uMinY) ? xRect.m_uMinY
					: ((uY > xRect.m_uMaxY) ? xRect.m_uMaxY : uY);
				xImage.Set(uY, uX, xImage.Get(uSourceY, uSourceX));
			}
		}
	}

	Vector3 ZM_HumanNoiseColour(const Vector3& xBase, float fU, float fV,
		u_int uSeed, float fValue, float fNoiseBase, float fNoiseSpan)
	{
		const float fNoise = ZM_GenNoise::FBM2D(
			fU * 8.0f, fV * 8.0f, uSeed, 3u, 2.0f, 0.5f);
		return ZM_HumanClampColour(xBase * (fValue * (fNoiseBase + fNoiseSpan * fNoise)));
	}

	Vector3 ZM_HumanSkinPaint(const Vector3& xBase, float fU, float fV,
		const ZM_HumanPaintContext& xPaint)
	{
		return ZM_HumanNoiseColour(xBase, fU, fV, xPaint.m_uSkinNoiseSeed,
			xPaint.m_fSkinValue, 0.98f, 0.04f);
	}

	Vector3 ZM_HumanHairPaint(const Vector3& xBase, float fU, float fV,
		const ZM_HumanPaintContext& xPaint)
	{
		return ZM_HumanNoiseColour(xBase, fU, fV, xPaint.m_uHairNoiseSeed,
			xPaint.m_fHairValue, 0.92f, 0.16f);
	}

	Vector3 ZM_HumanClothPaint(const Vector3& xBase, float fU, float fV,
		const ZM_HumanPaintContext& xPaint)
	{
		return ZM_HumanNoiseColour(xBase, fU, fV, xPaint.m_uOutfitNoiseSeed,
			xPaint.m_fOutfitValue, 0.94f, 0.12f);
	}

	Vector3 ZM_HumanSkinColour(ZM_HUMAN_SKIN_TONE eTone)
	{
		switch (eTone)
		{
		case ZM_HUMAN_SKIN_PALE:  return Vector3(0.94f, 0.82f, 0.74f);
		case ZM_HUMAN_SKIN_FAIR:  return Vector3(0.88f, 0.72f, 0.60f);
		case ZM_HUMAN_SKIN_TAN:   return Vector3(0.76f, 0.58f, 0.44f);
		case ZM_HUMAN_SKIN_BROWN: return Vector3(0.54f, 0.38f, 0.28f);
		case ZM_HUMAN_SKIN_DARK:  return Vector3(0.34f, 0.24f, 0.18f);
		default:                  return Vector3(0.80f, 0.66f, 0.55f);
		}
	}

	bool ZM_HumanThemePalette(ZM_HUMAN_ID eId, ZM_TypePalette& xTheme)
	{
		switch (eId)
		{
		case ZM_HUMAN_LEADER_FENNA:    xTheme = ZM_SynthTypePalette(ZM_TYPE_GRASS);    return true;
		case ZM_HUMAN_LEADER_BRAM:     xTheme = ZM_SynthTypePalette(ZM_TYPE_FIRE);     return true;
		case ZM_HUMAN_LEADER_MARIS:    xTheme = ZM_SynthTypePalette(ZM_TYPE_WATER);    return true;
		case ZM_HUMAN_LEADER_TESSA:    xTheme = ZM_SynthTypePalette(ZM_TYPE_ELECTRIC); return true;
		case ZM_HUMAN_LEADER_AQUILO:   xTheme = ZM_SynthTypePalette(ZM_TYPE_SKY);      return true;
		case ZM_HUMAN_LEADER_MORWENNA: xTheme = ZM_SynthTypePalette(ZM_TYPE_PHANTOM);  return true;
		case ZM_HUMAN_LEADER_HALVARD:  xTheme = ZM_SynthTypePalette(ZM_TYPE_ICE);      return true;
		case ZM_HUMAN_LEADER_VARDIS:   xTheme = ZM_SynthTypePalette(ZM_TYPE_DRAKE);    return true;
		case ZM_HUMAN_ELITE_CASSIA:    xTheme = ZM_SynthTypePalette(ZM_TYPE_VENOM);    return true;
		case ZM_HUMAN_ELITE_TORBEN:    xTheme = ZM_SynthTypePalette(ZM_TYPE_BRAWL);    return true;
		case ZM_HUMAN_ELITE_LUMEN:     xTheme = ZM_SynthTypePalette(ZM_TYPE_MIND);     return true;
		case ZM_HUMAN_ELITE_SABLE:     xTheme = ZM_SynthTypePalette(ZM_TYPE_UMBRAL);   return true;
		case ZM_HUMAN_CHAMPION_ELARA:  xTheme = ZM_SynthBlendPalette(ZM_TYPE_DRAKE, ZM_TYPE_SKY); return true;
		default: return false;
		}
	}

	Vector3 ZM_HumanHairColour(const ZM_HumanRecipe& xRecipe)
	{
		switch (xRecipe.m_eHairColour)
		{
		case ZM_HUMAN_HAIR_BLACK:  return Vector3(0.035f, 0.025f, 0.022f);
		case ZM_HUMAN_HAIR_BROWN:  return Vector3(0.180f, 0.085f, 0.035f);
		case ZM_HUMAN_HAIR_BLONDE: return Vector3(0.720f, 0.520f, 0.200f);
		case ZM_HUMAN_HAIR_AUBURN: return Vector3(0.400f, 0.120f, 0.035f);
		case ZM_HUMAN_HAIR_GREY:   return Vector3(0.340f, 0.360f, 0.380f);
		case ZM_HUMAN_HAIR_WHITE:  return Vector3(0.780f, 0.800f, 0.820f);
		case ZM_HUMAN_HAIR_DYED:
		{
			ZM_TypePalette xTheme;
			return ZM_HumanThemePalette(xRecipe.m_eId, xTheme)
				? xTheme.m_xAccent
				: Vector3(0.180f, 0.550f, 0.620f);
		}
		default: return Vector3(0.180f, 0.085f, 0.035f);
		}
	}

	ZM_HumanOutfitPalette ZM_HumanOutfitColours(const ZM_HumanRecipe& xRecipe)
	{
		ZM_HumanOutfitPalette xOut;
		ZM_TypePalette xTheme;
		const bool bHasTheme = ZM_HumanThemePalette(xRecipe.m_eId, xTheme);
		switch (xRecipe.m_eOutfit)
		{
		case ZM_HUMAN_OUTFIT_TRAVELER:
			xOut.m_xPrimary = Vector3(0.08f, 0.22f, 0.30f);
			xOut.m_xSecondary = Vector3(0.10f, 0.32f, 0.31f);
			xOut.m_xAccent = Vector3(0.65f, 0.36f, 0.12f);
			break;
		case ZM_HUMAN_OUTFIT_LABCOAT:
			xOut.m_xPrimary = Vector3(0.78f, 0.80f, 0.74f);
			xOut.m_xSecondary = Vector3(0.18f, 0.30f, 0.32f);
			xOut.m_xAccent = Vector3(0.08f, 0.48f, 0.49f);
			break;
		case ZM_HUMAN_OUTFIT_CASUAL:
			xOut.m_xPrimary = Vector3(0.44f, 0.18f, 0.09f);
			xOut.m_xSecondary = Vector3(0.20f, 0.30f, 0.16f);
			xOut.m_xAccent = Vector3(0.68f, 0.48f, 0.20f);
			break;
		case ZM_HUMAN_OUTFIT_LEADER:
			if (bHasTheme)
			{
				xOut.m_xPrimary = xTheme.m_xBase;
				xOut.m_xSecondary = xTheme.m_xBase * 0.65f;
				xOut.m_xAccent = xTheme.m_xAccent;
			}
			else
			{
				xOut.m_xPrimary = Vector3(0.16f, 0.22f, 0.28f);
				xOut.m_xSecondary = Vector3(0.09f, 0.14f, 0.18f);
				xOut.m_xAccent = Vector3(0.50f, 0.36f, 0.16f);
			}
			break;
		case ZM_HUMAN_OUTFIT_FORMAL:
			xOut.m_xPrimary = Vector3(0.055f, 0.075f, 0.120f);
			xOut.m_xSecondary = Vector3(0.12f, 0.14f, 0.17f);
			xOut.m_xAccent = bHasTheme ? xTheme.m_xAccent : Vector3(0.56f, 0.45f, 0.24f);
			break;
		case ZM_HUMAN_OUTFIT_WORKER:
			xOut.m_xPrimary = Vector3(0.42f, 0.28f, 0.08f);
			xOut.m_xSecondary = Vector3(0.08f, 0.20f, 0.28f);
			xOut.m_xAccent = Vector3(0.55f, 0.43f, 0.18f);
			break;
		case ZM_HUMAN_OUTFIT_UNIFORM:
			xOut.m_xPrimary = Vector3(0.08f, 0.30f, 0.34f);
			xOut.m_xSecondary = Vector3(0.06f, 0.13f, 0.22f);
			xOut.m_xAccent = Vector3(0.62f, 0.72f, 0.64f);
			break;
		default:
			xOut.m_xPrimary = Vector3(0.08f, 0.22f, 0.30f);
			xOut.m_xSecondary = Vector3(0.10f, 0.32f, 0.31f);
			xOut.m_xAccent = Vector3(0.65f, 0.36f, 0.12f);
			break;
		}
		return xOut;
	}

	bool ZM_HumanHairMask(u_int uStyle, float fU, float fV)
	{
		constexpr float afDepth[uZM_HUMAN_HAIR_STYLE_COUNT] =
		{
			0.22f, 0.36f, 0.18f, 0.34f, 0.30f, 0.20f
		};
		const u_int uSafeStyle = (uStyle < uZM_HUMAN_HAIR_STYLE_COUNT) ? uStyle : uHAIR_STYLE_CROP;
		if (fV <= afDepth[uSafeStyle])
		{
			return true;
		}

		switch (uSafeStyle)
		{
		case uHAIR_STYLE_BOB:
			return (fU < 0.18f || fU > 0.82f) && fV <= 0.56f;
		case uHAIR_STYLE_SWEPT:
			return ZM_HumanInRange(fU, 0.36f, 0.70f) && fV <= 0.32f;
		case uHAIR_STYLE_BUN:
			return (fU < 0.16f || fU > 0.84f) && fV <= 0.42f;
		case uHAIR_STYLE_LONG_BACK:
			return (fU < 0.24f || fU > 0.76f) && fV <= 0.72f;
		case uHAIR_STYLE_TOPKNOT:
			return ZM_HumanInRange(fU, 0.44f, 0.58f) && fV <= 0.34f;
		case uHAIR_STYLE_CROP:
		default:
			return false;
		}
	}

	Vector3 ZM_HumanTorsoPaint(const ZM_HumanRecipe& xRecipe,
		const ZM_HumanOutfitPalette& xOutfit, const ZM_HumanPaintContext& xPaint,
		float fU, float fV)
	{
		Vector3 xBase = xOutfit.m_xPrimary;
		if (ZM_HumanInRange(fU, 0.35f, 0.65f) && fV <= 0.12f)
		{
			xBase = xOutfit.m_xSecondary;
		}
		if (ZM_HumanInRange(fV, 0.68f, 0.76f))
		{
			xBase = xOutfit.m_xAccent;
		}

		switch (xRecipe.m_eOutfit)
		{
		case ZM_HUMAN_OUTFIT_TRAVELER:
			if (ZM_HumanInRange(fV, 0.18f, 0.24f))
			{
				xBase = xOutfit.m_xSecondary;
			}
			break;
		case ZM_HUMAN_OUTFIT_LABCOAT:
			if (ZM_HumanInRange(fU, 0.485f, 0.515f) && ZM_HumanInRange(fV, 0.10f, 0.90f))
			{
				xBase = xOutfit.m_xAccent;
			}
			if (ZM_HumanInRange(fU, 0.62f, 0.88f) && ZM_HumanInRange(fV, 0.48f, 0.62f))
			{
				xBase = xOutfit.m_xSecondary;
			}
			break;
		case ZM_HUMAN_OUTFIT_CASUAL:
			if (ZM_HumanInRange(fU, 0.20f, 0.80f) && ZM_HumanInRange(fV, 0.18f, 0.30f))
			{
				xBase = xOutfit.m_xAccent;
			}
			break;
		case ZM_HUMAN_OUTFIT_LEADER:
			if (ZM_HumanInRange(fV, 0.18f, 0.24f) || ZM_HumanInRange(fV, 0.30f, 0.36f))
			{
				xBase = xOutfit.m_xAccent;
			}
			break;
		case ZM_HUMAN_OUTFIT_FORMAL:
			if (ZM_HumanInRange(fU, 0.43f, 0.57f) && ZM_HumanInRange(fV, 0.08f, 0.68f))
			{
				xBase = xOutfit.m_xSecondary;
			}
			if (ZM_HumanInRange(fU, 0.485f, 0.515f) && ZM_HumanInRange(fV, 0.18f, 0.48f))
			{
				xBase = xOutfit.m_xAccent;
			}
			break;
		case ZM_HUMAN_OUTFIT_WORKER:
			if (ZM_HumanInRange(fU, 0.25f, 0.75f) && ZM_HumanInRange(fV, 0.18f, 0.62f))
			{
				xBase = xOutfit.m_xSecondary;
			}
			break;
		case ZM_HUMAN_OUTFIT_UNIFORM:
			if (ZM_HumanInRange(fU, 0.12f, 0.88f) && ZM_HumanInRange(fV, 0.12f, 0.24f))
			{
				xBase = xOutfit.m_xAccent;
			}
			if (ZM_HumanInRange(fU, 0.68f, 0.78f) && ZM_HumanInRange(fV, 0.28f, 0.36f))
			{
				xBase = xOutfit.m_xAccent;
			}
			break;
		default:
			break;
		}

		if (xRecipe.m_eAttachment == ZM_HUMAN_ATTACHMENT_SATCHEL
			&& ZM_HumanInRange(fV, 0.08f, 0.78f))
		{
			const float fStrapU = 0.22f + 0.56f * fV;
			if (fabsf(fU - fStrapU) <= 0.035f)
			{
				xBase = Vector3(0.280f, 0.120f, 0.035f);
			}
		}
		return ZM_HumanClothPaint(xBase, fU, fV, xPaint);
	}

	Vector3 ZM_HumanAttachmentColour(const ZM_HumanRecipe& xRecipe,
		const ZM_HumanOutfitPalette& xOutfit)
	{
		switch (xRecipe.m_eAttachment)
		{
		case ZM_HUMAN_ATTACHMENT_NONE:     return Vector3(0.025f, 0.030f, 0.035f);
		case ZM_HUMAN_ATTACHMENT_CAP:      return xOutfit.m_xAccent;
		case ZM_HUMAN_ATTACHMENT_HAT:      return xOutfit.m_xSecondary;
		case ZM_HUMAN_ATTACHMENT_BACKPACK: return xOutfit.m_xPrimary * 0.72f;
		case ZM_HUMAN_ATTACHMENT_GLASSES:  return Vector3(0.035f, 0.045f, 0.050f);
		case ZM_HUMAN_ATTACHMENT_SATCHEL:  return Vector3(0.280f, 0.120f, 0.035f);
		default: return Vector3(0.025f, 0.030f, 0.035f);
		}
	}

	void ZM_PaintHumanAtlas(const ZM_HumanRecipe& xRecipe,
		const ZM_HumanPaintContext& xPaint, ZM_GenImage& xImage)
	{
		const Vector3 xBackground(0.025f, 0.030f, 0.035f);
		const Vector3 xSkin = ZM_HumanSkinColour(xRecipe.m_eSkinTone);
		const Vector3 xHair = ZM_HumanHairColour(xRecipe);
		const ZM_HumanOutfitPalette xOutfit = ZM_HumanOutfitColours(xRecipe);
		ZM_SynthFillSolid(xImage, xBackground);

		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_HEAD,
			[&](float fU, float fV)
			{
				return ZM_HumanHairMask(xRecipe.m_uHairStyle, fU, fV)
					? ZM_HumanHairPaint(xHair, fU, fV, xPaint)
					: ZM_HumanSkinPaint(xSkin, fU, fV, xPaint);
			});

		// Fixed dark eyes; humans consume no EYE-domain stream.
		const Vector3 xIris(0.095f, 0.135f, 0.155f);
		const Vector3 xPupil(0.020f, 0.020f, 0.025f);
		ZM_SynthStampEyeDecal(xImage,
			xZM_HUMAN_UV_HEAD.U(0.437f), xZM_HUMAN_UV_HEAD.V(0.328f), 0.008f, xIris, xPupil);
		ZM_SynthStampEyeDecal(xImage,
			xZM_HUMAN_UV_HEAD.U(0.563f), xZM_HUMAN_UV_HEAD.V(0.328f), 0.008f, xIris, xPupil);

		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_TORSO,
			[&](float fU, float fV)
			{
				return ZM_HumanTorsoPaint(xRecipe, xOutfit, xPaint, fU, fV);
			});

		auto xPaintArm = [&](float fU, float fV)
		{
			Vector3 xBase = ZM_HumanSkinPaint(xSkin, fU, fV, xPaint);
			if (fV <= 0.285f)
			{
				xBase = ZM_HumanClothPaint(xOutfit.m_xPrimary, fU, fV, xPaint);
			}
			if (ZM_HumanInRange(fV, 0.25f, 0.31f))
			{
				xBase = ZM_HumanClothPaint(xOutfit.m_xAccent, fU, fV, xPaint);
			}
			return xBase;
		};
		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_ARM_L, xPaintArm);
		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_ARM_R, xPaintArm);

		auto xPaintLeg = [&](float fU, float fV)
		{
			const Vector3 xBase = (fV >= 0.72f)
				? Vector3(0.035f, 0.045f, 0.055f)
				: xOutfit.m_xSecondary;
			return ZM_HumanClothPaint(xBase, fU, fV, xPaint);
		};
		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_LEG_L, xPaintLeg);
		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_LEG_R, xPaintLeg);

		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_HAIR,
			[&](float fU, float fV)
			{
				return ZM_HumanHairPaint(xHair, fU, fV, xPaint);
			});

		const Vector3 xAttachment = ZM_HumanAttachmentColour(xRecipe, xOutfit);
		ZM_PaintHumanIsland(xImage, xZM_HUMAN_UV_ATTACHMENT,
			[&](float fU, float fV)
			{
				return (xRecipe.m_eAttachment == ZM_HUMAN_ATTACHMENT_NONE)
					? xBackground
					: ZM_HumanClothPaint(xAttachment, fU, fV, xPaint);
			});

		// Exactly one clamp-to-edge ring, corners included, around all eight cores.
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_HEAD);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_TORSO);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_ARM_L);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_ARM_R);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_LEG_L);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_LEG_R);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_HAIR);
		ZM_DilateHumanIsland(xImage, xZM_HUMAN_UV_ATTACHMENT);
	}

	void ZM_AppendHumanAppearancePart(ZM_GenMesh& xMesh, ZM_LoftRing* pxRings,
		u_int uNumRings, u_int uSegs, u_int uBone, const ZM_GenUVIsland& xIsland,
		float fHeightScale, float fSuperEllipse = 1.0f)
	{
		for (u_int u = 0u; u < uNumRings; ++u)
		{
			ZM_LoftRing& xRing = pxRings[u];
			xRing.m_fY = (xRing.m_fY + 1.0f) * fHeightScale;
			xRing.m_uBoneA = uBone;
			xRing.m_uBoneB = uBone;
			xRing.m_fBlendB = 0.0f;
			xRing.m_fSuperEllipse = fSuperEllipse;
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings = pxRings;
		xPart.m_uNumRings = uNumRings;
		xPart.m_uSegs = uSegs;
		xPart.m_xIsland = xIsland;
		xPart.m_bCapStart = true;
		xPart.m_bCapEnd = true;
		xPart.m_uSubdiv = 1u;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}

	void ZM_AppendHumanHairCap(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		ZM_LoftRing axRings[] =
		{
			{ 1.620f, 0.0f, -0.008f, 0.045f, 0.046f },
			{ 1.590f, 0.0f, -0.006f, 0.092f, 0.094f },
			{ 1.540f, 0.0f, -0.004f, 0.111f, 0.113f },
			{ 1.485f, 0.0f, -0.008f, 0.113f, 0.116f },
			{ 1.440f, 0.0f, -0.015f, 0.106f, 0.110f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axRings,
			sizeof(axRings) / sizeof(axRings[0]), 24u, uHUMAN_BONE_HEAD,
			xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
	}

	void ZM_AppendHumanHairStylePart(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		switch (xRecipe.m_uHairStyle)
		{
		case uHAIR_STYLE_CROP:
			return;
		case uHAIR_STYLE_BOB:
		{
			ZM_LoftRing axRings[] =
			{
				{ 1.500f, 0.0f, -0.100f, 0.085f, 0.025f },
				{ 1.420f, 0.0f, -0.112f, 0.100f, 0.030f },
				{ 1.315f, 0.0f, -0.108f, 0.090f, 0.028f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
			return;
		}
		case uHAIR_STYLE_SWEPT:
		{
			ZM_LoftRing axRings[] =
			{
				{ 1.690f, 0.040f, 0.000f, 0.020f, 0.024f },
				{ 1.650f, 0.032f, 0.005f, 0.060f, 0.045f },
				{ 1.590f, 0.018f, 0.004f, 0.090f, 0.060f },
				{ 1.535f, 0.000f, 0.000f, 0.082f, 0.052f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
			return;
		}
		case uHAIR_STYLE_BUN:
		{
			ZM_LoftRing axRings[] =
			{
				{ 1.510f, 0.0f, -0.125f, 0.028f, 0.030f },
				{ 1.455f, 0.0f, -0.145f, 0.065f, 0.060f },
				{ 1.385f, 0.0f, -0.135f, 0.050f, 0.050f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
			return;
		}
		case uHAIR_STYLE_LONG_BACK:
		{
			ZM_LoftRing axRings[] =
			{
				{ 1.500f, 0.0f, -0.102f, 0.086f, 0.026f },
				{ 1.380f, 0.0f, -0.120f, 0.105f, 0.035f },
				{ 1.220f, 0.0f, -0.125f, 0.100f, 0.036f },
				{ 1.080f, 0.0f, -0.115f, 0.080f, 0.030f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
			return;
		}
		case uHAIR_STYLE_TOPKNOT:
		{
			ZM_LoftRing axRings[] =
			{
				{ 1.735f, 0.0f, -0.006f, 0.018f, 0.020f },
				{ 1.705f, 0.0f, -0.006f, 0.045f, 0.046f },
				{ 1.650f, 0.0f, -0.006f, 0.055f, 0.056f },
				{ 1.610f, 0.0f, -0.006f, 0.030f, 0.032f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_HAIR, xRecipe.m_fHeightScale);
			return;
		}
		default:
			Zenith_Assert(false, "unsupported human hair style %u", xRecipe.m_uHairStyle);
			return;
		}
	}

	void ZM_AppendHumanCap(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		ZM_LoftRing axCrown[] =
		{
			{ 1.650f, 0.0f, -0.008f, 0.055f, 0.056f },
			{ 1.615f, 0.0f, -0.006f, 0.115f, 0.118f },
			{ 1.555f, 0.0f, -0.004f, 0.125f, 0.127f },
			{ 1.505f, 0.0f,  0.000f, 0.122f, 0.124f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axCrown,
			sizeof(axCrown) / sizeof(axCrown[0]), 24u, uHUMAN_BONE_HEAD,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale);

		ZM_LoftRing axVisor[] =
		{
			{ 1.525f, 0.0f, 0.095f, 0.075f, 0.055f },
			{ 1.505f, 0.0f, 0.112f, 0.090f, 0.065f },
			{ 1.485f, 0.0f, 0.110f, 0.080f, 0.055f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axVisor,
			sizeof(axVisor) / sizeof(axVisor[0]), 16u, uHUMAN_BONE_HEAD,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale, 0.80f);
	}

	void ZM_AppendHumanHat(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		ZM_LoftRing axCrown[] =
		{
			{ 1.730f, 0.0f, -0.006f, 0.040f, 0.042f },
			{ 1.700f, 0.0f, -0.006f, 0.100f, 0.102f },
			{ 1.580f, 0.0f, -0.004f, 0.124f, 0.126f },
			{ 1.535f, 0.0f,  0.000f, 0.126f, 0.128f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axCrown,
			sizeof(axCrown) / sizeof(axCrown[0]), 20u, uHUMAN_BONE_HEAD,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale);

		ZM_LoftRing axBrim[] =
		{
			{ 1.555f, 0.0f, 0.0f, 0.155f, 0.150f },
			{ 1.525f, 0.0f, 0.0f, 0.165f, 0.160f },
			{ 1.505f, 0.0f, 0.0f, 0.155f, 0.150f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axBrim,
			sizeof(axBrim) / sizeof(axBrim[0]), 24u, uHUMAN_BONE_HEAD,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale);
	}

	void ZM_AppendHumanBackpack(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		ZM_LoftRing axRings[] =
		{
			{ 1.080f, 0.0f, -0.145f, 0.115f, 0.070f },
			{ 1.020f, 0.0f, -0.175f, 0.160f, 0.095f },
			{ 0.520f, 0.0f, -0.180f, 0.175f, 0.105f },
			{ 0.300f, 0.0f, -0.160f, 0.145f, 0.085f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axRings,
			sizeof(axRings) / sizeof(axRings[0]), 20u, uHUMAN_BONE_SPINE,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale, 0.78f);
	}

	void ZM_AppendHumanGlasses(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		for (u_int uSide = 0u; uSide < 2u; ++uSide)
		{
			const float fCx = (uSide == 0u) ? -0.060f : 0.060f;
			ZM_LoftRing axRings[] =
			{
				{ 1.500f, fCx, 0.104f, 0.047f, 0.022f },
				{ 1.465f, fCx, 0.116f, 0.058f, 0.028f },
				{ 1.430f, fCx, 0.104f, 0.047f, 0.022f },
			};
			ZM_AppendHumanAppearancePart(xMesh, axRings,
				sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_HEAD,
				xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale, 0.72f);
		}
	}

	void ZM_AppendHumanSatchel(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		ZM_LoftRing axRings[] =
		{
			{ 0.840f, 0.235f, -0.080f, 0.075f, 0.045f },
			{ 0.770f, 0.250f, -0.105f, 0.110f, 0.070f },
			{ 0.380f, 0.250f, -0.110f, 0.120f, 0.075f },
			{ 0.300f, 0.235f, -0.090f, 0.090f, 0.055f },
		};
		ZM_AppendHumanAppearancePart(xMesh, axRings,
			sizeof(axRings) / sizeof(axRings[0]), 16u, uHUMAN_BONE_SPINE,
			xZM_HUMAN_UV_ATTACHMENT, xRecipe.m_fHeightScale, 0.72f);
	}
}

ZM_GenImage ZM_BuildHumanAlbedo(const ZM_HumanRecipe& xRecipe)
{
	// Exactly six unconditional ALBEDO draws, fixed before all appearance branches.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_ALBEDO);
	ZM_HumanPaintContext xPaint;
	xPaint.m_uSkinNoiseSeed = xRng.Next();
	xPaint.m_uHairNoiseSeed = xRng.Next();
	xPaint.m_uOutfitNoiseSeed = xRng.Next();
	xPaint.m_fSkinValue = xRng.NextFloatRange(0.96f, 1.04f);
	xPaint.m_fHairValue = xRng.NextFloatRange(0.90f, 1.10f);
	xPaint.m_fOutfitValue = xRng.NextFloatRange(0.90f, 1.10f);

	ZM_GenImage xImage(uZM_HUMAN_ALBEDO_RESOLUTION, uZM_HUMAN_ALBEDO_RESOLUTION);
	ZM_PaintHumanAtlas(xRecipe, xPaint, xImage);
	return xImage;
}

void ZM_AppendHumanHair(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	Zenith_Assert(xRecipe.m_uHairStyle < uZM_HUMAN_HAIR_STYLE_COUNT,
		"unsupported human hair style %u", xRecipe.m_uHairStyle);
	ZM_AppendHumanHairCap(xRecipe, xMesh);
	ZM_AppendHumanHairStylePart(xRecipe, xMesh);
}

void ZM_AppendHumanAttachment(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	switch (xRecipe.m_eAttachment)
	{
	case ZM_HUMAN_ATTACHMENT_NONE:     return;
	case ZM_HUMAN_ATTACHMENT_CAP:      ZM_AppendHumanCap(xRecipe, xMesh);      return;
	case ZM_HUMAN_ATTACHMENT_HAT:      ZM_AppendHumanHat(xRecipe, xMesh);      return;
	case ZM_HUMAN_ATTACHMENT_BACKPACK: ZM_AppendHumanBackpack(xRecipe, xMesh); return;
	case ZM_HUMAN_ATTACHMENT_GLASSES:  ZM_AppendHumanGlasses(xRecipe, xMesh);  return;
	case ZM_HUMAN_ATTACHMENT_SATCHEL:  ZM_AppendHumanSatchel(xRecipe, xMesh);  return;
	default:
		Zenith_Assert(false, "unsupported human attachment %u", (u_int)xRecipe.m_eAttachment);
		return;
	}
}

void ZM_AppendHumanAppearanceMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	ZM_AppendHumanHair(xRecipe, xMesh);
	ZM_AppendHumanAttachment(xRecipe, xMesh);
}
