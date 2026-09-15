from pathlib import Path
root=Path('C:/dev/Zenith')
def edit(p,old,new):
    p=root/p; s=p.read_text(); assert old in s,(p,old[:100]); p.write_text(s.replace(old,new))

edit('Zenith/Flux/Flux_GPUScene.h','auOut[2] = 0u;             // firstIndex','auOut[2] = uFirstIndex;    // firstIndex')
p=root/'Zenith/Flux/Flux_GPUSceneBuilder.cpp'
s=p.read_text()
marker='// Stage 4.3 (TAA): record one object'
i=s.index(marker)
helper='''// Each material section shares the mesh's VB/IB residency, but owns an index
// range in the bucket key. Do not conflate a mesh binding with a draw section.
static void AppendStaticModelSections(Flux_MeshGeometryRegistry& xRegistry,
    Flux_ModelInstance& xModel, uint32_t uMesh, Zenith_MaterialAsset* pxBlank,
    Flux_GPUSceneSourceItem& xItem)
{
    for (uint32_t uSection = 0; uSection < xModel.GetNumDrawSections(uMesh); ++uSection)
    {
        Flux_MeshDrawSection xSection;
        if (!xModel.GetDrawSection(uMesh, uSection, xSection)) continue;
        Flux_GPUSceneSourceSubmesh xSub;
        if (!BuildStaticSubmeshDesc(xRegistry, xModel.GetMeshInstance(uMesh),
            xModel.GetMeshMaterial(uMesh, xSection.m_uMaterialSlot), pxBlank, xSub)) continue;
        xSub.m_uFirstIndex = xSection.m_uFirstIndex;
        xSub.m_uIndexCount = xSection.m_uIndexCount;
        xItem.m_xSubmeshes.PushBack(xSub);
    }
}

'''.replace('    ','\t')
s=s[:i]+helper+s[i:]
a=s.index('\t\t\tFlux_GPUSceneSourceSubmesh xSub;',s.index('const uint32_t uNumMeshes'))
b=s.index('\n\t\t}',a)
s=s[:a]+'''\t\t\tAppendStaticModelSections(m_xUnifiedMeshGeometryRegistry, *pxModel,
                uMesh, pxBlankMaterial, xItem);'''.replace('                ','\t\t\t\t')+s[b:]
a=s.index('\t\t\t\t\tFlux_GPUSceneSourceSubmesh xStaticSub;')
b=s.index('\n\t\t\t\t\tcontinue;',a)
s=s[:a]+'''\t\t\t\t\tAppendStaticModelSections(m_xUnifiedMeshGeometryRegistry, *pxModel,
                        uMesh, pxBlankMaterial, xStaticOfAnimated);'''.replace('                        ','\t\t\t\t\t\t')+s[b:]
a=s.index('\t\t\t\tZenith_MaterialAsset* pxMat = pxModel->GetMaterial(uMesh);')
b=s.index('\n\t\t\t\t// Stable base',a)
s=s[:a]+s[b:]
a=s.index('\t\t\t\tFlux_GPUSceneBucketKey xKey;',s.index('m_xUnifiedSkinnedDrawById.Insert(uStableId, xSD);'))
b=s.index('\n\t\t\t}',a)
block=s[a:b]
block=block.replace('xKey.m_uMeshGeometryId   =','xKey.m_uFirstIndex = xSection.m_uFirstIndex;\n\t\t\t\txKey.m_uIndexCount = xSection.m_uIndexCount;\n\t\t\t\txKey.m_uMeshGeometryId   =',1)
prefix='''\t\t\t\tfor (uint32_t uSection = 0; uSection < pxModel->GetNumDrawSections(uMesh); ++uSection)
                {
                    Flux_MeshDrawSection xSection;
                    if (!pxModel->GetDrawSection(uMesh, uSection, xSection)) continue;
                    Zenith_MaterialAsset* pxMat = pxModel->GetMeshMaterial(uMesh, xSection.m_uMaterialSlot);
                    if (!pxMat) pxMat = pxBlankMaterial;
                    const auto eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
                    if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE) continue;
'''.replace('    ','\t')
s=s[:a]+prefix+'\n'.join('\t'+line for line in block.split('\n'))+'\n\t\t\t\t}'+s[b:]
p.write_text(s)

p=root/'Zenith/Flux/UnifiedMesh/Flux_UnifiedMesh.cpp'; s=p.read_text()
s=s.replace('kuINITIAL_BUCKETS * sizeof(u_int), m_xBucketIndexCountBuffer','kuINITIAL_BUCKETS * 2u * sizeof(u_int), m_xBucketIndexCountBuffer')
s=s.replace('m_uBucketMetaCapacity * sizeof(u_int), m_xBucketIndexCountBuffer','m_uBucketMetaCapacity * 2u * sizeof(u_int), m_xBucketIndexCountBuffer')
s=s.replace('m_auBucketIndexCountScratch.PushBack(0u);','m_auBucketIndexCountScratch.PushBack(0u);\n\t\tm_auBucketIndexCountScratch.PushBack(0u);')
s=s.replace('m_auBucketIndexCountScratch.Get(uSlot) = pxMesh->GetNumIndices();','''const u_int uFirstIndex = pxKey->m_uFirstIndex;
        const u_int uIndexCount = pxKey->m_uIndexCount == ~0u ? pxMesh->GetNumIndices() : pxKey->m_uIndexCount;
        if (uFirstIndex > pxMesh->GetNumIndices() || uIndexCount > pxMesh->GetNumIndices() - uFirstIndex) continue;
        m_auBucketIndexCountScratch.Get(uSlot * 2u) = uIndexCount;
        m_auBucketIndexCountScratch.Get(uSlot * 2u + 1u) = uFirstIndex;'''.replace('    ','\t'))
s=s.replace('m_auBucketIndexCountScratch.GetDataPointer(), uSlotCount * sizeof(u_int)','m_auBucketIndexCountScratch.GetDataPointer(), uSlotCount * 2u * sizeof(u_int)')
p.write_text(s)
edit('Zenith/Flux/Shaders/UnifiedMesh/Flux_UnifiedMesh_Reset.slang','bucketIndexCount[b]','bucketIndexCount[b * 2u]')
edit('Zenith/Flux/Shaders/UnifiedMesh/Flux_UnifiedMesh_Reset.slang','g_xPassSet.indirect[base + 2u] = 0u;','g_xPassSet.indirect[base + 2u] = g_xPassSet.bucketIndexCount[b * 2u + 1u];')
edit('Zenith/Flux/Shaders/UnifiedMesh/Flux_UnifiedMesh_Reset.slang','[numBuckets] per-bucket mesh index count','[numBuckets*2] indexCount, firstIndex')

p=root/'Zenith/Flux/Translucency/Flux_Translucency.cpp'; s=p.read_text()
s=s.replace('xItem.m_pxMeshInstance = pxMeshInstance;','xItem.m_pxMeshInstance = pxMeshInstance;\n\txItem.m_uIndexCount = pxMeshInstance->GetNumIndices();')
a=s.index('\t\t\t\tZenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);')
b=s.index('\n\t\t\t}',a)
block=s[a:b].replace('Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);','''Flux_MeshDrawSection xSection;
                if (!pxModelInstance->GetDrawSection(uMesh, uSection, xSection)) continue;
                Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMeshMaterial(uMesh, xSection.m_uMaterialSlot);'''.replace('    ','\t'))
block=block.replace('xPacket.PushBack(BuildTranslucentDrawItem(pxMeshInstance, pxMaterial, xModelMatrix, xCameraPos));','''auto xItem = BuildTranslucentDrawItem(pxMeshInstance, pxMaterial, xModelMatrix, xCameraPos);
                xItem.m_uFirstIndex = xSection.m_uFirstIndex;
                xItem.m_uIndexCount = xSection.m_uIndexCount;
                xPacket.PushBack(xItem);'''.replace('    ','\t'))
s=s[:a]+'''\t\t\t\tfor (uint32_t uSection = 0; uSection < pxModelInstance->GetNumDrawSections(uMesh); ++uSection)
                {
'''.replace('    ','\t')+'\n'.join('\t'+line for line in block.split('\n'))+'\n\t\t\t\t}'+s[b:]
s=s.replace('pxCmdList->DrawIndexed(xItem.m_pxMeshInstance->GetNumIndices());','pxCmdList->DrawIndexed(xItem.m_uIndexCount, 1u, 0u, xItem.m_uFirstIndex);')
p.write_text(s)
edit('Zenith/Flux/Translucency/Flux_TranslucencyImpl.h','struct Flux_TranslucentDrawItem\n{','struct Flux_TranslucentDrawItem\n{\n\tu_int m_uFirstIndex = 0u;\n\tu_int m_uIndexCount = 0u;')
