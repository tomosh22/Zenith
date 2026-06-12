//------------------------------------------------------------------------------
// Zenith_PropertyTuning unit tests.
// Included at the bottom of Zenith_PropertyTuning.cpp.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include <chrono>
#include <thread>

namespace
{
	struct TuneTestSubject
	{
		ZENITH_PROPERTIES_BEGIN(TuneTestSubject)
	public:
		ZENITH_PROPERTY(float, m_fDamage, 10.0f)
		ZENITH_PROPERTY_RANGED(float, m_fCooldown, 1.0f, 0.0f, 5.0f)
		ZENITH_PROPERTY(std::string, m_strWeaponName, "sword")
	};

	struct TuneChangeCapture
	{
		u_int m_uCount = 0;
		std::string m_strLastName;
	};

	void OnTuneChanged(void* pxUserData, const char* szPropertyName)
	{
		TuneChangeCapture* pxCapture = static_cast<TuneChangeCapture*>(pxUserData);
		pxCapture->m_uCount++;
		pxCapture->m_strLastName = szPropertyName ? szPropertyName : "";
	}

	std::string MakeTuningTestFilePath(const char* szLeafName)
	{
		std::error_code xEC;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xEC);
		if (xEC)
		{
			xDir = ".";
		}
		return (xDir / szLeafName).string();
	}

	// RAII cleanup so a failing assert mid-test can't leak bindings/files into
	// the next test.
	struct TuningTestScope
	{
		std::string m_strPath;
		explicit TuningTestScope(const char* szLeafName) : m_strPath(MakeTuningTestFilePath(szLeafName))
		{
			std::error_code xEC;
			std::filesystem::remove(m_strPath, xEC);
		}
		~TuningTestScope()
		{
			Zenith_PropertyTuning::UnbindAll();
			std::error_code xEC;
			std::filesystem::remove(m_strPath, xEC);
		}
	};
}

ZENITH_TEST(PropertyTuning, SaveAndApplyAcrossInstances)
{
	TuningTestScope xScope("zenith_tuning_test_roundtrip.ztune");

	// Author: bind (file doesn't exist yet - no apply), tweak values, save.
	TuneTestSubject xAuthor;
	const u_int uAuthorBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xAuthor, TuneTestSubject::GetPropertyTableStatic());
	ZENITH_ASSERT_NE(uAuthorBinding, Zenith_PropertyTuning::uINVALID_BINDING);
	ZENITH_ASSERT_EQ_FLOAT(xAuthor.m_fDamage, 10.0f, 0.0001f);	// missing file leaves defaults

	xAuthor.m_fDamage = 25.0f;
	xAuthor.m_strWeaponName = "axe";
	Zenith_PropertyTuning::SaveBinding(uAuthorBinding);

	// Consumer: binding to the now-existing file applies it immediately and
	// fires the change hook for each differing property.
	TuneTestSubject xConsumer;
	TuneChangeCapture xCapture;
	const u_int uConsumerBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xConsumer, TuneTestSubject::GetPropertyTableStatic(),
		&OnTuneChanged, &xCapture);
	ZENITH_ASSERT_NE(uConsumerBinding, Zenith_PropertyTuning::uINVALID_BINDING);
	ZENITH_ASSERT_EQ_FLOAT(xConsumer.m_fDamage, 25.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xConsumer.m_strWeaponName.c_str(), "axe");
	ZENITH_ASSERT_EQ(xCapture.m_uCount, 2u);	// m_fDamage + m_strWeaponName changed; m_fCooldown identical

	ZENITH_ASSERT_EQ(Zenith_PropertyTuning::GetBindingCount(), 2u);
}

ZENITH_TEST(PropertyTuning, ReapplyPicksUpFileChanges)
{
	TuningTestScope xScope("zenith_tuning_test_reapply.ztune");

	// Write v1 of the file from an author instance.
	TuneTestSubject xAuthor;
	xAuthor.m_fDamage = 30.0f;
	const u_int uAuthorBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xAuthor, TuneTestSubject::GetPropertyTableStatic());
	Zenith_PropertyTuning::SaveBinding(uAuthorBinding);

	// Live consumer bound to the file.
	TuneTestSubject xLive;
	TuneChangeCapture xCapture;
	const u_int uLiveBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xLive, TuneTestSubject::GetPropertyTableStatic(),
		&OnTuneChanged, &xCapture);
	ZENITH_ASSERT_EQ_FLOAT(xLive.m_fDamage, 30.0f, 0.0001f);

	// "Designer edits the file": author tweaks + saves, then the live binding
	// re-applies (ReapplyBinding is exactly what the FileWatcher MODIFIED event
	// drives - tests invoke it directly so they don't depend on watcher state).
	xAuthor.m_fDamage = 60.0f;
	xAuthor.m_fCooldown = 99.0f;	// out of range - the live instance must clamp to 5
	Zenith_PropertyTuning::SaveBinding(uAuthorBinding);

	xCapture.m_uCount = 0;
	Zenith_PropertyTuning::ReapplyBinding(uLiveBinding);
	ZENITH_ASSERT_EQ_FLOAT(xLive.m_fDamage, 60.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xLive.m_fCooldown, 5.0f, 0.0001f);	// clamped by the ranged descriptor
	ZENITH_ASSERT_EQ(xCapture.m_uCount, 2u);

	// Re-applying an unchanged file fires no hooks.
	xCapture.m_uCount = 0;
	Zenith_PropertyTuning::ReapplyBinding(uLiveBinding);
	ZENITH_ASSERT_EQ(xCapture.m_uCount, 0u);
}

ZENITH_TEST(PropertyTuning, RejectsForeignFile)
{
	TuningTestScope xScope("zenith_tuning_test_foreign.ztune");

	// Write a non-tuning file at the path.
	{
		Zenith_DataStream xStream;
		const u_int uGarbage = 0x11223344u;
		xStream << uGarbage;
		xStream << uGarbage;
		xStream.WriteToFile(xScope.m_strPath.c_str());
	}

	TuneTestSubject xSubject;
	const u_int uBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xSubject, TuneTestSubject::GetPropertyTableStatic());
	ZENITH_ASSERT_NE(uBinding, Zenith_PropertyTuning::uINVALID_BINDING);

	// Bad magic - values must be untouched.
	ZENITH_ASSERT_EQ_FLOAT(xSubject.m_fDamage, 10.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xSubject.m_strWeaponName.c_str(), "sword");
}

#ifdef ZENITH_TOOLS
ZENITH_TEST(PropertyTuning, WatcherDrivenLiveReload)
{
	TuningTestScope xScope("zenith_tuning_test_watch.ztune");

	// Author binding writes v1 of the file.
	TuneTestSubject xAuthor;
	const u_int uAuthorBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xAuthor, TuneTestSubject::GetPropertyTableStatic());
	ZENITH_ASSERT_NE(uAuthorBinding, Zenith_PropertyTuning::uINVALID_BINDING);
	Zenith_PropertyTuning::SaveBinding(uAuthorBinding);

	// Live consumer bound to the same file.
	TuneTestSubject xLive;
	const u_int uLiveBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xLive, TuneTestSubject::GetPropertyTableStatic());
	ZENITH_ASSERT_NE(uLiveBinding, Zenith_PropertyTuning::uINVALID_BINDING);

	// Author tweaks and saves; the directory watcher must deliver the change to
	// the live binding via Update() pumping - the same path Zenith_MainLoop
	// drives every frame. Generous timeout: OS notification latency.
	xAuthor.m_fDamage = 333.0f;
	Zenith_PropertyTuning::SaveBinding(uAuthorBinding);

	bool bApplied = false;
	for (u_int u = 0; u < 300; ++u)
	{
		Zenith_PropertyTuning::Update();
		if (xLive.m_fDamage == 333.0f)
		{
			bApplied = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ZENITH_ASSERT_TRUE(bApplied, "watcher-driven tuning reload did not arrive within ~3s");
}
#endif // ZENITH_TOOLS

ZENITH_TEST(PropertyTuning, UnbindStopsTracking)
{
	TuningTestScope xScope("zenith_tuning_test_unbind.ztune");

	TuneTestSubject xSubject;
	const u_int uBinding = Zenith_PropertyTuning::BindFile(xScope.m_strPath.c_str(), &xSubject, TuneTestSubject::GetPropertyTableStatic());
	ZENITH_ASSERT_EQ(Zenith_PropertyTuning::GetBindingCount(), 1u);

	Zenith_PropertyTuning::Unbind(uBinding);
	ZENITH_ASSERT_EQ(Zenith_PropertyTuning::GetBindingCount(), 0u);

	// Operations on a dead handle are safe no-ops.
	Zenith_PropertyTuning::SaveBinding(uBinding);
	Zenith_PropertyTuning::ReapplyBinding(uBinding);
}

#endif // ZENITH_TESTING
