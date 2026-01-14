#pragma once

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Entity.h"
#include <string>
#include <unordered_map>

class Zenith_DataStream;

/**
 * Zenith_Blackboard - Type-safe key-value store for behavior tree state
 *
 * Used to share data between behavior tree nodes and external systems.
 * Supports common types: float, int32, bool, Vector3, EntityID.
 */
class Zenith_Blackboard
{
public:
	Zenith_Blackboard() = default;
	~Zenith_Blackboard() = default;

	// Move semantics
	Zenith_Blackboard(Zenith_Blackboard&& xOther) noexcept = default;
	Zenith_Blackboard& operator=(Zenith_Blackboard&& xOther) noexcept = default;

	// Delete copy (blackboards should be moved or referenced, not copied)
	Zenith_Blackboard(const Zenith_Blackboard&) = delete;
	Zenith_Blackboard& operator=(const Zenith_Blackboard&) = delete;

	// ========== Setters ==========

	void SetFloat(const std::string& strKey, float fValue);
	void SetInt(const std::string& strKey, int32_t iValue);
	void SetBool(const std::string& strKey, bool bValue);
	void SetVector3(const std::string& strKey, const Zenith_Maths::Vector3& xValue);
	void SetEntityID(const std::string& strKey, Zenith_EntityID xValue);

	// ========== Getters with defaults ==========

	float GetFloat(const std::string& strKey, float fDefault = 0.0f) const;
	int32_t GetInt(const std::string& strKey, int32_t iDefault = 0) const;
	bool GetBool(const std::string& strKey, bool bDefault = false) const;
	Zenith_Maths::Vector3 GetVector3(const std::string& strKey, const Zenith_Maths::Vector3& xDefault = Zenith_Maths::Vector3(0.0f)) const;
	Zenith_EntityID GetEntityID(const std::string& strKey) const;

	// ========== Key management ==========

	bool HasKey(const std::string& strKey) const;
	void RemoveKey(const std::string& strKey);
	void Clear();

	// Get number of entries
	uint32_t GetSize() const { return static_cast<uint32_t>(m_xData.size()); }

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	enum class ValueType : uint8_t
	{
		FLOAT,
		INT,
		BOOL,
		VECTOR3,
		ENTITY_ID
	};

	struct BlackboardValue
	{
		ValueType m_eType;
		union
		{
			float m_fValue;
			int32_t m_iValue;
			bool m_bValue;
			struct { float x, y, z; } m_xVector3;
			uint64_t m_ulEntityIDPacked;
		};

		BlackboardValue() : m_eType(ValueType::FLOAT), m_fValue(0.0f) {}
	};

	std::unordered_map<std::string, BlackboardValue> m_xData;
};
