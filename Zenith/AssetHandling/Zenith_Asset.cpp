#include "Zenith.h"
#include "AssetHandling/Zenith_Asset.h"

bool Zenith_Asset::IsProcedural() const
{
	// Procedural assets have paths starting with "procedural://"
	return m_strPath.size() > 13 && m_strPath.substr(0, 13) == "procedural://";
}

uint32_t Zenith_Asset::AddRef()
{
	return m_uRefCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

uint32_t Zenith_Asset::Release()
{
	uint32_t uPrev = m_uRefCount.fetch_sub(1, std::memory_order_acq_rel);
	ZENITH_ASSERT(uPrev > 0, "Release called on asset with 0 ref count");
	return uPrev - 1;
}

uint32_t Zenith_Asset::GetRefCount() const
{
	return m_uRefCount.load(std::memory_order_relaxed);
}
