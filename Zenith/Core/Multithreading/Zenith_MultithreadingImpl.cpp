#include "Zenith.h"

#include "Core/Multithreading/Zenith_MultithreadingImpl.h"

u_int Zenith_MultithreadingImpl::AllocateThreadID(bool bMainThread)
{
	const u_int uID = m_uNextThreadID.fetch_add(1);
	if (bMainThread)
	{
		m_uMainThreadID = uID;
	}
	return uID;
}
