#pragma once

#include "ZenithECS/Zenith_Entity.h"

class Zenith_DataStream;

class ZM_WarpTrigger
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;
	static constexpr u_int uINVALID_BUILD_INDEX = 0xFFFFFFFFu;
	static constexpr u_int uTAG_CAPACITY = 32u;

	ZM_WarpTrigger() = delete;
	explicit ZM_WarpTrigger(Zenith_Entity& xParentEntity);

	ZM_WarpTrigger(const ZM_WarpTrigger&) = delete;
	ZM_WarpTrigger& operator=(const ZM_WarpTrigger&) = delete;
	ZM_WarpTrigger(ZM_WarpTrigger&&) noexcept = default;
	ZM_WarpTrigger& operator=(ZM_WarpTrigger&&) noexcept = default;

	void OnStart();
	void OnCollisionEnter(Zenith_Entity& xOther);
	void OnCollisionExit(Zenith_EntityID xOtherEntityID);

	bool Configure(u_int uTargetBuildIndex, const char* szSpawnTag);
	bool TryHandleOverlap(Zenith_Entity& xOther);
	void ResetOverlapLatch();

	u_int GetTargetBuildIndex() const { return m_uTargetBuildIndex; }
	const char* GetSpawnTag() const { return m_szSpawnTag; }
	bool IsLatched() const { return m_bLatched; }

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	void ReassertSensor();
	void ClearConfiguration();

	Zenith_Entity m_xParentEntity;
	Zenith_EntityID m_xOverlappingPlayerEntityID = INVALID_ENTITY_ID;
	u_int m_uTargetBuildIndex = uINVALID_BUILD_INDEX;
	char m_szSpawnTag[uTAG_CAPACITY] = {};
	bool m_bLatched = false;
};
