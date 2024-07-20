#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

extern void ExportAllMeshes();
extern void ExportAllTextures();
#ifdef ZENITH_TOOLS
int main()
{
	ExportAllMeshes();
	ExportAllTextures();
}
#endif