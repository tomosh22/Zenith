#include "Zenith.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux.h"

//#TO_TODO: need a platform independent way of handling this
#ifdef ZENITH_VULKAN
#include "Zenith_Vulkan_Pipeline.h"
#include "Flux/Flux_Graphics.h"
#endif

Zenith_DebugVariableTree Zenith_DebugVariables::s_xTree;

template<>
void Zenith_DebugVariableTree::LeafNode<bool>::ImGuiDisplay()
{
	ImGui::Checkbox(m_xName.back().c_str(), m_pData);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector2, float>::ImGuiDisplay()
{
	ImGui::SliderFloat2(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector3, float>::ImGuiDisplay()
{
	ImGui::SliderFloat3(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::Vector4, float>::ImGuiDisplay()
{
	ImGui::SliderFloat4(m_xName.back().c_str(), &m_pData->x, m_xMin, m_xMax);
}
template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<Zenith_Maths::UVector4, float>::ImGuiDisplay()
{
	int iTempX = static_cast<int>(m_pData->x);
	int iTempY = static_cast<int>(m_pData->y);
	int iTempZ = static_cast<int>(m_pData->z);
	int iTempW = static_cast<int>(m_pData->w);
	int aiTemp[4] = { iTempX, iTempY, iTempZ, iTempW };
	ImGui::InputInt4(m_xName.back().c_str(), aiTemp);
	m_pData->x = aiTemp[0];
	m_pData->y = aiTemp[1];
	m_pData->z = aiTemp[2];
	m_pData->w = aiTemp[3];
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<float, float>::ImGuiDisplay()
{
	ImGui::SliderFloat(m_xName.back().c_str(), m_pData, m_xMin, m_xMax, "%.7f");
}

template<>
void Zenith_DebugVariableTree::LeafNode<const Flux_ShaderResourceView>::ImGuiDisplay()
{
	//#TO_TODO: need a platform independent way of handling this
#ifdef ZENITH_VULKAN

	vk::DescriptorSetLayoutBinding xBinding = vk::DescriptorSetLayoutBinding()
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);

	vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(1)
		.setPBindings(&xBinding);

	vk::DescriptorSetLayout xLayout = Zenith_Vulkan::GetDevice().createDescriptorSetLayout(xInfo);

	vk::DescriptorSetAllocateInfo xAllocInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(Zenith_Vulkan::GetCurrentPerFrameDescriptorPool())
		.setDescriptorSetCount(1)
		.setPSetLayouts(&xLayout);

	vk::DescriptorSet xSet = Zenith_Vulkan::GetDevice().allocateDescriptorSets(xAllocInfo)[0];

	vk::DescriptorImageInfo xImageInfo = vk::DescriptorImageInfo()
		.setSampler(Flux_Graphics::s_xRepeatSampler.GetSampler())
		.setImageView(m_pData->m_xImageView)
		.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	vk::WriteDescriptorSet xImageWriteInfo = vk::WriteDescriptorSet()
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDstSet(xSet)
		.setDstBinding(0)
		.setDstArrayElement(0)
		.setDescriptorCount(1)
		.setPImageInfo(&xImageInfo);

	Zenith_Vulkan::GetDevice().updateDescriptorSets(1, &xImageWriteInfo, 0, nullptr);

	
	ImGui::Image(xSet, ImVec2(1024, 1024), { 0, 1 }, { 1, 0 });
#endif
}

template<>
void Zenith_DebugVariableTree::LeafNodeWithRange<uint32_t, uint32_t>::ImGuiDisplay()
{
	ImGui::SliderInt(m_xName.back().c_str(), (int*)(m_pData), m_xMin, m_xMax);
}

template<>
void Zenith_DebugVariableTree::LeafNodeReadOnly<uint32_t>::ImGuiDisplay()
{
	ImGui::Text("%s: %u", m_xName.back().c_str(), *m_pData);
}

void Zenith_DebugVariableTree::PfnLeafNode::ImGuiDisplay()
{
	if (ImGui::Button(m_xName.back().c_str()))
	{
		(*m_pData)();
	}
}

void Zenith_DebugVariableTree::TextNode::ImGuiDisplay()
{
	ImGui::Text(m_strText.c_str());
}

void Zenith_DebugVariableTree::TryAddNode(Node* pxNodeToAdd, Node* pxNode, std::vector<std::string>& xSplits, uint32_t uCurrentDepth, uint32_t uMaxDepth, bool& bSuccess, Node*& pxResult)
{
	Zenith_Assert(uCurrentDepth < xSplits.size() - 1, "Gone too deep");
	if (uCurrentDepth == uMaxDepth)
	{
		for (Node* pxChildNode : pxNode->m_xChildren)
		{
			if (pxChildNode->m_xName[uMaxDepth + 1] == xSplits[uMaxDepth])
			{
				delete pxNodeToAdd;
				pxResult = pxChildNode;
				bSuccess = true;
				return;
			}
		}
		pxNode->m_xChildren.push_back(pxNodeToAdd);
		pxResult = pxNodeToAdd;
		bSuccess = true;
		return;
	}
	for (Node* pxChildNode : pxNode->m_xChildren)
	{
		if (pxChildNode->m_xName[uCurrentDepth + 1] == xSplits[uCurrentDepth])
		{
			TryAddNode(pxNodeToAdd, pxChildNode, xSplits, uCurrentDepth + 1, uMaxDepth, bSuccess, pxResult);
		}
	}
}

void Zenith_DebugVariableTree::AddLeafNode(LeafNodeBase* pxLeafNode, std::vector<std::string>& xSplits)
{
	Node* pxParent = m_pxRoot;
	for (uint32_t u = 0; u < xSplits.size() - 1; u++)
	{
		Node* pxNodeToAdd = new Node;
		pxNodeToAdd->m_xName.push_back(ROOT_NAME);
		for (uint32_t uSub = 0; uSub < u + 1; uSub++)
		{
			pxNodeToAdd->m_xName.push_back(xSplits[uSub]);
		}
		bool bSuccess = false;
		TryAddNode(pxNodeToAdd, m_pxRoot, xSplits, 0, u, bSuccess, pxParent);
	}
	pxParent->m_xLeaves.push_back(pxLeafNode);
}

#endif