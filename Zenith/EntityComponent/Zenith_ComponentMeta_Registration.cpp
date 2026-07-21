#include "Zenith.h"
// Zenith_ColliderComponent.h pulls <Jolt/Jolt.h> raw (and Zenith_TransformComponent.h
// is pulled transitively through it). Disable the memory-tracking placement-new macro
// before those component headers to avoid clashing with Jolt's custom operator new,
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"

#ifdef ZENITH_TOOLS
// Editor "Add Component" menu registry. Engine-side only -- populated here so the
// ECS reflection core (Zenith_ComponentMeta.h/.cpp) never names it.
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

//------------------------------------------------------------------------------
// Engine-side component registration (ECS leaf-extraction Phase 4)
//
// This TU is the single place that knows the concrete built-in component set. It
// is the engine-installed registrar invoked by
// Zenith_ComponentMetaRegistry::EnsureInitialized (installed from
// Zenith_Engine::Initialise via SetComponentRegistrar). Keeping it here -- NOT in
// the ECS reflection core -- is what lets Zenith_ComponentMeta.h/.cpp stay a leaf
// that names no concrete component type, no editor registry, and no AI symbol.
//
// Dead-strip safety: this function is called explicitly from
// Zenith_Engine::Initialise (a TU always referenced by every game EXE), so every
// built-in registration runs regardless of MSVC /OPT:REF stripping -- the old
// per-component ZENITH_REGISTER_COMPONENT auto-registrars were vulnerable to TU
// dead-strip; the engine-installed registrar is immune.
//------------------------------------------------------------------------------

// Forward declaration of the AIAgent component registrar. Defined engine-side in
// EntityComponent/Components/Zenith_AIAgentComponent.cpp (same aggregate lib as this
// TU); declared here rather than via an #include to avoid pulling the heavy AIAgent
// header -- which transitively drags in the AI BehaviorTree/Navigation/Perception
// headers -- into this registration TU.
void Zenith_AI_RegisterComponents();

void Zenith_RegisterEngineComponents()
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Register every built-in component with its EXACT serialization order (lower
	// serializes first, to respect dependencies -- e.g. Terrain before Collider).
	// These orders are copied VERBATIM from the former hardcoded
	// GetSerializationOrder() map so scene save/load order is byte-for-byte
	// unchanged. RegisterComponent overwrites by name, so this is idempotent.
	xRegistry.RegisterComponent<Zenith_TransformComponent>("Transform", 0);
	xRegistry.RegisterComponent<Zenith_ModelComponent>("Model", 10);
	xRegistry.RegisterComponent<Zenith_TweenComponent>("Tween", 12);
	xRegistry.RegisterComponent<Zenith_AnimatorComponent>("Animator", 15);
	xRegistry.RegisterComponent<Zenith_CameraComponent>("Camera", 20);
	xRegistry.RegisterComponent<Zenith_LightComponent>("Light", 25);
	xRegistry.RegisterComponent<Zenith_TerrainComponent>("Terrain", 40);   // Must be before Collider
	xRegistry.RegisterComponent<Zenith_ColliderComponent>("Collider", 50);
	// Behaviour Graphs (the scripting-system replacement) at order 60 — the
	// slot the retired script component vacated, between Collider and UI.
	xRegistry.RegisterComponent<Zenith_GraphComponent>("Graph", 60);
	xRegistry.RegisterComponent<Zenith_UIComponent>("UI", 70);
	// These auto-registered but were absent from the old map's "named" block; they
	// were given explicit, distinct orders past 70 so std::sort (not stable)
	// orders them deterministically. All depend only on lower-ordered components.
	xRegistry.RegisterComponent<Zenith_InstancedMeshComponent>("InstancedMesh", 80);
	xRegistry.RegisterComponent<Zenith_ParticleEmitterComponent>("ParticleEmitter", 85);
	// Reusable bone-attachment (racket-on-hand / FPS weapon). Order 95 sits after
	// AIAgent(90) and before game components (100+); its OnLateUpdate follow runs
	// after every OnUpdate regardless of order, so the value is just a stable,
	// distinct serialization slot.
	xRegistry.RegisterComponent<Zenith_AttachmentComponent>("Attachment", 95);

	// AIAgent (Zenith_AIAgentComponent, engine-side) registers via the forwarder
	// (order 90 is passed inside Zenith_AI_RegisterComponents) so we don't pull the
	// heavy AIAgent header into this TU.
	Zenith_AI_RegisterComponents();

#ifdef ZENITH_TOOLS
	// Mirror every built-in into the editor "Add Component" menu registry. This is
	// the side-effect that used to live in Zenith_ComponentMetaRegistry::
	// RegisterComponent<T> (#ifdef ZENITH_TOOLS) before the ECS core was made
	// leaf-clean; moving it here preserves the editor menu without the ECS core
	// naming the editor registry. AIAgent's editor-registry entry is added by
	// Zenith_AI_RegisterComponents (consistent with the AIAgent meta registration
	// living in the AI module).
	// IMPORTANT: this insertion order is the editor menu order, so it is copied
	// VERBATIM from the former FinalizeRegistration() call sequence (Collider
	// before Terrain; Script/UI before InstancedMesh/ParticleEmitter) -- NOT the
	// serialization order used for the meta registry above -- so the "Add
	// Component" menu lists components in exactly the same order as before. AIAgent
	// is appended last by Zenith_AI_RegisterComponents (the AI cpp), matching the
	// historical behaviour where the AI forwarder ran after the built-ins.
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<Zenith_TransformComponent>("Transform");
	xEditorRegistry.RegisterComponent<Zenith_ModelComponent>("Model");
	xEditorRegistry.RegisterComponent<Zenith_TweenComponent>("Tween");
	xEditorRegistry.RegisterComponent<Zenith_AnimatorComponent>("Animator");
	xEditorRegistry.RegisterComponent<Zenith_CameraComponent>("Camera");
	xEditorRegistry.RegisterComponent<Zenith_LightComponent>("Light");
	xEditorRegistry.RegisterComponent<Zenith_ColliderComponent>("Collider");
	xEditorRegistry.RegisterComponent<Zenith_TerrainComponent>("Terrain");
	xEditorRegistry.RegisterComponent<Zenith_InstancedMeshComponent>("InstancedMesh");
	xEditorRegistry.RegisterComponent<Zenith_ParticleEmitterComponent>("ParticleEmitter");
	xEditorRegistry.RegisterComponent<Zenith_GraphComponent>("Graph");
	xEditorRegistry.RegisterComponent<Zenith_UIComponent>("UI");
	xEditorRegistry.RegisterComponent<Zenith_AttachmentComponent>("Attachment");
#endif
}

// The component-meta registry's serialization-order collision units. Hosted HERE,
// not in the ZenithECS leaf: the leaf may depend only on ZenithBase, while the test
// framework lives in Zenith/Core, so a .Tests.inl inside the leaf would break both
// the SentinelECS link proof and the ECS-leaf ratchet. This TU is always linked (it
// is the registrar every game calls through), so the static test registrations
// survive /OPT:REF in every config -- the same idiom as
// Zenith_Physics.Tests.inl -> Zenith_ColliderComponent.cpp.
#include "EntityComponent/Zenith_ComponentMetaRegistry.Tests.inl"
