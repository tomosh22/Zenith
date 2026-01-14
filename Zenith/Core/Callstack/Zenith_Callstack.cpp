#include "Zenith.h"

#include "Callstack/Zenith_Callstack.h"

#include <cstdio>

void Zenith_Callstack::Initialise()
{
	Platform_Initialise();
}

void Zenith_Callstack::Shutdown()
{
	Platform_Shutdown();
}

u_int Zenith_Callstack::Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames /*= 0*/)
{
	return Platform_Capture(apFrames, uMaxFrames, uSkipFrames);
}

bool Zenith_Callstack::Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut)
{
	return Platform_Symbolicate(pAddress, xFrameOut);
}

void Zenith_Callstack::SymbolicateBatch(void** apAddresses, Zenith_CallstackFrame* axFramesOut, u_int uCount)
{
	for (u_int i = 0; i < uCount; ++i)
	{
		Platform_Symbolicate(apAddresses[i], axFramesOut[i]);
	}
}

void Zenith_Callstack::FormatCallstack(void** apFrames, u_int uFrameCount, char* szBuffer, size_t ulBufferSize)
{
	if (ulBufferSize == 0 || szBuffer == nullptr)
	{
		return;
	}

	szBuffer[0] = '\0';
	size_t ulOffset = 0;

	for (u_int i = 0; i < uFrameCount && ulOffset < ulBufferSize - 1; ++i)
	{
		Zenith_CallstackFrame xFrame;
		if (Symbolicate(apFrames[i], xFrame))
		{
			int iWritten;
			if (xFrame.m_uLine > 0 && xFrame.m_acFile[0] != '\0')
			{
				iWritten = snprintf(szBuffer + ulOffset, ulBufferSize - ulOffset,
					"  [%u] %s (%s:%u)\n", i, xFrame.m_acSymbol, xFrame.m_acFile, xFrame.m_uLine);
			}
			else
			{
				iWritten = snprintf(szBuffer + ulOffset, ulBufferSize - ulOffset,
					"  [%u] %s (0x%p)\n", i, xFrame.m_acSymbol, apFrames[i]);
			}

			if (iWritten > 0)
			{
				ulOffset += static_cast<size_t>(iWritten);
			}
		}
		else
		{
			int iWritten = snprintf(szBuffer + ulOffset, ulBufferSize - ulOffset,
				"  [%u] <unknown> (0x%p)\n", i, apFrames[i]);
			if (iWritten > 0)
			{
				ulOffset += static_cast<size_t>(iWritten);
			}
		}
	}
}
