#pragma once

class Zenith_Core
{
public:
	static void Project_Startup();
	static void SetDt(const float fDt) { s_fDt = fDt; }
	static float GetDt() { return s_fDt; }

	static void AddTimePassed(const float fDt) { s_fTimePassed += fDt; }
	static float GetTimePassed() { return s_fTimePassed; }

private:
	static float s_fDt;
	static float s_fTimePassed;
};