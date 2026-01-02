#pragma once

#include "StateMachine/Zenith_StateMachine.h"

enum SUPERSECRET_TEXTURE_INDICES : uint32_t
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
	GAME_ASSETS_DIR"Textures/player0.ztx",

	GAME_ASSETS_DIR"Textures/ground_main.ztx",
	GAME_ASSETS_DIR"Textures/long_grass.ztx",
	GAME_ASSETS_DIR"Textures/pavement_center.ztx",
	GAME_ASSETS_DIR"Textures/pavement_edges.ztx",
	GAME_ASSETS_DIR"Textures/tree_ground.ztx",
	GAME_ASSETS_DIR"Textures/tree_base.ztx",
	GAME_ASSETS_DIR"Textures/tree_main.ztx",
	GAME_ASSETS_DIR"Textures/light_ground_edges.ztx",
};

class SuperSecret_State_InGame ZENITH_FINAL : public Zenith_State
{
	void OnEnter() override ZENITH_FINAL;
	void OnUpdate() override ZENITH_FINAL;
	void OnExit() override ZENITH_FINAL;
};