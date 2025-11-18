#include "Zenith.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
// #include "EntityComponent/Components/Zenith_ScriptComponent.h" // TODO: Implement script serialization

/**
 * Register all serializable component types
 *
 * This function should be called during engine initialization to register
 * all component types with the component registry system.
 */
void Zenith_RegisterAllComponents()
{
	// Register TransformComponent
	Zenith_ComponentRegistry::RegisterComponent(
		"TransformComponent",
		// Deserializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_TransformComponent>()) {
				// Transform is auto-created with entity, just deserialize it
				Zenith_TransformComponent& xComponent = xEntity.GetComponent<Zenith_TransformComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		},
		// Serializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_TransformComponent>()) {
				xStream << std::string("TransformComponent");
				Zenith_TransformComponent& xComponent = xEntity.GetComponent<Zenith_TransformComponent>();
				xComponent.WriteToDataStream(xStream);
			}
		}
	);

	// Register ModelComponent
	Zenith_ComponentRegistry::RegisterComponent(
		"ModelComponent",
		// Deserializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (!xEntity.HasComponent<Zenith_ModelComponent>()) {
				Zenith_ModelComponent& xComponent = xEntity.AddComponent<Zenith_ModelComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		},
		// Serializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_ModelComponent>()) {
				xStream << std::string("ModelComponent");
				Zenith_ModelComponent& xComponent = xEntity.GetComponent<Zenith_ModelComponent>();
				xComponent.WriteToDataStream(xStream);
			}
		}
	);

	// Register CameraComponent
	Zenith_ComponentRegistry::RegisterComponent(
		"CameraComponent",
		// Deserializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (!xEntity.HasComponent<Zenith_CameraComponent>()) {
				Zenith_CameraComponent& xComponent = xEntity.AddComponent<Zenith_CameraComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		},
		// Serializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_CameraComponent>()) {
				xStream << std::string("CameraComponent");
				Zenith_CameraComponent& xComponent = xEntity.GetComponent<Zenith_CameraComponent>();
				xComponent.WriteToDataStream(xStream);
			}
		}
	);

	// Register ColliderComponent
	Zenith_ComponentRegistry::RegisterComponent(
		"ColliderComponent",
		// Deserializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (!xEntity.HasComponent<Zenith_ColliderComponent>()) {
				Zenith_ColliderComponent& xComponent = xEntity.AddComponent<Zenith_ColliderComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		},
		// Serializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_ColliderComponent>()) {
				xStream << std::string("ColliderComponent");
				Zenith_ColliderComponent& xComponent = xEntity.GetComponent<Zenith_ColliderComponent>();
				xComponent.WriteToDataStream(xStream);
			}
		}
	);

	// Register TextComponent
	Zenith_ComponentRegistry::RegisterComponent(
		"TextComponent",
		// Deserializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (!xEntity.HasComponent<Zenith_TextComponent>()) {
				Zenith_TextComponent& xComponent = xEntity.AddComponent<Zenith_TextComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		},
		// Serializer
		[](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
			if (xEntity.HasComponent<Zenith_TextComponent>()) {
				xStream << std::string("TextComponent");
				Zenith_TextComponent& xComponent = xEntity.GetComponent<Zenith_TextComponent>();
				xComponent.WriteToDataStream(xStream);
			}
		}
	);

	// Note: TerrainComponent deserialization is complex due to its constructor requirements
	// It requires references to meshes and materials that must be loaded first
	// For now, terrain components will need to be reconstructed manually after scene load

	// Note: ScriptComponent serialization not yet implemented
	// Requires a script behavior factory system to serialize/deserialize polymorphic behavior pointers
}
