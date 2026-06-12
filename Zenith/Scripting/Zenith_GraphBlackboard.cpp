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

void Zenith_GraphBlackboard::RemoveValue(const std::string& strName)
{
	m_xValues.Remove(strName);
}

void Zenith_GraphBlackboard::Clear()
{
	m_xValues.Clear();
}

u_int Zenith_GraphBlackboard::CopyMatchingFrom(const Zenith_GraphBlackboard& xSource)
{
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
}

void Zenith_GraphBlackboard::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xValues.Clear();
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
