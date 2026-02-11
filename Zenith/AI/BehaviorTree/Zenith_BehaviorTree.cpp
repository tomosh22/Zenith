#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

Zenith_BehaviorTree::~Zenith_BehaviorTree()
{
	if (m_pxRootNode != nullptr)
	{
		delete m_pxRootNode;
		m_pxRootNode = nullptr;
	}
}

Zenith_BehaviorTree::Zenith_BehaviorTree(Zenith_BehaviorTree&& xOther) noexcept
	: m_pxRootNode(xOther.m_pxRootNode)
	, m_eLastStatus(xOther.m_eLastStatus)
	, m_strName(std::move(xOther.m_strName))
	, m_szCurrentNodeName(xOther.m_szCurrentNodeName)
	, m_bFirstTick(xOther.m_bFirstTick)
{
	xOther.m_pxRootNode = nullptr;
	xOther.m_szCurrentNodeName = "";
}

Zenith_BehaviorTree& Zenith_BehaviorTree::operator=(Zenith_BehaviorTree&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Delete existing root
		if (m_pxRootNode != nullptr)
		{
			delete m_pxRootNode;
		}

		m_pxRootNode = xOther.m_pxRootNode;
		m_eLastStatus = xOther.m_eLastStatus;
		m_strName = std::move(xOther.m_strName);
		m_szCurrentNodeName = xOther.m_szCurrentNodeName;
		m_bFirstTick = xOther.m_bFirstTick;

		xOther.m_pxRootNode = nullptr;
		xOther.m_szCurrentNodeName = "";
	}
	return *this;
}

void Zenith_BehaviorTree::SetRootNode(Zenith_BTNode* pxRoot)
{
	// Delete existing root if any
	if (m_pxRootNode != nullptr)
	{
		delete m_pxRootNode;
	}
	m_pxRootNode = pxRoot;
	m_bFirstTick = true;
}

BTNodeStatus Zenith_BehaviorTree::Tick(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt)
{
	if (m_pxRootNode == nullptr)
	{
		m_eLastStatus = BTNodeStatus::FAILURE;
		m_szCurrentNodeName = "";
		return m_eLastStatus;
	}

	// On first tick or after tree resets, call OnEnter
	if (m_bFirstTick)
	{
		m_pxRootNode->OnEnter(xAgent, xBlackboard);
		m_bFirstTick = false;
	}

	// Execute the tree
	m_eLastStatus = m_pxRootNode->Execute(xAgent, xBlackboard, fDt);

	// Update debug info
	m_szCurrentNodeName = m_pxRootNode->GetNodeName().c_str();

	// If tree completed, prepare for next execution
	if (m_eLastStatus != BTNodeStatus::RUNNING)
	{
		m_pxRootNode->OnExit(xAgent, xBlackboard);
		m_bFirstTick = true;
	}

	return m_eLastStatus;
}

void Zenith_BehaviorTree::Reset(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	if (m_pxRootNode != nullptr && !m_bFirstTick)
	{
		// If we were in the middle of execution, abort
		if (m_eLastStatus == BTNodeStatus::RUNNING)
		{
			m_pxRootNode->OnAbort(xAgent, xBlackboard);
		}
	}
	m_bFirstTick = true;
	m_eLastStatus = BTNodeStatus::FAILURE;
	m_szCurrentNodeName = "";
}

void Zenith_BehaviorTree::Abort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard)
{
	if (m_pxRootNode != nullptr && m_eLastStatus == BTNodeStatus::RUNNING)
	{
		m_pxRootNode->OnAbort(xAgent, xBlackboard);
	}
	m_bFirstTick = true;
	m_eLastStatus = BTNodeStatus::FAILURE;
}

void Zenith_BehaviorTree::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write tree name
	uint32_t uNameLen = static_cast<uint32_t>(m_strName.length());
	xStream << uNameLen;
	if (uNameLen > 0)
	{
		xStream.Write(m_strName.data(), uNameLen);
	}

	// Write whether we have a root
	bool bHasRoot = (m_pxRootNode != nullptr);
	xStream << bHasRoot;

	if (bHasRoot)
	{
		// Write root type name
		const char* szTypeName = m_pxRootNode->GetTypeName();
		uint32_t uTypeLen = static_cast<uint32_t>(strlen(szTypeName));
		xStream << uTypeLen;
		xStream.Write(szTypeName, uTypeLen);

		// Write root data
		m_pxRootNode->WriteToDataStream(xStream);
	}
}

void Zenith_BehaviorTree::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Delete existing root
	if (m_pxRootNode != nullptr)
	{
		delete m_pxRootNode;
		m_pxRootNode = nullptr;
	}

	// Read tree name
	uint32_t uNameLen = 0;
	xStream >> uNameLen;
	if (uNameLen > 0)
	{
		m_strName.resize(uNameLen);
		xStream.Read(m_strName.data(), uNameLen);
	}
	else
	{
		m_strName.clear();
	}

	// Read whether we have a root
	bool bHasRoot = false;
	xStream >> bHasRoot;

	if (bHasRoot)
	{
		// Read root type name
		uint32_t uTypeLen = 0;
		xStream >> uTypeLen;
		std::string strTypeName(uTypeLen, '\0');
		xStream.Read(strTypeName.data(), uTypeLen);

		// Note: Node creation requires a factory/registry
		// For now, tree deserialization is handled by Zenith_BTSerializer
		// which knows how to create nodes by type name
	}

	m_bFirstTick = true;
}

Zenith_BehaviorTree* Zenith_BehaviorTree::LoadFromFile(const std::string& strPath)
{
	// BT file serialization is not implemented yet.
	// Behavior trees must be created in code using the builder pattern.
	// Example:
	//   auto* pxTree = new Zenith_BehaviorTree();
	//   auto* pxRoot = new Zenith_BTSequence();
	//   pxRoot->AddChild(new Zenith_BTAction_Wait(1.0f));
	//   pxTree->SetRootNode(pxRoot);
	//
	// To use this feature, a Zenith_BTNodeFactory must be implemented to create
	// nodes by type name from serialized data.
	Zenith_Log(LOG_CATEGORY_AI, "WARNING: BehaviorTree::LoadFromFile not implemented. "
		"Behavior trees must be created in code. Path: %s", strPath.c_str());
	return nullptr;
}

bool Zenith_BehaviorTree::SaveToFile(const Zenith_BehaviorTree&, const std::string& strPath)
{
	// BT file serialization is not implemented yet.
	// WriteToDataStream can serialize tree structure, but loading requires a node factory.
	Zenith_Log(LOG_CATEGORY_AI, "WARNING: BehaviorTree::SaveToFile not implemented. Path: %s", strPath.c_str());
	return false;
}
