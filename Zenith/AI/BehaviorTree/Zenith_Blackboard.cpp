#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "DataStream/Zenith_DataStream.h"

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
	auto it = m_xData.find(strKey);
	if (it != m_xData.end() && it->second.m_eType == ValueType::FLOAT)
	{
		return it->second.m_fValue;
	}
	return fDefault;
}

int32_t Zenith_Blackboard::GetInt(const std::string& strKey, int32_t iDefault) const
{
	auto it = m_xData.find(strKey);
	if (it != m_xData.end() && it->second.m_eType == ValueType::INT)
	{
		return it->second.m_iValue;
	}
	return iDefault;
}

bool Zenith_Blackboard::GetBool(const std::string& strKey, bool bDefault) const
{
	auto it = m_xData.find(strKey);
	if (it != m_xData.end() && it->second.m_eType == ValueType::BOOL)
	{
		return it->second.m_bValue;
	}
	return bDefault;
}

Zenith_Maths::Vector3 Zenith_Blackboard::GetVector3(const std::string& strKey, const Zenith_Maths::Vector3& xDefault) const
{
	auto it = m_xData.find(strKey);
	if (it != m_xData.end() && it->second.m_eType == ValueType::VECTOR3)
	{
		return Zenith_Maths::Vector3(
			it->second.m_xVector3.x,
			it->second.m_xVector3.y,
			it->second.m_xVector3.z
		);
	}
	return xDefault;
}

Zenith_EntityID Zenith_Blackboard::GetEntityID(const std::string& strKey) const
{
	auto it = m_xData.find(strKey);
	if (it != m_xData.end() && it->second.m_eType == ValueType::ENTITY_ID)
	{
		if (it->second.m_ulEntityIDPacked == UINT64_MAX)
		{
			return INVALID_ENTITY_ID;
		}
		return Zenith_EntityID::FromPacked(it->second.m_ulEntityIDPacked);
	}
	return INVALID_ENTITY_ID;
}

// ========== Key management ==========

bool Zenith_Blackboard::HasKey(const std::string& strKey) const
{
	return m_xData.find(strKey) != m_xData.end();
}

void Zenith_Blackboard::RemoveKey(const std::string& strKey)
{
	m_xData.erase(strKey);
}

void Zenith_Blackboard::Clear()
{
	m_xData.clear();
}

// ========== Serialization ==========

void Zenith_Blackboard::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write number of entries
	uint32_t uCount = static_cast<uint32_t>(m_xData.size());
	xStream << uCount;

	for (const auto& xPair : m_xData)
	{
		// Write key
		uint32_t uKeyLen = static_cast<uint32_t>(xPair.first.length());
		xStream << uKeyLen;
		xStream.Write(xPair.first.data(), uKeyLen);

		// Write type
		xStream << static_cast<uint8_t>(xPair.second.m_eType);

		// Write value based on type
		switch (xPair.second.m_eType)
		{
		case ValueType::FLOAT:
			xStream << xPair.second.m_fValue;
			break;
		case ValueType::INT:
			xStream << xPair.second.m_iValue;
			break;
		case ValueType::BOOL:
			xStream << xPair.second.m_bValue;
			break;
		case ValueType::VECTOR3:
			xStream << xPair.second.m_xVector3.x;
			xStream << xPair.second.m_xVector3.y;
			xStream << xPair.second.m_xVector3.z;
			break;
		case ValueType::ENTITY_ID:
			xStream << xPair.second.m_ulEntityIDPacked;
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
		// Read key
		uint32_t uKeyLen = 0;
		xStream >> uKeyLen;
		std::string strKey(uKeyLen, '\0');
		xStream.Read(strKey.data(), uKeyLen);

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
