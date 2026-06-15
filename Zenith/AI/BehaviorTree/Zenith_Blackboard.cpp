#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

// ========== Setters ==========

void Zenith_Blackboard::SetFloat(const std::string& strKey, float fValue)
{
	BlackboardValue xValue;
	xValue.m_eType = ValueType::FLOAT;
	xValue.m_fValue = fValue;
	m_xData[strKey] = xValue;
}

void Zenith_Blackboard::SetInt(const std::string& strKey, int32_t iValue)
{
	BlackboardValue xValue;
	xValue.m_eType = ValueType::INT;
	xValue.m_iValue = iValue;
	m_xData[strKey] = xValue;
}

void Zenith_Blackboard::SetBool(const std::string& strKey, bool bValue)
{
	BlackboardValue xValue;
	xValue.m_eType = ValueType::BOOL;
	xValue.m_bValue = bValue;
	m_xData[strKey] = xValue;
}

void Zenith_Blackboard::SetVector3(const std::string& strKey, const Zenith_Maths::Vector3& xVec)
{
	BlackboardValue xValue;
	xValue.m_eType = ValueType::VECTOR3;
	xValue.m_xVector3.x = xVec.x;
	xValue.m_xVector3.y = xVec.y;
	xValue.m_xVector3.z = xVec.z;
	m_xData[strKey] = xValue;
}

void Zenith_Blackboard::SetEntityID(const std::string& strKey, Zenith_EntityID xID)
{
	BlackboardValue xValue;
	xValue.m_eType = ValueType::ENTITY_ID;
	xValue.m_ulEntityIDPacked = xID.IsValid() ? xID.GetPacked() : UINT64_MAX;
	m_xData[strKey] = xValue;
}

// ========== Getters with defaults ==========

float Zenith_Blackboard::GetFloat(const std::string& strKey, float fDefault) const
{
	const BlackboardValue* pxValue = m_xData.TryGet(strKey);
	if (pxValue != nullptr && pxValue->m_eType == ValueType::FLOAT)
	{
		return pxValue->m_fValue;
	}
	return fDefault;
}

int32_t Zenith_Blackboard::GetInt(const std::string& strKey, int32_t iDefault) const
{
	const BlackboardValue* pxValue = m_xData.TryGet(strKey);
	if (pxValue != nullptr && pxValue->m_eType == ValueType::INT)
	{
		return pxValue->m_iValue;
	}
	return iDefault;
}

bool Zenith_Blackboard::GetBool(const std::string& strKey, bool bDefault) const
{
	const BlackboardValue* pxValue = m_xData.TryGet(strKey);
	if (pxValue != nullptr && pxValue->m_eType == ValueType::BOOL)
	{
		return pxValue->m_bValue;
	}
	return bDefault;
}

Zenith_Maths::Vector3 Zenith_Blackboard::GetVector3(const std::string& strKey, const Zenith_Maths::Vector3& xDefault) const
{
	const BlackboardValue* pxValue = m_xData.TryGet(strKey);
	if (pxValue != nullptr && pxValue->m_eType == ValueType::VECTOR3)
	{
		return Zenith_Maths::Vector3(
			pxValue->m_xVector3.x,
			pxValue->m_xVector3.y,
			pxValue->m_xVector3.z
		);
	}
	return xDefault;
}

Zenith_EntityID Zenith_Blackboard::GetEntityID(const std::string& strKey) const
{
	const BlackboardValue* pxValue = m_xData.TryGet(strKey);
	if (pxValue != nullptr && pxValue->m_eType == ValueType::ENTITY_ID)
	{
		if (pxValue->m_ulEntityIDPacked == UINT64_MAX)
		{
			return INVALID_ENTITY_ID;
		}
		return Zenith_EntityID::FromPacked(pxValue->m_ulEntityIDPacked);
	}
	return INVALID_ENTITY_ID;
}

// ========== Key management ==========

bool Zenith_Blackboard::HasKey(const std::string& strKey) const
{
	return m_xData.Contains(strKey);
}

void Zenith_Blackboard::RemoveKey(const std::string& strKey)
{
	m_xData.Remove(strKey);
}

void Zenith_Blackboard::Clear()
{
	m_xData.Clear();
}

void Zenith_Blackboard::IterateEntries(EntryDisplayFunc pfnCallback, void* pUserData) const
{
	if (!pfnCallback) return;
	char acBuf[64];
	for (Zenith_HashMap<std::string, BlackboardValue>::Iterator xIt(m_xData); !xIt.Done(); xIt.Next())
	{
		const std::string& strKey = xIt.GetKey();
		const BlackboardValue& xValue = xIt.GetValue();
		const char* szType = "?";
		switch (xValue.m_eType)
		{
			case ValueType::FLOAT:
				szType = "float";
				snprintf(acBuf, sizeof(acBuf), "%.3f", xValue.m_fValue);
				break;
			case ValueType::INT:
				szType = "int";
				snprintf(acBuf, sizeof(acBuf), "%d", xValue.m_iValue);
				break;
			case ValueType::BOOL:
				szType = "bool";
				snprintf(acBuf, sizeof(acBuf), "%s", xValue.m_bValue ? "true" : "false");
				break;
			case ValueType::VECTOR3:
				szType = "Vector3";
				snprintf(acBuf, sizeof(acBuf), "(%.2f, %.2f, %.2f)",
					xValue.m_xVector3.x, xValue.m_xVector3.y, xValue.m_xVector3.z);
				break;
			case ValueType::ENTITY_ID:
				szType = "EntityID";
				snprintf(acBuf, sizeof(acBuf), "%llu", static_cast<unsigned long long>(xValue.m_ulEntityIDPacked));
				break;
		}
		pfnCallback(pUserData, strKey.c_str(), szType, acBuf);
	}
}

// ========== Serialization ==========

void Zenith_Blackboard::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write number of entries
	uint32_t uCount = static_cast<uint32_t>(m_xData.GetSize());
	xStream << uCount;

	for (Zenith_HashMap<std::string, BlackboardValue>::Iterator xIt(m_xData); !xIt.Done(); xIt.Next())
	{
		const std::string& strKey = xIt.GetKey();
		const BlackboardValue& xValue = xIt.GetValue();
		xStream << strKey;
		xStream << static_cast<uint8_t>(xValue.m_eType);

		// Write value based on type
		switch (xValue.m_eType)
		{
		case ValueType::FLOAT:
			xStream << xValue.m_fValue;
			break;
		case ValueType::INT:
			xStream << xValue.m_iValue;
			break;
		case ValueType::BOOL:
			xStream << xValue.m_bValue;
			break;
		case ValueType::VECTOR3:
			xStream << xValue.m_xVector3.x;
			xStream << xValue.m_xVector3.y;
			xStream << xValue.m_xVector3.z;
			break;
		case ValueType::ENTITY_ID:
			xStream << xValue.m_ulEntityIDPacked;
			break;
		}
	}
}

void Zenith_Blackboard::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	// Read number of entries
	uint32_t uCount = 0;
	xStream >> uCount;

	for (uint32_t u = 0; u < uCount; ++u)
	{
		std::string strKey;
		xStream >> strKey;

		// Read type
		uint8_t uType = 0;
		xStream >> uType;
		ValueType eType = static_cast<ValueType>(uType);

		// Read value based on type
		BlackboardValue xValue;
		xValue.m_eType = eType;

		switch (eType)
		{
		case ValueType::FLOAT:
			xStream >> xValue.m_fValue;
			break;
		case ValueType::INT:
			xStream >> xValue.m_iValue;
			break;
		case ValueType::BOOL:
			xStream >> xValue.m_bValue;
			break;
		case ValueType::VECTOR3:
			xStream >> xValue.m_xVector3.x;
			xStream >> xValue.m_xVector3.y;
			xStream >> xValue.m_xVector3.z;
			break;
		case ValueType::ENTITY_ID:
			xStream >> xValue.m_ulEntityIDPacked;
			break;
		}

		m_xData[strKey] = xValue;
	}
}
