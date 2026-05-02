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

#include "../../network/server_local.h"
#include "../../util.h"
#include "mob_common.h"

float mob_leg_swing_deg(const struct mob_wander* w, float phase_offset) {
	// Matches ModelQuadruped/ModelChicken.setRotationAngles in MC:
	//   leg.rotateAngleX = cos(limbSwing * 0.6662 [+ pi]) * 1.4 *
	//                      limbSwingAmount  (in radians)
	float rad = cosf(w->walk_distance * 0.6662F + phase_offset) * 1.4F
		* w->walk_amount;
	return rad * (180.0F / GLM_PIf);
}

void mob_passive_tick(struct entity* e, struct mob_wander* w,
					  struct server_local* s, float walk_speed,
					  float bbox_size) {
	assert(e && w && s);

	glm_vec3_copy(e->pos, e->pos_old);
	glm_vec2_copy(e->orient, e->orient_old);

	if(w->ticks <= 0) {
		float angle = rand_gen_flt(&s->rand_src) * 2.0F * GLM_PIf;
		w->dx = sinf(angle);
		w->dz = cosf(angle);
		w->ticks = 80 + (int)(rand_gen_flt(&s->rand_src) * 200.0F);
		e->orient[0] = angle;
	} else {
		w->ticks--;
	}

	e->vel[0] = w->dx * walk_speed;
	e->vel[2] = w->dz * walk_speed;
	e->vel[1] -= 0.08F;

	struct AABB bbox;
	aabb_setsize_centered(&bbox, bbox_size, bbox_size, bbox_size);

	bool collision_xz = false;
	for(int k = 0; k < 3; k++)
		entity_try_move(e, e->pos, e->vel, &bbox, (size_t[]) {1, 0, 2}[k],
						&collision_xz, &e->on_ground);

	e->vel[0] *= 0.91F;
	e->vel[2] *= 0.91F;
	e->vel[1] *= 0.98F;
	if(e->on_ground)
		e->vel[1] = 0.0F;

	// Mirror EntityLivingBase.onUpdate from MC. limbSwingAmount targets
	// clamp(horiz_speed * 4, 0..1), smoothed at rate 0.4 per tick; legs
	// stop swinging when the mob stops moving. limbSwing is then a
	// running counter that grows by amount each tick (so the phase only
	// advances while the mob is actually walking).
	float speed = sqrtf(e->vel[0] * e->vel[0] + e->vel[2] * e->vel[2]);
	float target = speed * 4.0F;
	if(target > 1.0F)
		target = 1.0F;
	w->walk_amount += (target - w->walk_amount) * 0.4F;
	w->walk_distance += w->walk_amount;

	entity_living_tick(e);
}
