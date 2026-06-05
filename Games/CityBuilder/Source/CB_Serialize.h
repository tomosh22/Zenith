#pragma once

#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_Serialize — tiny Zenith_Vector <-> Zenith_DataStream helpers for the
// free-form city save/load. Element types are POD (trivially copyable), so the
// stream's templated operator<< / operator>> writes/reads each one directly.
// ============================================================================
namespace CB_Serialize
{
	template<typename T>
	void WriteVec(Zenith_DataStream& xStream, const Zenith_Vector<T>& xVec)
	{
		const uint32_t uN = xVec.GetSize();
		xStream << uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			xStream << xVec.Get(i);
		}
	}

	template<typename T>
	void ReadVec(Zenith_DataStream& xStream, Zenith_Vector<T>& xVec)
	{
		uint32_t uN = 0;
		xStream >> uN;
		xVec.Clear();
		for (uint32_t i = 0; i < uN; ++i)
		{
			T xElem;
			xStream >> xElem;
			xVec.PushBack(xElem);
		}
	}
}
