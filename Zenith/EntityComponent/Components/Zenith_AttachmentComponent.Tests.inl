#include "UnitTests/Zenith_UnitTests.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Collections/Zenith_HashMap.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

// Coverage for the v2 attachment serialize -> scene-load resolve round-trip (WS1).
// Each test builds a tiny additive scene with a skeleton target + an item carrying
// the attachment, round-trips through a DataStream, then drives ResolveEntityReferences
// with a file-index -> EntityID map exactly as the scene loader does.

namespace
{
	struct AttachTestScene
	{
		Zenith_Scene xScene;
		Zenith_SceneData* pxData = nullptr;
		Zenith_Entity xSkel;
		Zenith_Entity xItem;

		explicit AttachTestScene(const char* szName)
		{
			xScene = g_xEngine.Scenes().LoadScene(szName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			pxData = g_xEngine.Scenes().GetSceneData(xScene);
			xSkel = g_xEngine.Scenes().CreateEntity(pxData, "Skel");
			xItem = g_xEngine.Scenes().CreateEntity(pxData, "Item");
		}
		~AttachTestScene() { g_xEngine.Scenes().UnloadSceneForced(xScene); }
	};
}

ZENITH_TEST(Attachment, SerializeResolveRoundTripAttached)
{
	AttachTestScene xFix("AttachTest_Attached");
	Zenith_AttachmentComponent& xAtt = xFix.xItem.AddComponent<Zenith_AttachmentComponent>();

	const Zenith_Maths::Matrix4 xOffset =
		glm::translate(Zenith_Maths::Matrix4(1.0f), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAtt.AttachToBone(xFix.xSkel, "RightHand", xOffset);

	Zenith_DataStream xStream;
	xAtt.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	xAtt.ReadFromDataStream(xStream);

	// Read leaves it pending (not yet resolved).
	ZENITH_ASSERT_FALSE(xAtt.IsAttached(), "attachment must stay detached until resolved");

	Zenith_HashMap<uint32_t, Zenith_EntityID> xMap;
	xMap[xFix.xSkel.GetEntityID().m_uIndex] = xFix.xSkel.GetEntityID();
	xAtt.ResolveEntityReferences(xMap);

	ZENITH_ASSERT_TRUE(xAtt.IsAttached(), "attachment must resolve to attached");
	ZENITH_ASSERT_TRUE(xAtt.GetSkeletonEntity().GetEntityID() == xFix.xSkel.GetEntityID(),
		"resolved skeleton entity must match");
	ZENITH_ASSERT_STREQ(xAtt.GetBoneName().c_str(), "RightHand", "bone name must round-trip");
	ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(xAtt.GetOffset()[3]),
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 1e-4f, "offset translation must round-trip");
}

ZENITH_TEST(Attachment, RegistryDispatchResolvesBinding)
{
	// Exercises the PRODUCTION seam — the component-meta registry dispatch that the
	// scene loader calls (Zenith_SceneData_Serialization.cpp), rather than the
	// component method directly. Proves the HasResolveEntityReferences concept matched,
	// the "Attachment" wrapper got wired into m_pfnResolveEntityReferences, and the
	// per-entity dispatch loop invokes it.
	AttachTestScene xFix("AttachTest_RegistryDispatch");
	Zenith_AttachmentComponent& xAtt = xFix.xItem.AddComponent<Zenith_AttachmentComponent>();
	const Zenith_Maths::Matrix4 xOffset =
		glm::translate(Zenith_Maths::Matrix4(1.0f), Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f));
	xAtt.AttachToBone(xFix.xSkel, "RightHand", xOffset);

	Zenith_DataStream xStream;
	xAtt.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	xAtt.ReadFromDataStream(xStream);
	ZENITH_ASSERT_FALSE(xAtt.IsAttached(), "pending before registry dispatch");

	Zenith_HashMap<uint32_t, Zenith_EntityID> xMap;
	xMap[xFix.xSkel.GetEntityID().m_uIndex] = xFix.xSkel.GetEntityID();
	Zenith_ComponentMetaRegistry::Get().DispatchResolveEntityReferences(xFix.xItem, xMap);

	ZENITH_ASSERT_TRUE(xAtt.IsAttached(), "registry dispatch must re-bind the attachment");
	ZENITH_ASSERT_TRUE(xAtt.GetSkeletonEntity().GetEntityID() == xFix.xSkel.GetEntityID(),
		"registry-resolved skeleton must match the remapped target");
	ZENITH_ASSERT_STREQ(xAtt.GetBoneName().c_str(), "RightHand", "bone name preserved through registry resolve");
}

ZENITH_TEST(Attachment, DetachedRoundTripStaysDetached)
{
	AttachTestScene xFix("AttachTest_Detached");
	Zenith_AttachmentComponent& xAtt = xFix.xItem.AddComponent<Zenith_AttachmentComponent>();

	// Attach then drop — the persisted state must be "detached".
	xAtt.AttachToBone(xFix.xSkel, "RightHand", Zenith_Maths::Matrix4(1.0f));
	xAtt.Detach();

	Zenith_DataStream xStream;
	xAtt.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	xAtt.ReadFromDataStream(xStream);

	// Even with the target present in the map, a detached payload stays detached.
	Zenith_HashMap<uint32_t, Zenith_EntityID> xMap;
	xMap[xFix.xSkel.GetEntityID().m_uIndex] = xFix.xSkel.GetEntityID();
	xAtt.ResolveEntityReferences(xMap);

	ZENITH_ASSERT_FALSE(xAtt.IsAttached(), "detached attachment must stay detached after resolve");
}

ZENITH_TEST(Attachment, ResolveMissStaysDetached)
{
	AttachTestScene xFix("AttachTest_Miss");
	Zenith_AttachmentComponent& xAtt = xFix.xItem.AddComponent<Zenith_AttachmentComponent>();
	xAtt.AttachToBone(xFix.xSkel, "RightHand", Zenith_Maths::Matrix4(1.0f));

	Zenith_DataStream xStream;
	xAtt.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	xAtt.ReadFromDataStream(xStream);

	// Empty map => the skeleton index is unresolvable (transient-excluded / cross-scene).
	Zenith_HashMap<uint32_t, Zenith_EntityID> xMap;
	xAtt.ResolveEntityReferences(xMap);

	ZENITH_ASSERT_FALSE(xAtt.IsAttached(), "unresolved attachment must be left detached");

	// Pending state is single-use: a second resolve with the target now present must
	// NOT re-bind (the pending flag was cleared by the first miss).
	Zenith_HashMap<uint32_t, Zenith_EntityID> xMap2;
	xMap2[xFix.xSkel.GetEntityID().m_uIndex] = xFix.xSkel.GetEntityID();
	xAtt.ResolveEntityReferences(xMap2);
	ZENITH_ASSERT_FALSE(xAtt.IsAttached(), "pending state must be cleared after a failed resolve");
}
