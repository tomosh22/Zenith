#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include <functional>
#include <unordered_map>
#include <string>

/**
 * Component Registry for Scene Serialization
 *
 * This system provides a mapping between component type names and their
 * deserialization logic, enabling dynamic component reconstruction during
 * scene loading.
 *
 * Usage:
 * - Components are registered at startup with REGISTER_COMPONENT macro
 * - During serialization, component type names are written to the stream
 * - During deserialization, components are created via factory functions
 */

class Zenith_ComponentRegistry
{
public:
	// Component type identifier (human-readable string)
	using ComponentTypeName = std::string;

	// Factory function signature: creates and deserializes a component for an entity
	using ComponentDeserializer = std::function<void(Zenith_Entity&, Zenith_DataStream&)>;

	// Serializer function signature: writes component type name and data to stream
	using ComponentSerializer = std::function<void(Zenith_Entity&, Zenith_DataStream&)>;

	/**
	 * Register a component type with its serialization and deserialization logic
	 *
	 * @param strTypeName - Human-readable component type name (e.g., "TransformComponent")
	 * @param fnDeserializer - Function to create and deserialize the component
	 * @param fnSerializer - Function to serialize the component if it exists
	 */
	static void RegisterComponent(
		const ComponentTypeName& strTypeName,
		ComponentDeserializer fnDeserializer,
		ComponentSerializer fnSerializer
	)
	{
		s_xDeserializers[strTypeName] = fnDeserializer;
		s_xSerializers[strTypeName] = fnSerializer;
	}

	/**
	 * Deserialize a component from a stream and attach it to an entity
	 *
	 * @param strTypeName - Component type name read from the stream
	 * @param xEntity - Entity to attach the component to
	 * @param xStream - Data stream containing component data
	 * @return true if component was successfully deserialized, false otherwise
	 */
	static bool DeserializeComponent(
		const ComponentTypeName& strTypeName,
		Zenith_Entity& xEntity,
		Zenith_DataStream& xStream
	)
	{
		auto it = s_xDeserializers.find(strTypeName);
		if (it != s_xDeserializers.end())
		{
			it->second(xEntity, xStream);
			return true;
		}
		return false;
	}

	/**
	 * Serialize a component from an entity to a stream (if it exists)
	 *
	 * @param strTypeName - Component type name to serialize
	 * @param xEntity - Entity containing the component
	 * @param xStream - Data stream to write to
	 * @return true if component exists and was serialized, false otherwise
	 */
	static bool SerializeComponent(
		const ComponentTypeName& strTypeName,
		Zenith_Entity& xEntity,
		Zenith_DataStream& xStream
	)
	{
		auto it = s_xSerializers.find(strTypeName);
		if (it != s_xSerializers.end())
		{
			it->second(xEntity, xStream);
			return true;
		}
		return false;
	}

	/**
	 * Get a list of all registered component type names
	 */
	static std::vector<ComponentTypeName> GetRegisteredComponentTypes()
	{
		std::vector<ComponentTypeName> xTypes;
		for (const auto& pair : s_xDeserializers)
		{
			xTypes.push_back(pair.first);
		}
		return xTypes;
	}

private:
	static std::unordered_map<ComponentTypeName, ComponentDeserializer> s_xDeserializers;
	static std::unordered_map<ComponentTypeName, ComponentSerializer> s_xSerializers;
};

// Helper macro for registering components
#define REGISTER_COMPONENT(ComponentClass, TypeName) \
	namespace { \
		struct ComponentClass##_Registrar { \
			ComponentClass##_Registrar() { \
				Zenith_ComponentRegistry::RegisterComponent( \
					TypeName, \
					[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) { \
						if (!xEntity.HasComponent<ComponentClass>()) { \
							ComponentClass& xComponent = xEntity.AddComponent<ComponentClass>(xEntity); \
							xComponent.ReadFromDataStream(xStream); \
						} \
					}, \
					[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) { \
						if (xEntity.HasComponent<ComponentClass>()) { \
							xStream << std::string(TypeName); \
							ComponentClass& xComponent = xEntity.GetComponent<ComponentClass>(); \
							xComponent.WriteToDataStream(xStream); \
						} \
					} \
				); \
			} \
		}; \
		static ComponentClass##_Registrar g_##ComponentClass##_Registrar; \
	}
