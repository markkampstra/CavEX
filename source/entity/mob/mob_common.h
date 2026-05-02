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

#ifndef MOB_COMMON_H
#define MOB_COMMON_H

#include "../entity.h"

// struct mob_wander lives in entity.h so per-mob data structs (in entity.h)
// can embed it. mob_common.h just provides the helpers that operate on it.

// Beta-equivalent leg-swing angle in degrees, given the mob's wander state.
// phase_offset = 0 for "this leg moves with limbSwing", GLM_PIf for "this
// leg moves opposite". Renders should pass swing_deg to render_model_box's
// rotation[0] (X-axis) so legs pivot forward/back at the hip.
float mob_leg_swing_deg(const struct mob_wander* w, float phase_offset);

// Standard passive-mob server tick: picks a new wander direction every
// 80-280 ticks, walks at walk_speed, applies gravity, runs entity_try_move
// against an axis-aligned cube bbox of bbox_size on a side, applies
// friction, calls entity_living_tick. Returns nothing because passive
// mobs aren't auto-removed (their tick_server returns false).
void mob_passive_tick(struct entity* e, struct mob_wander* w,
					  struct server_local* s, float walk_speed,
					  float bbox_size);

// Client-side counterpart. Replaces entity_default_client_tick for mob
// types: copies network_pos into pos_old/pos as usual, but also advances
// walk_amount and walk_distance from the observed per-tick movement so
// the leg-swing animation runs on the client without the server needing
// to sync wander state across the wire.
void mob_passive_tick_client(struct entity* e, struct mob_wander* w);

#endif
