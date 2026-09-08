#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the capacity guard's proof — it must assert, not crash
#include "Flux/RenderViews/Flux_ViewPassNames.h"
#include "Core/Zenith_Engine.h"                // LiveGraphKnowsTheBases walks the real graph
#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#include <cstring>   // strcmp / memcpy — the content key, spelled as the pool spells it

// ============================================================================
// Flux_ViewPassNames unit tests. Pure CPU (except the last, which READS the
// already-built render graph): no device, no asset, no graph mutation.
//
// WHAT THEY PIN: this pool is about to become the single source of per-view
// pass names, and a pass name is not cosmetic — it is the key FindPass /
// SetPassForceDisabled match on, the label the GPU timer table stores by
// POINTER for the life of the process, and (when two views collide on one
// name) the trigger for the graph's duplicate-name assert. So the tests hold
// the pool to four things a later refactor could quietly break: slot 0 is
// pointer identity, every returned pointer is stable forever, the key is
// string CONTENT and not a pointer, and every one of the 45 preview names
// that exist in the tree today is reproduced EXACTLY.
//
// The 45-row table is the clause that makes a SECOND outlier fail loudly: 44
// of the names are "<base> (Preview)" and one is not, and the only way to know
// a new one has appeared is to have transcribed all 45 from the source.
// ============================================================================

// ----------------------------------------------------------------------------
// 1. Slot 0 is POINTER IDENTITY, not an equal string. FindPass, the
//    force-disable overlay and the profiler labels all key off the main view's
//    historical names; handing back a pooled copy would work until something
//    compared pointers, and the GPU timer table stores the pointer forever.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, Slot0Identity)
{
	// ONE named pointer, not two spellings of "X": literal POOLING is a /GF
	// implementation detail, and this test is about the pool returning the
	// caller's own pointer, not about the compiler folding two literals.
	const char* const szBase = "X";
	ZENITH_ASSERT_TRUE(Flux_ViewPassName(szBase, kuFluxViewSlotMain) == szBase,
		"slot 0 must return the caller's own pointer");
	ZENITH_ASSERT_NULL(Flux_ViewSlotSuffix(kuFluxViewSlotMain),
		"slot 0 has no suffix — it never composes a name");
}

// ----------------------------------------------------------------------------
// 2. One base, eight slots, eight distinct names. A collision here is a
//    duplicate pass name in the graph, which FindPass answers with an INVALID
//    handle (Flux_RenderGraph.cpp:659-661) rather than an ambiguous one.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, UniqueAcrossAllSlots)
{
	const char* const szBase = "X";
	const char* aszNames[FLUX_MAX_RENDER_VIEWS] = {};
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		aszNames[u] = Flux_ViewPassName(szBase, u);
		ZENITH_ASSERT_NOT_NULL(aszNames[u], "every slot yields a name");
	}

	for (u_int uA = 0; uA < FLUX_MAX_RENDER_VIEWS; uA++)
	{
		for (u_int uB = uA + 1u; uB < FLUX_MAX_RENDER_VIEWS; uB++)
		{
			ZENITH_ASSERT_TRUE(strcmp(aszNames[uA], aszNames[uB]) != 0,
				"slots %u and %u must not produce the same pass name", uA, uB);
		}
	}

	ZENITH_ASSERT_STREQ(aszNames[kuFluxViewSlotPreview], "X (Preview)", "the material-preview slot's suffix");
	// Slot 6 is the animation-preview view (unit D2-a names the constant).
	ZENITH_ASSERT_STREQ(aszNames[6], "X (AnimPreview)", "the animation-preview slot's suffix");
}

// ----------------------------------------------------------------------------
// 3. The pointer is STABLE. Flux_RenderGraph.cpp:351 and Zenith_Vulkan.cpp:2201
//    both store the returned pointer for the life of the process, so a pool
//    that re-composed (or, worse, reallocated) per call would dangle.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, PointerStable)
{
	const char* const szBase = "X";
	const char* szFirst = Flux_ViewPassName(szBase, kuFluxViewSlotPreview);
	for (u_int u = 0; u < 8u; u++)
	{
		ZENITH_ASSERT_TRUE(Flux_ViewPassName(szBase, kuFluxViewSlotPreview) == szFirst,
			"repeated calls must return the SAME pointer");
	}
	// A different slot on the same base is a different, equally stable entry.
	const char* szOther = Flux_ViewPassName(szBase, 6u);
	ZENITH_ASSERT_TRUE(szOther != szFirst, "different slots are different entries");
	ZENITH_ASSERT_TRUE(Flux_ViewPassName(szBase, 6u) == szOther, "...and that entry is stable too");
}

// ----------------------------------------------------------------------------
// 4. The key is string CONTENT, never the pointer. MSVC /GF pools identical
//    literals WITHIN a TU, so a pointer key would pass a naive same-file test
//    and then intern the same base twice the moment two TUs used it. Building
//    the base at runtime defeats the pooling and proves the point; overwriting
//    the buffer afterwards proves the pool COPIED the bytes.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, ContentKeyed)
{
	// memcpy rather than strcpy: MSVC deprecates strcpy (C4996) and the repo
	// builds warnings-as-errors. Same thing — a base assembled at runtime.
	char acRuntimeBase[kuFLUX_VIEW_PASS_NAME_MAX_LEN] = {};
	const char acSourceBase[] = "ViewPassNamesContentKey";
	memcpy(acRuntimeBase, acSourceBase, sizeof(acSourceBase));

	const char* szFromBuffer  = Flux_ViewPassName(acRuntimeBase, kuFluxViewSlotPreview);
	const char* szFromLiteral = Flux_ViewPassName("ViewPassNamesContentKey", kuFluxViewSlotPreview);
	ZENITH_ASSERT_TRUE(szFromBuffer == szFromLiteral,
		"a runtime-built base and the identical literal must intern to ONE entry");

	// Scribble over the caller's buffer. If the pool had kept the pointer
	// instead of copying, the interned name would change under the graph.
	memset(acRuntimeBase, 'Z', sizeof(acRuntimeBase) - 1u);
	acRuntimeBase[sizeof(acRuntimeBase) - 1u] = '\0';
	ZENITH_ASSERT_STREQ(szFromBuffer, "ViewPassNamesContentKey (Preview)",
		"the pool copied the base — the interned name survives the caller's buffer");
}

// ----------------------------------------------------------------------------
// 5. Two bases differing in ONE character never share an entry, at any
//    non-main slot. (A prefix-truncating copy is the classic way to break it.)
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, DistinctBasesNeverCollide)
{
	const char* const szA = "ViewPassNamesNearMissA";
	const char* const szB = "ViewPassNamesNearMissB";
	for (u_int u = 1u; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		const char* szNameA = Flux_ViewPassName(szA, u);
		const char* szNameB = Flux_ViewPassName(szB, u);
		ZENITH_ASSERT_TRUE(szNameA != szNameB, "distinct bases get distinct entries at slot %u", u);
		ZENITH_ASSERT_TRUE(strcmp(szNameA, szNameB) != 0, "distinct bases get distinct strings at slot %u", u);
	}
}

// ----------------------------------------------------------------------------
// 6. Capacity is an ASSERT plus a degrade, never a crash and never a silent
//    overwrite. The test owns its OWN small pool: the global pool is what the
//    render graph holds pointers into, so nothing here may touch it.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, CapacityAsserts)
{
	Flux_ViewPassNamePool xSmall(2u);
	ZENITH_ASSERT_EQ(xSmall.GetCapacityBases(), 2u, "the pool honours its requested capacity");

	const char* const szOne   = "CapacityBaseOne";
	const char* const szTwo   = "CapacityBaseTwo";
	const char* const szThree = "CapacityBaseThree";

	// Nothing but the calls under test runs inside a capture scope: a FAILING
	// test assertion is itself a Zenith_DebugBreak sink, so a ZENITH_ASSERT_*
	// in there would fold its own failure into the hit count being measured.
	const char* szNameOne  = nullptr;
	const char* szNameTwo  = nullptr;
	u_int       uHitsFill  = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		szNameOne = xSmall.Name(szOne, kuFluxViewSlotPreview);
		szNameTwo = xSmall.Name(szTwo, kuFluxViewSlotPreview);
		uHitsFill = xCapture.GetHitCount();
	}
	ZENITH_ASSERT_EQ(uHitsFill, 0u, "filling the pool exactly to capacity asserts nothing");
	ZENITH_ASSERT_STREQ(szNameOne, "CapacityBaseOne (Preview)", "first base fits");
	ZENITH_ASSERT_STREQ(szNameTwo, "CapacityBaseTwo (Preview)", "second base fits");
	ZENITH_ASSERT_EQ(xSmall.GetUsedBaseCount(), 2u, "two bases interned");

	const char* szDegraded    = nullptr;
	u_int       uHitsOverflow = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		szDegraded    = xSmall.Name(szThree, kuFluxViewSlotPreview);
		uHitsOverflow = xCapture.GetHitCount();
	}
	ZENITH_ASSERT_EQ(uHitsOverflow, 1u, "one base past capacity asserts EXACTLY once");
	ZENITH_ASSERT_TRUE(szDegraded == szThree, "...and degrades to the caller's own base pointer");
	ZENITH_ASSERT_EQ(xSmall.GetUsedBaseCount(), 2u, "the refused base was not interned");

	// Slot 0 on a full pool is still pure identity — it interns nothing at all.
	const char* szIdentity    = nullptr;
	u_int       uHitsIdentity = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		szIdentity    = xSmall.Name(szThree, kuFluxViewSlotMain);
		uHitsIdentity = xCapture.GetHitCount();
	}
	ZENITH_ASSERT_EQ(uHitsIdentity, 0u, "slot 0 never touches the arena, so it cannot exhaust it");
	ZENITH_ASSERT_TRUE(szIdentity == szThree, "slot 0 is pointer identity even on a full pool");
}

// ----------------------------------------------------------------------------
// 7. The legacy row. "Preview LDR Transition" (Flux_HDR.cpp:759) is the ONE of
//    45 preview names that is not "<base> (Preview)", so it is a table row
//    keyed by content. Slot 6 is NOT a row and must fall through to the
//    generic form; and a base that merely LOOKS similar must not be captured.
// ----------------------------------------------------------------------------
ZENITH_TEST(ViewPassNames, LegacyRow)
{
	ZENITH_ASSERT_STREQ(Flux_ViewPassName("LDR Transition", kuFluxViewSlotPreview), "Preview LDR Transition",
		"the historical prefix spelling is preserved for the preview slot");
	ZENITH_ASSERT_STREQ(Flux_ViewPassName("LDR Transition", 6u), "LDR Transition (AnimPreview)",
		"slot 6 has no legacy row — it composes generically");
	ZENITH_ASSERT_STREQ(Flux_ViewPassName("HDR_ToneMapping", kuFluxViewSlotPreview), "HDR_ToneMapping (Preview)",
		"a genuine <base> (Preview) pair must NOT be treated as legacy");
}

// ----------------------------------------------------------------------------
// 8. Every historical preview name, reproduced EXACTLY.
//
// One row per preview pass name that exists in the tree today: 45 across 28
// AddPass sites. 44 are exactly "<base> (Preview)"; the last is the outlier.
// Every literal is transcribed from the source, with the owning feature and
// line in the trailing comment — the point is to have looked at all 45, so a
// 46th of a NEW shape fails here instead of silently formatting wrong.
//
// The attachment debug names at Flux_Skybox.cpp:261 and Flux_Graphics.cpp:223
// are deliberately excluded: they name RESOURCES, not passes.
// ----------------------------------------------------------------------------
namespace
{
	struct ViewPassNameHistoricalRow
	{
		const char* m_szBase;
		const char* m_szPreviewLiteral;
	};

	const ViewPassNameHistoricalRow s_axHistoricalPreviewNames[] =
	{
		{ "Apply Lighting",             "Apply Lighting (Preview)"             },  // Flux/DeferredShading/Flux_DeferredShading.cpp:317
		{ "Decal Normals Copy",         "Decal Normals Copy (Preview)"         },  // Flux/Decals/Flux_Decals.cpp:583
		{ "Decal Apply",                "Decal Apply (Preview)"                },  // Flux/Decals/Flux_Decals.cpp:589
		{ "Fog_Simple",                 "Fog_Simple (Preview)"                 },  // Flux/Fog/Flux_Fog.cpp:341
		{ "HDR_BloomThreshold",         "HDR_BloomThreshold (Preview)"         },  // Flux/HDR/Flux_HDR.cpp:658
		{ "HDR_BloomDownsample Mip1",   "HDR_BloomDownsample Mip1 (Preview)"   },  // Flux/HDR/Flux_HDR.cpp:669
		{ "HDR_BloomDownsample Mip2",   "HDR_BloomDownsample Mip2 (Preview)"   },  // Flux/HDR/Flux_HDR.cpp:669
		{ "HDR_BloomDownsample Mip3",   "HDR_BloomDownsample Mip3 (Preview)"   },  // Flux/HDR/Flux_HDR.cpp:670
		{ "HDR_BloomDownsample Mip4",   "HDR_BloomDownsample Mip4 (Preview)"   },  // Flux/HDR/Flux_HDR.cpp:670
		{ "HDR_BloomUpsample Mip3",     "HDR_BloomUpsample Mip3 (Preview)"     },  // Flux/HDR/Flux_HDR.cpp:687
		{ "HDR_BloomUpsample Mip2",     "HDR_BloomUpsample Mip2 (Preview)"     },  // Flux/HDR/Flux_HDR.cpp:687
		{ "HDR_BloomUpsample Mip1",     "HDR_BloomUpsample Mip1 (Preview)"     },  // Flux/HDR/Flux_HDR.cpp:688
		{ "HDR_BloomUpsample Mip0",     "HDR_BloomUpsample Mip0 (Preview)"     },  // Flux/HDR/Flux_HDR.cpp:688
		{ "HDR_ToneMapping",            "HDR_ToneMapping (Preview)"            },  // Flux/HDR/Flux_HDR.cpp:750
		{ "LDR Transition",             "Preview LDR Transition"               },  // Flux/HDR/Flux_HDR.cpp:759 — THE OUTLIER (prefix, not suffix)
		{ "HiZ Mip 0",                  "HiZ Mip 0 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:193
		{ "HiZ Mip 1",                  "HiZ Mip 1 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:193
		{ "HiZ Mip 2",                  "HiZ Mip 2 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:193
		{ "HiZ Mip 3",                  "HiZ Mip 3 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:193
		{ "HiZ Mip 4",                  "HiZ Mip 4 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:194
		{ "HiZ Mip 5",                  "HiZ Mip 5 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:194
		{ "HiZ Mip 6",                  "HiZ Mip 6 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:194
		{ "HiZ Mip 7",                  "HiZ Mip 7 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:194
		{ "HiZ Mip 8",                  "HiZ Mip 8 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:195
		{ "HiZ Mip 9",                  "HiZ Mip 9 (Preview)"                  },  // Flux/HiZ/Flux_HiZ.cpp:195
		{ "HiZ Mip 10",                 "HiZ Mip 10 (Preview)"                 },  // Flux/HiZ/Flux_HiZ.cpp:195
		{ "HiZ Mip 11",                 "HiZ Mip 11 (Preview)"                 },  // Flux/HiZ/Flux_HiZ.cpp:195
		{ "Particles",                  "Particles (Preview)"                  },  // Flux/Particles/Flux_Particles.cpp:426
		{ "SDFs",                       "SDFs (Preview)"                       },  // Flux/SDFs/Flux_SDFs.cpp:153
		{ "SSAO Generate",              "SSAO Generate (Preview)"              },  // Flux/SSAO/Flux_SSAO.cpp:387
		{ "SSAO Blur Legacy",           "SSAO Blur Legacy (Preview)"           },  // Flux/SSAO/Flux_SSAO.cpp:395
		{ "SSAO Blur H",                "SSAO Blur H (Preview)"                },  // Flux/SSAO/Flux_SSAO.cpp:404
		{ "SSAO Blur",                  "SSAO Blur (Preview)"                  },  // Flux/SSAO/Flux_SSAO.cpp:414
		{ "SSGI RayMarch",              "SSGI RayMarch (Preview)"              },  // Flux/SSGI/Flux_SSGI.cpp:428
		{ "SSGI Upsample",              "SSGI Upsample (Preview)"              },  // Flux/SSGI/Flux_SSGI.cpp:439
		{ "SSGI Denoise H",             "SSGI Denoise H (Preview)"             },  // Flux/SSGI/Flux_SSGI.cpp:450
		{ "SSGI Denoise V",             "SSGI Denoise V (Preview)"             },  // Flux/SSGI/Flux_SSGI.cpp:459
		{ "SSR RayMarch",               "SSR RayMarch (Preview)"               },  // Flux/SSR/Flux_SSR.cpp:647
		{ "SSR Upsample",               "SSR Upsample (Preview)"               },  // Flux/SSR/Flux_SSR.cpp:663
		{ "SSR DenoiseH",               "SSR DenoiseH (Preview)"               },  // Flux/SSR/Flux_SSR.cpp:682
		{ "SSR DenoiseV",               "SSR DenoiseV (Preview)"               },  // Flux/SSR/Flux_SSR.cpp:698
		{ "Skybox Sky-View LUT",        "Skybox Sky-View LUT (Preview)"        },  // Flux/Skybox/Flux_Skybox.cpp:619
		{ "Skybox",                     "Skybox (Preview)"                     },  // Flux/Skybox/Flux_Skybox.cpp:626
		{ "Translucency",               "Translucency (Preview)"               },  // Flux/Translucency/Flux_Translucency.cpp:235
		{ "Unified Mesh GBuffer",       "Unified Mesh GBuffer (Preview)"       },  // Flux/UnifiedMesh/Flux_UnifiedMesh.cpp:553
	};
}

ZENITH_TEST(ViewPassNames, EveryHistoricalPreviewNameReproduced)
{
	const u_int uNumRows = static_cast<u_int>(sizeof(s_axHistoricalPreviewNames) / sizeof(s_axHistoricalPreviewNames[0]));
	ZENITH_ASSERT_EQ(uNumRows, 45u, "45 preview pass names exist across 28 AddPass sites — a new one needs a row here");
	ZENITH_ASSERT_TRUE(uNumRows <= kuFLUX_VIEW_PASS_NAME_MAX_BASES,
		"the pool must hold every base the engine actually declares");

	for (u_int u = 0; u < uNumRows; u++)
	{
		const ViewPassNameHistoricalRow& xRow = s_axHistoricalPreviewNames[u];
		const char* szProduced = Flux_ViewPassName(xRow.m_szBase, kuFluxViewSlotPreview);
		ZENITH_ASSERT_TRUE(strcmp(szProduced, xRow.m_szPreviewLiteral) == 0,
			"base '%s' must produce '%s', produced '%s'", xRow.m_szBase, xRow.m_szPreviewLiteral, szProduced);
	}
}

// ----------------------------------------------------------------------------
// 9. The bases this pool is FOR are the ones the live graph actually declares.
//    The unit suite runs after Flux LateInitialise -> SetupRenderGraph
//    (Core/Zenith_Engine.cpp:811, then RunAllTests at :916), so the built graph
//    is readable here. This is the only test that reaches outside the pool: a
//    table of bases that no longer matches the engine would make tests 1-8 a
//    self-consistent fiction. Asserts nothing when the graph is absent — a
//    device-free harness must not red on a graph it never built.
// ----------------------------------------------------------------------------
namespace
{
	bool LiveGraphHasPassNamed(const Flux_RenderGraph& xGraph, const char* szName)
	{
		const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = xGraph.GetPasses();
		for (u_int u = 0; u < xPasses.GetSize(); u++)
		{
			const Flux_RenderGraph_Pass* pxPass = xPasses.Get(u);
			if (pxPass != nullptr && pxPass->m_szName != nullptr && strcmp(pxPass->m_szName, szName) == 0)
			{
				return true;
			}
		}
		return false;
	}
}

ZENITH_TEST(ViewPassNames, LiveGraphKnowsTheBases)
{
	if (!g_xEngine.FluxRenderer().IsRenderGraphValid())
	{
		return;
	}

	const Flux_RenderGraph& xGraph = g_xEngine.FluxRenderer().GetRenderGraph();

	// Always-on main-view passes — every one of these is an unconditional
	// AddPass inside its feature's SetupRenderGraph, and each is a base in the
	// table above. If one is renamed, the table above is stale.
	const char* const aszAlwaysOnBases[] =
	{
		"Apply Lighting",        // Flux/DeferredShading/Flux_DeferredShading.cpp:254
		"Unified Mesh GBuffer",  // Flux/UnifiedMesh/Flux_UnifiedMesh.cpp:493
		"Skybox",                // Flux/Skybox/Flux_Skybox.cpp:568
		"HDR_ToneMapping",       // Flux/HDR/Flux_HDR.cpp:730
	};

	for (const char* szBase : aszAlwaysOnBases)
	{
		ZENITH_ASSERT_TRUE(LiveGraphHasPassNamed(xGraph, szBase),
			"the live render graph must contain a pass named '%s'", szBase);
	}
}
