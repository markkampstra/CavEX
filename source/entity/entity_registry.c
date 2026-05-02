/*
	Copyright (c) 2026

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

#include <assert.h>
#include <string.h>

#include "entity.h"

// Static array indexed by enum entity_type; mirrors mcp940's
// EntityList.idToClassMapping but for our own type IDs. Each slot holds the
// vtable for one type. populated[t] tracks whether something installed it,
// since data_size = 0 is a legitimate value (no per-entity data).
static struct entity_type_def defs[ENTITY_TYPE_COUNT];
static bool populated[ENTITY_TYPE_COUNT];

// Forward-declared register entrypoints. Each entity type implementation
// (entity_local_player.c, entity_item.c, ...) defines one of these.
void entity_local_player_register(void);
void entity_item_register(void);
void entity_pig_register(void);
void entity_cow_register(void);
void entity_chicken_register(void);
void entity_sheep_register(void);

void entity_register_type(enum entity_type t, const struct entity_type_def* d) {
	assert(t >= 0 && t < ENTITY_TYPE_COUNT);
	assert(d);
	// data_size must fit in the in-place buffer or every entity will silently
	// corrupt memory when its type's tick code touches data fields.
	assert(d->data_size <= ENTITY_DATA_MAX);
	defs[t] = *d;
	populated[t] = true;
}

const struct entity_type_def* entity_get_def(enum entity_type t) {
	if(t < 0 || t >= ENTITY_TYPE_COUNT || !populated[t])
		return NULL;
	return &defs[t];
}

void entity_registry_init(void) {
	// Re-init clears any previous state so the engine can be re-launched in
	// a single process (e.g. tests).
	memset(defs, 0, sizeof(defs));
	memset(populated, 0, sizeof(populated));

	entity_local_player_register();
	entity_item_register();
	entity_pig_register();
	entity_cow_register();
	entity_chicken_register();
	entity_sheep_register();
}

bool entity_call_tick_client(struct entity* e) {
	const struct entity_type_def* d = entity_get_def(e->type);
	return (d && d->tick_client) ? d->tick_client(e) : false;
}

bool entity_call_tick_server(struct entity* e, struct server_local* s) {
	const struct entity_type_def* d = entity_get_def(e->type);
	return (d && d->tick_server) ? d->tick_server(e, s) : false;
}

void entity_call_render(struct entity* e, mat4 view, float tick_delta) {
	const struct entity_type_def* d = entity_get_def(e->type);
	if(d && d->render)
		d->render(e, view, tick_delta);
}

void entity_call_teleport(struct entity* e, vec3 pos) {
	const struct entity_type_def* d = entity_get_def(e->type);
	if(d && d->teleport)
		d->teleport(e, pos);
	else
		entity_default_teleport(e, pos);
}
