#include "Zenith.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"

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
	xStream << m_strName;

	const bool bHasRoot = (m_pxRootNode != nullptr);
	xStream << bHasRoot;

	if (bHasRoot)
	{
		// Root type name still written as length-prefixed raw C string because
		// GetTypeName() returns const char* — wrapping in std::string would
		// allocate per serialize. Keep the manual write.
		const char* szTypeName = m_pxRootNode->GetTypeName();
		const uint32_t uTypeLen = static_cast<uint32_t>(strlen(szTypeName));
		xStream << uTypeLen;
		xStream.Write(szTypeName, uTypeLen);

		m_pxRootNode->WriteToDataStream(xStream);
	}
}

void Zenith_BehaviorTree::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxRootNode != nullptr)
	{
		delete m_pxRootNode;
		m_pxRootNode = nullptr;
	}

	xStream >> m_strName;

	bool bHasRoot = false;
	xStream >> bHasRoot;

	if (bHasRoot)
	{
		uint32_t uTypeLen = 0;
		xStream >> uTypeLen;
		std::string strTypeName(uTypeLen, '\0');
		xStream.Read(strTypeName.data(), uTypeLen);

		// Note: Node creation requires a factory/registry — actual deserialization
		// is handled by Zenith_BTSerializer which knows how to create nodes by
		// type name.
	}

	m_bFirstTick = true;
}
