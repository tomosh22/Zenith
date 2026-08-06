//=============================================================================
// Zenith_TerrainEditor unit tests (headless-safe: standalone sessions only —
// no terrain entity resolves, so no hook / eviction / GPU path is reached).
// Included from the bottom of Zenith_TerrainEditor.cpp.
//=============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Noise.h"

#ifdef ZENITH_TESTING

#include "Editor/Zenith_EditorAutomation.h"

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

	// Byte-for-byte the layout WriteZtxtr emits for GrassType.ztxtr (i32 w,
	// i32 h, i32 depth, TextureFormat, u64 size, pixels). The terrain maps
	// predate the stream envelope, so the loader reads them through its legacy
	// branch — a header here would take a different path than the bake's file.
	void WriteGrassTypeZtxtr(const std::string& strPath, const Zenith_Vector<u_int8>& xTypes)
	{
		const size_t ulDataSize = static_cast<size_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE) *
			Zenith_TerrainEditor::uGRASS_TYPE_SIZE;
		Zenith_DataStream xStream;
		xStream << static_cast<int32_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE);
		xStream << static_cast<int32_t>(Zenith_TerrainEditor::uGRASS_TYPE_SIZE);
		xStream << static_cast<int32_t>(1);
		xStream << TEXTURE_FORMAT_R8_UNORM;
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

#endif // ZENITH_TESTING
