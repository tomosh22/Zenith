#include "UnitTests/Zenith_UnitTests.h"
#include <fstream>
#include <sstream>
#include <limits>

namespace
{
	struct GlbSkinFixture
	{
		std::filesystem::path m_xPath;
		GlbSkinFixture(int iFault)
		{
			m_xPath = std::filesystem::temp_directory_path() / ("zenith_shared_skin_fixture_" + std::to_string(iFault) + ".glb");
			std::vector<unsigned char> abBin;
			auto Append = [&](const void* p, size_t n)
			{
				const auto* b = static_cast<const unsigned char*>(p);
				abBin.insert(abBin.end(), b, b + n);
			};
			const float afPositions[] = { 0,1,0, 1,1,0, 0,1,1 };
			const float afNormals[] = { 0,1,0, 0,1,0, 0,1,0 };
			const float afUVs[] = { 0,0, 1,0, 0,1 };
			const unsigned char abJoints[] = { 0,0,0,0, 1,0,0,0, 0,1,0,0 };
			float afWeights[] = { 1,0,0,0, 1,0,0,0, .25f,.75f,0,0 };
			if (iFault == 1) afWeights[0] = .5f;
			if (iFault == 8) afWeights[0] = std::numeric_limits<float>::quiet_NaN();
			const unsigned short auIndices[] = { 0,1,2,0 }; // final ushort is alignment
			float afInverse[] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,-1,0,1,
				1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
			if (iFault == 2) afInverse[13] = -2;
			if (iFault == 9) afInverse[13] = -2;
			Append(afPositions, sizeof(afPositions)); Append(afNormals, sizeof(afNormals)); Append(afUVs, sizeof(afUVs));
			Append(abJoints, sizeof(abJoints)); Append(afWeights, sizeof(afWeights)); Append(auIndices, sizeof(auIndices)); Append(afInverse, sizeof(afInverse));
			std::ostringstream s;
			s << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,2]}],"nodes":[{"name":"Root","children":[1]},{"name":")"
				<< (iFault == 3 ? "Unknown" : "Tip") << R"(","translation":[0,)" << (iFault == 9 ? 2 : 1) << R"(,0]},{"mesh":0,"skin":0)"
				<< (iFault == 7 ? R"(,"translation":[1,0,0])" : "") << R"(}],"skins":[{"joints":[1,0],"inverseBindMatrices":6}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"JOINTS_0":3)"
				<< (iFault == 4 ? "" : R"(,"WEIGHTS_0":4)") << (iFault == 5 ? R"(,"JOINTS_1":3)" : "")
				<< R"(},"indices":5}]}],"buffers":[{"byteLength":292}],"bufferViews":[)"
				<< R"({"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},)"
				<< R"({"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":12},)"
				<< R"({"buffer":0,"byteOffset":108,"byteLength":48},{"buffer":0,"byteOffset":156,"byteLength":6},)"
				<< R"({"buffer":0,"byteOffset":164,"byteLength":128}],"accessors":[)"
				<< R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},)"
				<< R"({"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5121,"count":3,"type":"VEC4"},)"
				<< R"({"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":5,"componentType":5123,"count":3,"type":"SCALAR"},)"
				<< R"({"bufferView":6,"componentType":5126,"count":2,"type":"MAT4"}]})";
			std::string strJson = s.str(); while (strJson.size() % 4) strJson += ' ';
			const u_int auHeader[] = { 0x46546c67, 2, static_cast<u_int>(28 + strJson.size() + abBin.size()), static_cast<u_int>(strJson.size()), 0x4e4f534a };
			const u_int auBinHeader[] = { static_cast<u_int>(abBin.size()), 0x004e4942 };
			std::ofstream xFile(m_xPath, std::ios::binary | std::ios::trunc);
			xFile.write(reinterpret_cast<const char*>(auHeader), sizeof(auHeader)); xFile.write(strJson.data(), strJson.size());
			xFile.write(reinterpret_cast<const char*>(auBinHeader), sizeof(auBinHeader)); xFile.write(reinterpret_cast<const char*>(abBin.data()), abBin.size());
		}
		~GlbSkinFixture() { std::error_code ec; std::filesystem::remove(m_xPath, ec); }
	};

	void GlbSkinTestRig(Zenith_SkeletonAsset& xRig, bool bWrongParent = false)
	{
		xRig.AddBone("Root", -1, {0,0,0}, glm::identity<Zenith_Maths::Quat>(), {1,1,1});
		xRig.AddBone("Tip", bWrongParent ? -1 : 0, {0,1,0}, glm::identity<Zenith_Maths::Quat>(), {1,1,1});
		xRig.ComputeBindPoseMatrices();
	}
}

ZENITH_TEST(GlbImport, OpenSurfaceFacingSurvivesTranslationAndReflection)
{
	struct FixtureFile
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() / "zenith_glb_open_surface_winding.glb";
		~FixtureFile() { std::error_code ec; std::filesystem::remove(path, ec); }
	} source;
	// One CCW triangle with +Z normals. Its signed-volume sum changes sign
	// when translated through Z=0, although its facing must remain unchanged.
	const float data[] = {0,0,0, 1,0,0, 0,1,0, 0,0,1, 0,0,1, 0,0,1};
	for (int z : {-2,0,2}) for (int mirror : {1,-1})
	{
		std::ostringstream json;
		json << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0,"translation":[0,0,)"
			<< z << R"(],"scale":[)" << mirror << R"(,1,1]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}],)"
			<< R"("buffers":[{"byteLength":72}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36}],)"
			<< R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}]})";
		std::string text = json.str(); while (text.size() % 4) text += ' ';
		const u_int header[] = {0x46546c67,2,static_cast<u_int>(28+text.size()+sizeof(data)),static_cast<u_int>(text.size()),0x4e4f534a};
		const u_int binary[] = {sizeof(data),0x004e4942};
		{
			std::ofstream file(source.path,std::ios::binary | std::ios::trunc);
			file.write(reinterpret_cast<const char*>(header),sizeof(header)); file.write(text.data(),text.size());
			file.write(reinterpret_cast<const char*>(binary),sizeof(binary)); file.write(reinterpret_cast<const char*>(data),sizeof(data));
		}
		Zenith_MeshAsset mesh; Zenith_Tools_GlbImport::GlbImportResult result;
		ZENITH_ASSERT_TRUE(Zenith_Tools_GlbImport::LoadGlbMesh(source.path.string(),mesh,result), "open surface loads at z=%d mirror=%d",z,mirror);
		ZENITH_ASSERT_EQ(mesh.GetNumIndices(),3u,"one complete triangle");
		if (mesh.GetNumIndices() != 3) continue;
		const auto& a = mesh.m_xPositions.Get(mesh.m_xIndices.Get(0));
		const auto& b = mesh.m_xPositions.Get(mesh.m_xIndices.Get(1));
		const auto& c = mesh.m_xPositions.Get(mesh.m_xIndices.Get(2));
		ZENITH_ASSERT_TRUE(glm::cross(c-a,b-a).z > .99f,"engine front face must remain +Z at z=%d mirror=%d",z,mirror);
		ZENITH_ASSERT_EQ(mesh.m_xNormals.Get(0).z,1.f,"baked source normal must agree with facing");
	}
}

ZENITH_TEST(GlbSharedSkin, RemapsArtistJointOrderByName)
{
	GlbSkinFixture xSource(0); Zenith_SkeletonAsset xRig; GlbSkinTestRig(xRig);
	Zenith_MeshAsset xMesh; Zenith_Tools_GlbImport::GlbImportResult xResult;
	ZENITH_ASSERT_TRUE(Zenith_Tools_GlbImport::LoadGlbMeshForSharedSkeleton(xSource.m_xPath.string(), xRig, "game:Test/Shared.zskel", xMesh, xResult), "valid shared skin loads");
	ZENITH_ASSERT_TRUE(xMesh.HasSkinning(), "must retain artist weights");
	if (xMesh.GetNumVerts() != 3 || !xMesh.HasSkinning()) return;
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.Get(0).x, 1u, "glTF joint zero is the engine Tip bone");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.Get(1).x, 0u, "glTF joint one is the engine Root bone");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneWeights.Get(2).x, .25f, "preserves blended weights");
	ZENITH_ASSERT_TRUE(xMesh.GetSkeletonPath() == "game:Test/Shared.zskel", "caller controls the shared rig dependency");
}

ZENITH_TEST(GlbSharedSkin, RefusesCorruptSkinWithoutMutatingOutput)
{
	for (int iFault = 1; iFault <= 8; ++iFault)
	{
		GlbSkinFixture xSource(iFault); Zenith_SkeletonAsset xRig; GlbSkinTestRig(xRig, iFault == 6);
		Zenith_MeshAsset xMesh; xMesh.AddVertex({7,8,9}, {0,1,0}, {0,0});
		Zenith_Tools_GlbImport::GlbImportResult xResult;
		ZENITH_ASSERT_FALSE(Zenith_Tools_GlbImport::LoadGlbMeshForSharedSkeleton(xSource.m_xPath.string(), xRig, "game:Test/Shared.zskel", xMesh, xResult), "invalid fixture %d must fail", iFault);
		ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), 1u, "failure leaves caller geometry untouched");
		ZENITH_ASSERT_EQ(xMesh.m_xPositions.Get(0).x, 7.f, "sentinel position survives");
		ZENITH_ASSERT_FALSE(xMesh.HasSkinning(), "failure cannot leave a partially attached skin");
	}
}

ZENITH_TEST(GlbSharedSkin, ImportsArtistRestProportionsWithNamedTopology)
{
	GlbSkinFixture xSource(9); Zenith_SkeletonAsset xTemplate; GlbSkinTestRig(xTemplate);
	Zenith_SkeletonAsset xRig; Zenith_MeshAsset xMesh; Zenith_Tools_GlbImport::GlbImportResult xResult;
	ZENITH_ASSERT_FALSE(Zenith_Tools_GlbImport::LoadGlbMeshForSharedSkeleton(xSource.m_xPath.string(), xTemplate, "game:Test/Artist.zskel", xMesh, xResult), "the existing shared-bind API must still refuse different pivots");
	ZENITH_ASSERT_TRUE(Zenith_Tools_GlbImport::LoadGlbMeshWithSkeletonTopology(xSource.m_xPath.string(), xTemplate, "game:Test/Artist.zskel", xRig, xMesh, xResult), "explicit artist-rest import should succeed");
	ZENITH_ASSERT_EQ(xRig.GetNumBones(), 2u, "retains named topology");
	if (xRig.GetNumBones() != 2) return;
	ZENITH_ASSERT_EQ(xRig.GetBone(1).m_xBindPosition.y, 2.f, "uses the artist's actual joint");
	ZENITH_ASSERT_EQ(xRig.GetBone(1).m_xInverseBindPose[3].y, -2.f, "inverse bind follows the same joint");
	ZENITH_ASSERT_EQ(xTemplate.GetBone(1).m_xBindPosition.y, 1.f, "never modifies shared template");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.Get(0).x, 1u, "skin is remapped to engine order");
}

ZENITH_TEST(GlbSharedSkin, ArtistRestImportRefusesInconsistentSource)
{
	GlbSkinFixture xSource(2); Zenith_SkeletonAsset xTemplate; GlbSkinTestRig(xTemplate);
	Zenith_SkeletonAsset xRig; GlbSkinTestRig(xRig); Zenith_MeshAsset xMesh;
	xMesh.AddVertex({7,8,9}, {0,1,0}, {0,0}); Zenith_Tools_GlbImport::GlbImportResult xResult;
	ZENITH_ASSERT_FALSE(Zenith_Tools_GlbImport::LoadGlbMeshWithSkeletonTopology(xSource.m_xPath.string(), xTemplate, "game:Test/Artist.zskel", xRig, xMesh, xResult), "a changed inverse bind without matching joint transform is corruption");
	ZENITH_ASSERT_EQ(xRig.GetBone(1).m_xBindPosition.y, 1.f, "failure preserves output rig");
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), 1u, "failure preserves output mesh");
}

ZENITH_TEST(GlbSharedSkin, StaticPublicationCannotOverwriteSkinnedBundle)
{
	GlbSkinFixture xSource(10);
	struct Outputs
	{
		std::vector<std::filesystem::path> m_xPaths;
		~Outputs() { for (const auto& xPath : m_xPaths) { std::error_code ec; std::filesystem::remove(xPath, ec); } }
	} xOutputs;
	for (const char* szExtension : {".zmesh", ".zmodel"})
	{
		auto xPath = xSource.m_xPath; xPath.replace_extension(szExtension);
		ZENITH_ASSERT_FALSE(std::filesystem::exists(xPath), "reserved fixture output must be unused");
		if (std::filesystem::exists(xPath)) return;
		xOutputs.m_xPaths.push_back(xPath);
		std::ofstream xFile(xPath, std::ios::binary); xFile << "authored skin sentinel";
	}
	const auto xResult = Zenith_Tools_GlbImport::ImportGlbFile(xSource.m_xPath.string());
	ZENITH_ASSERT_FALSE(xResult.m_bSuccess, "static exporter must defer a skinned source to its rig owner");
	for (const auto& xPath : xOutputs.m_xPaths)
	{
		std::ifstream xFile(xPath, std::ios::binary);
		const std::string strBytes((std::istreambuf_iterator<char>(xFile)), std::istreambuf_iterator<char>());
		ZENITH_ASSERT_TRUE(strBytes == "authored skin sentinel", "refusal must preserve existing skinned outputs");
	}
}
