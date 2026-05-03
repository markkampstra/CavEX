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

	// CavEX render_model_box origin = MC_textureOffset + (dz, dz). See
	// note in entity_pig.c.
	//
	// Body (ModelSheep2). addBox(-4,-10,-7, 8,16,6) at rotPoint(0,5,2)
	// with rotateAngleX = pi/2. Pre-rotation CavEX dims (8, 6, 16).
	// MC tx=28, ty=8, dz=6 -> CavEX origin (34, 14). World X -4..4, Y
	// 12..18, Z -8..8 after rotation+translate.
	render_model_box(mv, (vec3) {-4.0F, 18.0F, -8.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {90.0F, 0.0F, 0.0F},
					 (ivec2) {34, 14}, (ivec3) {8, 6, 16}, 0.0F, false,
					 brightness);

	// Head (ModelSheep2). addBox(-3,-4,-6, 6,6,8) at rotPoint(0, 6, -8).
	// MC tx=0, ty=0, dz=8 -> CavEX origin (8, 8). World X -3..3, Y 14..20,
	// Z -14..-6.
	render_model_box(mv, (vec3) {-3.0F, 14.0F, -14.0F},
					 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
					 (ivec2) {8, 8}, (ivec3) {6, 8, 6}, 0.0F, false, brightness);

	// Four legs from ModelQuadruped(legHeight=12): addBox(-2,0,-2, 4,12,4)
	// -> CavEX (4, 4, 12). Pivot at top so the box swings from the hip.
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
						 (vec3) {2.0F, 12.0F, 2.0F},
						 (vec3) {swing, 0.0F, 0.0F}, (ivec2) {4, 20},
						 (ivec3) {4, 4, 12}, 0.0F, false, brightness);
	}

	// Wool fur overlay. ModelSheep1 redefines body+head+legs as separate
	// (smaller) cuboids puffed outward via the padding param. Skipped if
	// sheared. Color tint is not applied yet -- needs a shader-level change
	// to multiply v_color into the texture sample; for now wool is white
	// regardless of the dye-color metadata.
	if(!ENTITY_DATA(e, entity_sheep_data)->sheared) {
		gfx_bind_texture(&texture_mob_sheep_fur);

		// Wool body: ModelSheep1 addBox(-4,-10,-7, 8,16,6, 1.75) at the
		// same rotPoint as the base body. Same dims as base, padding
		// 1.75 puffs outward.
		render_model_box(mv, (vec3) {-4.0F, 18.0F, -8.0F},
						 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {90.0F, 0.0F, 0.0F},
						 (ivec2) {34, 14}, (ivec3) {8, 6, 16}, 1.75F, false,
						 brightness);

		// Wool head: ModelSheep1 addBox(-3,-4,-4, 6,6,6, 0.6) at
		// rotPoint(0, 6, -8). Smaller than ModelSheep2 head (depth 6 vs 8).
		// MC dz=6 -> CavEX origin (6, 6). World X -3..3, Y 16..22, Z -12..-6.
		render_model_box(mv, (vec3) {-3.0F, 16.0F, -12.0F},
						 (vec3) {0.0F, 0.0F, 0.0F}, (vec3) {0.0F, 0.0F, 0.0F},
						 (ivec2) {6, 6}, (ivec3) {6, 6, 6}, 0.6F, false,
						 brightness);

		// Wool legs: ModelSheep1 addBox(-2,0,-2, 4,6,4, 0.5) at rotPoint
		// (+/-3, 12, +7/-5). Half-height (6) vs the body legs (12), so
		// these only cover the top half of each leg. CavEX leg world Y
		// 6..12 (vs body legs at 0..12).
		for(int k = 0; k < 4; k++) {
			bool pair_a = (legs[k].x * legs[k].z) < 0.0F;
			float swing = pair_a ? swing_a : swing_b;
			render_model_box(mv, (vec3) {legs[k].x, 12.0F, legs[k].z},
							 (vec3) {2.0F, 6.0F, 2.0F},
							 (vec3) {swing, 0.0F, 0.0F}, (ivec2) {4, 20},
							 (ivec3) {4, 4, 6}, 0.5F, false, brightness);
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
