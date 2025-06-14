#pragma once
#include "Flux/Flux_Buffers.h"

class Flux_MeshAnimation
{
public:
	class AnimBone
	{
	public:
		AnimBone(const std::string& strName, const struct aiNodeAnim* pxChannel);
		~AnimBone() = default;

        void Update(const float fTimestamp)
        {
            m_xLocalTransform =
                InterpolatePosition(fTimestamp) *
                InterpolateRotation(fTimestamp) *
                InterpolateScaling(fTimestamp);
        }

	private:
        friend class Flux_MeshAnimation;

        uint32_t GetPositionIndex(float animationTime)
        {
            for (uint32_t index = 0; index < m_uNumPositions - 1; ++index)
            {
                if (animationTime < m_xPositions[index + 1].second)
                    return index;
            }
            return 0;
        }

        uint32_t GetRotationIndex(float animationTime)
        {
            for (uint32_t index = 0; index < m_uNumRotations - 1; ++index)
            {
                if (animationTime < m_xRotations[index + 1].second)
                    return index;
            }
            return 0;
        }

        uint32_t GetScaleIndex(float animationTime)
        {
            for (uint32_t index = 0; index < m_uNumScales - 1; ++index)
            {
                if (animationTime < m_xScales[index + 1].second)
                    return index;
            }
            return 0;
        }

        float GetScaleFactor(const float fLastTimeStamp, const float fNextTimeStamp, const float fAnimationTime)
        {
            float fScaleFactor = 0.0f;
            const float fMidWayLength = fAnimationTime - fLastTimeStamp;
            const float fFramesDiff = fNextTimeStamp - fLastTimeStamp;
            fScaleFactor = fMidWayLength / fFramesDiff;
            return fScaleFactor;
        }

        glm::mat4 InterpolatePosition(float animationTime)
        {
            if (m_uNumPositions == 1)
                return glm::translate(glm::mat4(1.0f), m_xPositions[0].first);

            uint32_t p0Index = GetPositionIndex(animationTime);
            uint32_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(m_xPositions[p0Index].second,
                m_xPositions[p1Index].second, animationTime);
            glm::vec3 finalPosition = glm::mix(m_xPositions[p0Index].first,
                m_xPositions[p1Index].first, scaleFactor);
            return glm::translate(glm::mat4(1.0f), finalPosition);
        }

        glm::mat4 InterpolateRotation(float animationTime)
        {
            if (m_uNumRotations == 1)
            {
                auto rotation = glm::normalize(m_xRotations[0].first);
                return glm::toMat4(rotation);
            }

            uint32_t p0Index = GetRotationIndex(animationTime);
            uint32_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(m_xRotations[p0Index].second,
                m_xRotations[p1Index].second, animationTime);
            glm::quat finalRotation = glm::slerp(m_xRotations[p0Index].first,
                m_xRotations[p1Index].first, scaleFactor);
            finalRotation = glm::normalize(finalRotation);
            return glm::toMat4(finalRotation);
        }

        /*figures out which scaling keys to interpolate b/w and performs the interpolation
        and returns the scale matrix*/
        glm::mat4 InterpolateScaling(float animationTime)
        {
            if (m_uNumScales == 1)
                return glm::scale(glm::mat4(1.0f), m_xScales[0].first);

            uint32_t p0Index = GetScaleIndex(animationTime);
            uint32_t p1Index = p0Index + 1;
            float scaleFactor = GetScaleFactor(m_xScales[p0Index].second,
                m_xScales[p1Index].second, animationTime);
            glm::vec3 finalScale = glm::mix(m_xScales[p0Index].first, m_xScales[p1Index].first
                , scaleFactor);
            return glm::scale(glm::mat4(1.0f), finalScale);
        }

		std::vector<std::pair<Zenith_Maths::Vector3, float>> m_xPositions;
		std::vector<std::pair<Zenith_Maths::Quat, float>> m_xRotations;
		std::vector<std::pair<Zenith_Maths::Vector3, float>> m_xScales;

        uint32_t m_uNumPositions = 0;
        uint32_t m_uNumRotations = 0;
        uint32_t m_uNumScales = 0;

		Zenith_Maths::Matrix4 m_xLocalTransform = glm::identity<Zenith_Maths::Matrix4>();
        std::string m_strName;


	};

    struct Node
    {
        Zenith_Maths::Matrix4 m_xTrans = glm::identity<Zenith_Maths::Matrix4>();
        uint32_t m_uChildCount = 0;
        std::vector<Node> m_xChildren;
        std::string m_strName;
    } m_xRootNode;

    void CalculateBoneTransform(const Node* const node, const glm::mat4& parentTransform);

    Flux_MeshAnimation() = delete;
    Flux_MeshAnimation(const std::string& strPath, class Flux_MeshGeometry& xParentGeometry);

    void Update(float fDt)
    {
        m_fCurrentTimestamp += m_uTicksPerSecond * fDt;
        m_fCurrentTimestamp = fmod(m_fCurrentTimestamp, m_fDuration);
        CalculateBoneTransform(&m_xRootNode, glm::mat4(1.0f));

        Flux_MemoryManager::UploadBufferData(m_xBoneBuffer.GetBuffer(), m_axAnimMatrices, sizeof(m_axAnimMatrices));
    }

#ifndef ZENITH_TOOLS
    private:
#endif

        float m_fDuration = 0.0f;
        uint32_t m_uTicksPerSecond = 0;
        std::unordered_map<std::string, AnimBone> m_xBones;
        class Flux_MeshGeometry& m_xParentGeometry;
        float m_fCurrentTimestamp = 0.0f;

        Zenith_Maths::Matrix4 m_axAnimMatrices[100];
        public:
        Flux_DynamicConstantBuffer m_xBoneBuffer;

        
};

