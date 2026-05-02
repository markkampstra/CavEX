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
#include "../../util.h"
#include "../entity.h"

// Phase 3 first-pass pig. Wanders in a random direction for ~80-280 ticks,
// then picks a new one. Uses entity_try_move so it respects block
// collision; gravity falls back to the same constants entity_item uses
// since pigs have no special swim/fly behavior. Renders as a simple
// untextured cuboid for now -- a proper textured pig model lands once we
// have a mob texture atlas in assets/.

#define PIG_WALK_SPEED 0.06F
#define PIG_GRAVITY 0.08F
#define PIG_HEALTH 10

static bool entity_pig_tick_server(struct entity* e, struct server_local* s) {
	assert(e && s);

	glm_vec3_copy(e->pos, e->pos_old);
	glm_vec2_copy(e->orient, e->orient_old);

	struct entity_pig_data* pd = ENTITY_DATA(e, entity_pig_data);

	if(pd->wander_ticks <= 0) {
		float angle = rand_gen_flt(&s->rand_src) * 2.0F * GLM_PIf;
		pd->wander_dx = sinf(angle);
		pd->wander_dz = cosf(angle);
		pd->wander_ticks = 80 + (int)(rand_gen_flt(&s->rand_src) * 200.0F);
		// face the direction we're walking so the rendered yaw matches
		e->orient[0] = angle;
	} else {
		pd->wander_ticks--;
	}

	e->vel[0] = pd->wander_dx * PIG_WALK_SPEED;
	e->vel[2] = pd->wander_dz * PIG_WALK_SPEED;
	e->vel[1] -= PIG_GRAVITY;

	struct AABB bbox;
	aabb_setsize_centered(&bbox, 0.9F, 0.9F, 0.9F);

	bool collision_xz = false;
	for(int k = 0; k < 3; k++)
		entity_try_move(e, e->pos, e->vel, &bbox, (size_t[]) {1, 0, 2}[k],
						&collision_xz, &e->on_ground);

	e->vel[0] *= 0.91F;
	e->vel[2] *= 0.91F;
	e->vel[1] *= 0.98F;
	if(e->on_ground)
		e->vel[1] = 0.0F;

	entity_living_tick(e);
	return false;
}

static bool entity_pig_tick_client(struct entity* e) {
	return entity_default_client_tick(e);
}

static void entity_pig_render(struct entity* e, mat4 view, float tick_delta) {
	assert(e);

	vec3 pos_lerp;
	glm_vec3_lerp(e->pos_old, e->pos, tick_delta, pos_lerp);

	struct block_data in_block;
	entity_get_block(e, floorf(pos_lerp[0]), floorf(pos_lerp[1]),
					 floorf(pos_lerp[2]), &in_block);
	float brightness = gfx_lookup_light((in_block.torch_light << 4)
										| in_block.sky_light);

	// Bind the player skin until we have a real pig texture in the atlas.
	// All UV coords below sample whatever's there, so the pig looks pink-ish
	// against the player's colour palette but is at least visible.
	gfx_bind_texture(&texture_mob_char);

	mat4 model;
	glm_translate_make(model, pos_lerp);
	glm_rotate_y(model, e->orient[0], model);
	glm_scale_uni(model, 1.0F / 16.0F);

	mat4 mv;
	glm_mat4_mul(view, model, mv);

	// Body cube: 10 wide x 16 long x 8 tall (in 1/16 block units).
	render_model_box(mv, (vec3) {-5.0F, 6.0F, -8.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (ivec2) {28, 8},
					 (ivec3) {10, 16, 8}, 0.0F, false, brightness);

	// Head cube
	render_model_box(mv, (vec3) {-4.0F, 8.0F, -16.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (ivec2) {0, 0}, (ivec3) {8, 8, 8}, 0.0F, false, brightness);

	// Four legs at the body's corners
	struct {
		float x, z;
	} legs[4] = {
		{-3.0F, -7.0F}, {1.0F, -7.0F}, {-3.0F, 4.0F}, {1.0F, 4.0F},
	};
	for(int k = 0; k < 4; k++) {
		render_model_box(mv, (vec3) {legs[k].x, 0.0F, legs[k].z},
						 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
						 (ivec2) {0, 16}, (ivec3) {4, 6, 4}, 0.0F, false,
						 brightness);
	}

	struct AABB shadow_bbox;
	aabb_setsize_centered(&shadow_bbox, 0.9F, 0.1F, 0.9F);
	aabb_translate(&shadow_bbox, pos_lerp[0], pos_lerp[1] - 0.04F, pos_lerp[2]);
	entity_shadow(e, &shadow_bbox, view);
}

static const struct entity_type_def entity_pig_def = {
	.name = "pig",
	.data_size = sizeof(struct entity_pig_data),
	.default_max_health = PIG_HEALTH,
	.tick_server = entity_pig_tick_server,
	.tick_client = entity_pig_tick_client,
	.render = entity_pig_render,
	.teleport = entity_default_teleport,
	.on_damage = NULL,
	.on_death = NULL,
};

void entity_pig_register(void) {
	entity_register_type(ENTITY_PIG, &entity_pig_def);
}

void entity_pig(uint32_t id, struct entity* e, bool server, void* world) {
	assert(e && world);

	e->id = id;
	e->type = ENTITY_PIG;

	entity_default_init(e, server, world);

	struct entity_pig_data* pd = ENTITY_DATA(e, entity_pig_data);
	pd->wander_ticks = 0;
	pd->wander_dx = 0.0F;
	pd->wander_dz = 0.0F;

	e->max_health = PIG_HEALTH;
	e->health = PIG_HEALTH;
}
