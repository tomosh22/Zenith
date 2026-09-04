#include "Core/Zenith_Engine.h"
// Model-space offset tests. These exercise the CONCRETE component and the matrix
// the render-gather actually composes, so they live aggregate-side and are hosted
// by Zenith_ModelComponent.cpp, whose TU is always linked.
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <string>

//==============================================================================
// Helpers
//==============================================================================
namespace
{
	// A model component on its own entity. NO model is loaded: the offset is a
	// property of the component's matrix composition, and loading a .zmodel would
	// drag a GPU asset into a unit test that does not look at one.
	Zenith_ModelComponent& MakeModelComponent(
		Zenith_SceneData* pxSceneData, const char* szName,
		const Zenith_Maths::Vector3& xPosition)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPosition);
		return xEntity.AddComponent<Zenith_ModelComponent>();
	}

	// The translation column of whatever BuildRenderMatrix composed -- i.e. where
	// the renderer would actually put this model.
	Zenith_Maths::Vector3 RenderTranslation(const Zenith_ModelComponent& xModel)
	{
		Zenith_Maths::Matrix4 xMatrix;
		xModel.BuildRenderMatrix(xMatrix);
		return Zenith_Maths::Vector3(xMatrix[3]);
	}
}

//==============================================================================
// The offset composes into the matrix the renderer uses
//==============================================================================
ZENITH_TEST(ModelComponent, ModelSpaceOffsetComposesIntoTheRenderMatrix)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene(
		"MC_OffsetComposes", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_ModelComponent& xModel =
		MakeModelComponent(pxSceneData, "Model", Zenith_Maths::Vector3(3.0f, 7.0f, -2.0f));

	// DEFAULT IS ZERO, and that is the whole compatibility story: a component that
	// never sets an offset must compose the exact matrix it composed before the
	// feature existed.
	ZENITH_ASSERT_NEAR_VEC3(RenderTranslation(xModel),
		Zenith_Maths::Vector3(3.0f, 7.0f, -2.0f), 1.0e-5f,
		"an unset offset must leave the transform's own matrix untouched");

	xModel.SetModelSpaceOffset(Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f));
	ZENITH_ASSERT_NEAR_VEC3(RenderTranslation(xModel),
		Zenith_Maths::Vector3(3.0f, 8.5f, -2.0f), 1.0e-5f,
		"the offset did not reach the matrix the render-gather builds");

	// MODEL space, not world: it is post-multiplied, so the entity's SCALE carries
	// it. That is what lets a caller state an offset in the rig's own loft units and
	// have the visual scale convert it -- Zenithmon's human placement depends on it.
	xModel.GetParentEntity().GetComponent<Zenith_TransformComponent>()
		.SetScale(Zenith_Maths::Vector3(2.0f));
	ZENITH_ASSERT_NEAR_VEC3(RenderTranslation(xModel),
		Zenith_Maths::Vector3(3.0f, 10.0f, -2.0f), 1.0e-5f,
		"the offset was applied in WORLD space -- it must scale with the entity");
}

//==============================================================================
// ★ THE REGRESSION: a pool relocation must not reset the offset
//==============================================================================

// Component pools RELOCATE their elements when they grow, move-constructing every
// component into the new storage. The offset is NOT serialized and is re-applied
// only when its owner (re)loads a model -- and a caller like Zenithmon's
// ZM_GreyboxVisual early-returns while the model it already loaded is still the one
// it wants. So a field dropped by the move operations is UNRECOVERABLE in normal
// running: the human silently reverts to a zero offset and renders half underground,
// for the rest of the session, with nothing logged and no test on the LOAD path able
// to see it.
//
// ★ THE RELOCATION IS ASSERTED, NOT ASSUMED. If the pool never actually grew, every
// clause below would pass while exercising nothing -- so this fails outright rather
// than reporting a vacuous success.
ZENITH_TEST(ModelComponent, ModelSpaceOffsetSurvivesPoolGrowth)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene(
		"MC_OffsetPoolGrowth", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	const Zenith_Maths::Vector3 xOffset(0.0f, 0.995145f, 0.0f);

	Zenith_Entity xFirst = g_xEngine.Scenes().CreateEntity(pxSceneData, "First");
	xFirst.GetComponent<Zenith_TransformComponent>()
		.SetPosition(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));
	xFirst.AddComponent<Zenith_ModelComponent>().SetModelSpaceOffset(xOffset);

	// Re-resolved through the ENTITY every time, never cached: the pool is about to
	// move the component out from under any pointer we kept.
	const void* pFirstAddress = xFirst.TryGetComponent<Zenith_ModelComponent>();
	ZENITH_ASSERT_NOT_NULL(pFirstAddress, "the first component was not created");

	bool bRelocated = false;
	for (u_int u = 0u; u < 256u; ++u)
	{
		char acName[32];
		snprintf(acName, sizeof(acName), "Filler%u", u);
		MakeModelComponent(pxSceneData, acName, Zenith_Maths::Vector3(0.0f));

		const void* pNow = xFirst.TryGetComponent<Zenith_ModelComponent>();
		ZENITH_ASSERT_NOT_NULL(pNow, "the first component vanished during growth");
		if (pNow != pFirstAddress)
		{
			bRelocated = true;
			break;
		}
	}

	ZENITH_ASSERT_TRUE(bRelocated,
		"the component pool never relocated in 256 additions -- this test proved "
		"NOTHING about the move operations. Raise the count or check the pool's "
		"growth policy rather than deleting the clause");

	Zenith_ModelComponent* pxFirst = xFirst.TryGetComponent<Zenith_ModelComponent>();
	ZENITH_ASSERT_NOT_NULL(pxFirst, "the relocated component is unreachable");
	if (pxFirst == nullptr) { return; }

	ZENITH_ASSERT_NEAR_VEC3(pxFirst->GetModelSpaceOffset(), xOffset, 1.0e-6f,
		"a pool relocation reset the model-space offset -- the move operations are "
		"not transferring it");
	ZENITH_ASSERT_NEAR_VEC3(RenderTranslation(*pxFirst),
		Zenith_Maths::Vector3(0.0f, 10.0f + xOffset.y, 0.0f), 1.0e-5f,
		"the relocated component composes a matrix without its offset");
}

// The same claim about move ASSIGNMENT, which the pool uses on a different path and
// which a fix applied only to the constructor would leave broken.
ZENITH_TEST(ModelComponent, ModelSpaceOffsetSurvivesMoveAssignment)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene(
		"MC_OffsetMoveAssign", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xSourceEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "Source");
	xSourceEntity.GetComponent<Zenith_TransformComponent>()
		.SetPosition(Zenith_Maths::Vector3(0.0f, 4.0f, 0.0f));
	Zenith_ModelComponent xSource(xSourceEntity);
	xSource.SetModelSpaceOffset(Zenith_Maths::Vector3(0.0f, 2.25f, 0.0f));

	Zenith_Entity xTargetEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "Target");
	Zenith_ModelComponent xTarget(xTargetEntity);
	ZENITH_ASSERT_NEAR_VEC3(xTarget.GetModelSpaceOffset(), Zenith_Maths::Vector3(0.0f),
		1.0e-6f, "a fresh component must start with no offset");

	xTarget = std::move(xSource);

	ZENITH_ASSERT_NEAR_VEC3(xTarget.GetModelSpaceOffset(),
		Zenith_Maths::Vector3(0.0f, 2.25f, 0.0f), 1.0e-6f,
		"move assignment dropped the model-space offset");
	ZENITH_ASSERT_NEAR_VEC3(RenderTranslation(xTarget),
		Zenith_Maths::Vector3(0.0f, 6.25f, 0.0f), 1.0e-5f,
		"the move-assigned component composes a matrix without its offset");
}
