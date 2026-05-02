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
#include "mob_common.h"

// Phase 3 first-pass pig. Wanders in a random direction for ~80-280 ticks,
// then picks a new one. Uses entity_try_move so it respects block
// collision; gravity falls back to the same constants entity_item uses
// since pigs have no special swim/fly behavior. Renders as a simple
// untextured cuboid for now -- a proper textured pig model lands once we
// have a mob texture atlas in assets/.

#define PIG_WALK_SPEED 0.06F
#define PIG_HEALTH 10

static bool entity_pig_tick_server(struct entity* e, struct server_local* s) {
	assert(e && s);
	mob_passive_tick(e, &ENTITY_DATA(e, entity_pig_data)->wander, s,
					 PIG_WALK_SPEED, 0.9F);
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

	// gfx_lighting(false) is required: when lighting is enabled, the vertex
	// shader replaces a_color with a lookup-table sample driven by the
	// a_light attribute. gfx_draw_quads (which render_model_box uses) only
	// binds a_pos/a_color/a_texcoord, so the stale a_light values from the
	// world chunk render bleed in -- the model renders pitch-black or
	// effectively invisible. Restore lighting after for everything that
	// follows in the render path (world alpha pass, etc.).
	gfx_lighting(false);

	// Bind the player skin until we have a real pig texture in the atlas.
	gfx_bind_texture(&texture_mob_char);

	mat4 model;
	glm_translate_make(model, pos_lerp);
	// Beta convention: model has head at -Z; the renderer applies a 180°
	// offset so a yaw-zero entity (facing +Z) ends up with its head on
	// the +Z side. Without this the pig walks backward.
	glm_rotate_y(model, e->orient[0] + GLM_PIf, model);
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

	// Four legs with walking animation. Standard quadruped trot
	// (mcp940 ModelQuadruped.setRotationAngles): legs at the same diagonal
	// share a phase, the other diagonal is offset by pi. Pivot at the top
	// of each leg so the box swings from the hip.
	const struct mob_wander* w = &ENTITY_DATA(e, entity_pig_data)->wander;
	float swing_a = mob_leg_swing_deg(w, 0.0F);
	float swing_b = mob_leg_swing_deg(w, GLM_PIf);
	struct leg {
		float x, z;
		float phase; // a == diagonal pair 1, b == pair 2
	} legs[4] = {
		{-3.0F, -7.0F, 0.0F},  // front-left
		{1.0F, -7.0F, 1.0F},   // front-right
		{-3.0F, 4.0F, 1.0F},   // back-left
		{1.0F, 4.0F, 0.0F},    // back-right
	};
	for(int k = 0; k < 4; k++) {
		float swing = (legs[k].phase == 0.0F) ? swing_a : swing_b;
		render_model_box(mv, (vec3) {legs[k].x, 6.0F, legs[k].z},
						 (vec3) {2.0F, 6.0F, 2.0F},
						 (vec3) {swing, 0.0F, 0.0F}, (ivec2) {0, 16},
						 (ivec3) {4, 6, 4}, 0.0F, false, brightness);
	}

	gfx_lighting(true);

	struct AABB shadow_bbox;
	aabb_setsize_centered(&shadow_bbox, 0.9F, 0.1F, 0.9F);
	aabb_translate(&shadow_bbox, pos_lerp[0], pos_lerp[1] - 0.04F, pos_lerp[2]);
	entity_shadow(e, &shadow_bbox, view);
}

static void entity_pig_on_death(struct entity* e, struct server_local* s) {
	// Beta pigs drop 0-2 raw porkchops on death.
	int count = (int)(rand_gen_flt(&s->rand_src) * 3.0F); // 0,1,2
	for(int k = 0; k < count; k++) {
		struct item_data drop = {
			.id = ITEM_PORKCHOP,
			.count = 1,
			.durability = 0,
		};
		vec3 dpos = {e->pos[0], e->pos[1] + 0.5F, e->pos[2]};
		server_local_spawn_item(dpos, &drop, false, s);
	}
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
	.on_death = entity_pig_on_death,
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
	pd->wander.ticks = 0;
	pd->wander.dx = 0.0F;
	pd->wander.dz = 0.0F;
	pd->wander.walk_distance = 0.0F;
	pd->wander.walk_amount = 0.0F;

	e->max_health = PIG_HEALTH;
	e->health = PIG_HEALTH;
}
