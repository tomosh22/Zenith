#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "DataStream/Zenith_DataStream.h"

// ========== Zenith_BTNode ==========

void Zenith_BTNode::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write node name
	uint32_t uNameLen = static_cast<uint32_t>(m_strNodeName.length());
	xStream << uNameLen;
	if (uNameLen > 0)
	{
		xStream.Write(m_strNodeName.data(), uNameLen);
	}
}

void Zenith_BTNode::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read node name
	uint32_t uNameLen = 0;
	xStream >> uNameLen;
	if (uNameLen > 0)
	{
		m_strNodeName.resize(uNameLen);
		xStream.Read(m_strNodeName.data(), uNameLen);
	}
	else
	{
		m_strNodeName.clear();
	}
}

// ========== Zenith_BTComposite ==========

Zenith_BTComposite::~Zenith_BTComposite()
{
	// Delete all children (we own them)
	for (uint32_t u = 0; u < m_axChildren.GetSize(); ++u)
	{
		if (m_axChildren.Get(u) != nullptr)
		{
			m_axChildren.Get(u)->SetParent(nullptr);
			delete m_axChildren.Get(u);
		}
	}
	m_axChildren.Clear();
}

void Zenith_BTComposite::AddChild(Zenith_BTNode* pxChild)
{
	Zenith_Assert(pxChild != nullptr, "Cannot add null child to composite node");
	Zenith_Assert(!pxChild->HasParent(), "Node already has a parent! Each BT node can only belong to one parent. "
		"This would cause double-delete. Node name: %s", pxChild->GetNodeName().c_str());

	pxChild->SetParent(this);
	m_axChildren.PushBack(pxChild);
}

Zenith_BTNode* Zenith_BTComposite::GetChild(uint32_t uIndex) const
{
	Zenith_Assert(uIndex < m_axChildren.GetSize(), "Child index %u out of bounds (size: %u)", uIndex, m_axChildren.GetSize());
	return m_axChildren.Get(uIndex);
}

void Zenith_BTComposite::OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	// Reset child index when entering this node
	m_uCurrentChild = 0;
}

void Zenith_BTComposite::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write base class data
	Zenith_BTNode::WriteToDataStream(xStream);

	// Write child count
	uint32_t uChildCount = m_axChildren.GetSize();
	xStream << uChildCount;

	// Write each child (type name + data)
	for (uint32_t u = 0; u < uChildCount; ++u)
	{
		const Zenith_BTNode* pxChild = m_axChildren.Get(u);

		// Write type name
		const char* szTypeName = pxChild->GetTypeName();
		uint32_t uTypeLen = static_cast<uint32_t>(strlen(szTypeName));
		xStream << uTypeLen;
		xStream.Write(szTypeName, uTypeLen);

		// Write child data
		pxChild->WriteToDataStream(xStream);
	}
}

void Zenith_BTComposite::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read base class data
	Zenith_BTNode::ReadFromDataStream(xStream);

	// Note: Child deserialization requires a node factory
	// This will be handled by Zenith_BTSerializer which knows how to create nodes by type
	// For now, just read the count and skip - actual deserialization done externally
	uint32_t uChildCount = 0;
	xStream >> uChildCount;

	// Children will be populated by the serializer after creation
}

// ========== Zenith_BTDecorator ==========

Zenith_BTDecorator::~Zenith_BTDecorator()
{
	// Delete child (we own it)
	if (m_pxChild != nullptr)
	{
		delete m_pxChild;
		m_pxChild = nullptr;
	}
}

void Zenith_BTDecorator::SetChild(Zenith_BTNode* pxChild)
{
	// Delete existing child if any
	if (m_pxChild != nullptr)
	{
		m_pxChild->SetParent(nullptr);
		delete m_pxChild;
	}

	if (pxChild != nullptr)
	{
		Zenith_Assert(!pxChild->HasParent(), "Node already has a parent! Each BT node can only belong to one parent. "
			"This would cause double-delete. Node name: %s", pxChild->GetNodeName().c_str());
		pxChild->SetParent(this);
	}
	m_pxChild = pxChild;
}

void Zenith_BTDecorator::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write base class data
	Zenith_BTNode::WriteToDataStream(xStream);

	// Write whether we have a child
	bool bHasChild = (m_pxChild != nullptr);
	xStream << bHasChild;

	if (bHasChild)
	{
		// Write type name
		const char* szTypeName = m_pxChild->GetTypeName();
		uint32_t uTypeLen = static_cast<uint32_t>(strlen(szTypeName));
		xStream << uTypeLen;
		xStream.Write(szTypeName, uTypeLen);

		// Write child data
		m_pxChild->WriteToDataStream(xStream);
	}
}

void Zenith_BTDecorator::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read base class data
	Zenith_BTNode::ReadFromDataStream(xStream);

	// Read whether we have a child
	bool bHasChild = false;
	xStream >> bHasChild;

	// Child will be populated by the serializer after creation
}
