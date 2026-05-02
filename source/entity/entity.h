/*
	Copyright (c) 2023 ByteBit/xtreme8000

	This file is part of CavEX.

	CavEX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	CavEX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with CavEX.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ENTITY_H
#define ENTITY_H

#include <m-lib/m-dict.h>
#include <stdbool.h>

#include "../cglm/cglm.h"
#include "../item/items.h"

enum entity_type {
	ENTITY_LOCAL_PLAYER,
	ENTITY_ITEM,
};

// Damage causes; mirrors Beta 1.7.3's net.minecraft.util.DamageSource
// constants. Used to gate behaviour (e.g. armour ignores DMG_DROWN/DMG_VOID,
// fall damage skipped in water, etc.) once those rules land in Phase 3+.
enum damage_source {
	DMG_GENERIC,
	DMG_FALL,
	DMG_DROWN,
	DMG_LAVA,
	DMG_FIRE,
	DMG_MOB,
	DMG_PLAYER,
	DMG_VOID,
	DMG_CACTUS,
	DMG_SUFFOCATE,
};

struct server_local;

struct entity {
	uint32_t id;
	bool on_server;
	void* world;
	int delay_destroy;

	vec3 pos;
	vec3 pos_old;
	vec3 vel;
	vec2 orient;
	vec2 orient_old;
	bool on_ground;

	vec3 network_pos;

	// Health / damage state. health <= 0 means dead. hurt_time / death_time
	// are animation timers ticked down each frame; air is the drowning timer
	// (Beta default 300, drowning damage starts when it hits 0).
	int16_t health;
	int16_t max_health;
	int16_t hurt_time;
	int16_t death_time;
	int16_t air;
	vec3 motion_pushback;

	bool (*tick_client)(struct entity*);
	bool (*tick_server)(struct entity*, struct server_local*);
	void (*render)(struct entity*, mat4, float);
	void (*teleport)(struct entity*, vec3);
	// Optional per-type damage hook. NULL means "use the default rules in
	// entity_damage". Non-NULL lets a mob override (e.g. creeper starts fuse).
	void (*on_damage)(struct entity*, int amount, enum damage_source src);

	enum entity_type type;
	union entity_data {
		struct entity_local_player {
			int jump_ticks;
			bool capture_input;
		} local_player;
		struct entity_item {
			struct item_data item;
			int age;
		} item;
	} data;
};

DICT_DEF2(dict_entity, uint32_t, M_BASIC_OPLIST, struct entity, M_POD_OPLIST)

#include "../world.h"

void entity_local_player(uint32_t id, struct entity* e, struct world* w);
bool entity_local_player_block_collide(vec3 pos, struct block_info* blk_info);

void entity_item(uint32_t id, struct entity* e, bool server, void* world,
				 struct item_data it);

uint32_t entity_gen_id(dict_entity_t dict);
void entities_client_tick(dict_entity_t dict);
void entities_client_render(dict_entity_t dict, struct camera* c,
							float tick_delta);

void entity_default_init(struct entity* e, bool server, void* world);
void entity_default_teleport(struct entity* e, vec3 pos);
bool entity_default_client_tick(struct entity* e);

void entity_shadow(struct entity* e, struct AABB* a, mat4 view);

bool entity_get_block(struct entity* e, w_coord_t x, w_coord_t y, w_coord_t z,
					  struct block_data* blk);
bool entity_intersection_threshold(struct entity* e, struct AABB* aabb,
								   vec3 old_pos, vec3 new_pos,
								   float* threshold);
bool entity_intersection(struct entity* e, struct AABB* a,
						 bool (*test)(struct AABB* entity,
									  struct block_info* blk_info));
bool entity_block_aabb_test(struct AABB* entity, struct block_info* blk_info);
bool entity_aabb_intersection(struct entity* e, struct AABB* a);
void entity_try_move(struct entity* e, vec3 pos, vec3 vel, struct AABB* bbox,
					 size_t coord, bool* collision_xz, bool* on_ground);

#endif