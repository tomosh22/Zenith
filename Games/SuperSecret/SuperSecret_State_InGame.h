#pragma once

#include "StateMachine/Zenith_StateMachine.h"

enum SUPERSECRET_TEXTURE_INDICES
{
	SUPERSECRET_TEXTURE_INDEX__PLAYER0,

	SUPERSECRET_TEXTURE_INDEX__GROUND_MAIN,
	SUPERSECRET_TEXTURE_INDEX__LONG_GRASS,
	SUPERSECRET_TEXTURE_INDEX__PAVEMENT_CENTER,
	SUPERSECRET_TEXTURE_INDEX__PAVEMENT_EDGES,
	SUPERSECRET_TEXTURE_INDEX__TREE_GROUND,
	SUPERSECRET_TEXTURE_INDEX__TREE_BASE,
	SUPERSECRET_TEXTURE_INDEX__TREE_MAIN,
	SUPERSECRET_TEXTURE_INDEX__LIGHT_GROUND_EDGES,


	SUPERSECRET_TEXTURE_INDEX__COUNT,
};

static const char* g_aszTextureNames[SUPERSECRET_TEXTURE_INDEX__COUNT]
{
	"Player0",

	"GroundMain",
	"LongGrass",
	"PavementCenter",
	"PavementEdges",
	"TreeGround",
	"TreeBase",
	"TreeMain",
	"LightGroundEdges",
};

static const char* g_aszTextureFilenames[SUPERSECRET_TEXTURE_INDEX__COUNT]
{
	ASSETS_ROOT"Textures/player0.ztx",

	ASSETS_ROOT"Textures/ground_main.ztx",
	ASSETS_ROOT"Textures/long_grass.ztx",
	ASSETS_ROOT"Textures/pavement_center.ztx",
	ASSETS_ROOT"Textures/pavement_edges.ztx",
	ASSETS_ROOT"Textures/tree_ground.ztx",
	ASSETS_ROOT"Textures/tree_base.ztx",
	ASSETS_ROOT"Textures/tree_main.ztx",
	ASSETS_ROOT"Textures/light_ground_edges.ztx",
};

class SuperSecret_State_InGame ZENITH_FINAL : public Zenith_State
{
	void OnEnter() override ZENITH_FINAL;
	void OnUpdate() override ZENITH_FINAL;
	void OnExit() override ZENITH_FINAL;
};