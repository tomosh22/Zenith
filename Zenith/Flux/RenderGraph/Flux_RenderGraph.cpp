#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"

void Flux_RenderGraph::AssertMutable(const char* szFn)
{
    Zenith_Assert(!m_bCompiled || m_bDirty, "Flux_RenderGraph::%s called after Compile() without MarkDirty()", szFn);
}

Flux_RenderGraph_Pass* Flux_RenderGraph::GetPass(u_int uIdx) const
{
    Zenith_Assert(uIdx < m_xPasses.GetSize(), "Flux_RenderGraph: pass index %u out of bounds", uIdx);
    return m_xPasses.Get(uIdx);
}

void Flux_RenderGraph::TrackResource(void* pRes, const char* szName)
{
    if (m_xResources.find(pRes) == m_xResources.end())
    {
        Flux_RenderGraph_Resource xRes;
        xRes.m_xResource = Flux_GraphResource();
        xRes.m_szName = szName;
        xRes.m_uFirstWrite = UINT32_MAX;
        xRes.m_uLastRead = UINT32_MAX;
        m_xResources[pRes] = xRes;
    }
}

u_int Flux_RenderGraph::AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData, u_int uInitialCapacity)
{
    AssertMutable("AddPass");
    Zenith_Assert(szName && pfnOnRecord, "Flux_RenderGraph::AddPass: null name or callback");
    Flux_RenderGraph_Pass* pxPass = new Flux_RenderGraph_Pass();
    pxPass->m_szName = szName;
    pxPass->m_pfnOnRecord = pfnOnRecord;
    pxPass->m_pUserData = pUserData;
    pxPass->m_pxCommandList = new Flux_CommandList(szName, uInitialCapacity);
    m_xPasses.PushBack(pxPass);
    return m_xPasses.GetSize() - 1;
}

void Flux_RenderGraph::AddResourceUsage(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount, bool bWrite)
{
    AssertMutable(bWrite ? "Write" : "Read");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: invalid pass index %u", uPassIndex);
    Zenith_Assert(uMipCount > 0, "Flux_RenderGraph: mip count must be > 0");
    TrackResource(&xImage, "<image>");
    Flux_RenderGraph_ResourceUsage xUsage;
    xUsage.m_xResource = Flux_GraphResource(xImage);
    xUsage.m_eAccess = eAccess;
    xUsage.m_uMipLevel = uMip;
    xUsage.m_uMipCount = uMipCount;
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (bWrite) pxPass->m_xWrites.PushBack(xUsage);
    else pxPass->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::AddBufferUsage(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess, bool bWrite)
{
    AssertMutable(bWrite ? "WriteBuffer" : "ReadBuffer");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: invalid pass index %u", uPassIndex);
    TrackResource(&xBuffer, "<buffer>");
    Flux_RenderGraph_ResourceUsage xUsage;
    xUsage.m_xResource = Flux_GraphResource(xBuffer);
    xUsage.m_eAccess = eAccess;
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (bWrite) pxPass->m_xWrites.PushBack(xUsage);
    else pxPass->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::Read(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AddResourceUsage(uPassIndex, xImage, eAccess, uMip, uMipCount, false);
}

void Flux_RenderGraph::Write(u_int uPassIndex, Flux_RenderAttachment& xImage, ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
    AddResourceUsage(uPassIndex, xImage, eAccess, uMip, uMipCount, true);
}

void Flux_RenderGraph::ReadBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AddBufferUsage(uPassIndex, xBuffer, eAccess, false);
}

void Flux_RenderGraph::WriteBuffer(u_int uPassIndex, Flux_Buffer& xBuffer, ResourceAccess eAccess)
{
    AddBufferUsage(uPassIndex, xBuffer, eAccess, true);
}

void Flux_RenderGraph::DependsOn(u_int uDependentPass, u_int uDependencyPass)
{
    AssertMutable("DependsOn");
    Zenith_Assert(uDependentPass < m_xPasses.GetSize() && uDependencyPass < m_xPasses.GetSize(), "Flux_RenderGraph::DependsOn: invalid pass index");
    Zenith_Assert(uDependentPass != uDependencyPass, "Flux_RenderGraph::DependsOn: self-dependency on pass %u", uDependentPass);
    GetPass(uDependentPass)->m_xExplicitDependencies.PushBack(uDependencyPass);
}

void Flux_RenderGraph::SetEnabled(u_int uPassIndex, bool bEnabled)
{
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph::SetEnabled: invalid pass index %u", uPassIndex);
    Flux_RenderGraph_Pass* pxPass = GetPass(uPassIndex);
    if (pxPass->m_bEnabled != bEnabled) { pxPass->m_bEnabled = bEnabled; m_bEnabledMaskDirty = true; }
}

void Flux_RenderGraph::SetTargetSetup(u_int uPassIndex, const Flux_TargetSetup& xTargetSetup)
{
    AssertMutable("SetTargetSetup");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: SetTargetSetup: invalid pass index %u", uPassIndex);
    GetPass(uPassIndex)->m_pxTargetSetup = &xTargetSetup;
}

void Flux_RenderGraph::SetPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare)
{
    AssertMutable("SetPrepare");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: SetPrepare: invalid pass index %u", uPassIndex);
    GetPass(uPassIndex)->m_pfnOnPrepare = pfnOnPrepare;
}

void Flux_RenderGraph::SetClear(u_int uPassIndex, bool bClearTargets)
{
    AssertMutable("SetClear");
    Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "Flux_RenderGraph: SetClear: invalid pass index %u", uPassIndex);
    GetPass(uPassIndex)->m_bRequestsClear = bClearTargets;
}

void Flux_RenderGraph::MarkDirty() { m_bDirty = true; }

void Flux_RenderGraph::Clear()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) delete m_xPasses.Get(i);
    m_xPasses.Clear();
    m_xResources.clear();
    m_xTraffic.clear();
    m_xBarrierState.clear();
    m_xSetupNeedsClear.clear();
    m_xSetupClearAssigned.clear();
    m_xEdgeSet.clear();
    m_xAdjacency.Clear();
    m_xInDegree.Clear();
    m_xExecutionOrder.Clear();
    m_bCompiled = false;
    m_bDirty = true;
    m_bEnabledMaskDirty = false;
}

void Flux_RenderGraph::BuildResourceTraffic()
{
    m_xTraffic.clear();
    for (u_int uPass = 0; uPass < m_xPasses.GetSize(); uPass++)
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPass);
        if (!pxPass->m_bEnabled) continue;
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null write", pxPass->m_szName);
            m_xTraffic[pRes].m_xWriters.PushBack(uPass);
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            Zenith_Assert(pRes, "Flux_RenderGraph::BuildResourceTraffic: pass '%s' has null read", pxPass->m_szName);
            m_xTraffic[pRes].m_xReaders.PushBack(uPass);
        }
    }
}

void Flux_RenderGraph::Validate() const
{
    for (auto& xPair : m_xTraffic)
    {
        if (xPair.second.m_xReaders.GetSize() > 0 && xPair.second.m_xWriters.GetSize() == 0)
        {
            auto it = m_xResources.find(xPair.first);
            const char* sz = (it != m_xResources.end()) ? it->second.m_szName : "<unknown>";
            Zenith_Assert(false, "Flux_RenderGraph: resource '%s' read but never written", sz);
        }
    }
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        bool bIsGfx = pxP->m_pxTargetSetup && *pxP->m_pxTargetSetup != Flux_Graphics::s_xNullTargetSetup;
        if (bIsGfx) Zenith_Assert(pxP->m_xWrites.GetSize() > 0, "Flux_RenderGraph: graphics pass '%s' has no Write()", pxP->m_szName);
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            Zenith_Assert(d < m_xPasses.GetSize(), "Flux_RenderGraph: pass '%s' invalid dep %u", pxP->m_szName, d);
        }
        if (pxP->m_xWrites.GetSize() > 0) Zenith_Assert(pxP->m_pfnOnRecord, "Flux_RenderGraph: pass '%s' has writes but no callback", pxP->m_szName);
    }
}

void Flux_RenderGraph::InitAdjacencyData()
{
    const u_int uN = m_xPasses.GetSize();
    m_xAdjacency.Clear(); m_xAdjacency.Reserve(uN);
    m_xInDegree.Clear(); m_xInDegree.Reserve(uN);
    for (u_int i = 0; i < uN; i++) { m_xAdjacency.EmplaceBack(); m_xInDegree.PushBack(0); }
    m_xEdgeSet.clear();
}

void Flux_RenderGraph::BuildAdjacencyFromTraffic()
{
    for (auto& xP : m_xTraffic)
    {
        const Zenith_Vector<u_int>& xWriters = xP.second.m_xWriters;
        const Zenith_Vector<u_int>& xReaders = xP.second.m_xReaders;
        AddReaderWriterEdges(xReaders, xWriters);
        AddWriterChainEdges(xWriters);
    }
}

void Flux_RenderGraph::AddReaderWriterEdges(const Zenith_Vector<u_int>& axReaders, const Zenith_Vector<u_int>& axWriters)
{
    for (Zenith_Vector<u_int>::Iterator itR(axReaders); !itR.Done(); itR.Next())
    {
        u_int uReader = itR.GetData();
        if (IsAlsoWriter(uReader, axWriters)) continue;
        u_int uBestWriter = FindBestWriter(uReader, axWriters);
        if (uBestWriter != UINT32_MAX) AddEdgeIfNew(uBestWriter, uReader);
    }
}

bool Flux_RenderGraph::IsAlsoWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters)
{
    for (Zenith_Vector<u_int>::Iterator itW(axWriters); !itW.Done(); itW.Next())
        if (itW.GetData() == uReader) return true;
    return false;
}

u_int Flux_RenderGraph::FindBestWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters)
{
    u_int uBestWriter = UINT32_MAX;
    for (Zenith_Vector<u_int>::Iterator itW(axWriters); !itW.Done(); itW.Next())
    {
        u_int uWriter = itW.GetData();
        if (uWriter >= uReader) break;
        uBestWriter = uWriter;
    }
    return uBestWriter;
}

void Flux_RenderGraph::AddWriterChainEdges(const Zenith_Vector<u_int>& axWriters)
{
    for (u_int w = 0; w + 1 < axWriters.GetSize(); w++)
    {
        u_int64 ulKey = (static_cast<u_int64>(axWriters.Get(w)) << 32) | axWriters.Get(w + 1);
        if (m_xEdgeSet.find(ulKey) == m_xEdgeSet.end())
        {
            m_xEdgeSet.insert(ulKey);
            m_xAdjacency.Get(axWriters.Get(w)).PushBack(axWriters.Get(w + 1));
            m_xInDegree.Get(axWriters.Get(w + 1))++;
        }
    }
}

void Flux_RenderGraph::AddEdgeIfNew(u_int uFrom, u_int uTo)
{
    u_int64 ulKey = (static_cast<u_int64>(uFrom) << 32) | uTo;
    if (m_xEdgeSet.find(ulKey) == m_xEdgeSet.end())
    {
        m_xEdgeSet.insert(ulKey);
        m_xAdjacency.Get(uFrom).PushBack(uTo);
        m_xInDegree.Get(uTo)++;
    }
}

void Flux_RenderGraph::AddExplicitDependencies()
{
    const u_int uN = m_xPasses.GetSize();
    for (u_int i = 0; i < uN; i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        if (!pxP->m_bEnabled) continue;
        for (Zenith_Vector<u_int>::Iterator it(pxP->m_xExplicitDependencies); !it.Done(); it.Next())
        {
            u_int d = it.GetData();
            if (m_xPasses.Get(d)->m_bEnabled) AddEdgeIfNew(d, i);
        }
    }
}

bool Flux_RenderGraph::TopologicalSort()
{
    InitAdjacencyData();
    BuildAdjacencyFromTraffic();
    AddExplicitDependencies();
    
    m_xExecutionOrder.Clear();
    Zenith_Vector<u_int> xQueue;
    const u_int uN = m_xPasses.GetSize();
    
    for (u_int i = 0; i < uN; i++)
        if (m_xPasses.Get(i)->m_bEnabled && m_xInDegree.Get(i) == 0)
            xQueue.PushBack(i);
    
    u_int uProcessed = 0;
    for (u_int qf = 0; qf < xQueue.GetSize(); qf++)
    {
        u_int uCurrent = xQueue.Get(qf);
        uProcessed++;
        m_xExecutionOrder.PushBack(uCurrent);
        m_xPasses.Get(uCurrent)->m_uTopologicalOrder = m_xExecutionOrder.GetSize() - 1;
        for (Zenith_Vector<u_int>::Iterator it(m_xAdjacency.Get(uCurrent)); !it.Done(); it.Next())
        {
            u_int uNeighbor = it.GetData();
            if (--m_xInDegree.Get(uNeighbor) == 0)
                xQueue.PushBack(uNeighbor);
        }
    }
    
    u_int uEnabledCount = 0;
    for (u_int i = 0; i < uN; i++)
        if (m_xPasses.Get(i)->m_bEnabled) uEnabledCount++;
    
    if (uProcessed != uEnabledCount)
    {
        Zenith_Assert(false, "Flux_RenderGraph: Cycle detected!");
        return false;
    }
    
    return true;
}

void Flux_RenderGraph::ComputeResourceLifetimes()
{
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xWrite = it.GetData();
            void* pRes = xWrite.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (uPassIdx < itRes->second.m_uFirstWrite) itRes->second.m_uFirstWrite = uPassIdx;
        }
        for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
        {
            const Flux_RenderGraph_ResourceUsage& xRead = it.GetData();
            void* pRes = xRead.m_xResource.GetVoidPtr();
            auto itRes = m_xResources.find(pRes);
            Zenith_Assert(itRes != m_xResources.end(), "Flux_RenderGraph::ComputeResourceLifetimes: resource not tracked");
            if (itRes->second.m_uLastRead == UINT32_MAX || uPassIdx > itRes->second.m_uLastRead)
                itRes->second.m_uLastRead = uPassIdx;
        }
    }
}

void Flux_RenderGraph::GenerateBarriers()
{
    m_xBarrierState.clear();
    for (Zenith_Vector<u_int>::Iterator itE(m_xExecutionOrder); !itE.Done(); itE.Next())
    {
        u_int uPassIdx = itE.GetData();
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(uPassIdx);
        pxPass->m_xPrologueBarriers.Clear();
        pxPass->m_xEpilogueBarriers.Clear();
        GenerateImageBarriers(*pxPass, pxPass->m_xReads, true);
        GenerateImageBarriers(*pxPass, pxPass->m_xWrites, false);
    }
}

void Flux_RenderGraph::GenerateImageBarriers(Flux_RenderGraph_Pass& rxPass, const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bIsRead)
{
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxUsages); !it.Done(); it.Next())
    {
        const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
        if (rxUsage.m_xResource.GetKind() != Flux_GraphResourceKind::Image) continue;
        Flux_RenderAttachment* pxImg = rxUsage.m_xResource.AsImage();
        auto itState = m_xBarrierState.find(pxImg);
        bool bFirst = itState == m_xBarrierState.end();
        ResourceAccess ePrev = bFirst ? RESOURCE_ACCESS_UNDEFINED : itState->second;
        if (bFirst || ePrev != rxUsage.m_eAccess)
        {
            Flux_RenderGraph_ImageBarrier xB;
            xB.m_pxAttachment = pxImg;
            xB.m_ePrevAccess = ePrev;
            xB.m_eNewAccess = rxUsage.m_eAccess;
            xB.m_bDiscard = bFirst || (!bIsRead && rxPass.m_bClearTargets);
            rxPass.m_xPrologueBarriers.PushBack(xB);
        }
        m_xBarrierState[pxImg] = rxUsage.m_eAccess;
    }
}

void Flux_RenderGraph::ResolveClearFlags()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++) m_xPasses.Get(i)->m_bClearTargets = false;
    CollectClearRequirements();
    AssignClearFlags();
}

void Flux_RenderGraph::CollectClearRequirements()
{
    m_xSetupNeedsClear.clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled || !pxPass->m_pxTargetSetup) continue;
        auto itSetup = m_xSetupNeedsClear.find(pxPass->m_pxTargetSetup);
        if (itSetup == m_xSetupNeedsClear.end()) m_xSetupNeedsClear[pxPass->m_pxTargetSetup] = pxPass->m_bRequestsClear;
        else itSetup->second = itSetup->second || pxPass->m_bRequestsClear;
    }
}

void Flux_RenderGraph::AssignClearFlags()
{
    m_xSetupClearAssigned.clear();
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bIsCompute || !pxPass->m_bEnabled || !pxPass->m_pxTargetSetup) continue;
        auto itNeeds = m_xSetupNeedsClear.find(pxPass->m_pxTargetSetup);
        if (itNeeds == m_xSetupNeedsClear.end() || !itNeeds->second) continue;
        if (m_xSetupClearAssigned.find(pxPass->m_pxTargetSetup) == m_xSetupClearAssigned.end())
        { pxPass->m_bClearTargets = true; m_xSetupClearAssigned.insert(pxPass->m_pxTargetSetup); }
    }
}

bool Flux_RenderGraph::Compile()
{
    if (!m_bDirty) return m_bCompiled;
    if (m_xPasses.GetSize() == 0) { m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false; return true; }
    BuildResourceTraffic();
    Validate();
    if (!TopologicalSort()) return false;
    ComputeResourceLifetimes();
    MarkPassesAsComputeOrGraphics();
    ResolveClearFlags();
    GenerateBarriers();
    m_bCompiled = true; m_bDirty = false; m_bEnabledMaskDirty = false;
#ifdef ZENITH_DEBUG
    Zenith_Log(LOG_CATEGORY_RENDERER, "RenderGraph: Compiled %u passes", m_xExecutionOrder.GetSize());
#endif
    return true;
}

void Flux_RenderGraph::MarkPassesAsComputeOrGraphics()
{
    for (u_int i = 0; i < m_xPasses.GetSize(); i++)
    {
        Flux_RenderGraph_Pass* pxP = m_xPasses.Get(i);
        pxP->m_bIsCompute = !pxP->m_pxTargetSetup || *pxP->m_pxTargetSetup == Flux_Graphics::s_xNullTargetSetup;
    }
}

struct Flux_RenderGraph_RecordTaskData { Flux_RenderGraph* m_pxGraph; u_int m_uPassIndex; };

void Flux_RenderGraph_RecordPassTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex)
{
    auto* pxData = static_cast<Flux_RenderGraph_RecordTaskData*>(pData);
    Flux_RenderGraph* pxGraph = pxData[uInvocationIndex].m_pxGraph;
    const u_int uPassIndex = pxData[uInvocationIndex].m_uPassIndex;
    (void)uWorkerIndex;
    const Zenith_Vector<u_int>& xExecOrder = pxGraph->GetExecutionOrder();
    const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = pxGraph->GetPasses();
    Flux_RenderGraph_Pass& xPass = *xPasses.Get(xExecOrder.Get(uPassIndex));
    xPass.m_pxCommandList->Reset();
    if (!xPass.m_bEnabled || !xPass.m_pfnOnRecord) return;
    xPass.m_pfnOnRecord(xPass.m_pxCommandList, xPass.m_pUserData);
}

void Flux_RenderGraph::Execute()
{
    Zenith_Assert(m_bCompiled, "Flux_RenderGraph::Execute: must call Compile() first");
    Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
    if (m_xExecutionOrder.GetSize() == 0) return;
    if (m_bEnabledMaskDirty) { ResolveClearFlags(); m_bEnabledMaskDirty = false; }
    CallPrepareCallbacks();
    RecordCommandLists();
    SubmitRecordedLists();
}

void Flux_RenderGraph::CallPrepareCallbacks()
{
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (pxPass->m_bEnabled && pxPass->m_pfnOnPrepare) pxPass->m_pfnOnPrepare(pxPass->m_pUserData);
    }
}

void Flux_RenderGraph::RecordCommandLists()
{
    const u_int uNumPasses = m_xExecutionOrder.GetSize();
    if (uNumPasses == 0) return;
    
    auto* pxTaskData = static_cast<Flux_RenderGraph_RecordTaskData*>(Zenith_MemoryManagement::Allocate(sizeof(Flux_RenderGraph_RecordTaskData) * uNumPasses));
    for (u_int i = 0; i < uNumPasses; i++)
    {
        pxTaskData[i].m_pxGraph = this;
        pxTaskData[i].m_uPassIndex = i;
    }
    
    Zenith_TaskArray xTasks(ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS, Flux_RenderGraph_RecordPassTask, pxTaskData, uNumPasses, true);
    Zenith_TaskSystem::SubmitTaskArray(&xTasks);
    xTasks.WaitUntilComplete();
    Zenith_MemoryManagement::Deallocate(pxTaskData);
}

void Flux_RenderGraph::SubmitRecordedLists()
{
    for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
    {
        Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
        if (!pxPass->m_bEnabled || !pxPass->m_pfnOnRecord) continue;
        if (pxPass->m_pxCommandList->GetCommandCount() == 0 && !pxPass->m_bClearTargets) continue;
        const Flux_TargetSetup& xTargetSetup = pxPass->m_pxTargetSetup ? *pxPass->m_pxTargetSetup : Flux_Graphics::s_xNullTargetSetup;
        bool bDepthReadOnly = CheckDepthReadOnly(xTargetSetup, *pxPass);
        Flux::SubmitCommandList(pxPass->m_pxCommandList, xTargetSetup, pxPass->m_bClearTargets, bDepthReadOnly, pxPass);
    }
}

bool Flux_RenderGraph::CheckDepthReadOnly(const Flux_TargetSetup& rxTargetSetup, const Flux_RenderGraph_Pass& rxPass)
{
    if (!rxTargetSetup.m_pxDepthStencil) return false;
    void* pDepthRes = static_cast<void*>(rxTargetSetup.m_pxDepthStencil);
    for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator itR(rxPass.m_xReads); !itR.Done(); itR.Next())
    {
        if (itR.GetData().m_xResource.GetVoidPtr() == pDepthRes) return true;
    }
    return false;
}