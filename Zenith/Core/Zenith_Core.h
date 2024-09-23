#pragma once

class Zenith_Core
{
public:
	static void SetDt(const float fDt) { s_fDt = fDt; }
	static float GetDt() { return s_fDt; }

	static void AddTimePassed(const float fDt) { s_fTimePassed += fDt; }
	static float GetTimePassed() { return s_fTimePassed; }

	static void Zenith_MainLoop();
	static void UpdateTimers();

	//#TO_TODO: this should be private, currently set by main
	static std::chrono::high_resolution_clock::time_point s_xLastFrameTime;
private:
	static float s_fDt;
	static float s_fTimePassed;
};