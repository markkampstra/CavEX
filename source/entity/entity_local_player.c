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

#include "../block/blocks_data.h"
#include "../platform/input.h"
#include "entity.h"

#define EYE_HEIGHT 1.62F

static void liquid_aabb(struct AABB* out, struct block_info* blk_info) {
	int block_height = (blk_info->block->metadata & 0x8) ?
		16 :
		(8 - blk_info->block->metadata) * 2 * 7 / 8;
	aabb_setsize(out, 1.0F, (float)block_height / 16.0F, 1.0F);
	aabb_translate(out, blk_info->x, blk_info->y, blk_info->z);
}

static bool test_in_lava(struct AABB* entity, struct block_info* blk_info) {
	if(blk_info->block->type != BLOCK_LAVA_FLOW
	   && blk_info->block->type != BLOCK_LAVA_STILL)
		return false;

	struct AABB bbox;
	liquid_aabb(&bbox, blk_info);
	return aabb_intersection(entity, &bbox);
}

static bool test_in_water(struct AABB* entity, struct block_info* blk_info) {
	if(blk_info->block->type != BLOCK_WATER_FLOW
	   && blk_info->block->type != BLOCK_WATER_STILL)
		return false;

	struct AABB bbox;
	liquid_aabb(&bbox, blk_info);
	return aabb_intersection(entity, &bbox);
}

static bool test_in_liquid(struct AABB* entity, struct block_info* blk_info) {
	return test_in_water(entity, blk_info) || test_in_lava(entity, blk_info);
}

static bool test_in_fire(struct AABB* entity, struct block_info* blk_info) {
	if(blk_info->block->type != BLOCK_FIRE)
		return false;
	struct AABB box;
	aabb_setsize(&box, 1.0F, 1.0F, 1.0F);
	aabb_translate(&box, blk_info->x, blk_info->y, blk_info->z);
	return aabb_intersection(entity, &box);
}

static bool test_in_cactus(struct AABB* entity, struct block_info* blk_info) {
	if(blk_info->block->type != BLOCK_CACTUS)
		return false;
	// Beta cactus is shrunk 1/16 per side; an entity must overlap the
	// shrunken interior, not just the cube, to take damage.
	struct AABB box;
	aabb_setsize(&box, 14.0F / 16.0F, 1.0F, 14.0F / 16.0F);
	aabb_translate(&box, blk_info->x + 1.0F / 16.0F, blk_info->y,
				   blk_info->z + 1.0F / 16.0F);
	return aabb_intersection(entity, &box);
}

static bool entity_tick(struct entity* e) {
	assert(e);

	glm_vec3_copy(e->pos, e->pos_old);
	glm_vec2_copy(e->orient, e->orient_old);

	for(int k = 0; k < 3; k++)
		if(fabsf(e->vel[k]) < 0.005F)
			e->vel[k] = 0.0F;

	struct AABB bbox;
	aabb_setsize_centered(&bbox, 0.6F, 1.0F, 0.6F);
	aabb_translate(&bbox, e->pos[0], e->pos[1] + 1.8F / 2.0F - EYE_HEIGHT,
				   e->pos[2]);

	bool in_water = entity_intersection(e, &bbox, test_in_water);
	bool in_lava = entity_intersection(e, &bbox, test_in_lava);

	float slipperiness
		= (in_lava || in_water) ? 1.0F : (e->on_ground ? 0.6F : 1.0F);

	int forward = 0;
	int strafe = 0;
	bool jumping = false;

	struct entity_local_player* lp = ENTITY_DATA(e, entity_local_player);
	if(lp->capture_input) {
		if(input_held(IB_FORWARD))
			forward++;

		if(input_held(IB_BACKWARD))
			forward--;

		if(input_held(IB_RIGHT))
			strafe++;

		if(input_held(IB_LEFT))
			strafe--;

		jumping = input_held(IB_JUMP);
	}

	int dist = forward * forward + strafe * strafe;

	if(dist > 0) {
		float distf = fmaxf(sqrtf(dist), 1.0F);
		float dx = (forward * sinf(e->orient[0]) - strafe * cosf(e->orient[0]))
			/ distf;
		float dy = (strafe * sinf(e->orient[0]) + forward * cosf(e->orient[0]))
			/ distf;

		e->vel[0] += 0.1F * powf(0.6F / slipperiness, 3.0F) * dx;
		e->vel[2] += 0.1F * powf(0.6F / slipperiness, 3.0F) * dy;
	}

	if(lp->jump_ticks > 0)
		lp->jump_ticks--;

	if(jumping) {
		if(in_water || in_lava) {
			e->vel[1] += 0.04F;
		} else if(e->on_ground && lp->jump_ticks == 0) {
			e->vel[1] = 0.42F;
			lp->jump_ticks = 10;
		}
	} else {
		lp->jump_ticks = 0;
	}

	aabb_setsize_centered(&bbox, 0.6F, 1.8F, 0.6F);
	aabb_translate(&bbox, 0.0F, 1.8F / 2.0F - EYE_HEIGHT, 0.0F);

	// unstuck player
	struct AABB tmp1 = bbox, tmp2 = bbox;
	float unstuck_move = 0.01F;
	aabb_translate(&tmp1, e->pos[0], e->pos[1], e->pos[2]);
	aabb_translate(&tmp2, e->pos[0], e->pos[1] + unstuck_move, e->pos[2]);

	// is the player stuck in the floor due to inaccuracy?
	if(entity_aabb_intersection(e, &tmp1)
	   && !entity_aabb_intersection(e, &tmp2)) {
		e->pos[1] += unstuck_move;
	}

	vec3 new_pos, new_vel;
	glm_vec3_copy(e->pos, new_pos);
	glm_vec3_copy(e->vel, new_vel);

	bool collision_xz = false;

	for(int k = 0; k < 3; k++)
		entity_try_move(e, e->pos, e->vel, &bbox, (size_t[]) {1, 0, 2}[k],
						&collision_xz, &e->on_ground);

	if(e->on_ground) {
		bool collision = false;
		bool ground = e->on_ground;

		new_vel[1] = 0.6F;
		entity_try_move(e, new_pos, new_vel, &bbox, 1, &collision, &ground);

		new_vel[1] = 0.0F;
		entity_try_move(e, new_pos, new_vel, &bbox, 0, &collision, &ground);
		entity_try_move(e, new_pos, new_vel, &bbox, 2, &collision, &ground);

		new_vel[1] = -0.6F;
		entity_try_move(e, new_pos, new_vel, &bbox, 1, &collision, &ground);

		if(new_pos[1] > e->pos_old[1]
		   && glm_vec3_distance2(e->pos_old, e->pos)
			   < glm_vec3_distance2(e->pos_old, new_pos)) {
			collision_xz = collision;
			e->on_ground = ground;
			glm_vec3_copy(new_pos, e->pos);
			glm_vec3_copy(new_vel, e->vel);
		}
	}

	if(in_lava) {
		e->vel[0] *= 0.5F;
		e->vel[2] *= 0.5F;
		e->vel[1] = e->vel[1] * 0.5F - 0.02F;
	} else if(in_water) {
		e->vel[0] *= 0.8F;
		e->vel[2] *= 0.8F;
		e->vel[1] = e->vel[1] * 0.8F - 0.02F;
	} else {
		e->vel[0] *= slipperiness * 0.91F;
		e->vel[2] *= slipperiness * 0.91F;
		e->vel[1] -= 0.08F;

		struct block_data blk;
		if(entity_get_block(e, floorf(e->pos[0]),
							floorf(e->pos[1] - EYE_HEIGHT), floorf(e->pos[2]),
							&blk)
		   && blk.type == BLOCK_LADDER) {
			if(collision_xz)
				e->vel[1] = 0.12F;

			e->vel[0] = fmaxf(fminf(e->vel[0], 0.15F), -0.15F);
			e->vel[1] = fmaxf(e->vel[1], -0.15F);
			e->vel[2] = fmaxf(fminf(e->vel[2], 0.15F), -0.15F);
		}

		e->vel[1] *= 0.98F;
	}

	if(collision_xz && (in_lava || in_water)) {
		struct AABB tmp;
		aabb_setsize_centered(&tmp, 0.6F, 1.8F, 0.6F);
		aabb_translate(&tmp, e->pos[0] + e->vel[0],
					   e->pos[1] + e->vel[1] + 1.8F / 2.0F - 1.62F + 0.6F,
					   e->pos[2] + e->vel[2]);

		if(!entity_intersection(e, &tmp, test_in_liquid))
			e->vel[1] = 0.3F;
	}

	// Fall-damage tracking. Accumulate downward travel each tick; when
	// the player lands (on_ground), turn that into damage with the Beta
	// formula and reset. Water/lava cancel fall damage.
	float dy = e->pos[1] - e->pos_old[1];
	if(dy < 0.0F)
		e->fall_distance += -dy;
	if(e->on_ground) {
		int fall_dmg = (int)ceilf(e->fall_distance - 3.0F);
		if(fall_dmg > 0 && !in_water && !in_lava)
			entity_damage(e, fall_dmg, DMG_FALL);
		e->fall_distance = 0.0F;
	}

	// Drowning: when the head AABB is in water, run the air timer down. At
	// air <= 0 take 2 HP every 20 ticks, matching Beta. Refill instantly
	// once the head clears.
	struct AABB head_bbox;
	aabb_setsize_centered(&head_bbox, 0.6F, 0.6F, 0.6F);
	aabb_translate(&head_bbox, e->pos[0], e->pos[1], e->pos[2]);
	bool head_in_water = entity_intersection(e, &head_bbox, test_in_water);
	if(head_in_water) {
		if(e->air > 0) {
			e->air--;
		} else {
			e->air--;
			if(e->air <= -20) {
				entity_damage(e, 2, DMG_DROWN);
				e->air = 0;
			}
		}
	} else if(e->air < 300) {
		e->air = 300;
	}

	// Lava / fire / cactus contact damage. The hurt_time i-frame in
	// entity_damage gates the rate so attempting damage every tick still
	// produces sensible numbers (~ 8 HP/sec lava, 2 HP/sec fire/cactus).
	if(in_lava)
		entity_damage(e, 4, DMG_LAVA);
	if(entity_intersection(e, &bbox, test_in_fire))
		entity_damage(e, 1, DMG_FIRE);
	if(entity_intersection(e, &bbox, test_in_cactus))
		entity_damage(e, 1, DMG_CACTUS);

	entity_living_tick(e);
	return false;
}

bool entity_local_player_block_collide(vec3 pos, struct block_info* blk_info) {
	assert(pos && blk_info);

	struct AABB bbox;
	aabb_setsize_centered(&bbox, 0.6F, 1.8F, 0.6F);
	aabb_translate(&bbox, pos[0], 1.8F / 2.0F - EYE_HEIGHT + pos[1], pos[2]);

	return entity_block_aabb_test(&bbox, blk_info);
}

static const struct entity_type_def entity_local_player_def = {
	.name = "local_player",
	.data_size = sizeof(struct entity_local_player),
	.default_max_health = 20,
	.tick_client = entity_tick,
	.tick_server = NULL,
	.render = NULL,
	.teleport = entity_default_teleport,
	.on_damage = NULL,
	.on_death = NULL,
};

void entity_local_player_register(void) {
	entity_register_type(ENTITY_LOCAL_PLAYER, &entity_local_player_def);
}

void entity_local_player(uint32_t id, struct entity* e, struct world* w) {
	assert(e && w);

	e->id = id;
	e->type = ENTITY_LOCAL_PLAYER;

	entity_default_init(e, false, w);

	struct entity_local_player* lp = ENTITY_DATA(e, entity_local_player);
	lp->jump_ticks = 0;
	lp->capture_input = false;

	// Beta player has 20 HP (10 hearts) and 300 ticks of breath.
	e->max_health = 20;
	e->health = 20;
	e->air = 300;
}
