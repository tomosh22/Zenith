#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_FontAsset.h"

#include <type_traits>

#include "AssetHandling/Zenith_AssetHandle.inl"

template class Zenith_AssetHandle<Zenith_TextureAsset>;
template class Zenith_AssetHandle<Zenith_MaterialAsset>;
template class Zenith_AssetHandle<Zenith_MeshAsset>;
template class Zenith_AssetHandle<Zenith_SkeletonAsset>;
template class Zenith_AssetHandle<Zenith_ModelAsset>;
template class Zenith_AssetHandle<Zenith_AnimationAsset>;
template class Zenith_AssetHandle<Zenith_MeshGeometryAsset>;
template class Zenith_AssetHandle<Zenith_FontAsset>;
