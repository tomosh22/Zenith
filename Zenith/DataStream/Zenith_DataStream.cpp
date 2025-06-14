#include "Zenith.h"

#include "DataStream/Zenith_DataStream.h"

Zenith_DataStream::Zenith_DataStream(uint64_t ulSize)
	: m_bOwnsData(true)
	, m_ulDataSize(ulSize)
	, m_ulCursor(0)
	, m_pData(new uint8_t[ulSize])
{
}

Zenith_DataStream::Zenith_DataStream(void* pData, uint64_t ulSize)
	: m_bOwnsData(false)
	, m_ulDataSize(ulSize)
	, m_ulCursor(0)
	, m_pData(pData)
{
}

Zenith_DataStream::~Zenith_DataStream()
{
	if (m_bOwnsData)
	{
		delete[] m_pData;
	}
}

void Zenith_DataStream::SetCursor(uint64_t ulCursor)
{
	m_ulCursor = ulCursor;
}