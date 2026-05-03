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

#define SHEEP_WALK_SPEED 0.05F
#define SHEEP_HEALTH 8

static bool entity_sheep_tick_server(struct entity* e, struct server_local* s) {
	mob_passive_tick(e, &ENTITY_DATA(e, entity_sheep_data)->wander, s,
					 SHEEP_WALK_SPEED, 0.9F);
	return false;
}

static bool entity_sheep_tick_client(struct entity* e) {
	mob_passive_tick_client(e, &ENTITY_DATA(e, entity_sheep_data)->wander);
	return false;
}

static void entity_sheep_render(struct entity* e, mat4 view, float tick_delta) {
	assert(e);

	vec3 pos_lerp;
	glm_vec3_lerp(e->pos_old, e->pos, tick_delta, pos_lerp);

	struct block_data in_block;
	entity_get_block(e, floorf(pos_lerp[0]), floorf(pos_lerp[1]),
					 floorf(pos_lerp[2]), &in_block);
	float brightness = gfx_lookup_light((in_block.torch_light << 4)
										| in_block.sky_light);

	gfx_lighting(false);
	gfx_bind_texture(&texture_mob_sheep);

	mat4 model;
	glm_translate_make(model, pos_lerp);
	glm_rotate_y(model, e->orient[0] + GLM_PIf, model);
	glm_scale_uni(model, 1.0F / 16.0F);

	mat4 mv;
	glm_mat4_mul(view, model, mv);

	const struct mob_wander* w = &ENTITY_DATA(e, entity_sheep_data)->wander;
	float swing_a = mob_leg_swing_deg(w, 0.0F);
	float swing_b = mob_leg_swing_deg(w, GLM_PIf);

	// Body. ModelSheep2.body addBox(-4,-10,-7, 8,16,6) at rotPoint(0,5,2)
	// with rotateAngleX = pi/2. Pre-rotation CavEX dims (8, 6, 16).
	// World X -4..4, Y 12..18, Z -8..8 after rotation+translate.
	render_model_box(mv, (vec3) {-4.0F, 18.0F, -8.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {90.0F, 0.0F, 0.0F},
					 (ivec2) {28, 8}, (ivec3) {8, 6, 16}, 0.0F, false,
					 brightness);

	// Head. ModelSheep2.head addBox(-3,-4,-6, 6,6,8) at rotPoint(0, 6, -8).
	// World X -3..3, Y 14..20, Z -14..-6.
	render_model_box(mv, (vec3) {-3.0F, 14.0F, -14.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (ivec2) {0, 0}, (ivec3) {6, 8, 6}, 0.0F, false, brightness);

	// Four legs (3x3x12 vertical) with the standard quadruped trot.
	struct leg {
		float x, z;
	} legs[4] = {
		{-3.0F, 7.0F},
		{3.0F, 7.0F},
		{-3.0F, -5.0F},
		{3.0F, -5.0F},
	};
	for(int k = 0; k < 4; k++) {
		bool pair_a = (legs[k].x * legs[k].z) < 0.0F;
		float swing = pair_a ? swing_a : swing_b;
		render_model_box(mv, (vec3) {legs[k].x, 12.0F, legs[k].z},
						 (vec3) {1.5F, 12.0F, 1.5F},
						 (vec3) {swing, 0.0F, 0.0F}, (ivec2) {0, 16},
						 (ivec3) {3, 3, 12}, 0.0F, false, brightness);
	}

	// Wool fur overlay: same body and legs with the fur texture and a
	// padding (puffs the box outward by `padding` on each side per
	// render_model_box). Matches ModelSheep1: body padding 1.75, legs 0.5.
	// Skipped if the sheep has been sheared. Color tint is not applied
	// yet -- needs a shader-level change to multiply v_color into the
	// texture sample; for now wool is white regardless of the
	// dye-color metadata.
	if(!ENTITY_DATA(e, entity_sheep_data)->sheared) {
		gfx_bind_texture(&texture_mob_sheep_fur);
		// Wool body: same dims/rotation as base body, with padding 1.75
		// to puff outward (ModelSheep1.body padding).
		render_model_box(mv, (vec3) {-4.0F, 18.0F, -8.0F},
						 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {90.0F, 0.0F, 0.0F},
						 (ivec2) {28, 8}, (ivec3) {8, 6, 16}, 1.75F, false,
						 brightness);
		for(int k = 0; k < 4; k++) {
			bool pair_a = (legs[k].x * legs[k].z) < 0.0F;
			float swing = pair_a ? swing_a : swing_b;
			render_model_box(mv, (vec3) {legs[k].x, 12.0F, legs[k].z},
							 (vec3) {1.5F, 12.0F, 1.5F},
							 (vec3) {swing, 0.0F, 0.0F}, (ivec2) {0, 16},
							 (ivec3) {3, 3, 12}, 0.5F, false, brightness);
		}
	}

	gfx_lighting(true);

	struct AABB shadow_bbox;
	aabb_setsize_centered(&shadow_bbox, 0.9F, 0.1F, 0.9F);
	aabb_translate(&shadow_bbox, pos_lerp[0], pos_lerp[1] - 0.04F, pos_lerp[2]);
	entity_shadow(e, &shadow_bbox, view);
}

static void entity_sheep_on_death(struct entity* e, struct server_local* s) {
	struct entity_sheep_data* sd = ENTITY_DATA(e, entity_sheep_data);
	if(!sd->sheared) {
		struct item_data drop = {
			.id = BLOCK_WOOL,
			.count = 1,
			.durability = sd->wool_color, // dye color metadata
		};
		vec3 dpos = {e->pos[0], e->pos[1] + 0.5F, e->pos[2]};
		server_local_spawn_item(dpos, &drop, false, s);
	}
}

static const struct entity_type_def entity_sheep_def = {
	.name = "sheep",
	.data_size = sizeof(struct entity_sheep_data),
	.default_max_health = SHEEP_HEALTH,
	.tick_server = entity_sheep_tick_server,
	.tick_client = entity_sheep_tick_client,
	.render = entity_sheep_render,
	.teleport = entity_default_teleport,
	.on_damage = NULL,
	.on_death = entity_sheep_on_death,
};

void entity_sheep_register(void) {
	entity_register_type(ENTITY_SHEEP, &entity_sheep_def);
}

void entity_sheep(uint32_t id, struct entity* e, bool server, void* world,
				  uint8_t wool_color) {
	assert(e && world);
	e->id = id;
	e->type = ENTITY_SHEEP;
	entity_default_init(e, server, world);
	struct entity_sheep_data* sd = ENTITY_DATA(e, entity_sheep_data);
	sd->wander.ticks = 0;
	sd->wander.dx = 0.0F;
	sd->wander.dz = 0.0F;
	sd->wander.walk_distance = 0.0F;
	sd->wander.walk_amount = 0.0F;
	sd->wander.tick_count = 0;
	sd->wool_color = wool_color & 0x0F;
	sd->sheared = false;
	e->max_health = SHEEP_HEALTH;
	e->health = SHEEP_HEALTH;
}
