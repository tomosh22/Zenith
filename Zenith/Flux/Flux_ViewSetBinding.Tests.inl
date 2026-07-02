#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_ViewSetBinding.h"
#include "Flux/Flux_PersistentSetLayouts.h"   // kaszViewMemberNames / kuViewBindingCount (anti-drift test)
#include <cstring>

// ============================================================================
// Phase 5.5 — VIEW/GLOBAL graph-Read() validator unit tests.
//
// Cover the pure decision (Flux_ValidateViewSetReads): a GRAPH_RESOURCE member
// that is enabled, produced this frame, and sampled MUST have a declared Read();
// every other source class, every disabled feature, and any handle not produced
// this frame are exempt. Plus the registry classification (CPU global buffers
// exempt; unknown member → not found). No device/reflection/render-graph — the
// backend glue layers handle/read resolution on top of this decision.
// ============================================================================

namespace
{
	Flux_ViewSetSampledMember VSMember(const char* szName, Flux_ViewSetSourceClass eSrc,
		bool bEnabled, bool bHandleValid, bool bReadDeclared)
	{
		Flux_ViewSetSampledMember xM;
		xM.m_szMemberName      = szName;
		xM.m_eSource           = eSrc;
		xM.m_bEnabled          = bEnabled;
		xM.m_bGraphHandleValid = bHandleValid;
		xM.m_bReadDeclared     = bReadDeclared;
		return xM;
	}
}

ZENITH_TEST(ViewSetBinding, GraphResourceWithDeclaredReadPasses)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xCSM", FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, true, /*read*/true),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "graph resource with a declared Read() must pass");
}

ZENITH_TEST(ViewSetBinding, GraphResourceMissingReadIsViolation)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xCSM", FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, true, /*read*/false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(!xChk.m_bAllDeclared, "sampled graph resource without a Read() must be flagged");
	ZENITH_ASSERT_TRUE(xChk.m_szMissingMember && strcmp(xChk.m_szMissingMember, "g_xCSM") == 0,
		"violation must name the offending member");
}

ZENITH_TEST(ViewSetBinding, DisabledFeatureIsExempt)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xSSR", FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, /*enabled*/false, true, /*read*/false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "a disabled feature binds a placeholder — no Read() demanded");
}

ZENITH_TEST(ViewSetBinding, InvalidHandleIsExempt)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xSSGI", FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, /*handleValid*/false, /*read*/false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "a handle not produced this frame is exempt (nothing to barrier)");
}

ZENITH_TEST(ViewSetBinding, ExternalPersistentIsExempt)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xBRDFLUT", FLUX_VIEWSET_SOURCE_EXTERNAL_PERSISTENT, true, true, /*read*/false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "disk/long-lived external resources are not graph-tracked → exempt");
}

ZENITH_TEST(ViewSetBinding, CpuGlobalBufferIsExempt)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xView",       FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, true, true, false),
		VSMember("g_xGlobal",     FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, true, true, false),
		VSMember("g_axMaterials", FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER, true, true, false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 3u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "frame-indexed CPU buffers are graph-invisible → never demand a Read()");
}

ZENITH_TEST(ViewSetBinding, FallbackDisabledIsExempt)
{
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xHiZ", FLUX_VIEWSET_SOURCE_FALLBACK_DISABLED, true, true, false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 1u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "an explicitly-disabled-fallback member is exempt");
}

ZENITH_TEST(ViewSetBinding, NoMembersSampledPasses)
{
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(nullptr, 0u);
	ZENITH_ASSERT_TRUE(xChk.m_bAllDeclared, "a pass that samples no VIEW/GLOBAL member trivially passes");
}

ZENITH_TEST(ViewSetBinding, FirstViolationReported)
{
	// Mix: one passing graph resource, then two violators — the first violator wins.
	const Flux_ViewSetSampledMember axM[] = {
		VSMember("g_xCSM",   FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, true, /*read*/true),
		VSMember("g_xDepth", FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, true, /*read*/false),
		VSMember("g_xSSR",   FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE, true, true, /*read*/false),
	};
	Flux_ViewSetReadCheck xChk = Flux_ValidateViewSetReads(axM, 3u);
	ZENITH_ASSERT_TRUE(!xChk.m_bAllDeclared, "an undeclared graph resource must fail the check");
	ZENITH_ASSERT_TRUE(xChk.m_szMissingMember && strcmp(xChk.m_szMissingMember, "g_xDepth") == 0,
		"the FIRST violating member must be reported");
}

ZENITH_TEST(ViewSetBinding, RegistryClassifiesSpineMembers)
{
	const Flux_ViewSetBinding* pxGlobal = Flux_FindViewSetBinding("g_xGlobal");
	const Flux_ViewSetBinding* pxMats   = Flux_FindViewSetBinding("g_axMaterials");
	const Flux_ViewSetBinding* pxView   = Flux_FindViewSetBinding("g_xView");
	ZENITH_ASSERT_TRUE(pxGlobal && pxGlobal->m_uSet == 0u && pxGlobal->m_eSource == FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER,
		"g_xGlobal must be a GLOBAL (set 0) CPU buffer");
	ZENITH_ASSERT_TRUE(pxMats && pxMats->m_uSet == 0u && pxMats->m_eSource == FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER,
		"g_axMaterials must be a GLOBAL (set 0) CPU buffer");
	ZENITH_ASSERT_TRUE(pxView && pxView->m_uSet == 1u && pxView->m_eSource == FLUX_VIEWSET_SOURCE_CPU_GLOBAL_BUFFER,
		"g_xView must be a VIEW (set 1) CPU buffer");

	// Phase 5.4: g_xCSM is the first GRAPH_RESOURCE promoted into VIEW. The draw-time
	// validator's enforcement for it hinges on this row existing + wired correctly
	// (set 1, GRAPH_RESOURCE, a non-null handle accessor, always-enabled). Assert the
	// function pointer is non-null without invoking it (the accessor dereferences
	// g_xEngine and is not device-free).
	const Flux_ViewSetBinding* pxCSM = Flux_FindViewSetBinding("g_xCSM");
	ZENITH_ASSERT_TRUE(pxCSM != nullptr, "g_xCSM must be registered as a VIEW-set member");
	ZENITH_ASSERT_TRUE(pxCSM && pxCSM->m_uSet == 1u, "g_xCSM must live in the VIEW set (1)");
	ZENITH_ASSERT_TRUE(pxCSM && pxCSM->m_eSource == FLUX_VIEWSET_SOURCE_GRAPH_RESOURCE,
		"g_xCSM must be classified GRAPH_RESOURCE (graph-tracked → requires a declared Read())");
	ZENITH_ASSERT_TRUE(pxCSM && pxCSM->m_pfnGetVRAMHandle != nullptr,
		"a GRAPH_RESOURCE row must carry a non-null handle accessor (else enforcement is silently disabled)");
	ZENITH_ASSERT_TRUE(pxCSM && pxCSM->m_pfnIsEnabled == nullptr,
		"g_xCSM is always enabled (the CSM array is always allocated; cleared-to-far when shadows are off)");
}

ZENITH_TEST(ViewSetBinding, RegistryUnknownMemberNotFound)
{
	ZENITH_ASSERT_TRUE(Flux_FindViewSetBinding("g_xNotARealMember") == nullptr,
		"an unregistered member resolves to nullptr (caller treats it as exempt)");
	ZENITH_ASSERT_TRUE(Flux_FindViewSetBinding(nullptr) == nullptr,
		"a null name resolves to nullptr");
}

ZENITH_TEST(ViewSetBinding, EveryViewMemberHasRegistryRow)
{
	// Anti-drift (W4): the canonical VIEW member list (Flux_PersistentSetLayouts, mirrored
	// from Bindings.slang) and the registry must not drift. The boot assert already ties the
	// manifest to reflection; this ties it to the registry, so a VIEW member promoted without
	// a registry row — which silently drops its graph-Read() enforcement — fails at boot.
	for (u_int u = 0; u < Flux_PersistentSetLayouts::kuViewBindingCount; u++)
	{
		const char* szName = Flux_PersistentSetLayouts::kaszViewMemberNames[u];
		const Flux_ViewSetBinding* pxRow = Flux_FindViewSetBinding(szName);
		ZENITH_ASSERT_TRUE(pxRow != nullptr,
			"every canonical VIEW member must have a Flux_ViewSetBinding row (else its graph-Read() enforcement is silently dropped)");
		ZENITH_ASSERT_TRUE(pxRow && pxRow->m_uSet == Flux_PersistentSetLayouts::kuSetView,
			"a VIEW member's registry row must record set 1 (VIEW)");
	}
}

ZENITH_TEST(ViewSetBinding, PerCameraMemberRejectedFromSharedViewSet)
{
	// W2 guard: a per-camera resource in the SHARED VIEW set would alias the main camera
	// for secondary views while multi-view (Phase 5.6) is off — must be rejected.
	const char* szOff = nullptr;
	Flux_ViewSetBinding axMixed[2];
	axMixed[0].m_szMemberName = "g_xView";  axMixed[0].m_uSet = Flux_PersistentSetLayouts::kuSetView; axMixed[0].m_bPerCamera = false;
	axMixed[1].m_szMemberName = "g_xDepth"; axMixed[1].m_uSet = Flux_PersistentSetLayouts::kuSetView; axMixed[1].m_bPerCamera = true;
	ZENITH_ASSERT_TRUE(!Flux_ViewSetRegistryRespectsViewSharing(axMixed, 2u, /*multiView*/false, &szOff),
		"a per-camera VIEW member must be rejected while multi-view is unsupported");
	ZENITH_ASSERT_TRUE(szOff && strcmp(szOff, "g_xDepth") == 0, "the offending per-camera member must be named");

	// Once multi-view exists, per-camera VIEW members are fine.
	ZENITH_ASSERT_TRUE(Flux_ViewSetRegistryRespectsViewSharing(axMixed, 2u, /*multiView*/true, nullptr),
		"per-camera VIEW members are fine once multi-view (Phase 5.6) exists");

	// The per-camera flag only matters for the shared VIEW set (set 1), not GLOBAL (set 0).
	Flux_ViewSetBinding axGlobal[1];
	axGlobal[0].m_szMemberName = "g_xGlobalPerCam"; axGlobal[0].m_uSet = Flux_PersistentSetLayouts::kuSetGlobal; axGlobal[0].m_bPerCamera = true;
	ZENITH_ASSERT_TRUE(Flux_ViewSetRegistryRespectsViewSharing(axGlobal, 1u, false, nullptr),
		"the per-camera guard only governs the shared VIEW set");

	// And the LIVE registry must itself be clean under the current multi-view setting.
	const Zenith_Vector<Flux_ViewSetBinding>& xReg = Flux_GetViewSetBindingRegistry();
	ZENITH_ASSERT_TRUE(Flux_ViewSetRegistryRespectsViewSharing(xReg.GetDataPointer(), xReg.GetSize(), kbFluxPerViewSharedResourcesSupported, nullptr),
		"the live VIEW-set registry must respect view-sharing under the current multi-view setting");
}
