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

#include <math.h>

#include "../../block/blocks_data.h"
#include "../../graphics/render_model.h"
#include "../../network/server_local.h"
#include "../../platform/gfx.h"
#include "../../platform/texture.h"
#include "../entity.h"
#include "mob_common.h"

#define COW_WALK_SPEED 0.05F
#define COW_HEALTH 10

static bool entity_cow_tick_server(struct entity* e, struct server_local* s) {
	mob_passive_tick(e, &ENTITY_DATA(e, entity_cow_data)->wander, s,
					 COW_WALK_SPEED, 0.9F);
	return false;
}

static bool entity_cow_tick_client(struct entity* e) {
	return entity_default_client_tick(e);
}

static void entity_cow_render(struct entity* e, mat4 view, float tick_delta) {
	assert(e);

	vec3 pos_lerp;
	glm_vec3_lerp(e->pos_old, e->pos, tick_delta, pos_lerp);

	struct block_data in_block;
	entity_get_block(e, floorf(pos_lerp[0]), floorf(pos_lerp[1]),
					 floorf(pos_lerp[2]), &in_block);
	float brightness = gfx_lookup_light((in_block.torch_light << 4)
										| in_block.sky_light);

	gfx_lighting(false);
	gfx_bind_texture(&texture_mob_char);

	mat4 model;
	glm_translate_make(model, pos_lerp);
	glm_rotate_y(model, e->orient[0], model);
	glm_scale_uni(model, 1.0F / 16.0F);

	mat4 mv;
	glm_mat4_mul(view, model, mv);

	// Cow body: slightly larger than pig (12x18x10 in 1/16 units)
	render_model_box(mv, (vec3) {-6.0F, 5.0F, -9.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (ivec2) {18, 4},
					 (ivec3) {12, 18, 10}, 0.0F, false, brightness);

	// Head cube — taller and slightly bigger than pig's
	render_model_box(mv, (vec3) {-4.0F, 9.0F, -17.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (ivec2) {0, 0}, (ivec3) {8, 8, 6}, 0.0F, false, brightness);

	// Four legs (cow uses 12px tall vs pig's 6)
	struct {
		float x, z;
	} legs[4] = {
		{-4.0F, -7.0F}, {0.0F, -7.0F}, {-4.0F, 5.0F}, {0.0F, 5.0F},
	};
	for(int k = 0; k < 4; k++) {
		render_model_box(mv, (vec3) {legs[k].x, 0.0F, legs[k].z},
						 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
						 (ivec2) {0, 16}, (ivec3) {4, 12, 4}, 0.0F, false,
						 brightness);
	}

	gfx_lighting(true);

	struct AABB shadow_bbox;
	aabb_setsize_centered(&shadow_bbox, 0.9F, 0.1F, 0.9F);
	aabb_translate(&shadow_bbox, pos_lerp[0], pos_lerp[1] - 0.04F, pos_lerp[2]);
	entity_shadow(e, &shadow_bbox, view);
}

static const struct entity_type_def entity_cow_def = {
	.name = "cow",
	.data_size = sizeof(struct entity_cow_data),
	.default_max_health = COW_HEALTH,
	.tick_server = entity_cow_tick_server,
	.tick_client = entity_cow_tick_client,
	.render = entity_cow_render,
	.teleport = entity_default_teleport,
	.on_damage = NULL,
	.on_death = NULL,
};

void entity_cow_register(void) {
	entity_register_type(ENTITY_COW, &entity_cow_def);
}

void entity_cow(uint32_t id, struct entity* e, bool server, void* world) {
	assert(e && world);

	e->id = id;
	e->type = ENTITY_COW;

	entity_default_init(e, server, world);

	struct entity_cow_data* cd = ENTITY_DATA(e, entity_cow_data);
	cd->wander.ticks = 0;
	cd->wander.dx = 0.0F;
	cd->wander.dz = 0.0F;

	e->max_health = COW_HEALTH;
	e->health = COW_HEALTH;
}
