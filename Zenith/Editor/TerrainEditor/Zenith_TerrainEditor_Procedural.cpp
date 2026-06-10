#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditorUndo.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor_Noise.h"
#include "Editor/Zenith_UndoSystem.h"

//=============================================================================
// Procedural heightfield generation + auto-splat classification.
//
// Determinism is load-bearing here: RenderTest regenerates its terrain from a
// fixed seed and CI hash-compares the result across runs, so generation is
// single-threaded, integer-hash-noise-only, fixed iteration order.
//=============================================================================

void Zenith_TerrainEditor::GenerateProcedural(const Zenith_TerrainProceduralParams& xParams)
{
	EnsureImagesAllocated();

	// Whole-field rewrite — not undoable (a before/after pair would be 128 MB).
	if (g_xEngine.UndoSystem().GetUndoStackSize() > 0 || g_xEngine.UndoSystem().GetRedoStackSize() > 0)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Procedural generation clears the undo history");
		g_xEngine.UndoSystem().Clear();
	}

	const float fRidged = std::max(0.0f, std::min(1.0f, xParams.m_fRidgedBlend));
	for (u_int uZ = 0; uZ < uHEIGHTFIELD_SIZE; uZ++)
	{
		float* pfRow = m_xHeightfield.Row(uZ);
		const float fNZ = static_cast<float>(uZ) * xParams.m_fFrequency;
		for (u_int uX = 0; uX < uHEIGHTFIELD_SIZE; uX++)
		{
			const float fNX = static_cast<float>(uX) * xParams.m_fFrequency;
			float fValue = Zenith_TerrainNoise::FBM(fNX, fNZ, xParams.m_uSeed,
				xParams.m_uOctaves, xParams.m_fLacunarity, xParams.m_fGain);
			if (fRidged > 0.0f)
			{
				const float fRidgedValue = Zenith_TerrainNoise::RidgedFBM(fNX, fNZ, xParams.m_uSeed,
					xParams.m_uOctaves, xParams.m_fLacunarity, xParams.m_fGain);
				fValue = fValue * (1.0f - fRidged) + fRidgedValue * fRidged;
			}
			const float fHeight = xParams.m_fBaseHeight + (fValue - 0.5f) * 2.0f * xParams.m_fAmplitude;
			pfRow[uX] = std::max(0.0f, std::min(1.0f, fHeight));
		}
	}

	// Everything changed: mark the whole grid dirty (chunks + AABBs + flags).
	MarkHeightRegionDirty(0.0f, 0.0f,
		static_cast<float>(uHEIGHTFIELD_SIZE - 1), static_cast<float>(uHEIGHTFIELD_SIZE - 1));

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Procedural heightfield generated (seed %u, base %.3f, amp %.3f, freq %.5f, oct %u)",
		xParams.m_uSeed, xParams.m_fBaseHeight, xParams.m_fAmplitude, xParams.m_fFrequency, xParams.m_uOctaves);
}

namespace
{
	float SmoothBand(float fValue, float fMin, float fMax, float fFeather)
	{
		fFeather = std::max(0.001f, fFeather);
		auto SmoothStep = [](float fEdge0, float fEdge1, float fX)
		{
			const float fT = std::max(0.0f, std::min(1.0f, (fX - fEdge0) / (fEdge1 - fEdge0)));
			return fT * fT * (3.0f - 2.0f * fT);
		};
		return SmoothStep(fMin - fFeather, fMin + fFeather, fValue)
			* (1.0f - SmoothStep(fMax - fFeather, fMax + fFeather, fValue));
	}
}

void Zenith_TerrainEditor::RunAutoSplat()
{
	EnsureImagesAllocated();

	// Undoable: one full-splat before/after pair (32 MB) fits the budget.
	Zenith_Vector<u_int8> xBefore;
	CopyMapRegion(Zenith_TerrainEditMap::Splat, 0, 0, uSPLATMAP_SIZE, uSPLATMAP_SIZE, xBefore);

	const float fWorldPerTexel = fTERRAIN_WORLD_SIZE / static_cast<float>(uSPLATMAP_SIZE);
	u_int8* pSplat = m_xSplatmap.GetDataPointer();

	for (u_int uZ = 0; uZ < uSPLATMAP_SIZE; uZ++)
	{
		const float fWorldZ = (static_cast<float>(uZ) + 0.5f) * fWorldPerTexel;
		for (u_int uX = 0; uX < uSPLATMAP_SIZE; uX++)
		{
			const float fWorldX = (static_cast<float>(uX) + 0.5f) * fWorldPerTexel;

			const float fHeight = SampleHeightWorld(fWorldX, fWorldZ);

			// Slope from central differences at the splat texel spacing.
			const float fHL = SampleHeightWorld(fWorldX - fWorldPerTexel, fWorldZ);
			const float fHR = SampleHeightWorld(fWorldX + fWorldPerTexel, fWorldZ);
			const float fHD = SampleHeightWorld(fWorldX, fWorldZ - fWorldPerTexel);
			const float fHU = SampleHeightWorld(fWorldX, fWorldZ + fWorldPerTexel);
			const float fDYDX = (fHR - fHL) / (2.0f * fWorldPerTexel);
			const float fDYDZ = (fHU - fHD) / (2.0f * fWorldPerTexel);
			const float fSlopeDeg = atanf(sqrtf(fDYDX * fDYDX + fDYDZ * fDYDZ)) * 57.29578f;

			float afWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float fTotal = 0.0f;
			for (u_int uSlot = 0; uSlot < 4; uSlot++)
			{
				const Zenith_TerrainAutoSplatRule& xRule = m_axAutoSplatRules[uSlot];
				if (!xRule.m_bEnabled)
				{
					continue;
				}
				float fW = xRule.m_fWeight
					* SmoothBand(fHeight, xRule.m_fHeightMin, xRule.m_fHeightMax, xRule.m_fHeightFeather)
					* SmoothBand(fSlopeDeg, xRule.m_fSlopeMinDeg, xRule.m_fSlopeMaxDeg, xRule.m_fSlopeFeather);
				if (xRule.m_fNoiseJitter > 0.0f)
				{
					const float fJitter = Zenith_TerrainNoise::ValueNoise(
						fWorldX * 0.02f, fWorldZ * 0.02f, xRule.m_uNoiseSeed);
					fW *= 1.0f + (fJitter - 0.5f) * 2.0f * xRule.m_fNoiseJitter;
				}
				afWeights[uSlot] = std::max(0.0f, fW);
				fTotal += afWeights[uSlot];
			}

			u_int8* pTexel = pSplat + (static_cast<size_t>(uZ) * uSPLATMAP_SIZE + uX) * 4;
			if (fTotal <= 0.0001f)
			{
				pTexel[0] = 255; pTexel[1] = 0; pTexel[2] = 0; pTexel[3] = 0;
				continue;
			}

			// Normalize to a 255 sum; channel 0 absorbs rounding drift.
			u_int uSum = 0;
			for (u_int uSlot = 1; uSlot < 4; uSlot++)
			{
				pTexel[uSlot] = static_cast<u_int8>(std::min(255.0f, roundf(afWeights[uSlot] / fTotal * 255.0f)));
				uSum += pTexel[uSlot];
			}
			pTexel[0] = static_cast<u_int8>(255 - std::min(255u, uSum));
		}
	}

	m_bSplatGPUDirty = true;
	m_bSessionDirty = true;

	Zenith_Vector<u_int8> xAfter;
	CopyMapRegion(Zenith_TerrainEditMap::Splat, 0, 0, uSPLATMAP_SIZE, uSPLATMAP_SIZE, xAfter);

	const u_int64 ulNewBytes = static_cast<u_int64>(xBefore.GetSize()) + xAfter.GetSize();
	if (m_ulUndoBytesLive + ulNewBytes > ulUNDO_BUDGET_BYTES)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Terrain undo budget exceeded - clearing undo history");
		g_xEngine.UndoSystem().Clear();
	}
	Zenith_UndoCommand_TerrainEdit* pxCommand = new Zenith_UndoCommand_TerrainEdit(
		*this, Zenith_TerrainEditMap::Splat, 0, 0, uSPLATMAP_SIZE, uSPLATMAP_SIZE,
		std::move(xBefore), std::move(xAfter));
	g_xEngine.UndoSystem().Execute(pxCommand);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Auto-splat complete");
}

#endif // ZENITH_TOOLS
