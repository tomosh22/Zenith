#include "DataStream/Zenith_StreamEnvelope.h"   // Zenith_WriteStreamHeader — the .ztxtr envelope
#include "AssetHandling/Zenith_AssetTypeIds.h"     // uZENITH_TEXTURE_* ids/schema
//=============================================================================
// Zenith_TerrainEditor unit tests (headless-safe: standalone sessions only —
// no terrain entity resolves, so no hook / eviction / GPU path is reached).
// Included from the bottom of Zenith_TerrainEditor.cpp.
//=============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Noise.h"

#ifdef ZENITH_TESTING

#include "Editor/Zenith_EditorAutomation.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

// GrassTypeSaveGuard restores the real GrassTypes.zdata byte-for-byte.
#include <fstream>

namespace
{
	// Deterministic FNV-1a over sampled heightfield texels (sampling keeps the
	// determinism test fast in Debug).
	u_int64 HashHeightfieldSamples(const Zenith_Image& xField)
	{
		u_int64 ulHash = 14695981039346656037ull;
		const u_int uCount = xField.GetWidth() * xField.GetHeight();
		const float* pfData = xField.Row(0);
		for (u_int u = 0; u < uCount; u += 97)
		{
			u_int uBits;
			memcpy(&uBits, &pfData[u], sizeof(uBits));
			ulHash ^= uBits;
			ulHash *= 1099511628211ull;
		}
		return ulHash;
	}

	double SumHeightfield(const Zenith_Image& xField)
	{
		double dSum = 0.0;
		const u_int uCount = xField.GetWidth() * xField.GetHeight();
		const float* pfData = xField.Row(0);
		for (u_int u = 0; u < uCount; u++)
		{
			dSum += pfData[u];
		}
		return dSum;
	}
}

ZENITH_TEST(TerrainEditor, FalloffCurves)
{
	// Every falloff: 1 at the centre, 0 at the rim, monotonically decreasing.
	for (int i = 0; i < static_cast<int>(Zenith_TerrainBrushFalloff::Count); i++)
	{
		const Zenith_TerrainBrushFalloff eFalloff = static_cast<Zenith_TerrainBrushFalloff>(i);
		ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainEditor::EvaluateFalloff(eFalloff, 0.0f), 1.0f, 0.001f, "Falloff must be 1 at centre");
		ZENITH_ASSERT_LE(Zenith_TerrainEditor::EvaluateFalloff(eFalloff, 1.0f), 0.001f, "Falloff must be 0 at rim");
		float fPrev = 1.0f;
		for (float fD = 0.1f; fD <= 1.0f; fD += 0.1f)
		{
			const float fW = Zenith_TerrainEditor::EvaluateFalloff(eFalloff, fD);
			ZENITH_ASSERT_LE(fW, fPrev + 0.0001f, "Falloff must decrease monotonically");
			fPrev = fW;
		}
	}
}

ZENITH_TEST(TerrainEditor, RaiseDabIsLocalAndPositive)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	const float fBefore = xEditor.SampleHeightWorld(100.0f, 100.0f);
	const float fFarBefore = xEditor.SampleHeightWorld(400.0f, 400.0f);

	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 100.0f, 100.0f, 20.0f, 1.0f, 0.0f);

	ZENITH_ASSERT_GT(xEditor.SampleHeightWorld(100.0f, 100.0f), fBefore, "Raise must lift the centre");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.SampleHeightWorld(400.0f, 400.0f), fFarBefore, 0.0001f, "Raise must not touch texels outside the radius");
	// The dab marked its chunks session-dirty.
	ZENITH_ASSERT_TRUE(xEditor.HasUnbakedChanges(), "Dab must mark the session dirty");
	ZENITH_ASSERT_TRUE(xEditor.IsChunkSessionDirty((100 / 64) * 64 + (100 / 64)), "Dab chunk must be session-dirty");
}

ZENITH_TEST(TerrainEditor, StrokeUndoRoundtrip)
{
	g_xEngine.UndoSystem().Clear();
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();

		const float fBefore = xEditor.SampleHeightWorld(200.0f, 200.0f);

		xEditor.BeginStroke();
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 200.0f, 200.0f, 16.0f, 1.0f, 0.0f);
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 210.0f, 200.0f, 16.0f, 1.0f, 0.0f);
		xEditor.EndStroke();

		const float fAfter = xEditor.SampleHeightWorld(200.0f, 200.0f);
		ZENITH_ASSERT_GT(fAfter, fBefore, "Stroke must raise the terrain");
		ZENITH_ASSERT_TRUE(g_xEngine.UndoSystem().CanUndo(), "Stroke must push an undo command");

		g_xEngine.UndoSystem().Undo();
		ZENITH_ASSERT_EQ_FLOAT(xEditor.SampleHeightWorld(200.0f, 200.0f), fBefore, 0.0001f, "Undo must restore pre-stroke heights");

		g_xEngine.UndoSystem().Redo();
		ZENITH_ASSERT_EQ_FLOAT(xEditor.SampleHeightWorld(200.0f, 200.0f), fAfter, 0.0001f, "Redo must re-apply the stroke");

		// The commands reference the local editor — clear before scope exit.
		g_xEngine.UndoSystem().Clear();
	}
}

ZENITH_TEST(TerrainEditor, ProceduralGenerationIsDeterministic)
{
	g_xEngine.UndoSystem().Clear();

	Zenith_TerrainProceduralParams xParams;
	xParams.m_uSeed = 1337;
	xParams.m_uOctaves = 2;   // keep the Debug-build runtime sane

	u_int64 ulHashA, ulHashB;
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();
		xEditor.GenerateProcedural(xParams);
		ulHashA = HashHeightfieldSamples(xEditor.GetHeightfield());
	}
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();
		xEditor.GenerateProcedural(xParams);
		ulHashB = HashHeightfieldSamples(xEditor.GetHeightfield());
	}
	ZENITH_ASSERT_EQ(ulHashA, ulHashB, "Same seed must produce a byte-identical heightfield");

	// A different seed must diverge.
	xParams.m_uSeed = 4242;
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();
		xEditor.GenerateProcedural(xParams);
		ZENITH_ASSERT_NE(HashHeightfieldSamples(xEditor.GetHeightfield()), ulHashA, "Different seed must change the heightfield");
	}
}

ZENITH_TEST(TerrainEditor, SplatPaintKeepsWeightsNormalized)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();
	// OpenStandalone seeds from any baked terrain textures in the game's
	// assets dir (game-dependent — RenderTest ships a generated set, DP none).
	// This test asserts against pristine defaults, so reset explicitly.
	xEditor.ResetImagesToDefaults();

	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::SplatPaint, 500.0f, 500.0f, 60.0f, 1.0f, 2.0f /* layer 2 */);

	const Zenith_Vector<u_int8>& xSplat = xEditor.GetSplatmap();
	// Splat texel at world (500,500) -> texel (250,250).
	const u_int uTexel = (250u * Zenith_TerrainEditor::uSPLATMAP_SIZE + 250u) * 4;
	const u_int uSum = xSplat.Get(uTexel) + xSplat.Get(uTexel + 1) + xSplat.Get(uTexel + 2) + xSplat.Get(uTexel + 3);
	ZENITH_ASSERT_EQ(uSum, 255u, "Painted splat weights must sum to 255");
	ZENITH_ASSERT_GT(static_cast<u_int>(xSplat.Get(uTexel + 2)), 0u, "Painted layer must gain weight");

	// An untouched texel keeps the default layer-0 weight.
	const u_int uFar = (1000u * Zenith_TerrainEditor::uSPLATMAP_SIZE + 1000u) * 4;
	ZENITH_ASSERT_EQ(static_cast<u_int>(xSplat.Get(uFar)), 255u, "Untouched splat texels must keep their weights");
}

ZENITH_TEST(TerrainEditor, ThermalErosionConservesMass)
{
	g_xEngine.UndoSystem().Clear();

	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// A steep spike that thermal relaxation must slump.
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 300.0f, 300.0f, 8.0f, 1.0f, 0.0f);
	for (u_int u = 0; u < 30; u++)
	{
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 300.0f, 300.0f, 8.0f, 1.0f, 0.0f);
	}

	const double dSumBefore = SumHeightfield(xEditor.GetHeightfield());
	const float fPeakBefore = xEditor.SampleHeightWorld(300.0f, 300.0f);

	Zenith_TerrainErosionParams xParams;
	xParams.m_uHydraulicDroplets = 0;
	xParams.m_uThermalIterations = 4;
	xParams.m_fTalusAngleDeg = 30.0f;
	xParams.m_bRegionOnly = true;
	xParams.m_fRegionCentreX = 300.0f;
	xParams.m_fRegionCentreZ = 300.0f;
	xParams.m_fRegionRadius = 64.0f;
	xEditor.RunErosion(xParams, true);

	const double dSumAfter = SumHeightfield(xEditor.GetHeightfield());
	ZENITH_ASSERT_TRUE(fabs(dSumAfter - dSumBefore) < 0.05, "Thermal erosion must conserve total height mass");
	ZENITH_ASSERT_LT(xEditor.SampleHeightWorld(300.0f, 300.0f), fPeakBefore, "Thermal erosion must slump the spike");
}

ZENITH_TEST(TerrainEditor, HydraulicErosionStaysFiniteAndBounded)
{
	g_xEngine.UndoSystem().Clear();

	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	Zenith_TerrainProceduralParams xGen;
	xGen.m_uSeed = 7;
	xGen.m_uOctaves = 2;
	xEditor.GenerateProcedural(xGen);

	const double dSumBefore = SumHeightfield(xEditor.GetHeightfield());

	Zenith_TerrainErosionParams xParams;
	xParams.m_uSeed = 7;
	xParams.m_uHydraulicDroplets = 10000;
	xParams.m_uThermalIterations = 0;
	xEditor.RunErosion(xParams, true);

	const Zenith_Image& xField = xEditor.GetHeightfield();
	const float* pfData = xField.Row(0);
	const u_int uCount = xField.GetWidth() * xField.GetHeight();
	for (u_int u = 0; u < uCount; u += 31)
	{
		ZENITH_ASSERT_TRUE(pfData[u] == pfData[u], "Erosion must not produce NaNs");
		ZENITH_ASSERT_TRUE(pfData[u] >= -0.1f && pfData[u] <= 1.1f, "Erosion must keep heights near the normalized range");
	}
	// Droplets evaporate carrying a little sediment — mass change must stay small.
	const double dSumAfter = SumHeightfield(xEditor.GetHeightfield());
	ZENITH_ASSERT_TRUE(fabs(dSumAfter - dSumBefore) / std::max(1.0, dSumBefore) < 0.01,
		"Hydraulic erosion must not change total mass by more than 1%%");
}

ZENITH_TEST(TerrainEditor, AutoSplatWeightsSumTo255)
{
	g_xEngine.UndoSystem().Clear();
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();

		Zenith_TerrainProceduralParams xGen;
		xGen.m_uSeed = 99;
		xGen.m_uOctaves = 2;
		xGen.m_fAmplitude = 0.25f;
		xEditor.GenerateProcedural(xGen);

		Zenith_TerrainAutoSplatRule xGrassRule;
		xGrassRule.m_bEnabled = true;
		xGrassRule.m_fSlopeMaxDeg = 25.0f;
		xEditor.SetAutoSplatRule(0, xGrassRule);

		Zenith_TerrainAutoSplatRule xRockRule;
		xRockRule.m_bEnabled = true;
		xRockRule.m_fSlopeMinDeg = 18.0f;
		xEditor.SetAutoSplatRule(1, xRockRule);

		xEditor.RunAutoSplat();

		const Zenith_Vector<u_int8>& xSplat = xEditor.GetSplatmap();
		for (u_int uSample = 0; uSample < 64; uSample++)
		{
			const u_int uTexel = (Zenith_TerrainNoise::HashUInt(uSample) % (Zenith_TerrainEditor::uSPLATMAP_SIZE * Zenith_TerrainEditor::uSPLATMAP_SIZE)) * 4;
			const u_int uSum = xSplat.Get(uTexel) + xSplat.Get(uTexel + 1) + xSplat.Get(uTexel + 2) + xSplat.Get(uTexel + 3);
			ZENITH_ASSERT_EQ(uSum, 255u, "Auto-splat weights must sum to 255 at every texel");
		}

		// RunAutoSplat pushed an undo command referencing the local editor.
		g_xEngine.UndoSystem().Clear();
	}
}

ZENITH_TEST(TerrainEditor, StampRoundtrip)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// Build a feature, capture it, stamp it elsewhere.
	for (u_int u = 0; u < 20; u++)
	{
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 150.0f, 150.0f, 24.0f, 1.0f, 0.0f);
	}
	xEditor.SampleStamp(150.0f, 150.0f, 30.0f);
	ZENITH_ASSERT_TRUE(xEditor.HasStamp(), "SampleStamp must capture a stamp");

	const float fTargetBefore = xEditor.SampleHeightWorld(600.0f, 600.0f);
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Stamp, 600.0f, 600.0f, 30.0f, 1.0f, 0.0f);
	ZENITH_ASSERT_GT(xEditor.SampleHeightWorld(600.0f, 600.0f), fTargetBefore, "Stamping a hill must raise the target");
}

ZENITH_TEST(TerrainEditor, HeightfieldRaycastHitsSurface)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// Flat field at 0.1 normalized == 51.2 m.
	Zenith_Image& xField = const_cast<Zenith_Image&>(xEditor.GetHeightfield());
	const u_int uCount = xField.GetWidth() * xField.GetHeight();
	float* pfData = xField.Row(0);
	for (u_int u = 0; u < uCount; u++)
	{
		pfData[u] = 0.1f;
	}

	Zenith_Maths::Vector3 xHit;
	const bool bHit = xEditor.RaycastHeightfield(
		Zenith_Maths::Vector3(100.0f, 200.0f, 100.0f),
		Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), xHit);
	ZENITH_ASSERT_TRUE(bHit, "Vertical ray must hit the flat field");
	ZENITH_ASSERT_EQ_FLOAT(xHit.y, 51.2f, 0.1f, "Hit height must match the field");
	ZENITH_ASSERT_EQ_FLOAT(xHit.x, 100.0f, 0.1f, "Hit X must be under the ray");

	// A ray pointing away from the terrain must miss.
	const bool bMiss = xEditor.RaycastHeightfield(
		Zenith_Maths::Vector3(100.0f, 200.0f, 100.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xHit);
	ZENITH_ASSERT_FALSE(bMiss, "Upward ray must miss the terrain");
}

ZENITH_TEST(TerrainEditor, GrassDensityPaint)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();
	// OpenStandalone seeds from any baked terrain textures in the game's
	// assets dir (game-dependent — RenderTest ships a generated set, DP none).
	// This test asserts against pristine defaults, so reset explicitly.
	xEditor.ResetImagesToDefaults();

	// Default density is 0 (grass is painted in); paint a meadow.
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassDensity, 800.0f, 800.0f, 100.0f, 1.0f, 0.8f);

	const Zenith_Image& xDensity = xEditor.GetGrassDensity();
	// World 800 -> grass texel 200 (1024 texels over 4096 m).
	ZENITH_ASSERT_GT(xDensity.At(200, 200), 0.7f, "Painted meadow must raise the density");
	ZENITH_ASSERT_EQ_FLOAT(xDensity.At(100, 100), 0.0f, 0.001f, "Untouched density texels must stay at 0");
}

ZENITH_TEST(TerrainEditor, BrushIndicatorDecalArmsForOneFrame)
{
	// The editor brush indicator arms the decal editor slot for exactly one
	// Prepare/pack: the cursor re-arms every frame while valid, so a missed
	// frame must make the indicator vanish rather than go stale.
	Flux_DecalsImpl& xDecals = g_xEngine.Decals();
	xDecals.Reset();

	xDecals.SetEditorDecal(Zenith_Maths::Vector3(100.0f, 50.0f, 100.0f),
		30.0f, 60.0f, Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.85f), nullptr);
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 1u, "Armed editor decal must pack exactly one instance");
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 0u, "Editor decal must disarm after one pack");

	// Re-arming works, and gameplay ring decals coexist with the editor slot.
	xDecals.SpawnDecal(Zenith_Maths::Vector3(50.0f, 10.0f, 50.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), nullptr, 0.5f, 10.0f);
	xDecals.SetEditorDecal(Zenith_Maths::Vector3(100.0f, 50.0f, 100.0f),
		30.0f, 60.0f, Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.85f), nullptr);
	ZENITH_ASSERT_EQ(xDecals.TickAndPackDense(0.016f), 2u, "Ring decal + editor decal must both pack");

	xDecals.Reset();
}

ZENITH_TEST(TerrainEditor, SetTreeBrushSettingsWritesFields)
{
	Zenith_TerrainEditor xEditor;

	xEditor.SetTreeBrushSettings(40, 0.75f, 1.6f, 2.5f, 34.0f, 4242u);

	ZENITH_ASSERT_EQ(xEditor.m_xBrush.m_uTreesPerDab, 40u, "trees-per-dab must be written");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.m_xBrush.m_fTreeScaleMin, 0.75f, 0.0001f, "scale min must be written");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.m_xBrush.m_fTreeScaleMax, 1.6f, 0.0001f, "scale max must be written");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.m_xBrush.m_fTreeSpacing, 2.5f, 0.0001f, "spacing must be written");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.m_xBrush.m_fTreeMaxSlopeDeg, 34.0f, 0.0001f, "max slope must be written");
	ZENITH_ASSERT_EQ(xEditor.GetTreeRngState_ForTest(), 4242u, "non-zero seed must seed the scatter RNG");

	// uSeed == 0 keeps the fixed default (the xorshift state must never be zero).
	xEditor.SetTreeBrushSettings(3, 0.85f, 1.35f, 4.0f, 38.0f, 0u);
	ZENITH_ASSERT_EQ(xEditor.GetTreeRngState_ForTest(), 0x51A7E5u, "zero seed must fall back to the default RNG seed");
}

ZENITH_TEST(TerrainEditor, SetTreeBrushSeedIsDeterministic)
{
	// Same seed => same scatter RNG state regardless of the other brush params:
	// this is what makes a re-authored scene's tree placement byte-stable.
	Zenith_TerrainEditor xA;
	Zenith_TerrainEditor xB;
	xA.SetTreeBrushSettings(12, 0.9f, 1.2f, 3.0f, 40.0f, 777u);
	xB.SetTreeBrushSettings(99, 0.5f, 2.0f, 1.0f, 10.0f, 777u);
	ZENITH_ASSERT_EQ(xA.GetTreeRngState_ForTest(), xB.GetTreeRngState_ForTest(),
		"same seed must produce the same scatter RNG state");
}

ZENITH_TEST(TerrainEditor, SetTreeBrushAutomationStepRoutes)
{
	// Packing: the step encodes its args into the action's int/float slots.
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_TerrainSetTreeBrush(40, 0.85f, 1.45f, 3.0f, 34.0f, 4242);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one action must be queued");
	const Zenith_EditorAction& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::TERRAIN_EDITOR_SET_TREE_BRUSH,
		"action type must be TERRAIN_EDITOR_SET_TREE_BRUSH");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 40, "trees-per-dab packs into aiArgs[0]");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[1], 4242, "seed packs into aiArgs[1]");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 0.85f, 0.0001f, "scale min packs into afArgs[0]");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[1], 1.45f, 0.0001f, "scale max packs into afArgs[1]");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[2], 3.0f, 0.0001f, "spacing packs into afArgs[2]");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[3], 34.0f, 0.0001f, "max slope packs into afArgs[3]");

	// Routing: draining the queue drives the global terrain editor's setter,
	// proving the enum sits in the terrain sub-range and the executor case is
	// wired. ExecuteNextStep clears the queue when complete, so the packing
	// asserts above (which read xAction) must precede this.
	Zenith_TerrainEditor& xEditor = g_xEngine.TerrainEditor();
	xEditor.SetTreeBrushSettings(1, 0.0f, 0.0f, 0.0f, 0.0f, 1u);  // sentinel (clearly != below)
	xAuto.Begin();
	xAuto.ExecuteNextStep();
	ZENITH_ASSERT_EQ(xEditor.m_xBrush.m_uTreesPerDab, 40u, "executor must route trees-per-dab to the terrain editor");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.m_xBrush.m_fTreeMaxSlopeDeg, 34.0f, 0.0001f, "executor must route max slope");
	ZENITH_ASSERT_EQ(xEditor.GetTreeRngState_ForTest(), 4242u, "executor must route the seed");
	xEditor.Close();  // ExecuteTerrainEditorAction auto-opened a standalone session
}

namespace
{
	// The grass-type map is 1024 texels over the 4096 m terrain, so a world
	// coordinate maps to texel world*0.25. The whole-map scans below walk 1M
	// texels each, so they read through the raw pointer to stay cheap in Debug.
	u_int GrassTypeAt(const Zenith_TerrainEditor& xEditor, u_int uX, u_int uZ)
	{
		return static_cast<u_int>(xEditor.GetGrassType().Get(
			uZ * Zenith_TerrainEditor::uGRASS_TYPE_SIZE + uX));
	}

	void CopyGrassType(const Zenith_TerrainEditor& xEditor, Zenith_Vector<u_int8>& xOut)
	{
		const Zenith_Vector<u_int8>& xSrc = xEditor.GetGrassType();
		xOut.Resize(xSrc.GetSize(), static_cast<u_int8>(0));
		memcpy(xOut.GetDataPointer(), xSrc.GetDataPointer(), xSrc.GetSize());
	}

	u_int CountGrassTypeDifferences(const Zenith_TerrainEditor& xEditor,
		const Zenith_Vector<u_int8>& xReference)
	{
		const Zenith_Vector<u_int8>& xLive = xEditor.GetGrassType();
		if (xLive.GetSize() != xReference.GetSize())
		{
			return xLive.GetSize() + xReference.GetSize();
		}
		const u_int8* puLive = xLive.GetDataPointer();
		const u_int8* puReference = xReference.GetDataPointer();
		u_int uDifferences = 0;
		for (u_int u = 0; u < xLive.GetSize(); u++)
		{
			if (puLive[u] != puReference[u])
			{
				uDifferences++;
			}
		}
		return uDifferences;
	}

	// Texels holding a value the caller never painted. A type index is
	// categorical, so ANY such texel is a blend the map must never contain.
	u_int CountGrassTypeTexelsOutside(const Zenith_TerrainEditor& xEditor,
		const u_int8* puAllowed, u_int uAllowedCount)
	{
		const Zenith_Vector<u_int8>& xTypes = xEditor.GetGrassType();
		const u_int8* puTypes = xTypes.GetDataPointer();
		u_int uUnexpected = 0;
		for (u_int u = 0; u < xTypes.GetSize(); u++)
		{
			bool bAllowed = false;
			for (u_int uA = 0; uA < uAllowedCount; uA++)
			{
				if (puTypes[u] == puAllowed[uA])
				{
					bAllowed = true;
					break;
				}
			}
			if (!bAllowed)
			{
				uUnexpected++;
			}
		}
		return uUnexpected;
	}

	u_int CountGrassTypeTexelsEqualTo(const Zenith_TerrainEditor& xEditor, u_int8 uValue)
	{
		const Zenith_Vector<u_int8>& xTypes = xEditor.GetGrassType();
		const u_int8* puTypes = xTypes.GetDataPointer();
		u_int uCount = 0;
		for (u_int u = 0; u < xTypes.GetSize(); u++)
		{
			if (puTypes[u] == uValue)
			{
				uCount++;
			}
		}
		return uCount;
	}

	// A scratch directory wiped on scope ENTRY and EXIT, so a mid-test failure
	// cannot leak a 1 MB .ztxtr into the next run.
	struct TerrainEditorScratchDir
	{
		std::filesystem::path m_xPath;

		explicit TerrainEditorScratchDir(const char* szLeafName)
		{
			std::error_code xEC;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xEC);
			if (xEC)
			{
				xRoot = ".";
			}
			m_xPath = xRoot / "zenith_terraineditor_tests" / szLeafName;
			std::filesystem::remove_all(m_xPath, xEC);
			std::filesystem::create_directories(m_xPath, xEC);
		}

		~TerrainEditorScratchDir()
		{
			std::error_code xEC;
			std::filesystem::remove_all(m_xPath, xEC);
		}

		std::string File(const char* szLeafName) const
		{
			return (m_xPath / szLeafName).string();
		}
	};

	// Byte-for-byte the layout WriteZtxtr emits for GrassType.ztxtr: envelope,
	// i32 w, i32 h, i32 depth, TextureFormat, u32 mip count, u64 size, pixels.
	// There is one .ztxtr layout and this must stay in step with the bake's.
	void WriteGrassTypeZtxtr(const std::string& strPath, const Zenith_Vector<u_int8>& xTypes)
	{
		const size_t ulDataSize = static_cast<size_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE) *
			Zenith_TerrainEditor::uGRASS_TYPE_SIZE;
		Zenith_DataStream xStream;
		Zenith_WriteStreamHeader(xStream, uZENITH_TEXTURE_ASSET_TYPE_ID, uZENITH_TEXTURE_SCHEMA_V2);
		xStream << static_cast<int32_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE);
		xStream << static_cast<int32_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE);
		xStream << static_cast<int32_t>(1);
		xStream << TEXTURE_FORMAT_R8_UNORM;
		xStream << static_cast<uint32_t>(1);   // this level only
		xStream << ulDataSize;
		xStream.WriteData(xTypes.GetDataPointer(), ulDataSize);
		xStream.WriteToFile(strPath.c_str());
	}
}

ZENITH_TEST(TerrainEditor, GrassTypeDabHardEdge)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();
	// OpenStandalone seeds from any baked terrain textures in the game's
	// assets dir (game-dependent). This test asserts against pristine
	// defaults, so reset explicitly.
	xEditor.ResetImagesToDefaults();
	// The claimed disc is where the falloff reaches 0.5, which is falloff-
	// dependent: Smooth reaches it at HALF the brush radius. Pin the curve so
	// the inside/outside sample coordinates below stay exact.
	xEditor.m_xBrush.m_eFalloff = Zenith_TerrainBrushFalloff::Smooth;

	// World (800,800) r=200 -> texel centre (200,200), texel radius 50,
	// claimed radius 25.
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 800.0f, 800.0f, 200.0f, 1.0f, 7.0f);

	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 200, 200), 7u, "Dab centre must hold the painted type index exactly");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 210, 200), 7u, "Texels well inside the claimed disc must hold the painted index");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 200, 215), 7u, "Texels well inside the claimed disc must hold the painted index");
	// Inside the brush radius but outside the claimed disc: a blending kernel
	// would leave a partial value here, a hard-edged stamp leaves it untouched.
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 235, 200), 0u, "Texels past the falloff threshold must stay untouched");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 100, 100), 0u, "Texels outside the brush must stay untouched");

	ZENITH_ASSERT_GT(CountGrassTypeTexelsEqualTo(xEditor, static_cast<u_int8>(7)), 0u, "The dab must claim at least one texel");
	const u_int8 auAfterFirstDab[] = { 0, 7 };
	ZENITH_ASSERT_EQ(CountGrassTypeTexelsOutside(xEditor, auAfterFirstDab, 2), 0u,
		"A categorical paint must never produce an intermediate index anywhere in the map");

	// 255 is the erase sentinel and the extreme of the float->u8 tool-value
	// path — it must survive the cast rather than saturate to another index.
	// World (820,800) -> texel centre (205,200), same claimed radius 25.
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 820.0f, 800.0f, 200.0f, 1.0f, 255.0f);

	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 205, 200), 255u, "The erase sentinel must overwrite the overlap exactly");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 195, 200), 255u, "The erase sentinel must overwrite the overlap exactly");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 178, 200), 7u, "Texels only the first dab claimed must keep its index");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 228, 200), 255u, "Texels only the second dab claims must take the sentinel");

	const u_int8 auAfterSecondDab[] = { 0, 7, 255 };
	ZENITH_ASSERT_EQ(CountGrassTypeTexelsOutside(xEditor, auAfterSecondDab, 3), 0u,
		"Overlapping categorical dabs must never blend into a third index");
}

ZENITH_TEST(TerrainEditor, GrassTypeStrokeUndoRoundtrip)
{
	g_xEngine.UndoSystem().Clear();
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();
		xEditor.ResetImagesToDefaults();

		// The stroke-undo machinery strides by GetMapTexelBytes; the grass-type
		// map is the only 1-byte map, so this is what pins the per-map stride.
		Zenith_Vector<u_int8> xBefore;
		CopyGrassType(xEditor, xBefore);

		xEditor.BeginStroke();
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 1200.0f, 1200.0f, 120.0f, 1.0f, 5.0f);
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 1240.0f, 1200.0f, 120.0f, 1.0f, 5.0f);
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 1280.0f, 1200.0f, 120.0f, 1.0f, 5.0f);
		xEditor.EndStroke();

		Zenith_Vector<u_int8> xAfter;
		CopyGrassType(xEditor, xAfter);
		ZENITH_ASSERT_GT(CountGrassTypeDifferences(xEditor, xBefore), 0u, "The stroke must change the grass-type map");
		ZENITH_ASSERT_TRUE(g_xEngine.UndoSystem().CanUndo(), "A grass-type stroke must push an undo command");

		g_xEngine.UndoSystem().Undo();
		ZENITH_ASSERT_EQ(CountGrassTypeDifferences(xEditor, xBefore), 0u, "Undo must restore the pre-stroke map byte-for-byte");

		g_xEngine.UndoSystem().Redo();
		ZENITH_ASSERT_EQ(CountGrassTypeDifferences(xEditor, xAfter), 0u, "Redo must re-apply the stroke byte-for-byte");

		// The commands reference the local editor — clear before scope exit.
		g_xEngine.UndoSystem().Clear();
	}
}

ZENITH_TEST(TerrainEditor, GrassTypeZtxtrSaveLoadRoundTrip)
{
	// Absent-file fallback, through the real load path: a named set that was
	// never baked resolves to a directory with no GrassType.ztxtr, and
	// OpenStandalone's LoadImagesFromAssets must leave the documented all-zero
	// default rather than failing or leaking a previous set's pixels.
	{
		Zenith_TerrainEditor xUnbaked;
		ZENITH_ASSERT_TRUE(xUnbaked.SetAssetSet("ZenithUnitTestUnbakedSet"), "A well-formed set name must stage");
		xUnbaked.OpenStandalone();
		ZENITH_ASSERT_EQ(xUnbaked.GetGrassType().GetSize(),
			Zenith_TerrainEditor::uGRASS_TYPE_SIZE * Zenith_TerrainEditor::uGRASS_TYPE_SIZE,
			"The grass-type map is one byte per texel");
		ZENITH_ASSERT_EQ(CountGrassTypeTexelsEqualTo(xUnbaked, static_cast<u_int8>(0)),
			xUnbaked.GetGrassType().GetSize(),
			"A set with no GrassType texture must load as the all-zero default type");
	}

	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();
	xEditor.ResetImagesToDefaults();
	xEditor.m_xBrush.m_eFalloff = Zenith_TerrainBrushFalloff::Smooth;

	// Distinct indices at known texels (world*0.25), including the erase
	// sentinel, so a round trip that swapped or truncated bytes cannot pass.
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 512.0f, 512.0f, 160.0f, 1.0f, 3.0f);
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 1536.0f, 512.0f, 160.0f, 1.0f, 9.0f);
	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 512.0f, 1536.0f, 160.0f, 1.0f, 255.0f);

	Zenith_Vector<u_int8> xPainted;
	CopyGrassType(xEditor, xPainted);

	const TerrainEditorScratchDir xScratch("grass_type_roundtrip");
	const std::string strPath = xScratch.File("GrassType" ZENITH_TEXTURE_EXT);
	WriteGrassTypeZtxtr(strPath, xEditor.GetGrassType());

	// Zero the live map so a match below cannot come from the bytes that are
	// already there.
	Zenith_Vector<u_int8>& xLiveTypes = const_cast<Zenith_Vector<u_int8>&>(xEditor.GetGrassType());
	memset(xLiveTypes.GetDataPointer(), 0, xLiveTypes.GetSize());
	ZENITH_ASSERT_EQ(CountGrassTypeTexelsEqualTo(xEditor, static_cast<u_int8>(0)), xLiveTypes.GetSize(),
		"The map must be zeroed before the load half of the round trip");

	// Read back through the single engine-wide .ztxtr parser — the one the
	// editor's own loader calls.
	Flux_SurfaceInfo xInfo;
	Zenith_Vector<uint8_t> xBytes;
	ZENITH_ASSERT_TRUE(Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes).IsOk(),
		"The written GrassType texture must parse");
	ZENITH_ASSERT_EQ(xInfo.m_uWidth, Zenith_TerrainEditor::uGRASS_TYPE_SIZE, "Round-tripped width must match the map");
	ZENITH_ASSERT_EQ(xInfo.m_uHeight, Zenith_TerrainEditor::uGRASS_TYPE_SIZE, "Round-tripped height must match the map");
	ZENITH_ASSERT_TRUE(xInfo.m_eFormat == TEXTURE_FORMAT_R8_UNORM, "Grass type must round-trip as a single-channel 8-bit texture");
	ZENITH_ASSERT_EQ(xBytes.GetSize(), xLiveTypes.GetSize(), "The payload must be exactly one byte per texel");

	// The size-guarded memcpy LoadImagesFromAssets performs for this map.
	if (xBytes.GetSize() == xLiveTypes.GetSize())
	{
		memcpy(xLiveTypes.GetDataPointer(), xBytes.GetDataPointer(), xBytes.GetSize());
	}

	ZENITH_ASSERT_EQ(CountGrassTypeDifferences(xEditor, xPainted), 0u,
		"The loaded map must match the painted map byte-for-byte");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 128, 128), 3u, "Painted index 3 must survive the round trip");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 384, 128), 9u, "Painted index 9 must survive the round trip");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 128, 384), 255u, "The erase sentinel must survive the round trip");
	ZENITH_ASSERT_EQ(GrassTypeAt(xEditor, 900, 900), 0u, "Unpainted texels must round-trip as the default type");
}

ZENITH_TEST(TerrainEditor, SculptNotifyLatchesOnHeightStroke)
{
	g_xEngine.UndoSystem().Clear();
	{
		Zenith_TerrainEditor xEditor;
		xEditor.OpenStandalone();

		ZENITH_ASSERT_FALSE(xEditor.ConsumeHeightsEditedSinceGrassPush(),
			"A fresh session has sculpted nothing, so grass placed on the surface is still valid");

		// Grass paints dirty grass state directly; only a HEIGHT edit silently
		// invalidates grass already placed on the old surface.
		xEditor.BeginStroke();
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::GrassType, 2000.0f, 2000.0f, 80.0f, 1.0f, 4.0f);
		xEditor.EndStroke();
		ZENITH_ASSERT_FALSE(xEditor.ConsumeHeightsEditedSinceGrassPush(),
			"A grass-type-only stroke must not claim the surface moved");

		xEditor.BeginStroke();
		xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, 2000.0f, 2000.0f, 80.0f, 1.0f, 0.0f);
		xEditor.EndStroke();
		ZENITH_ASSERT_TRUE(xEditor.ConsumeHeightsEditedSinceGrassPush(),
			"A height stroke must latch the sculpt notification");
		ZENITH_ASSERT_FALSE(xEditor.ConsumeHeightsEditedSinceGrassPush(),
			"Consuming the notification must clear it");

		// The strokes' commands reference the local editor — clear before scope exit.
		g_xEngine.UndoSystem().Clear();
	}
}

//=============================================================================
// Grass types — the working copy + the GRASS_TYPES_* automation family.
//=============================================================================

namespace
{
	// GrassTypes_Save has NO path parameter by design: it writes the one
	// canonical game:Vegetation/GrassTypes.zdata a game boot-loads. A test that
	// exercises it therefore has to restore the real path, and the ENGINE table
	// too — Save applies, so a leaked table would change every later test's
	// grass. Both halves are captured here and put back by the destructor.
	struct GrassTypeSaveGuard
	{
		std::string           m_strResolved;
		bool                  m_bExisted = false;
		Zenith_Vector<u_int8> m_xOriginalBytes;
		Flux_GrassTypeTable   m_xEngineTable;

		GrassTypeSaveGuard()
		{
			m_strResolved = Zenith_AssetRegistry::ResolvePath(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
			m_xEngineTable = g_xEngine.Grass().GetTypeTable();

			std::error_code xEC;
			m_bExisted = std::filesystem::exists(Path(), xEC) && !xEC;
			if (m_bExisted)
			{
				// Byte-exact restore rather than delete-if-we-made-it: a repo that
				// DOES ship a table must come out of this test unchanged.
				const uintmax_t ulSize = std::filesystem::file_size(Path(), xEC);
				if (!xEC && ulSize > 0)
				{
					m_xOriginalBytes.Resize(static_cast<u_int>(ulSize), 0u);
					std::ifstream xIn(Path(), std::ios::binary);
					xIn.read(reinterpret_cast<char*>(m_xOriginalBytes.GetDataPointer()),
						static_cast<std::streamsize>(ulSize));
				}
			}
		}

		~GrassTypeSaveGuard()
		{
			std::error_code xEC;
			if (m_bExisted && m_xOriginalBytes.GetSize() > 0)
			{
				std::ofstream xOut(Path(), std::ios::binary);
				xOut.write(reinterpret_cast<const char*>(m_xOriginalBytes.GetDataPointer()),
					static_cast<std::streamsize>(m_xOriginalBytes.GetSize()));
			}
			else
			{
				std::filesystem::remove(Path(), xEC);
			}

			// Through Apply, not a bare SetTypeTable: Save re-placed the grass, so
			// the blade records standing in the world still carry the test's types
			// until something re-places them again.
			Zenith_TerrainEditor& xTerrainEditor = g_xEngine.TerrainEditor();
			xTerrainEditor.GrassTypes() = m_xEngineTable;
			xTerrainEditor.GrassTypes_Apply();
		}

		std::filesystem::path Path() const { return std::filesystem::path(m_strResolved); }

		bool FileExists() const
		{
			std::error_code xEC;
			return std::filesystem::exists(Path(), xEC) && !xEC;
		}
	};
}

ZENITH_TEST(TerrainEditor, GrassTypesWorkingCopyIsolatesTheEngineTable)
{
	// The whole point of a working copy: an in-progress edit must never reach
	// the placement CS, so nothing but Apply/Save may move the engine table.
	const Flux_GrassTypeTable xEngineBefore = g_xEngine.Grass().GetTypeTable();

	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// Open seeds the working copy FROM the engine, so an editor booting onto a
	// game that ships an authored table does not offer to overwrite it with the
	// built-ins.
	ZENITH_ASSERT_EQ(xEditor.GrassTypes().GetCount(), xEngineBefore.GetCount(),
		"opening a session must seed the working copy from the live engine table");

	xEditor.GrassTypes().SetCount(9u);
	xEditor.GrassTypes().SetName(0u, "WorkingOnly");
	xEditor.GrassTypes().Get(0u).m_fHeightMax = 4.25f;

	ZENITH_ASSERT_EQ(g_xEngine.Grass().GetTypeTable().GetCount(), xEngineBefore.GetCount(),
		"editing the working copy must not touch the engine table");
	ZENITH_ASSERT_STREQ(g_xEngine.Grass().GetTypeTable().GetName(0u).c_str(), xEngineBefore.GetName(0u).c_str(),
		"editing the working copy must not touch the engine names");

	// Reload discards the edit by re-reading the engine.
	xEditor.GrassTypes_Reload();
	ZENITH_ASSERT_EQ(xEditor.GrassTypes().GetCount(), xEngineBefore.GetCount(), "Reload must restore the engine count");
	ZENITH_ASSERT_STREQ(xEditor.GrassTypes().GetName(0u).c_str(), xEngineBefore.GetName(0u).c_str(),
		"Reload must restore the engine names");

	// Reset is the OTHER discard: back to the built-ins, engine untouched.
	xEditor.GrassTypes().SetCount(11u);
	xEditor.GrassTypes_Reset();
	ZENITH_ASSERT_EQ(xEditor.GrassTypes().GetCount(), 4u, "Reset must restore the four built-in types");
	ZENITH_ASSERT_STREQ(xEditor.GrassTypes().GetName(0u).c_str(), "Meadow", "Reset must restore the built-in names");
	ZENITH_ASSERT_EQ(g_xEngine.Grass().GetTypeTable().GetCount(), xEngineBefore.GetCount(),
		"Reset must not touch the engine table");

	// Apply is the one verb that does.
	xEditor.GrassTypes().SetCount(6u);
	xEditor.GrassTypes().SetName(5u, "AppliedType");
	xEditor.GrassTypes_Apply();
	ZENITH_ASSERT_EQ(g_xEngine.Grass().GetTypeTable().GetCount(), 6u, "Apply must push the working copy to the engine");
	ZENITH_ASSERT_STREQ(g_xEngine.Grass().GetTypeTable().GetName(5u).c_str(), "AppliedType",
		"Apply must push the working copy names");

	// Apply validates the WORKING copy too, not only the engine copy: a slider
	// left holding a value the engine clamped would disagree with the screen.
	xEditor.GrassTypes().Get(0u).m_fClumpScale = 0.0f;   // divides by zero in the Voronoi search
	xEditor.GrassTypes_Apply();
	ZENITH_ASSERT_GT(xEditor.GrassTypes().Get(0u).m_fClumpScale, 0.0f,
		"Apply must clamp the working copy in place, not only the engine copy");

	// The engine table is global state no scene owns — put it back.
	g_xEngine.Grass().SetTypeTable(xEngineBefore);
}

ZENITH_TEST(TerrainEditor, GrassTypesAutomationFamilyRoutesEndToEnd)
{
	const Flux_GrassTypeTable xEngineBefore = g_xEngine.Grass().GetTypeTable();
	Zenith_TerrainEditor& xEditor = g_xEngine.TerrainEditor();

	// Sentinel: clearly different from everything the recipe below authors, so a
	// step that silently no-ops cannot pass by leaving a plausible value behind.
	xEditor.GrassTypes().SetCount(2u);
	xEditor.GrassTypes().SetName(4u, "SENTINEL");

	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_GrassTypesCreate();
	xAuto.AddStep_GrassTypesSetCount(5);
	xAuto.AddStep_GrassTypesSetName(4, "Reeds");
	xAuto.AddStep_GrassTypesSetParamFloat(4, "HeightMax", 2.0f);
	xAuto.AddStep_GrassTypesSetParamFloat(4, "WindResponse", 2.5f);
	xAuto.AddStep_GrassTypesSetParamColor(4, "BaseColour", 0.9f, 0.1f, 0.05f);
	xAuto.AddStep_GrassTypesSetParamColor(4, "TipColour", 0.2f, 0.8f, 0.3f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 7u, "seven steps must be queued");

	// Packing, read BEFORE the drain clears the queue (the SetTreeBrush test
	// orders it the same way, for the same reason).
	const Zenith_EditorAction& xNameAction = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_TRUE(xNameAction.m_eType == Zenith_EditorActionType::GRASS_TYPES_SET_NAME,
		"step 2 must be GRASS_TYPES_SET_NAME");
	ZENITH_ASSERT_EQ(xNameAction.m_aiArgs[0], 4, "the type index packs into aiArgs[0]");
	ZENITH_ASSERT_STREQ(xNameAction.m_szArg1.c_str(), "Reeds", "the name packs into szArg1");
	const Zenith_EditorAction& xColourAction = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_STREQ(xColourAction.m_szArg1.c_str(), "BaseColour", "the param name packs into szArg1");
	ZENITH_ASSERT_EQ_FLOAT(xColourAction.m_afArgs[2], 0.05f, 0.0001f, "blue packs into afArgs[2]");

	xAuto.Begin();
	while (!xAuto.IsComplete())
	{
		xAuto.ExecuteNextStep();
	}

	// Routing: the whole block reached ExecuteGrassTypeAction, which proves every
	// member sits inside the contiguous range the router compares against.
	const Flux_GrassTypeTable& xWorking = xEditor.GrassTypes();
	ZENITH_ASSERT_EQ(xWorking.GetCount(), 5u, "SetCount must reach the working table");
	ZENITH_ASSERT_STREQ(xWorking.GetName(4u).c_str(), "Reeds", "SetName must reach the working table");
	ZENITH_ASSERT_EQ_FLOAT(xWorking.Get(4u).m_fHeightMax, 2.0f, 0.0001f, "HeightMax must reach m_fHeightMax");
	ZENITH_ASSERT_EQ_FLOAT(xWorking.Get(4u).m_fWindResponse, 2.5f, 0.0001f, "WindResponse must reach m_fWindResponse");
	ZENITH_ASSERT_EQ_FLOAT(xWorking.Get(4u).m_xBaseColour.x, 0.9f, 0.0001f, "BaseColour must reach m_xBaseColour");
	ZENITH_ASSERT_EQ_FLOAT(xWorking.Get(4u).m_xTipColour.y, 0.8f, 0.0001f, "TipColour must reach m_xTipColour");

	// Create is a full reset, so the sentinel is gone and entries 0..3 are the
	// built-ins even though the count is now 5.
	ZENITH_ASSERT_STREQ(xWorking.GetName(0u).c_str(), "Meadow", "Create must seed the built-in set");

	// The engine table is untouched until a Save (or a panel Apply) runs.
	ZENITH_ASSERT_EQ(g_xEngine.Grass().GetTypeTable().GetCount(), xEngineBefore.GetCount(),
		"the authoring steps alone must not move the engine table");

	g_xEngine.Grass().SetTypeTable(xEngineBefore);
}

ZENITH_TEST(TerrainEditor, GrassTypesSaveWritesTheAssetAndApplies)
{
	// Restores the real .zdata path AND the engine table on scope exit. Save has
	// no temp-path variant, so the only honest test is the real path with a
	// byte-exact restore.
	GrassTypeSaveGuard xGuard;

	auto RunAutomation = [](Zenith_EditorAutomation& xAutomation)
	{
		xAutomation.Begin();
		while (!xAutomation.IsComplete())
		{
			xAutomation.ExecuteNextStep();
		}
	};

	// ★★ PRIME THE REGISTRY CACHE WITH A DELIBERATELY WRONG TABLE FIRST.
	// The round-trip clause at the bottom reads the written bytes back through
	// Zenith_AssetRegistry::GetView, which returns a CACHED asset when the path is
	// already loaded — so without an eviction it can answer with a table that has
	// nothing to do with the bytes this test just wrote, and pass or fail on
	// something else entirely.
	//
	// That is not hypothetical: Flux_GrassImpl::LoadAuthoredTypeTable
	// (Flux_Grass.cpp) boot-loads this exact path through GetView, but ONLY when
	// the file exists — it early-returns on absence, by design, so a missing table
	// does not log two load failures on every boot of every game. The consequence
	// is that the cache is primed on a game that SHIPS an authored table and empty
	// on one that does not, and this unit's outcome followed the game rather than
	// the code: it was red on RenderTest (whose automation authors 4 types) and
	// green on Zenithmon (which authors none) for the same engine tree, with the
	// stale 4 read as the "round-tripped" count.
	//
	// So the precondition is CREATED here rather than depended upon. Priming with 5
	// — distinct from this test's 3, from the 4 built-ins Create seeds, and from
	// RenderTest's 4 — means a regression of the eviction below reds on EVERY game
	// instead of only on one whose pin CI never runs.
	Zenith_EditorAutomation xPrime;
	xPrime.AddStep_GrassTypesCreate();
	xPrime.AddStep_GrassTypesSetCount(5);
	xPrime.AddStep_GrassTypesSetName(4, "StaleCacheSentinel");
	xPrime.AddStep_GrassTypesSave();
	RunAutomation(xPrime);

	// Evict whatever the boot cached, then load — so the cache now holds the
	// 5-entry table on every game, not "whatever this game happened to ship".
	Zenith_AssetRegistry::ForceUnload(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	const Zenith_GrassTypeTableAsset* pxPrimed =
		Zenith_AssetRegistry::GetView<Zenith_GrassTypeTableAsset>(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	ZENITH_ASSERT_TRUE(pxPrimed != nullptr && pxPrimed->LoadedOk(),
		"the priming save must itself round-trip, or the stale-cache precondition "
		"below is not actually set up");
	ZENITH_ASSERT_EQ(pxPrimed->GetTable().GetCount(), 5u,
		"the cache must hold the 5-entry priming table — if it does not, the "
		"eviction assertion further down proves nothing");

	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_GrassTypesCreate();
	xAuto.AddStep_GrassTypesSetCount(3);
	xAuto.AddStep_GrassTypesSetName(2, "SavedType");
	xAuto.AddStep_GrassTypesSetParamFloat(2, "Density", 0.125f);
	xAuto.AddStep_GrassTypesSave();
	RunAutomation(xAuto);

	ZENITH_ASSERT_TRUE(xGuard.FileExists(), "GrassTypesSave must create the .zdata (parent directories included)");

	// Save applies LAST, so a file that reached disk without taking effect in the
	// running editor — the one failure an author cannot see — fails here.
	const Flux_GrassTypeTable& xEngine = g_xEngine.Grass().GetTypeTable();
	ZENITH_ASSERT_EQ(xEngine.GetCount(), 3u, "Save must apply the saved table to the engine");
	ZENITH_ASSERT_STREQ(xEngine.GetName(2u).c_str(), "SavedType", "Save must apply the saved names");
	ZENITH_ASSERT_EQ_FLOAT(xEngine.Get(2u).m_fDensity, 0.125f, 0.0001f, "Save must apply the saved params");

	// The bytes must LOAD back through the real asset path, not merely exist: a
	// file whose envelope or table version were wrong would still be a file.
	//
	// ★ THE EVICTION IS THE ASSERTION. GetView returns the CACHED asset for a path
	// that is already loaded, and the priming block above deliberately left a
	// 5-entry one there, so without this ForceUnload every clause below would read
	// that stale sibling and the whole round-trip claim would be vacuous. Dropping
	// the entry first is what makes the load come off disk — still THROUGH the
	// registry, so the envelope and table-version handling this clause exists to
	// cover are genuinely exercised, rather than side-stepped by parsing the file
	// by hand.
	Zenith_AssetRegistry::ForceUnload(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	Zenith_GrassTypeTableAsset* pxLoaded =
		Zenith_AssetRegistry::GetView<Zenith_GrassTypeTableAsset>(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	ZENITH_ASSERT_TRUE(pxLoaded != nullptr, "the written file must load through the registry");
	if (pxLoaded != nullptr)
	{
		ZENITH_ASSERT_TRUE(pxLoaded->LoadedOk(), "the written file must pass the fail-safe reader");
		// A count of 5 here specifically means the eviction above stopped working
		// and this is the priming table, not the written one.
		ZENITH_ASSERT_EQ(pxLoaded->GetTable().GetCount(), 3u,
			"the round-tripped count must match (a 5 means GetView answered from the "
			"cache instead of the bytes just written)");
		ZENITH_ASSERT_STREQ(pxLoaded->GetTable().GetName(2u).c_str(), "SavedType", "the round-tripped name must match");
		ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetTable().Get(2u).m_fDensity, 0.125f, 0.0001f,
			"the round-tripped param must match");
	}
	// The registry now caches an asset keyed on the path the guard is about to
	// restore — drop it rather than leave a stale table for a later GetView.
	Zenith_AssetRegistry::ForceUnload(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
}

//=============================================================================
// Configurable dimensions: the world <-> heightfield-pixel conversions.
//=============================================================================

ZENITH_TEST(TerrainEditor, DimensionValidationMatchesTheSpec)
{
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// A fresh session starts on the historical shape.
	ZENITH_ASSERT_TRUE(xEditor.GetDimensions() == Zenith_TerrainDimensions::Default(),
		"a fresh session must start on the default dimensions");
	ZENITH_ASSERT_TRUE(xEditor.WorldSize() == 4096.0f, "the default session domain is 4096m");
	ZENITH_ASSERT_TRUE(xEditor.HeightPxPerWorld() == 1.0f,
		"a default session must be EXACTLY one heightfield pixel per metre — the identity every "
		"default-dimensioned sculpt reproduces");

	const Zenith_TerrainDimensions xSmall{ 64.0f, 64u, 6u, 9u };
	ZENITH_ASSERT_TRUE(xEditor.SetDimensions(xSmall), "a valid spec must stage");
	ZENITH_ASSERT_TRUE(xEditor.GetDimensions() == xSmall, "the staged spec must be observable");
	ZENITH_ASSERT_TRUE(xEditor.GetDimensionsValidationError().empty(),
		"a valid spec must clear the validation error");
	ZENITH_ASSERT_TRUE(xEditor.WorldSize() == 576.0f,
		"the domain is the LONGER axis of a 6x9 grid at 64m");

	// A refused spec is transactional: the session keeps what it had.
	const Zenith_TerrainDimensions xBad{ 64.0f, 48u, 6u, 9u };   // 48 is not a power of two
	ZENITH_ASSERT_FALSE(xEditor.SetDimensions(xBad), "a non-power-of-two quad count must be refused");
	ZENITH_ASSERT_TRUE(xEditor.GetDimensions() == xSmall,
		"a refused spec must leave the staged one untouched");
	ZENITH_ASSERT_FALSE(xEditor.GetDimensionsValidationError().empty(),
		"a refused spec must report why");
}

ZENITH_TEST(TerrainEditor, BrushDabLandsOnThePredictedTexelsAtNonDefaultDimensions)
{
	// ★ THE STRAGGLER DETECTOR. Roughly ninety sites in these files used to pass a
	// WORLD coordinate straight into a PIXEL-indexed function, which was correct
	// only because one pixel was one metre. Every one of those is invisible at
	// default dimensions, so this test is the only thing that can see them: it
	// sculpts a NON-default session and checks the edit landed where the
	// conversion says it should — and, just as importantly, that it did NOT land
	// where the old 1m/px assumption would have put it.
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	// 384 x 576 m over the same 4096px heightfield: about 7.11 px per metre, so a
	// world coordinate and its pixel coordinate are far apart.
	const Zenith_TerrainDimensions xSmall{ 64.0f, 64u, 6u, 9u };
	ZENITH_ASSERT_TRUE(xEditor.SetDimensions(xSmall), "fixture spec must stage");
	const float fPxPerWorld = xEditor.HeightPxPerWorld();
	ZENITH_ASSERT_GT(fPxPerWorld, 7.0f, "the fixture must actually differ from 1 px per metre");

	const float fWORLD_X = 100.0f;
	const float fWORLD_Z = 100.0f;
	const float fWORLD_RADIUS = 4.0f;

	// The texel the dab must move, and one the old 1m/px reading would have moved
	// instead. They are ~600 px apart, so no falloff can blur them together.
	const float fExpectedPxX = fWORLD_X * fPxPerWorld;
	const float fStalePxX = fWORLD_X;
	ZENITH_ASSERT_GT(fExpectedPxX - fStalePxX, 100.0f,
		"the correct and the stale pixel must be far enough apart that only one can be inside the brush");

	const float fBeforeAtTarget = xEditor.SampleHeightNorm(fExpectedPxX, fWORLD_Z * fPxPerWorld);
	const float fBeforeAtStale = xEditor.SampleHeightNorm(fStalePxX, fWORLD_Z);

	xEditor.ApplyBrushDab(Zenith_TerrainBrushTool::Raise, fWORLD_X, fWORLD_Z,
		fWORLD_RADIUS, 1.0f, 0.0f);

	ZENITH_ASSERT_GT(xEditor.SampleHeightNorm(fExpectedPxX, fWORLD_Z * fPxPerWorld), fBeforeAtTarget,
		"the dab must raise the texel the world->pixel conversion points at");
	ZENITH_ASSERT_EQ_FLOAT(xEditor.SampleHeightNorm(fStalePxX, fWORLD_Z), fBeforeAtStale, 1.0e-6f,
		"the dab must NOT touch the texel the old one-pixel-per-metre reading would have hit");

	// SampleHeightWorld must agree with the same conversion -- it is the other
	// direction of the same weld, and the raycast/tree paths stand on it.
	ZENITH_ASSERT_EQ_FLOAT(xEditor.SampleHeightWorld(fWORLD_X, fWORLD_Z),
		xEditor.SampleHeightNorm(fExpectedPxX, fWORLD_Z * fPxPerWorld) * Zenith_TerrainEditor::fTERRAIN_MAX_HEIGHT,
		1.0e-4f,
		"SampleHeightWorld must read the same texel the brush wrote");

	// The dab's chunk-dirty bit must be the chunk the WORLD position falls in
	// (chunk 1 of a 64m grid at x=100m), not the chunk a pixel-as-metre reading
	// would have produced.
	const u_int uExpectedChunkX = static_cast<u_int>(fWORLD_X / xSmall.m_fChunkWorldSize);
	const u_int uExpectedChunkZ = static_cast<u_int>(fWORLD_Z / xSmall.m_fChunkWorldSize);
	ZENITH_ASSERT_EQ(uExpectedChunkX, 1u, "100m at 64m chunks is chunk 1");
	ZENITH_ASSERT_TRUE(xEditor.IsChunkSessionDirty(uExpectedChunkX * Zenith_TerrainEditor::uCHUNK_GRID + uExpectedChunkZ),
		"the dab must dirty the chunk its WORLD position falls in");
}

ZENITH_TEST(TerrainEditor, DefaultDimensionsReproduceTheOnePixelPerMetreArithmetic)
{
	// The other half of the straggler detector: at default dimensions the new
	// conversion has to be the IDENTITY, or commit A moves every existing sculpt.
	Zenith_TerrainEditor xEditor;
	xEditor.OpenStandalone();

	ZENITH_ASSERT_TRUE(xEditor.HeightPxPerWorld() == 1.0f, "exactly one pixel per metre");
	ZENITH_ASSERT_TRUE(xEditor.WorldPerHeightPx() == 1.0f, "exactly one metre per pixel");
	for (u_int u = 0; u <= 8u; u++)
	{
		const float fWorld = static_cast<float>(u) * 512.0f;
		ZENITH_ASSERT_TRUE(xEditor.WorldToHeightPx(fWorld) == fWorld,
			"world->pixel must be the identity at default dimensions (case %u)", u);
		ZENITH_ASSERT_TRUE(xEditor.HeightPxToWorld(fWorld) == fWorld,
			"pixel->world must be the identity at default dimensions (case %u)", u);
	}

	// And the image-to-image ratios are dimension-independent by construction.
	ZENITH_ASSERT_TRUE(Zenith_TerrainEditor::fSPLAT_PX_PER_HEIGHT_PX == 0.5f,
		"2048 splat texels over a 4096px heightfield is one half, whatever the terrain measures");
	ZENITH_ASSERT_TRUE(Zenith_TerrainEditor::fGRASS_DENSITY_PX_PER_HEIGHT_PX == 0.25f,
		"1024 grass texels over a 4096px heightfield is one quarter");
	ZENITH_ASSERT_TRUE(Zenith_TerrainEditor::fGRASS_TYPE_PX_PER_HEIGHT_PX == 0.25f,
		"1024 grass-type texels over a 4096px heightfield is one quarter");
}

#endif // ZENITH_TESTING
