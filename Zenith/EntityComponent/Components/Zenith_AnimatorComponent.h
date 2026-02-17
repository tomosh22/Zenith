#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"

class Zenith_ModelComponent;

class Zenith_AnimatorComponent
{
public:
	Zenith_AnimatorComponent(Zenith_Entity& xEntity);
	~Zenith_AnimatorComponent();

	// Move semantics (required for component pools)
	Zenith_AnimatorComponent(Zenith_AnimatorComponent&& xOther) noexcept;
	Zenith_AnimatorComponent& operator=(Zenith_AnimatorComponent&& xOther) noexcept;

	// No copy
	Zenith_AnimatorComponent(const Zenith_AnimatorComponent&) = delete;
	Zenith_AnimatorComponent& operator=(const Zenith_AnimatorComponent&) = delete;

	// ========== ECS Lifecycle (auto-called via ComponentMeta) ==========
	void OnStart();              // Auto-discovers skeleton from ModelComponent
	void OnUpdate(float fDt);    // Evaluates state machine, updates pose, uploads GPU
	void OnDestroy();

	// ========== Controller Access ==========
	Flux_AnimationController& GetController() { return m_xController; }
	const Flux_AnimationController& GetController() const { return m_xController; }

	// ========== State Machine Parameter Shortcuts ==========
	void SetFloat(const std::string& strName, float fValue);
	void SetInt(const std::string& strName, int32_t iValue);
	void SetBool(const std::string& strName, bool bValue);
	void SetTrigger(const std::string& strName);
	float GetFloat(const std::string& strName) const;
	int32_t GetInt(const std::string& strName) const;
	bool GetBool(const std::string& strName) const;

	// ========== Convenience ==========
#ifdef ZENITH_TOOLS
	void PlayAnimation(const std::string& strClipName, float fBlendTime = 0.15f);
#endif
	void CrossFade(const std::string& strStateName, float fDuration = 0.15f);
	void Stop();
	void SetPaused(bool bPaused);
	bool IsPaused() const;
	void SetPlaybackSpeed(float fSpeed);
	float GetPlaybackSpeed() const;

	// ========== Clip Management ==========
	Flux_AnimationClip* AddClipFromFile(const std::string& strPath);
	Flux_AnimationClip* GetClip(const std::string& strName);

	// ========== State Machine ==========
	Flux_AnimationStateMachine& GetStateMachine();
	Flux_AnimationStateMachine* CreateStateMachine(const std::string& strName = "Default");
	bool HasStateMachine() const;

	// ========== State Info Query ==========
	Flux_AnimatorStateInfo GetCurrentAnimatorStateInfo() const;

	// ========== IK ==========
	void SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPos, float fWeight = 1.0f);
	void ClearIKTarget(const std::string& strChainName);

	// ========== Update Mode ==========
	void SetUpdateMode(Flux_AnimationUpdateMode eMode);
	Flux_AnimationUpdateMode GetUpdateMode() const;

	// ========== Initialization State ==========
	bool IsInitialized() const;

	// ========== Serialization ==========
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	void TryDiscoverSkeleton();
	void UpdateWorldMatrix();
	void SyncModelInstanceAnimation();

	Zenith_Entity m_xParentEntity;
	Flux_AnimationController m_xController;
	Zenith_ModelComponent* m_pxCachedModelComponent = nullptr;
	uint32_t m_uDiscoveryRetryCount = 0;
};
