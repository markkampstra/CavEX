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
	ENTITY_LOCAL_PLAYER = 0,
	ENTITY_ITEM,
	ENTITY_TYPE_COUNT,
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

// Per-entity custom storage. Each entity_type_def declares how big its data
// struct is via data_size, and the type's tick/render code casts e->data
// through the ENTITY_DATA(e, T) macro. The buffer is sized for the largest
// known type so we never need to allocate or free anything; m-lib's POD
// oplist on dict_entity continues to work unchanged.
//
// If a new type's struct exceeds ENTITY_DATA_MAX, the static_assert in
// entity_registry.c will fail at compile time -- raise the limit there.
#define ENTITY_DATA_MAX 64
#define ENTITY_DATA(e, T) ((struct T*)(void*)((e)->data))

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
	// Distance fallen since the last on_ground=true tick. Beta fall damage
	// is ceil(fall_distance - 3) HP applied on landing.
	float fall_distance;
	vec3 motion_pushback;

	enum entity_type type;
	uint8_t data[ENTITY_DATA_MAX];
};

DICT_DEF2(dict_entity, uint32_t, M_BASIC_OPLIST, struct entity, M_POD_OPLIST)

#include "../world.h"

// Per-type behaviour table. Each entity_type registers exactly one of these
// at startup via entity_register_type(); the engine looks it up by type to
// dispatch ticks, rendering, teleports, damage hooks, etc.
struct entity_type_def {
	const char* name;
	size_t data_size;          // must be <= ENTITY_DATA_MAX
	int16_t default_max_health;

	bool (*tick_client)(struct entity*);
	bool (*tick_server)(struct entity*, struct server_local*);
	void (*render)(struct entity*, mat4 view, float tick_delta);
	void (*teleport)(struct entity*, vec3 pos);
	void (*on_damage)(struct entity*, int amount, enum damage_source src);
	void (*on_death)(struct entity*, struct server_local*);
};

void entity_register_type(enum entity_type t, const struct entity_type_def* d);
const struct entity_type_def* entity_get_def(enum entity_type t);
void entity_registry_init(void);  // calls each type's registration once

// Convenience: dispatch the type's hook if registered. Safe to call when no
// def has been installed (returns false / no-op).
bool entity_call_tick_client(struct entity* e);
bool entity_call_tick_server(struct entity* e, struct server_local* s);
void entity_call_render(struct entity* e, mat4 view, float tick_delta);
void entity_call_teleport(struct entity* e, vec3 pos);

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

// Living-entity helpers. entity_living_tick decrements hurt_time/death_time
// and should be called once per server tick from any tick_server function
// of an entity that can take damage.
void entity_living_tick(struct entity* e);
// Apply damage to the entity. No-op for non-living entities (max_health == 0)
// or already-dead entities (health <= 0). Honors hurt_time i-frames except
// for DMG_VOID (insta-kill bypass).
void entity_damage(struct entity* e, int amount, enum damage_source src);
// Set health to 0 and start the death animation; the entity is destroyed
// after the death-animation timer expires.
void entity_kill(struct entity* e);

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

// Per-type private data structs. Declared here so headers that need to peek
// (server_local for ENTITY_ITEM payloads, screens for ENTITY_LOCAL_PLAYER's
// capture_input flag) can do so via the ENTITY_DATA() cast macro.
struct entity_local_player {
	int jump_ticks;
	bool capture_input;
};

struct entity_item {
	struct item_data item;
	int age;
};

#endif
