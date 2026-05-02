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

// Standard passive-mob server tick: picks a new wander direction every
// 80-280 ticks, walks at walk_speed, applies gravity, runs entity_try_move
// against an axis-aligned cube bbox of bbox_size on a side, applies
// friction, calls entity_living_tick. Returns nothing because passive
// mobs aren't auto-removed (their tick_server returns false).
void mob_passive_tick(struct entity* e, struct mob_wander* w,
					  struct server_local* s, float walk_speed,
					  float bbox_size);

#endif
