#pragma once

#include <chrono>

// Per-frame timing state. Owned by Zenith_Engine, written by
// Zenith_Core::UpdateTimers each main-loop iteration, read by
// gameplay/render code that needs delta time or accumulated time.
//
// Phase 2 of the engine refactor: this replaces the namespace-scope
// globals Zenith_Core::g_fDt / g_fTimePassed / g_xLastFrameTime that
// used to live in Zenith_Core.cpp. The data shape is unchanged; only
// ownership moves -- the variables now live on the single
// Zenith_Engine instance instead of in module-scope statics.
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
};
