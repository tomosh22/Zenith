#include "Zenith.h"
#include "Memory/Zenith_MemoryAccounting.h"

#if ZENITH_MEMORY_TRACKING_ANY

Zenith_MemorySource     Zenith_MemoryAccounting::s_axSources[uMAX_SOURCES] = {};
Zenith_MemorySourcePoll Zenith_MemoryAccounting::s_apfnPoll[uMAX_SOURCES] = {};
u_int                   Zenith_MemoryAccounting::s_uCount = 0;
bool                    Zenith_MemoryAccounting::s_bInitialised = false;

void Zenith_MemoryAccounting::Initialise()
{
	s_uCount = 0;
	s_bInitialised = true;
}

void Zenith_MemoryAccounting::RegisterSource(const char* szName, Zenith_MemorySourcePoll pfnPoll, u_int64 ulBudgetBytes, bool bIsVRAM)
{
	if (s_uCount >= uMAX_SOURCES)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "Zenith_MemoryAccounting: source table full (%u), dropping '%s'", uMAX_SOURCES, szName ? szName : "?");
		return;
	}
	if (pfnPoll == nullptr)
	{
		return;
	}

	Zenith_MemorySource& xSource = s_axSources[s_uCount];
	xSource.m_szName = szName;
	xSource.m_ulBytes = 0;
	xSource.m_ulAllocCount = 0;
	xSource.m_ulBudgetBytes = ulBudgetBytes;
	xSource.m_bIsVRAM = bIsVRAM;
	s_apfnPoll[s_uCount] = pfnPoll;
	++s_uCount;
}

void Zenith_MemoryAccounting::PollAll()
{
	for (u_int i = 0; i < s_uCount; ++i)
	{
		s_apfnPoll[i](s_axSources[i]);
	}
}

u_int Zenith_MemoryAccounting::GetSourceCount()
{
	return s_uCount;
}

const Zenith_MemorySource& Zenith_MemoryAccounting::GetSource(u_int uIndex)
{
	// Callers iterate [0, GetSourceCount()); clamp defensively.
	if (uIndex >= s_uCount)
	{
		static const Zenith_MemorySource s_xEmpty = {};
		return s_xEmpty;
	}
	return s_axSources[uIndex];
}

u_int64 Zenith_MemoryAccounting::GetTotalProcessRAM()
{
	u_int64 ulTotal = 0;
	for (u_int i = 0; i < s_uCount; ++i)
	{
		if (!s_axSources[i].m_bIsVRAM)
		{
			ulTotal += s_axSources[i].m_ulBytes;
		}
	}
	return ulTotal;
}

u_int64 Zenith_MemoryAccounting::GetTotalVRAM()
{
	u_int64 ulTotal = 0;
	for (u_int i = 0; i < s_uCount; ++i)
	{
		if (s_axSources[i].m_bIsVRAM)
		{
			ulTotal += s_axSources[i].m_ulBytes;
		}
	}
	return ulTotal;
}

#endif // ZENITH_MEMORY_TRACKING_ANY
