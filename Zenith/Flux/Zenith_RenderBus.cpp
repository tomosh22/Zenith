#include "Zenith.h"

#include "Flux/Zenith_RenderBus.h"

namespace
{
#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_Vector<Zenith_RenderBus::DrawCall> s_xDrawCalls;
	u_int                                     s_uCurrentFrame = 0;
#endif
}

namespace Zenith_RenderBus
{
	void RecordDrawCall(const char* szName,
	                    const Zenith_Maths::Matrix4& xWorldMatrix,
	                    u_int uVertexCount,
	                    u_int uMaterialId)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		DrawCall xEntry;
		xEntry.m_szName       = szName;
		xEntry.m_xWorldMatrix = xWorldMatrix;
		xEntry.m_uVertexCount = uVertexCount;
		xEntry.m_uMaterialId  = uMaterialId;
		xEntry.m_uFrame       = s_uCurrentFrame;
		s_xDrawCalls.PushBack(xEntry);
#else
		(void)szName;
		(void)xWorldMatrix;
		(void)uVertexCount;
		(void)uMaterialId;
#endif
	}

#ifdef ZENITH_INPUT_SIMULATOR
	const Zenith_Vector<DrawCall>& GetSubmittedDrawCallsForTest()
	{
		return s_xDrawCalls;
	}

	void ClearDrawCallsForTest()
	{
		s_xDrawCalls.Clear();
	}

	void AdvanceFrameForTest()
	{
		++s_uCurrentFrame;
	}
#endif
}
