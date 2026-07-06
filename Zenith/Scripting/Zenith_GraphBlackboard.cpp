#include "Zenith.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "DataStream/Zenith_DataStream.h"

void Zenith_GraphBlackboard::SetValue(const std::string& strName, const Zenith_PropertyValue& xValue)
{
	m_xValues[strName] = xValue;
}

const Zenith_PropertyValue* Zenith_GraphBlackboard::TryGetValue(const std::string& strName) const
{
	return m_xValues.TryGet(strName);
}

float Zenith_GraphBlackboard::GetFloat(const std::string& strName, float fDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_FLOAT) ? pxValue->GetFloat() : fDefault;
}

bool Zenith_GraphBlackboard::GetBool(const std::string& strName, bool bDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_BOOL) ? pxValue->GetBool() : bDefault;
}

int32_t Zenith_GraphBlackboard::GetInt32(const std::string& strName, int32_t iDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_INT32) ? pxValue->GetInt32() : iDefault;
}

Zenith_Maths::Vector2 Zenith_GraphBlackboard::GetVector2(const std::string& strName, const Zenith_Maths::Vector2& xDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_VECTOR2) ? pxValue->GetVector2() : xDefault;
}

Zenith_Maths::Vector3 Zenith_GraphBlackboard::GetVector3(const std::string& strName, const Zenith_Maths::Vector3& xDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_VECTOR3) ? pxValue->GetVector3() : xDefault;
}

Zenith_Maths::Vector4 Zenith_GraphBlackboard::GetVector4(const std::string& strName, const Zenith_Maths::Vector4& xDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_VECTOR4) ? pxValue->GetVector4() : xDefault;
}

std::string Zenith_GraphBlackboard::GetString(const std::string& strName, const char* szDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_STRING) ? pxValue->GetString() : std::string(szDefault ? szDefault : "");
}

u_int64 Zenith_GraphBlackboard::GetPackedEntityID(const std::string& strName, u_int64 ulDefault) const
{
	const Zenith_PropertyValue* pxValue = TryGetValue(strName);
	return (pxValue && pxValue->GetType() == PROPERTY_TYPE_ENTITY_ID) ? pxValue->GetPackedEntityID() : ulDefault;
}

void Zenith_GraphBlackboard::RemoveValue(const std::string& strName)
{
	m_xValues.Remove(strName);
}

Zenith_Vector<Zenith_PropertyValue>& Zenith_GraphBlackboard::GetOrCreateList(const std::string& strName)
{
	return m_xLists[strName];
}

const Zenith_Vector<Zenith_PropertyValue>* Zenith_GraphBlackboard::TryGetList(const std::string& strName) const
{
	return m_xLists.TryGet(strName);
}

void Zenith_GraphBlackboard::RemoveList(const std::string& strName)
{
	m_xLists.Remove(strName);
}

void Zenith_GraphBlackboard::Clear()
{
	m_xValues.Clear();
	m_xLists.Clear();
}

u_int Zenith_GraphBlackboard::CopyMatchingFrom(const Zenith_GraphBlackboard& xSource)
{
	// Lists are always ad-hoc runtime data (never declared), so name+type
	// matching cannot apply - they carry over verbatim. Dropping them would
	// wipe runtime inventories/query results on every editor save.
	for (Zenith_HashMap<std::string, Zenith_Vector<Zenith_PropertyValue>>::Iterator xIt(xSource.m_xLists); !xIt.Done(); xIt.Next())
	{
		m_xLists[xIt.GetKey()] = xIt.GetValue();
	}

	u_int uDropped = 0;
	for (Zenith_HashMap<std::string, Zenith_PropertyValue>::Iterator xIt(xSource.m_xValues); !xIt.Done(); xIt.Next())
	{
		const std::string& strName = xIt.GetKey();
		const Zenith_PropertyValue& xSourceValue = xIt.GetValue();

		Zenith_PropertyValue* pxDest = m_xValues.TryGet(strName);
		if (!pxDest)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"Zenith_GraphBlackboard: dropping variable '%s' on migration (no longer declared)", strName.c_str());
			++uDropped;
			continue;
		}
		if (pxDest->GetType() != xSourceValue.GetType())
		{
			// Never reinterpret across a type change - keep the new declaration's
			// default and report the drop.
			Zenith_Log(LOG_CATEGORY_CORE,
				"Zenith_GraphBlackboard: dropping variable '%s' on migration (type changed %u -> %u)",
				strName.c_str(), xSourceValue.GetType(), pxDest->GetType());
			++uDropped;
			continue;
		}
		*pxDest = xSourceValue;
	}
	return uDropped;
}

u_int Zenith_GraphBlackboard::ApplyOverridesFrom(const Zenith_GraphBlackboard& xSource)
{
	// Lists restore verbatim (always ad-hoc - see CopyMatchingFrom).
	for (Zenith_HashMap<std::string, Zenith_Vector<Zenith_PropertyValue>>::Iterator xIt(xSource.m_xLists); !xIt.Done(); xIt.Next())
	{
		m_xLists[xIt.GetKey()] = xIt.GetValue();
	}

	u_int uDropped = 0;
	for (Zenith_HashMap<std::string, Zenith_PropertyValue>::Iterator xIt(xSource.m_xValues); !xIt.Done(); xIt.Next())
	{
		const std::string& strName = xIt.GetKey();
		const Zenith_PropertyValue& xSourceValue = xIt.GetValue();

		Zenith_PropertyValue* pxDest = m_xValues.TryGet(strName);
		if (pxDest && pxDest->GetType() != xSourceValue.GetType())
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"Zenith_GraphBlackboard: dropping override '%s' (declared type %u, stored type %u)",
				strName.c_str(), pxDest->GetType(), xSourceValue.GetType());
			++uDropped;
			continue;
		}
		m_xValues[strName] = xSourceValue;	// matching declared OR ad-hoc restore
	}
	return uDropped;
}

void Zenith_GraphBlackboard::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const u_int uCount = m_xValues.GetSize();
	xStream << uCount;
	for (Zenith_HashMap<std::string, Zenith_PropertyValue>::Iterator xIt(m_xValues); !xIt.Done(); xIt.Next())
	{
		xStream << xIt.GetKey();
		xStream << xIt.GetValue();	// tagged form (type byte + payload)
	}

	// List section (Zenith_GraphComponent v2 onward reads it - v1 blobs stop
	// after the values above).
	const u_int uListCount = m_xLists.GetSize();
	xStream << uListCount;
	for (Zenith_HashMap<std::string, Zenith_Vector<Zenith_PropertyValue>>::Iterator xIt(m_xLists); !xIt.Done(); xIt.Next())
	{
		xStream << xIt.GetKey();
		const Zenith_Vector<Zenith_PropertyValue>& axList = xIt.GetValue();
		const u_int uElementCount = axList.GetSize();
		xStream << uElementCount;
		for (u_int u = 0; u < uElementCount; ++u)
		{
			xStream << axList.Get(u);
		}
	}
}

void Zenith_GraphBlackboard::ReadFromDataStream(Zenith_DataStream& xStream, bool bWithLists)
{
	m_xValues.Clear();
	m_xLists.Clear();
	u_int uCount = 0;
	xStream >> uCount;
	for (u_int u = 0; u < uCount; ++u)
	{
		std::string strName;
		xStream >> strName;
		Zenith_PropertyValue xValue;
		xStream >> xValue;
		m_xValues[strName] = xValue;
	}

	if (!bWithLists)
	{
		return;	// pre-list-era stream (container-versioned): values only
	}
	u_int uListCount = 0;
	xStream >> uListCount;
	for (u_int uList = 0; uList < uListCount; ++uList)
	{
		std::string strName;
		xStream >> strName;
		u_int uElementCount = 0;
		xStream >> uElementCount;
		Zenith_Vector<Zenith_PropertyValue>& axList = m_xLists[strName];
		for (u_int u = 0; u < uElementCount; ++u)
		{
			Zenith_PropertyValue xValue;
			xStream >> xValue;
			axList.PushBack(xValue);
		}
	}
}

void Zenith_GraphBlackboard::VisitAll(VisitFn pfnVisit, void* pxUserData) const
{
	if (!pfnVisit)
	{
		return;
	}
	for (Zenith_HashMap<std::string, Zenith_PropertyValue>::Iterator xIt(m_xValues); !xIt.Done(); xIt.Next())
	{
		pfnVisit(pxUserData, xIt.GetKey(), xIt.GetValue());
	}
}
