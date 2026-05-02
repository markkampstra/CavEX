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

#include "../../graphics/gui_util.h"
#include "../../network/server_interface.h"
#include "../../platform/gfx.h"
#include "../../platform/input.h"
#include "../game_state.h"
#include "screen.h"

// Index into the buttons array.
static int selection;

#define BTN_RESPAWN 0
#define BTN_TITLE 1
#define BTN_COUNT 2

static const char* labels[BTN_COUNT] = {
	"Respawn",
	"Title screen",
};

static void screen_dead_reset(struct screen* s, int width, int height) {
	(void)s;
	(void)width;
	(void)height;
	input_pointer_enable(true);
	if(gstate.local_player)
		gstate.local_player->data.local_player.capture_input = false;
	selection = BTN_RESPAWN;
}

static void screen_dead_update(struct screen* s, float dt) {
	(void)s;
	(void)dt;

	if(input_pressed(IB_GUI_UP) && selection > 0)
		selection--;
	if(input_pressed(IB_GUI_DOWN) && selection < BTN_COUNT - 1)
		selection++;

	if(input_pressed(IB_GUI_CLICK)) {
		if(selection == BTN_RESPAWN) {
			// Phase 1 respawn is intentionally minimal: refill HP, zero
			// out velocity and the fall-distance accumulator, and hand
			// control back to the in-game screen. The player stays at the
			// position they died at -- if that's still in lava, they get
			// to die again. Phase 2/3 should add a server RPC that
			// teleports to the world's spawn point.
			if(gstate.local_player) {
				gstate.local_player->health
					= gstate.local_player->max_health;
				gstate.local_player->hurt_time = 0;
				gstate.local_player->death_time = 0;
				gstate.local_player->fall_distance = 0.0F;
				gstate.local_player->air = 300;
				glm_vec3_zero(gstate.local_player->vel);
				gstate.local_player->delay_destroy = -1;
			}
			screen_set(&screen_ingame);
		} else if(selection == BTN_TITLE) {
			screen_set(&screen_select_world);
			svin_rpc_send(&(struct server_rpc) {
				.type = SRPC_UNLOAD_WORLD,
			});
		}
	}
}

static void screen_dead_render2D(struct screen* s, int width, int height) {
	(void)s;
	gutil_bg();

	const char* title = "You Died!";
	gutil_text((width - gutil_font_width(title, 32)) / 2, height / 4, title, 32,
			   true);

	int btn_w = 200;
	int btn_h = 24;
	int spacing = 8;
	int total_h = BTN_COUNT * btn_h + (BTN_COUNT - 1) * spacing;
	int y0 = (height - total_h) / 2 + 32;

	for(int k = 0; k < BTN_COUNT; k++) {
		int x = (width - btn_w) / 2;
		int y = y0 + k * (btn_h + spacing);
		gfx_texture(false);
		// outline + fill; brighter when selected
		uint8_t fill = (selection == k) ? 96 : 48;
		gutil_texquad_col(x, y, 0, 0, 0, 0, btn_w, btn_h, 160, 160, 160, 255);
		gutil_texquad_col(x + 1, y + 1, 0, 0, 0, 0, btn_w - 2, btn_h - 2, fill,
						  fill, fill, 255);
		gfx_texture(true);
		const char* lab = labels[k];
		int tw = gutil_font_width(lab, 16);
		gutil_text(x + (btn_w - tw) / 2, y + (btn_h - 16) / 2, (char*)lab, 16,
				   true);
	}

	int icon_offset = 32;
	icon_offset += gutil_control_icon(icon_offset, IB_GUI_UP, "Change selection");
	icon_offset += gutil_control_icon(icon_offset, IB_GUI_CLICK, "Confirm");
}

struct screen screen_dead = {
	.reset = screen_dead_reset,
	.update = screen_dead_update,
	.render2D = screen_dead_render2D,
	.render3D = NULL,
	.render_world = false,
};
