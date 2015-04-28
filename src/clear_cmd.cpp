/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file clear_cmd.cpp Commands related to clear tiles. */

#include "stdafx.h"
#include "map/ground.h"
#include "map/water.h"
#include "map/slope.h"
#include "command_func.h"
#include "landscape.h"
#include "genworld.h"
#include "viewport_func.h"
#include "water.h"
#include "company_base.h"
#include "company_func.h"
#include "town.h"
#include "sound_func.h"
#include "core/random_func.hpp"
#include "newgrf_generic.h"
#include "bridge.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/clear_land.h"
#include "table/tree_land.h"

static CommandCost ClearTile_Clear(TileIndex tile, DoCommandFlag flags)
{
	static const Price clear_price_table[] = {
		PR_CLEAR_GRASS,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROCKS,
		PR_CLEAR_ROUGH,
	};

	Money cost;

	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_GROUND_VOID:
			return_cmd_error(STR_ERROR_OFF_EDGE_OF_MAP);

		case TT_GROUND_FIELDS:
			cost = _price[PR_CLEAR_FIELDS];
			break;

		case TT_GROUND_CLEAR:
			if (IsSnowTile(tile)) {
				cost = _price[PR_CLEAR_ROUGH];
			} else if (IsClearGround(tile, GROUND_GRASS) && GetClearDensity(tile) == 0) {
				cost = 0;
			} else {
				cost = _price[clear_price_table[GetClearGround(tile)]];
			}
			break;

		case TT_GROUND_TREES:
			if (Company::IsValidID(_current_company)) {
				Town *t = LocalAuthorityTownFromTile(tile);
				if (t != NULL) ChangeTownRating(t, RATING_TREE_DOWN_STEP, RATING_TREE_MINIMUM, flags);
			}
			cost = GetTreeCount(tile) * _price[PR_CLEAR_TREES];
			if (IsInsideMM(GetTreeType(tile), TREE_RAINFOREST, TREE_CACTUS)) cost *= 4;
			break;
	}

	if (flags & DC_EXEC) DoClearSquare(tile);

	return CommandCost(EXPENSES_CONSTRUCTION, cost);
}

void DrawVoidTile(TileInfo *ti)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ti->tileh), PALETTE_ALL_BLACK);
}

void DrawClearLandTile(const TileInfo *ti, byte set)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ti->tileh) + set * 19, PAL_NONE);
}

static void DrawClearLandFence(const TileInfo *ti)
{
	/* combine fences into one sprite object */
	StartSpriteCombine();

	int maxz = GetSlopeMaxPixelZ(ti->tileh);

	uint fence_nw = GetFence(ti->tile, DIAGDIR_NW);
	if (fence_nw != 0) {
		int z = GetSlopePixelZInCorner(ti->tileh, CORNER_W);
		SpriteID sprite = _clear_land_fence_sprites[fence_nw - 1] + _fence_mod_by_tileh_nw[ti->tileh];
		AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y - 15, 16, 31, maxz - z + 4, ti->z + z, false, 0, 15, -z);
	}

	uint fence_ne = GetFence(ti->tile, DIAGDIR_NE);
	if (fence_ne != 0) {
		int z = GetSlopePixelZInCorner(ti->tileh, CORNER_E);
		SpriteID sprite = _clear_land_fence_sprites[fence_ne - 1] + _fence_mod_by_tileh_ne[ti->tileh];
		AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x - 15, ti->y, 31, 16, maxz - z + 4, ti->z + z, false, 15, 0, -z);
	}

	uint fence_sw = GetFence(ti->tile, DIAGDIR_SW);
	uint fence_se = GetFence(ti->tile, DIAGDIR_SE);

	if (fence_sw != 0 || fence_se != 0) {
		int z = GetSlopePixelZInCorner(ti->tileh, CORNER_S);

		if (fence_sw != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_sw - 1] + _fence_mod_by_tileh_sw[ti->tileh];
			AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y, 16, 16, maxz - z + 4, ti->z + z, false, 0, 0, -z);
		}

		if (fence_se != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_se - 1] + _fence_mod_by_tileh_se[ti->tileh];
			AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y, 16, 16, maxz - z + 4, ti->z + z, false, 0, 0, -z);
		}
	}
	EndSpriteCombine();
}

struct TreeListEnt : PalSpriteID {
	byte x, y;
};

static void DrawTrees(TileInfo *ti)
{
	uint tmp = CountBits(ti->tile + ti->x + ti->y);
	uint index = GB(tmp, 0, 2) + (GetTreeType(ti->tile) << 2);

	/* different tree styles above one of the grounds */
	if (IsSnowTile(ti->tile) && GetClearDensity(ti->tile) >= 2 &&
			IsInsideMM(index, TREE_SUB_ARCTIC << 2, TREE_RAINFOREST << 2)) {
		index += 164 - (TREE_SUB_ARCTIC << 2);
	}

	assert(index < lengthof(_tree_layout_sprite));

	const PalSpriteID *s = _tree_layout_sprite[index];
	const TreePos *d = _tree_layout_xy[GB(tmp, 2, 2)];

	/* combine trees into one sprite object */
	StartSpriteCombine();

	TreeListEnt te[4];

	/* put the trees to draw in a list */
	uint trees = GetTreeCount(ti->tile);

	for (uint i = 0; i < trees; i++) {
		SpriteID sprite = s[0].sprite + (i == trees - 1 ? GetTreeGrowth(ti->tile) : 3);
		PaletteID pal = s[0].pal;

		te[i].sprite = sprite;
		te[i].pal    = pal;
		te[i].x = d->x;
		te[i].y = d->y;
		s++;
		d++;
	}

	/* draw them in a sorted way */
	int z = ti->z + GetSlopeMaxPixelZ(ti->tileh) / 2;

	for (; trees > 0; trees--) {
		uint min = te[0].x + te[0].y;
		uint mi = 0;

		for (uint i = 1; i < trees; i++) {
			if ((uint)(te[i].x + te[i].y) < min) {
				min = te[i].x + te[i].y;
				mi = i;
			}
		}

		AddSortableSpriteToDraw(te[mi].sprite, te[mi].pal, ti->x + te[mi].x, ti->y + te[mi].y, 16 - te[mi].x, 16 - te[mi].y, 0x30, z, IsTransparencySet(TO_TREES), -te[mi].x, -te[mi].y);

		/* replace the removed one with the last one */
		te[mi] = te[trees - 1];
	}

	EndSpriteCombine();
}

static void DrawTile_Clear(TileInfo *ti)
{
	switch (GetTileSubtype(ti->tile)) {
		case TT_GROUND_VOID:
			DrawVoidTile(ti);
			return;

		case TT_GROUND_FIELDS:
			DrawGroundSprite(_clear_land_sprites_farmland[GetFieldType(ti->tile)] + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
			DrawClearLandFence(ti);
			DrawBridgeMiddle(ti);
			break;

		default:
			switch (GetFullClearGround(ti->tile)) {
				case GROUND_GRASS:
					DrawClearLandTile(ti, GetClearDensity(ti->tile));
					break;

				case GROUND_SHORE:
					DrawShoreTile(ti->tileh);
					break;

				case GROUND_ROUGH:
					DrawGroundSprite (ti->tileh != SLOPE_FLAT ?
							SPR_FLAT_ROUGH_LAND + SlopeToSpriteOffset(ti->tileh) :
							_landscape_clear_sprites_rough[GB(ti->x ^ ti->y, 4, 3)],
						PAL_NONE);
					break;

				case GROUND_ROCKS:
					DrawGroundSprite((HasGrfMiscBit(GMB_SECOND_ROCKY_TILE_SET) && (TileHash(ti->x, ti->y) & 1) ? SPR_FLAT_ROCKY_LAND_2 : SPR_FLAT_ROCKY_LAND_1) + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
					break;

				default:
					DrawGroundSprite(_clear_land_sprites_snow_desert[GetClearDensity(ti->tile)] + SlopeToSpriteOffset(ti->tileh), PAL_NONE);
					break;
			}

			if (!IsTileSubtype(ti->tile, TT_GROUND_TREES)) {
				DrawBridgeMiddle(ti);
			} else if (!IsInvisibilitySet(TO_TREES)) {
				DrawTrees(ti);
			}

			break;
	}
}


static int GetSlopePixelZ_Clear(TileIndex tile, uint x, uint y)
{
	if (IsTileSubtype(tile, TT_GROUND_VOID)) return TilePixelHeight(tile);

	int z;
	Slope tileh = GetTilePixelSlope(tile, &z);

	return z + GetPartialPixelZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Clear(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static void UpdateFences(TileIndex tile)
{
	assert(IsFieldsTile(tile));
	bool dirty = false;

	bool neighbour = IsFieldsTile(TILE_ADDXY(tile, 1, 0));
	if (!neighbour && GetFence(tile, DIAGDIR_SW) == 0) {
		SetFence(tile, DIAGDIR_SW, 3);
		dirty = true;
	}

	neighbour = IsFieldsTile(TILE_ADDXY(tile, 0, 1));
	if (!neighbour && GetFence(tile, DIAGDIR_SE) == 0) {
		SetFence(tile, DIAGDIR_SE, 3);
		dirty = true;
	}

	neighbour = IsFieldsTile(TILE_ADDXY(tile, -1, 0));
	if (!neighbour && GetFence(tile, DIAGDIR_NE) == 0) {
		SetFence(tile, DIAGDIR_NE, 3);
		dirty = true;
	}

	neighbour = IsFieldsTile(TILE_ADDXY(tile, 0, -1));
	if (!neighbour && GetFence(tile, DIAGDIR_NW) == 0) {
		SetFence(tile, DIAGDIR_NW, 3);
		dirty = true;
	}

	if (dirty) MarkTileDirtyByTile(tile);
}


/** Convert to or from snowy tiles. */
static void TileLoopClearAlps(TileIndex tile)
{
	int k = GetTileZ(tile) - GetSnowLine() + 1;

	if (!IsSnowTile(tile)) {
		/* No snow, make it if needed, otherwise do nothing. */
		if (k < 0) return;
		MakeSnow(tile);
	} else {
		/* Update snow density. */
		uint cur_density = GetClearDensity(tile);
		uint req_density = (k < 0) ? 0u : min((uint)k, 3);

		if (cur_density < req_density) {
			AddClearDensity(tile, 1);
		} else if (cur_density > req_density) {
			AddClearDensity(tile, -1);
		} else if (k < 0) {
			ClearSnow(tile);
		} else {
			/* Density at the required level. */
			if (IsTileSubtype(tile, TT_GROUND_TREES) && cur_density == 3) {
				uint32 r = Random();
				if (Chance16I(1, 200, r) && _settings_client.sound.ambient) {
					SndPlayTileFx((r & 0x80000000) ? SND_39_HEAVY_WIND : SND_34_WIND, tile);
				}
			}
			return;
		}
	}

	MarkTileDirtyByTile(tile);
}

/**
 * Tests if at least one surrounding tile is desert
 * @param tile tile to check
 * @return does this tile have at least one desert tile around?
 */
static inline bool NeighbourIsDesert(TileIndex tile)
{
	return GetTropicZone(tile + TileDiffXY(  1,  0)) == TROPICZONE_DESERT ||
			GetTropicZone(tile + TileDiffXY( -1,  0)) == TROPICZONE_DESERT ||
			GetTropicZone(tile + TileDiffXY(  0,  1)) == TROPICZONE_DESERT ||
			GetTropicZone(tile + TileDiffXY(  0, -1)) == TROPICZONE_DESERT;
}

static void TileLoopClearDesert(TileIndex tile)
{
	/* Expected desert level - 0 if it shouldn't be desert */
	uint expected = GetTropicZone(tile) == TROPICZONE_DESERT ? 3 :
		NeighbourIsDesert(tile) ? 1 : 0;

	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_GROUND_FIELDS:
			if (expected == 0) return;
			MakeClear(tile, GROUND_DESERT, expected);
			break;

		case TT_GROUND_TREES:
			if (GetTropicZone(tile) == TROPICZONE_RAINFOREST) {
				static const SoundFx forest_sounds[] = {
					SND_42_LOON_BIRD,
					SND_43_LION,
					SND_44_MONKEYS,
					SND_48_DISTANT_BIRD
				};
				uint32 r = Random();

				if (Chance16I(1, 200, r) && _settings_client.sound.ambient) SndPlayTileFx(forest_sounds[GB(r, 16, 2)], tile);
				return;
			}
			/* fall through */
		case TT_GROUND_CLEAR: {
			/* Current desert level - 0 if it is not desert */
			uint current = IsClearGround(tile, GROUND_DESERT) ? GetClearDensity(tile) : 0;

			if (current == expected) return;

			if (expected == 0) {
				SetClearGroundDensity(tile, GROUND_GRASS, 3);
			} else {
				/* Transition from clear to desert is not smooth (after clearing desert tile) */
				SetClearGroundDensity(tile, GROUND_DESERT, expected);
			}
			break;
		}
	}

	MarkTileDirtyByTile(tile);
}

extern void AddNeighbouringTree(TileIndex tile);

static void HandleTreeGrowth(TileIndex tile)
{
	switch (GetTreeGrowth(tile)) {
		case 3: // regular sized tree
			if (_settings_game.game_creation.landscape == LT_TROPIC &&
					GetTreeType(tile) != TREE_CACTUS &&
					GetTropicZone(tile) == TROPICZONE_DESERT) {
				AddTreeGrowth(tile, 1);
			} else {
				switch (GB(Random(), 0, 3)) {
					case 0: // start destructing
						AddTreeGrowth(tile, 1);
						break;

					case 1: // add a tree
						if (GetTreeCount(tile) < 4) {
							AddTreeCount(tile, 1);
							SetTreeGrowth(tile, 0);
							break;
						}
						/* FALL THROUGH */

					case 2: // add a neighbouring tree
						AddNeighbouringTree(tile);
						break;

					default:
						return;
				}
			}
			break;

		case 6: // final stage of tree destruction
			if (GetTreeCount(tile) > 1) {
				/* more than one tree, delete it */
				AddTreeCount(tile, -1);
				SetTreeGrowth(tile, 3);
			} else {
				/* just one tree, change type into clear */
				Ground g = GetClearGround(tile);
				if (g == GROUND_SHORE) {
					MakeShore(tile);
				} else {
					MakeClear(tile, g, GetClearDensity(tile));
				}
			}
			break;

		default:
			AddTreeGrowth(tile, 1);
			break;
	}
}

static void TileLoop_Clear(TileIndex tile)
{
	if (IsTileSubtype(tile, TT_GROUND_VOID)) return;

	if (!IsTileSubtype(tile, TT_GROUND_FIELDS) && GetClearGround(tile) == GROUND_SHORE) {
		TileLoop_Water(tile);
	} else {
		/* If the tile is at any edge flood it to prevent maps without water. */
		if (_settings_game.construction.freeform_edges && DistanceFromEdge(tile) == 1) {
			int z;
			if (IsTileFlat(tile, &z) && z == 0) {
				DoFloodTile(tile);
				MarkTileDirtyByTile(tile);
				return;
			}
		}

		switch (_settings_game.game_creation.landscape) {
			case LT_TROPIC: TileLoopClearDesert(tile); break;
			case LT_ARCTIC: TileLoopClearAlps(tile);   break;
		}
	}

	AmbientSoundEffect(tile);

	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_GROUND_FIELDS:
			UpdateFences(tile);

			if (_game_mode == GM_EDITOR) return;

			if (GetClearCounter(tile) < 7) {
				AddClearCounter(tile, 1);
				return;
			}

			SetClearCounter(tile, 0);

			if (GetIndustryIndexOfField(tile) == INVALID_INDUSTRY && GetFieldType(tile) >= 7) {
				/* This farmfield is no longer farmfield, so make it grass again */
				MakeClear(tile, GROUND_GRASS, 2);
			} else {
				uint field_type = GetFieldType(tile);
				field_type = (field_type < 8) ? field_type + 1 : 0;
				SetFieldType(tile, field_type);
			}
			break;

		case TT_GROUND_CLEAR:
			if (GetClearGround(tile) == GROUND_GRASS) {
				if (GetClearDensity(tile) == 3) return;

				if (_game_mode != GM_EDITOR) {
					if (GetClearCounter(tile) < 7) {
						AddClearCounter(tile, 1);
						return;
					}
					SetClearCounter(tile, 0);
					AddClearDensity(tile, 1);
				} else {
					SetClearGroundDensity(tile, GB(Random(), 0, 8) > 21 ? GROUND_GRASS : GROUND_ROUGH, 3);
				}
			}
			break;

		case TT_GROUND_TREES: {
			uint counter = GetClearCounter(tile);

			/* Handle growth of grass (under trees) at every 8th processings, like it's done for grass on clear tiles. */
			if ((counter & 7) == 7 && GetClearGround(tile) == GROUND_GRASS) {
				if (GetClearDensity(tile) < 3) {
					AddClearDensity(tile, 1);
					MarkTileDirtyByTile(tile);
				}
			}
			if (counter < 15) {
				AddClearCounter(tile, 1);
				return;
			}
			SetClearCounter(tile, 0);
			HandleTreeGrowth(tile);
			break;
		}
	}

	MarkTileDirtyByTile(tile);
}

void GenerateClearTile()
{
	uint i, gi;
	TileIndex tile;

	/* add rough tiles */
	i = ScaleByMapSize(GB(Random(), 0, 10) + 0x400);
	gi = ScaleByMapSize(GB(Random(), 0, 7) + 0x80);

	SetGeneratingWorldProgress(GWP_ROUGH_ROCKY, gi + i);
	do {
		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		tile = RandomTile();
		if (IsClearTile(tile) && !IsClearGround(tile, GROUND_DESERT)) SetClearGroundDensity(tile, GROUND_ROUGH, 3);
	} while (--i);

	/* add rocky tiles */
	i = gi;
	do {
		uint32 r = Random();
		tile = RandomTileSeed(r);

		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		if (IsClearTile(tile) && !IsClearGround(tile, GROUND_DESERT)) {
			uint j = GB(r, 16, 4) + 5;
			for (;;) {
				TileIndex tile_new;

				SetClearGroundDensity(tile, GROUND_ROCKS, 3);
				do {
					if (--j == 0) goto get_out;
					tile_new = tile + TileOffsByDiagDir((DiagDirection)GB(Random(), 0, 2));
				} while (!IsClearTile(tile_new) || IsClearGround(tile_new, GROUND_DESERT));
				tile = tile_new;
			}
get_out:;
		}
	} while (--i);
}

static const StringID _clear_land_str[] = {
	STR_LAI_CLEAR_DESCRIPTION_GRASS,
	STR_LAI_CLEAR_DESCRIPTION_GRASS,
	STR_LAI_CLEAR_DESCRIPTION_ROUGH_LAND,
	STR_LAI_CLEAR_DESCRIPTION_ROCKS,
	STR_LAI_CLEAR_DESCRIPTION_DESERT,
};

static void GetTileDesc_Clear(TileIndex tile, TileDesc *td)
{
	switch (GetTileSubtype(tile)) {
		default: NOT_REACHED();

		case TT_GROUND_VOID:
			td->str = STR_EMPTY;
			td->owner[0] = OWNER_NONE;
			return;

		case TT_GROUND_FIELDS:
			td->str = STR_LAI_CLEAR_DESCRIPTION_FIELDS;
			break;

		case TT_GROUND_CLEAR:
			if (IsSnowTile(tile)) {
				td->str = STR_LAI_CLEAR_DESCRIPTION_SNOW_COVERED_LAND;
			} else if (IsClearGround(tile, GROUND_GRASS) && GetClearDensity(tile) == 0) {
				td->str = STR_LAI_CLEAR_DESCRIPTION_BARE_LAND;
			} else {
				td->str = _clear_land_str[GetClearGround(tile)];
			}
			break;

		case TT_GROUND_TREES: {
			TreeType tt = GetTreeType(tile);
			if (IsInsideMM(tt, TREE_RAINFOREST, TREE_CACTUS)) {
				td->str = STR_LAI_TREE_NAME_RAINFOREST;
			} else {
				td->str = tt == TREE_CACTUS ? STR_LAI_TREE_NAME_CACTUS_PLANTS : STR_LAI_TREE_NAME_TREES;
			}
			break;
		}
	}

	td->owner[0] = GetTileOwner(tile);
}

static void ChangeTileOwner_Clear(TileIndex tile, Owner old_owner, Owner new_owner)
{
	return;
}

static CommandCost TerraformTile_Clear(TileIndex tile, DoCommandFlag flags, int z_new, Slope tileh_new)
{
	if (IsTileSubtype(tile, TT_GROUND_VOID)) return_cmd_error(STR_ERROR_OFF_EDGE_OF_MAP);

	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}

extern const TileTypeProcs _tile_type_clear_procs = {
	DrawTile_Clear,           ///< draw_tile_proc
	GetSlopePixelZ_Clear,     ///< get_slope_z_proc
	ClearTile_Clear,          ///< clear_tile_proc
	NULL,                     ///< add_accepted_cargo_proc
	GetTileDesc_Clear,        ///< get_tile_desc_proc
	NULL,                     ///< get_tile_railway_status_proc
	NULL,                     ///< get_tile_road_status_proc
	NULL,                     ///< get_tile_waterway_status_proc
	NULL,                     ///< click_tile_proc
	NULL,                     ///< animate_tile_proc
	TileLoop_Clear,           ///< tile_loop_proc
	ChangeTileOwner_Clear,    ///< change_tile_owner_proc
	NULL,                     ///< add_produced_cargo_proc
	GetFoundation_Clear,      ///< get_foundation_proc
	TerraformTile_Clear,      ///< terraform_tile_proc
};
