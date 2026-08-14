#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_WINDOWS)

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Backend/Flux_IndirectDraw.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Input/Zenith_InputSimulator.h"
#include "Profiling/Zenith_Profiling.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"

#ifdef ZENITH_VULKAN
#include "Vulkan/Zenith_Vulkan.h"
#endif

namespace
{
	constexpr const char* szTIPResetPass = "Terrain Reset Count and Indirect Arguments";
	constexpr const char* szTIPCullingPass = "Terrain Culling Compute";
	constexpr const char* szTIPGBufferPass = "Terrain GBuffer";
	constexpr const char* szTIPCPUZone = "Flux Record Pass";
	constexpr float fTIPFixedDt = 1.0f / 60.0f;
	constexpr int iTIPMaxFrames = 14000;

	FILE* g_pTIPOutput = nullptr;
	bool g_bTIPFailed = false;
	int g_iTIPWarmup = 120;
	int g_iTIPSamplesRequested = 300;
	int g_iTIPSamplesWritten = 0;
	u_int64 g_uTIPLastGPUCapture = 0;

	struct TIPTelemetry
	{
		uint32_t m_uNative = 0;
		uint32_t m_uPaddedMulti = 0;
		uint32_t m_uPaddedSingle = 0;
		uint32_t m_uFailClosed = 0;
		uint32_t m_uFixedIndirect = 0;
	};
	TIPTelemetry g_xTIPPreviousTelemetry;
	Flux_IndirectExecutionMode g_eTIPExpectedMode = Flux_IndirectExecutionMode::FAILED_CLOSED;

	const char* TIPExecutionModeName(Flux_IndirectExecutionMode eMode)
	{
		switch (eMode)
		{
		case Flux_IndirectExecutionMode::NATIVE_COUNT: return "NATIVE_COUNT";
		case Flux_IndirectExecutionMode::PADDED_MULTI: return "PADDED_MULTI";
		case Flux_IndirectExecutionMode::PADDED_SINGLE: return "PADDED_SINGLE";
		default: return "FAILED_CLOSED";
		}
	}

	const char* TIPArgValue(const char* szPrefix)
	{
		const size_t uPrefixLength = std::strlen(szPrefix);
		for (int i = 1; i < __argc; ++i)
		{
			if (std::strncmp(__argv[i], szPrefix, uPrefixLength) == 0)
				return __argv[i] + uPrefixLength;
		}
		return nullptr;
	}

	bool TIPHasArg(const char* szExact)
	{
		for (int i = 1; i < __argc; ++i)
			if (std::strcmp(__argv[i], szExact) == 0) return true;
		return false;
	}

	bool TIPParsePositiveInt(const char* szPrefix, int& iValueOut)
	{
		const char* szValue = TIPArgValue(szPrefix);
		if (szValue == nullptr || *szValue == '\0') return false;
		char* szEnd = nullptr;
		const long iValue = std::strtol(szValue, &szEnd, 10);
		if (szEnd == szValue || *szEnd != '\0' || iValue <= 0 || iValue > 6000)
			return false;
		iValueOut = static_cast<int>(iValue);
		return true;
	}

	void TIPFail(const char* szReason)
	{
		if (!g_bTIPFailed)
			Zenith_Error(LOG_CATEGORY_RENDERER, "[TerrainIndirectPerformance] %s", szReason);
		g_bTIPFailed = true;
	}

	void TIPSetCamera(const char* szCamera)
	{
		RenderTest_GameplayState::s_bPhotoModeActive = true;
		RenderTest_GameplayState::s_fPhotoOffsetX = 0.0f;
		RenderTest_GameplayState::s_fPhotoOffsetY = 2.5f;
		RenderTest_GameplayState::s_fPhotoOffsetZ = 0.0f;
		RenderTest_GameplayState::s_fPhotoYaw = 2.3562f;
		if (std::strcmp(szCamera, "dense") == 0)
			RenderTest_GameplayState::s_fPhotoPitch = -0.20f;
		else if (std::strcmp(szCamera, "horizon") == 0)
			RenderTest_GameplayState::s_fPhotoPitch = 0.45f;
		else if (std::strcmp(szCamera, "culled") == 0)
			// Same near-vertical cull-all pose whose visible-count readback is
			// asserted to be zero by TerrainIndirectCompatibility.
			RenderTest_GameplayState::s_fPhotoPitch = 1.50f;
		else
			TIPFail("--terrain-indirect-perf-camera must be dense, horizon, or culled");
	}

	TIPTelemetry TIPReadTelemetry()
	{
		TIPTelemetry xResult;
#ifdef ZENITH_VULKAN
		auto& xVulkan = static_cast<Zenith_Vulkan&>(g_xEngine.FluxBackend());
		auto& xTelemetry = xVulkan.GetIndirectDrawTelemetry();
		xResult.m_uNative = xTelemetry.m_uNativeCount.load(std::memory_order_relaxed);
		xResult.m_uPaddedMulti = xTelemetry.m_uPaddedMulti.load(std::memory_order_relaxed);
		xResult.m_uPaddedSingle = xTelemetry.m_uPaddedSingle.load(std::memory_order_relaxed);
		xResult.m_uFailClosed = xTelemetry.m_uFailClosed.load(std::memory_order_relaxed);
		xResult.m_uFixedIndirect = xTelemetry.m_uFixedIndirect.load(std::memory_order_relaxed);
#endif
		return xResult;
	}

	double TIPFindGPUPass(const char* szName, bool& bFoundOut)
	{
		double fTotal = 0.0;
		bFoundOut = false;
		const auto& xPasses = g_xEngine.Profiling().GetGPUPasses();
		for (u_int i = 0; i < xPasses.GetSize(); ++i)
		{
			const Zenith_Profiling::GPUPass& xPass = xPasses.Get(i);
			if (xPass.m_szName != nullptr && std::strcmp(xPass.m_szName, szName) == 0)
			{
				fTotal += xPass.m_fMilliseconds;
				bFoundOut = true;
			}
		}
		return fTotal;
	}

	void Setup_TerrainIndirectPerformance()
	{
		g_bTIPFailed = false;
		g_iTIPSamplesWritten = 0;
		g_pTIPOutput = nullptr;

#ifndef ZENITH_VULKAN
		Zenith_AutomatedTestRunner::RequestSkip(
			"TerrainIndirectPerformance currently requires Vulkan GPU timestamp readback");
		return;
#endif

		const char* szCamera = TIPArgValue("--terrain-indirect-perf-camera=");
		const char* szOutput = TIPArgValue("--terrain-indirect-perf-output=");
		if (szCamera == nullptr || szOutput == nullptr || *szOutput == '\0')
		{
			TIPFail("missing required terrain performance camera/output arguments");
			return;
		}
		if (!TIPParsePositiveInt("--terrain-indirect-perf-warmup=", g_iTIPWarmup) ||
			!TIPParsePositiveInt("--terrain-indirect-perf-samples=", g_iTIPSamplesRequested))
		{
			TIPFail("warmup/samples must be integer values in [1, 6000]");
			return;
		}
		const bool bDeveloperSanity = TIPHasArg("--terrain-indirect-perf-developer-sanity");
		if (!bDeveloperSanity && (g_iTIPWarmup < 120 || g_iTIPSamplesRequested < 300))
		{
			TIPFail("performance contract requires warmup >= 120 and samples >= 300");
			return;
		}
		if (!Zenith_InputSimulator::HasFixedDtOverride() ||
			std::fabs(Zenith_InputSimulator::GetFixedDt() - fTIPFixedDt) > 1.0e-6f)
		{
			TIPFail("the automated harness must run with --fixed-dt 0.016666667");
			return;
		}

		TIPSetCamera(szCamera);
		if (g_bTIPFailed) return;

#ifdef ZENITH_VULKAN
		auto& xVulkan = static_cast<Zenith_Vulkan&>(g_xEngine.FluxBackend());
		g_eTIPExpectedMode = Flux_SelectIndirectExecutionMode(
			xVulkan.GetIndirectDrawCapabilities(),
			Flux_TerrainConfig::TOTAL_CHUNKS,
			Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX,
			xVulkan.GetIndirectDrawOverride());
		if (g_eTIPExpectedMode == Flux_IndirectExecutionMode::FAILED_CLOSED)
		{
			TIPFail("the terrain indirect request resolved to FAILED_CLOSED");
			return;
		}
#endif

		if (fopen_s(&g_pTIPOutput, szOutput, "wb") != 0 || g_pTIPOutput == nullptr)
		{
			TIPFail("could not open the requested raw CSV output path");
			return;
		}
		std::fprintf(g_pTIPOutput,
			"sample_index,gpu_capture_serial,reset_gpu_ms,culling_gpu_ms,gbuffer_gpu_ms,total_gpu_ms,"
			"reset_cpu_record_ms,culling_cpu_record_ms,gbuffer_cpu_record_ms,"
			"indirect_native_delta,indirect_padded_multi_delta,indirect_padded_single_delta,"
			"indirect_fail_closed_delta,indirect_fixed_delta\n");
		std::fflush(g_pTIPOutput);

		g_uTIPLastGPUCapture = g_xEngine.Profiling().GetGPUCaptureSerial();
		g_xTIPPreviousTelemetry = TIPReadTelemetry();
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectPerformance] camera=%s expected=%s warmup=%d samples=%d fixedDt=%.9f output=%s",
			szCamera, TIPExecutionModeName(g_eTIPExpectedMode),
			g_iTIPWarmup, g_iTIPSamplesRequested,
			Zenith_InputSimulator::GetFixedDt(), szOutput);
	}

	bool Step_TerrainIndirectPerformance(int iFrame)
	{
		if (g_bTIPFailed) return false;
		if (iFrame < g_iTIPWarmup)
		{
			// Keep both baselines moving throughout warmup. The first emitted row is
			// therefore one steady-state frame, not a delta accumulated over all warmup.
			g_uTIPLastGPUCapture = g_xEngine.Profiling().GetGPUCaptureSerial();
			g_xTIPPreviousTelemetry = TIPReadTelemetry();
			return true;
		}

		Zenith_Profiling& xProfiler = g_xEngine.Profiling();
		const u_int64 uCapture = xProfiler.GetGPUCaptureSerial();
		if (uCapture == 0 || uCapture == g_uTIPLastGPUCapture)
		{
			TIPFail("GPU timestamp readback did not publish a fresh capture for a sample frame");
			return false;
		}
		g_uTIPLastGPUCapture = uCapture;

		bool bResetGPU = false, bCullingGPU = false, bGBufferGPU = false;
		const double fResetGPU = TIPFindGPUPass(szTIPResetPass, bResetGPU);
		const double fCullingGPU = TIPFindGPUPass(szTIPCullingPass, bCullingGPU);
		const double fGBufferGPU = TIPFindGPUPass(szTIPGBufferPass, bGBufferGPU);
		const double fTotalGPU = xProfiler.GetGPUTotalMs();
		double fResetCPU = 0.0, fCullingCPU = 0.0, fGBufferCPU = 0.0;
		const bool bResetCPU = xProfiler.GetDisplayLabeledZoneTotalMs(szTIPCPUZone, szTIPResetPass, fResetCPU);
		const bool bCullingCPU = xProfiler.GetDisplayLabeledZoneTotalMs(szTIPCPUZone, szTIPCullingPass, fCullingCPU);
		const bool bGBufferCPU = xProfiler.GetDisplayLabeledZoneTotalMs(szTIPCPUZone, szTIPGBufferPass, fGBufferCPU);
		if (!bResetGPU || !bCullingGPU || !bGBufferGPU || !bResetCPU || !bCullingCPU || !bGBufferCPU)
		{
			TIPFail("a required terrain GPU pass or CPU record label was absent from a sampled frame");
			return false;
		}
		if (!std::isfinite(fResetGPU) || !std::isfinite(fCullingGPU) || !std::isfinite(fGBufferGPU) ||
			!std::isfinite(fTotalGPU) || !std::isfinite(fResetCPU) || !std::isfinite(fCullingCPU) ||
			!std::isfinite(fGBufferCPU) || fResetGPU < 0.0 || fCullingGPU < 0.0 ||
			fGBufferGPU < 0.0 || fTotalGPU <= 0.0 || fResetCPU < 0.0 ||
			fCullingCPU < 0.0 || fGBufferCPU < 0.0)
		{
			TIPFail("a sampled terrain timing was non-finite, negative, or had an empty total GPU value");
			return false;
		}

		const TIPTelemetry xNow = TIPReadTelemetry();
		const TIPTelemetry xDelta = {
			xNow.m_uNative - g_xTIPPreviousTelemetry.m_uNative,
			xNow.m_uPaddedMulti - g_xTIPPreviousTelemetry.m_uPaddedMulti,
			xNow.m_uPaddedSingle - g_xTIPPreviousTelemetry.m_uPaddedSingle,
			xNow.m_uFailClosed - g_xTIPPreviousTelemetry.m_uFailClosed,
			xNow.m_uFixedIndirect - g_xTIPPreviousTelemetry.m_uFixedIndirect
		};
		g_xTIPPreviousTelemetry = xNow;
		if (xDelta.m_uFailClosed != 0)
		{
			TIPFail("indirect recorder entered FAILED_CLOSED during the sample window");
			return false;
		}
		const bool bExpectedTierOnly =
			(g_eTIPExpectedMode == Flux_IndirectExecutionMode::NATIVE_COUNT &&
				xDelta.m_uNative > 0 && xDelta.m_uPaddedMulti == 0 && xDelta.m_uPaddedSingle == 0) ||
			(g_eTIPExpectedMode == Flux_IndirectExecutionMode::PADDED_MULTI &&
				xDelta.m_uNative == 0 && xDelta.m_uPaddedMulti > 0 && xDelta.m_uPaddedSingle == 0) ||
			(g_eTIPExpectedMode == Flux_IndirectExecutionMode::PADDED_SINGLE &&
				xDelta.m_uNative == 0 && xDelta.m_uPaddedMulti == 0 && xDelta.m_uPaddedSingle > 0);
		if (!bExpectedTierOnly)
		{
			TIPFail("sample telemetry did not contain exactly the capability-selected indirect tier");
			return false;
		}

		std::fprintf(g_pTIPOutput,
			"%d,%llu,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u,%u,%u,%u\n",
			g_iTIPSamplesWritten,
			static_cast<unsigned long long>(uCapture),
			fResetGPU, fCullingGPU, fGBufferGPU, fTotalGPU,
			fResetCPU, fCullingCPU, fGBufferCPU,
			xDelta.m_uNative, xDelta.m_uPaddedMulti, xDelta.m_uPaddedSingle,
			xDelta.m_uFailClosed, xDelta.m_uFixedIndirect);
		std::fflush(g_pTIPOutput);
		++g_iTIPSamplesWritten;
		return g_iTIPSamplesWritten < g_iTIPSamplesRequested;
	}

	bool Verify_TerrainIndirectPerformance()
	{
		if (g_pTIPOutput != nullptr)
		{
			std::fclose(g_pTIPOutput);
			g_pTIPOutput = nullptr;
		}
		if (g_bTIPFailed) return false;
		if (g_iTIPSamplesWritten != g_iTIPSamplesRequested)
		{
			Zenith_Error(LOG_CATEGORY_RENDERER,
				"[TerrainIndirectPerformance] collected %d samples, expected %d",
				g_iTIPSamplesWritten, g_iTIPSamplesRequested);
			return false;
		}
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[TerrainIndirectPerformance] PASS collected=%d", g_iTIPSamplesWritten);
		return true;
	}

	void Teardown_TerrainIndirectPerformance()
	{
		if (g_pTIPOutput != nullptr)
		{
			std::fclose(g_pTIPOutput);
			g_pTIPOutput = nullptr;
		}
		RenderTest_GameplayState::s_bPhotoModeActive = false;
	}

	const Zenith_AutomatedTest g_xTerrainIndirectPerformance = {
		"TerrainIndirectPerformance",
		&Setup_TerrainIndirectPerformance,
		&Step_TerrainIndirectPerformance,
		&Verify_TerrainIndirectPerformance,
		iTIPMaxFrames,
		true,  // requires a real graphics backend
		true,  // manual-only: long-running measurement, selected explicitly by the runner
		&Teardown_TerrainIndirectPerformance
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainIndirectPerformance);
}

#endif // ZENITH_INPUT_SIMULATOR && ZENITH_WINDOWS
