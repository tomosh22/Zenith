#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"

// Workstream B coverage: .zmodel joined the shared stream-envelope versioning idiom.
// Build a small model (path-based bindings), serialize, assert the envelope identity,
// then round-trip it back through the status-returning ParseStream.

ZENITH_TEST(ModelAsset, StreamEnvelopeRoundtrip)
{
	Zenith_ModelAsset xModel;
	xModel.SetName("EnvelopeModel");

	Zenith_Vector<std::string> xMats;
	xMats.PushBack("game:Materials/a.zmtrl");
	xMats.PushBack("game:Materials/b.zmtrl");
	xModel.AddMeshByPath("game:Meshes/body.zmesh", xMats);
	xModel.SetSkeletonPath("game:Skeletons/rig.zskel");
	xModel.AddAnimationPath("game:Anims/idle.zanim");

	Zenith_DataStream xStream;
	xModel.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_MODEL_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "model write must emit the shared stream envelope");
	if (xHdr.IsOk())
	{
		ZENITH_ASSERT_EQ(xHdr.Value().m_uAssetTypeId, uZENITH_MODEL_ASSET_TYPE_ID, "model envelope type id");
		ZENITH_ASSERT_EQ(xHdr.Value().m_uSchemaVersion, uZENITH_MODEL_SCHEMA_CURRENT, "model envelope schema");
	}

	xStream.SetCursor(0);
	Zenith_ModelAsset xLoaded;
	ZENITH_ASSERT_TRUE(xLoaded.ParseStream(xStream).IsOk(), "model ParseStream must accept its own output");
	ZENITH_ASSERT_EQ(xLoaded.GetName(), std::string("EnvelopeModel"), "name round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetNumMeshes(), 1u, "mesh binding count round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.HasSkeleton(), "skeleton flag round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetNumAnimations(), 1u, "animation count round-trips");
}
