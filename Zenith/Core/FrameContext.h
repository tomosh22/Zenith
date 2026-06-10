#pragma once

#include "Core/ZenithConfig.h"

#include <chrono>

// Per-frame timing + frame-index state. Owned by Zenith_Engine, written by
// Zenith_Core each main-loop iteration, read by gameplay/render code that
// needs delta time, accumulated time, or the current frame index.
//
// Phase 2 of the engine refactor: this replaces the namespace-scope
// globals Zenith_Core::g_fDt / g_fTimePassed / g_xLastFrameTime that
// used to live in Zenith_Core.cpp. The data shape is unchanged; only
// ownership moves -- the variables now live on the single
// Zenith_Engine instance instead of in module-scope statics.
//
// m_uFrameIndex is THE engine frame index — the single source of truth for
// frame counting engine-wide (it subsumed Flux_RendererImpl::m_uFrameCounter).
// Single-writer rule: only Zenith_Core::Zenith_MainLoop advances it, as the
// final act of the loop, AFTER Swapchain::EndFrame — so present uses the ring
// slot for frame N before the ring index moves to N+1. Skipped frames
// (swapchain out-of-date) deliberately do NOT advance it: a rapid-resize
// sequence of consecutive skips would otherwise wrap the ring past valid
// fences and shorten the deferred-VRAM-deletion grace period.
class FrameContext
{
public:
	FrameContext() = default;
	~FrameContext() = default;

	FrameContext(const FrameContext&) = delete;
	FrameContext& operator=(const FrameContext&) = delete;

	void SetDt(float fDt)              { m_fDt = fDt; }
	float GetDt() const                { return m_fDt; }

	void AddTimePassed(float fDt)      { m_fTimePassed += fDt; }
	float GetTimePassed() const        { return m_fTimePassed; }

	// Monotonic frame index since engine initialise. Advanced once per
	// presented (or headless) frame by Zenith_MainLoop — nowhere else.
	u_int GetFrameIndex() const        { return m_uFrameIndex; }
	void AdvanceFrameIndex()           { m_uFrameIndex++; }

	// Current frames-in-flight ring slot in [0, MAX_FRAMES_IN_FLIGHT).
	u_int GetRingIndex() const         { return m_uFrameIndex % MAX_FRAMES_IN_FLIGHT; }

	void SetLastFrameTime(std::chrono::high_resolution_clock::time_point xTime)
	{
		m_xLastFrameTime = xTime;
	}
	std::chrono::high_resolution_clock::time_point GetLastFrameTime() const
	{
		return m_xLastFrameTime;
	}

private:
	float m_fDt = 0.f;
	float m_fTimePassed = 0.f;
	std::chrono::high_resolution_clock::time_point m_xLastFrameTime;

	// THE engine frame index. See the class comment for the single-writer
	// rule and skipped-frame semantics.
	u_int m_uFrameIndex = 0;

	// Unit tests snapshot/zero/restore the frame index around the
	// Flux_PerFrame ring-scheduler tests.
	friend class Zenith_UnitTests;
};
