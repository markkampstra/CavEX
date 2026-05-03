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

#define CHICKEN_WALK_SPEED 0.05F
#define CHICKEN_HEALTH 4

static bool entity_chicken_tick_server(struct entity* e, struct server_local* s) {
	mob_passive_tick(e, &ENTITY_DATA(e, entity_chicken_data)->wander, s,
					 CHICKEN_WALK_SPEED, 0.4F);
	return false;
}

static bool entity_chicken_tick_client(struct entity* e) {
	mob_passive_tick_client(e, &ENTITY_DATA(e, entity_chicken_data)->wander);
	return false;
}

static void entity_chicken_render(struct entity* e, mat4 view, float tick_delta) {
	assert(e);

	vec3 pos_lerp;
	glm_vec3_lerp(e->pos_old, e->pos, tick_delta, pos_lerp);

	struct block_data in_block;
	entity_get_block(e, floorf(pos_lerp[0]), floorf(pos_lerp[1]),
					 floorf(pos_lerp[2]), &in_block);
	float brightness = gfx_lookup_light((in_block.torch_light << 4)
										| in_block.sky_light);

	gfx_lighting(false);
	gfx_bind_texture(&texture_mob_chicken);

	mat4 model;
	glm_translate_make(model, pos_lerp);
	glm_rotate_y(model, e->orient[0] + GLM_PIf, model);
	glm_scale_uni(model, 1.0F / 16.0F);

	mat4 mv;
	glm_mat4_mul(view, model, mv);

	const struct mob_wander* w = &ENTITY_DATA(e, entity_chicken_data)->wander;
	float leg_a = mob_leg_swing_deg(w, 0.0F);
	float leg_b = mob_leg_swing_deg(w, GLM_PIf);

	// Body cube: 6 wide x 8 deep x 6 tall after the chicken's pi/2 X
	// rotation in the reference model. box[] is (X, Z, Y).
	render_model_box(mv, (vec3) {-3.0F, 5.0F, -4.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (ivec2) {0, 9},
					 (ivec3) {6, 8, 6}, 0.0F, false, brightness);

	// Head cube: 4 wide x 3 deep x 6 tall, just in front of the body.
	render_model_box(mv, (vec3) {-2.0F, 11.0F, -6.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (ivec2) {0, 0}, (ivec3) {4, 3, 6}, 0.0F, false, brightness);

	// Two legs (3x3x5 vertical) alternating; matches ModelChicken.
	// Pivot at top-front of each leg so swing axis is the hip joint.
	render_model_box(mv, (vec3) {1.0F, 5.0F, 1.0F},
					 (vec3) {1.0F, 5.0F, 3.0F},
					 (vec3) {leg_a, 0.0F, 0.0F}, (ivec2) {26, 0},
					 (ivec3) {3, 3, 5}, 0.0F, false, brightness);
	render_model_box(mv, (vec3) {-2.0F, 5.0F, 1.0F},
					 (vec3) {1.0F, 5.0F, 3.0F},
					 (vec3) {leg_b, 0.0F, 0.0F}, (ivec2) {26, 0},
					 (ivec3) {3, 3, 5}, 0.0F, false, brightness);

	// Wings: oscillating flap around Z (vertical-flap axis). The reference
	// uses an unbounded rotateAngleZ = ageInTicks (radians) which renders
	// as a continuous blur; an oscillation looks closer to a real chicken.
	int t = ENTITY_DATA(e, entity_chicken_data)->wander.tick_count;
	float wing_deg = sinf((float)t * 0.3F) * 25.0F;
	// Right wing (mob's right -> -X side). Pivot at body-attached top edge.
	render_model_box(mv, (vec3) {-3.0F, 11.0F, 0.0F},
					 (vec3) {0.0F, 4.0F, 3.0F},
					 (vec3) {0.0F, 0.0F, wing_deg}, (ivec2) {24, 13},
					 (ivec3) {1, 6, 4}, 0.0F, false, brightness);
	// Left wing (+X side). Mirrored UV.
	render_model_box(mv, (vec3) {3.0F, 11.0F, 0.0F},
					 (vec3) {1.0F, 4.0F, 3.0F},
					 (vec3) {0.0F, 0.0F, -wing_deg}, (ivec2) {24, 13},
					 (ivec3) {1, 6, 4}, 0.0F, true, brightness);

	gfx_lighting(true);

	struct AABB shadow_bbox;
	aabb_setsize_centered(&shadow_bbox, 0.4F, 0.1F, 0.4F);
	aabb_translate(&shadow_bbox, pos_lerp[0], pos_lerp[1] - 0.04F, pos_lerp[2]);
	entity_shadow(e, &shadow_bbox, view);
}

static void entity_chicken_on_death(struct entity* e, struct server_local* s) {
	int feathers = (int)(rand_gen_flt(&s->rand_src) * 3.0F);
	for(int k = 0; k < feathers; k++) {
		struct item_data drop = {
			.id = ITEM_FEATHER,
			.count = 1,
			.durability = 0,
		};
		vec3 dpos = {e->pos[0], e->pos[1] + 0.3F, e->pos[2]};
		server_local_spawn_item(dpos, &drop, false, s);
	}
}

static const struct entity_type_def entity_chicken_def = {
	.name = "chicken",
	.data_size = sizeof(struct entity_chicken_data),
	.default_max_health = CHICKEN_HEALTH,
	.tick_server = entity_chicken_tick_server,
	.tick_client = entity_chicken_tick_client,
	.render = entity_chicken_render,
	.teleport = entity_default_teleport,
	.on_damage = NULL,
	.on_death = entity_chicken_on_death,
};

void entity_chicken_register(void) {
	entity_register_type(ENTITY_CHICKEN, &entity_chicken_def);
}

void entity_chicken(uint32_t id, struct entity* e, bool server, void* world) {
	assert(e && world);
	e->id = id;
	e->type = ENTITY_CHICKEN;
	entity_default_init(e, server, world);
	struct entity_chicken_data* cd = ENTITY_DATA(e, entity_chicken_data);
	cd->wander.ticks = 0;
	cd->wander.dx = 0.0F;
	cd->wander.dz = 0.0F;
	cd->wander.walk_distance = 0.0F;
	cd->wander.walk_amount = 0.0F;
	cd->wander.tick_count = 0;
	e->max_health = CHICKEN_HEALTH;
	e->health = CHICKEN_HEALTH;
}
