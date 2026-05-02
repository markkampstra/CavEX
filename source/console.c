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

#ifdef PLATFORM_PC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "daytime.h"
#include "game/game_state.h"
#include "platform/thread.h"
#include "platform/time.h"

#define CONSOLE_LINE_MAX 256
#define CONSOLE_QUEUE 8
#define CONSOLE_ARGV_MAX 8

struct console_msg {
	char line[CONSOLE_LINE_MAX];
};

static struct console_msg msg_pool[CONSOLE_QUEUE];
static struct thread_channel filled_chan;
static struct thread_channel empty_chan;

typedef void (*cmd_fn)(int argc, char** argv);
struct cmd {
	const char* name;
	const char* usage;
	cmd_fn fn;
};

static void cmd_help(int argc, char** argv);
static void cmd_quit(int argc, char** argv);
static void cmd_time(int argc, char** argv);
static void cmd_tp(int argc, char** argv);

static const struct cmd cmds[] = {
	{"help", "help", cmd_help},
	{"quit", "quit", cmd_quit},
	{"time", "time <ticks|day|night>", cmd_time},
	{"tp", "tp <x> <y> <z>", cmd_tp},
};
static const size_t cmd_count = sizeof(cmds) / sizeof(cmds[0]);

static void* console_reader_thread(void* user) {
	(void)user;
	while(1) {
		struct console_msg* m;
		if(!tchannel_receive(&empty_chan, (void**)&m, true))
			return NULL;

		if(!fgets(m->line, sizeof(m->line), stdin)) {
			// EOF or read error: park the buffer and stop reading
			tchannel_send(&empty_chan, m, true);
			return NULL;
		}

		tchannel_send(&filled_chan, m, true);
	}
}

void console_init(void) {
	tchannel_init(&filled_chan, CONSOLE_QUEUE);
	tchannel_init(&empty_chan, CONSOLE_QUEUE);
	for(size_t k = 0; k < CONSOLE_QUEUE; k++)
		tchannel_send(&empty_chan, &msg_pool[k], true);

	struct thread t;
	thread_create(&t, console_reader_thread, NULL, 4);

	printf("[console] ready, type `help` for commands\n");
}

static void execute(char* line) {
	size_t n = strlen(line);
	while(n > 0
		  && (line[n - 1] == '\n' || line[n - 1] == '\r'
			  || line[n - 1] == ' ' || line[n - 1] == '\t'))
		line[--n] = '\0';
	if(n == 0)
		return;

	char* argv[CONSOLE_ARGV_MAX];
	int argc = 0;
	char* p = line;
	while(argc < CONSOLE_ARGV_MAX && *p) {
		while(*p == ' ' || *p == '\t')
			p++;
		if(!*p)
			break;
		argv[argc++] = p;
		while(*p && *p != ' ' && *p != '\t')
			p++;
		if(*p)
			*p++ = '\0';
	}
	if(argc == 0)
		return;

	for(size_t k = 0; k < cmd_count; k++) {
		if(strcmp(argv[0], cmds[k].name) == 0) {
			cmds[k].fn(argc, argv);
			return;
		}
	}

	printf("[console] unknown command: %s (try `help`)\n", argv[0]);
}

void console_poll(void) {
	struct console_msg* m;
	while(tchannel_receive(&filled_chan, (void**)&m, false)) {
		execute(m->line);
		tchannel_send(&empty_chan, m, true);
	}
}

static void cmd_help(int argc, char** argv) {
	(void)argc;
	(void)argv;
	printf("[console] commands:\n");
	for(size_t k = 0; k < cmd_count; k++)
		printf("  %s\n", cmds[k].usage);
}

static void cmd_quit(int argc, char** argv) {
	(void)argc;
	(void)argv;
	gstate.quit = true;
}

static void cmd_time(int argc, char** argv) {
	if(argc < 2) {
		printf("[console] usage: time <ticks|day|night>\n");
		return;
	}

	long t;
	if(strcmp(argv[1], "day") == 0) {
		t = 0;
	} else if(strcmp(argv[1], "night") == 0) {
		t = DAY_LENGTH_TICKS / 2;
	} else {
		char* end;
		t = strtol(argv[1], &end, 10);
		if(*end != '\0') {
			printf("[console] invalid time: %s\n", argv[1]);
			return;
		}
	}

	gstate.world_time = t;
	gstate.world_time_start = time_get();
	printf("[console] time set to %ld\n", t);
}

static void cmd_tp(int argc, char** argv) {
	if(argc < 4) {
		printf("[console] usage: tp <x> <y> <z>\n");
		return;
	}
	if(!gstate.local_player) {
		printf("[console] no player loaded\n");
		return;
	}

	vec3 pos = {
		(float)atof(argv[1]),
		(float)atof(argv[2]),
		(float)atof(argv[3]),
	};
	gstate.local_player->teleport(gstate.local_player, pos);
	printf("[console] teleported to (%.2f, %.2f, %.2f)\n", pos[0], pos[1],
		   pos[2]);
}

#endif
