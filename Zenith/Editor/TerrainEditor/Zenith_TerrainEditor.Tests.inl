//=============================================================================
// Zenith_TerrainEditor unit tests (headless-safe: standalone sessions only —
// no terrain entity resolves, so no hook / eviction / GPU path is reached).
// Included from the bottom of Zenith_TerrainEditor.cpp.
//=============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor_Noise.h"

#ifdef ZENITH_TESTING

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
	xDecals.ResetForTest();

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

	xDecals.ResetForTest();
}

#endif // ZENITH_TESTING
