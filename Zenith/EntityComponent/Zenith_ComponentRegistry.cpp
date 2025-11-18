#include "Zenith.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"

// Initialize static members
std::unordered_map<Zenith_ComponentRegistry::ComponentTypeName, Zenith_ComponentRegistry::ComponentDeserializer>
	Zenith_ComponentRegistry::s_xDeserializers;

std::unordered_map<Zenith_ComponentRegistry::ComponentTypeName, Zenith_ComponentRegistry::ComponentSerializer>
	Zenith_ComponentRegistry::s_xSerializers;
