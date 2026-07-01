#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "Maths/Zenith_Maths.h"

// Workstream B coverage: .zskel joined the shared stream-envelope versioning idiom.
// Build a tiny two-bone skeleton, serialize, assert the envelope identity, then
// round-trip it back through the status-returning ParseStream.

ZENITH_TEST(SkeletonAsset, StreamEnvelopeRoundtrip)
{
	Zenith_SkeletonAsset xSkel;
	const uint32_t uRoot = xSkel.AddBone("root", -1,
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	xSkel.AddBone("child", static_cast<int32_t>(uRoot),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	xSkel.ComputeBindPoseMatrices();

	Zenith_DataStream xStream;
	xSkel.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_SKELETON_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "skeleton write must emit the shared stream envelope");
	if (xHdr.IsOk())
	{
		ZENITH_ASSERT_EQ(xHdr.Value().m_uAssetTypeId, uZENITH_SKELETON_ASSET_TYPE_ID, "skeleton envelope type id");
		ZENITH_ASSERT_EQ(xHdr.Value().m_uSchemaVersion, uZENITH_SKELETON_SCHEMA_CURRENT, "skeleton envelope schema");
	}

	xStream.SetCursor(0);
	Zenith_SkeletonAsset xLoaded;
	ZENITH_ASSERT_TRUE(xLoaded.ParseStream(xStream).IsOk(), "skeleton ParseStream must accept its own output");
	ZENITH_ASSERT_EQ(xLoaded.GetNumBones(), xSkel.GetNumBones(), "bone count round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.HasBone("root"), "root bone name round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.HasBone("child"), "child bone name round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetBone(1).m_iParentIndex, static_cast<int32_t>(uRoot), "parent index round-trips");
}
