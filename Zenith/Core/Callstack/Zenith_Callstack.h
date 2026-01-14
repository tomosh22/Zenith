#pragma once

static constexpr u_int uCALLSTACK_MAX_FRAMES = 64;
static constexpr u_int uCALLSTACK_SYMBOL_MAX_LENGTH = 512;

struct Zenith_CallstackFrame
{
	void* m_pAddress;
	char m_acSymbol[uCALLSTACK_SYMBOL_MAX_LENGTH];
	char m_acFile[uCALLSTACK_SYMBOL_MAX_LENGTH];
	u_int m_uLine;
};

class Zenith_Callstack
{
public:
	static void Initialise();
	static void Shutdown();

	// Capture current callstack (returns number of frames captured)
	// uSkipFrames: How many frames to skip (typically 2-3 for internal calls)
	static u_int Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames = 0);

	// Symbolicate a single address
	static bool Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut);

	// Symbolicate multiple frames (batch for efficiency)
	static void SymbolicateBatch(void** apAddresses, Zenith_CallstackFrame* axFramesOut, u_int uCount);

	// Format callstack to string for logging
	static void FormatCallstack(void** apFrames, u_int uFrameCount, char* szBuffer, size_t ulBufferSize);

private:
	static void Platform_Initialise();
	static void Platform_Shutdown();
	static u_int Platform_Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames);
	static bool Platform_Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut);
};
