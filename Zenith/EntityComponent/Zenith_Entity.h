#pragma once

class Zenith_Scene;
class Zenith_DataStream;
using Zenith_EntityID = unsigned int;

class Zenith_Entity
{
public:
	Zenith_Entity() = default;
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args);

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	T& GetComponent() const;

	template<typename T>
	void RemoveComponent();

	Zenith_EntityID GetEntityID() { return m_uEntityID; }
	class Zenith_Scene* m_pxParentScene;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	Zenith_EntityID m_uParentEntityID = -1;

	// Name accessors (name stored in scene, not entity)
	const std::string& GetName() const;
	void SetName(const std::string& strName);
private:
	Zenith_EntityID m_uEntityID;
};
