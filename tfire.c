/* tfire.c */
/* shamelessly lifted from: http://fabiensanglard.net/doom_fire_psx */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <ncurses.h>
#include <signal.h>

#define DELAY_MAX 100
#define DELAY_DEF 50

/* fire 'levels' */
#define FIRE_MAX 7

/* chars/colors are FIRE_MAX + 1 elements, first one 'blank' */
const char firechars[8] = { ' ', '.', 'o', '+', 'x', 'X', '*', '#' };
const int fire256colors[8] = { 232, 52, 88, 160, 166, 142, 185, 186 };
const int gray256colors[8] = { 232, 235, 238, 240, 242, 246, 250, 252 };

int *pixel_ptr;

int bgcolor = -1;
int delay = DELAY_DEF;

int color_flag = 0;
int fade_flag = 0;
int inverse_flag = 0;

void quit() {
	free(pixel_ptr);
	endwin();
	exit(0);
}

void spread(int x, int y) {
	int src = y * COLS + x;
	int rnd = rand() * 3 & 3;
	int dst = src - rnd + 1;

	if (pixel_ptr[src] == 0) {
		src -= COLS;

		if (src < 0) {
			src = 0;
		}

		pixel_ptr[src] = 0;

		/* wredrawln() may fix flickering */
		/* also seems to fix stuck characters on linux console */
		wredrawln(stdscr, y, 1);
	} else {
		dst -= COLS;

		if (dst < 0) {
			dst = 0;
		}

		/* static row of decreasing numbers */
		/* pixel_ptr[src - COLS] = pixel_ptr[src] - 1; */

		/* move the lines a bit */
		/* lines flicker, try wredrawln() above */
		/* pixel_ptr[src - COLS] = pixel_ptr[src] - (rand() & 1); */

		/* as above, with left/right movement */
		pixel_ptr[dst] = pixel_ptr[src] - (rand() & 1);
	}
}

void draw () {
	for (int x = 0; x < COLS; x++) {
		for (int y = 0; y < LINES; y++) {
			spread(x, y);

			int p = pixel_ptr[y * COLS + x];
			int c = firechars[p];

			if (inverse_flag && p != 0) {
				standout();
				c = ' ';
			}

			/* mvprintw(y, x, "%i", p); */
			/* mvaddch(y, x, firechars[p]); */
			mvaddch(y, x, c | COLOR_PAIR(p));

			if (inverse_flag) {
				standend();
			}
		}
	}
}

void init_colors() {
	if (color_flag < 1 || !has_colors() || !can_change_color()) {
		color_flag = 0;
		return;
	}

	start_color();
	use_default_colors();

	for (int i = 1; i < 9; i++) {
		switch (color_flag) {
			/* 256 color red/orange/yellow */
			case 1:
				init_pair(i, fire256colors[i], bgcolor);
				break;
			/* 256 color grayscale */
			case 2:
				init_pair(i, gray256colors[i], bgcolor);
				break;
			default:
				break;
		}
	}

	bkgd(' ' | COLOR_PAIR(1));
}

void set_row(int y, int c) {
	y < 0 ? y = 0 : y;
	y > LINES ? y = LINES : y;
	c < 0 ? c = 0 : c;
	c > FIRE_MAX ? c = FIRE_MAX : c;

	for (int i = 0; i < COLS; i ++) {
		pixel_ptr[y * COLS + i] = c;
	}
}

void init_pixels () {
	pixel_ptr = calloc(LINES * COLS, sizeof(int));
}

void resize_win(int sig) {
	endwin();
	refresh();
	free(pixel_ptr);
	init_pixels();
	set_row(LINES - 1, FIRE_MAX);
	refresh();
}

void fade (int in) {
	/* first nonblank char to last char in firechars[] */
	static int fadeinch = 1;
	static int fadeoutch = FIRE_MAX;

	if (in) {
		set_row(LINES - 1, (fadeinch > FIRE_MAX ?
					fadeinch = FIRE_MAX : fadeinch++));
	} else {
		set_row(LINES - 1, (fadeoutch < 0 ?
					fadeoutch = 0 : fadeoutch--));
	}
}

int main (int argc, char *argv[]) {
	int opt, busy;
	int fadeout;
	int fademax;
	int fadetime;

	signal(SIGWINCH, resize_win);

	while ((opt = getopt(argc, argv, ":Bbcd:fgi")) != -1) {
		switch (opt) {
			case 'B':
				bgcolor = 16;
				break;
			case 'b':
				bgcolor = 0;
				break;
			case 'c':
				color_flag = 1;
				break;
			case 'd':
				delay = atoi(optarg);
				if (delay < 0 || delay > DELAY_MAX || !isdigit(*optarg)) {
				    delay = DELAY_DEF;
				}
				break;
			case 'f':
				fade_flag = 1;
				break;
			case 'g':
				color_flag = 2;
				break;
			case 'i':
				inverse_flag = 1;
				break;
			default:
				printf("usage: %s [-Bbcfgi] [-d 1..%i] \n", argv[0], DELAY_MAX);
				exit(1);
		}
	}

	initscr();
	noecho();
	curs_set(0);
	timeout(delay);
	keypad(stdscr, TRUE);
	leaveok(stdscr, TRUE);

	init_colors();
	init_pixels();
	set_row(LINES - 1, FIRE_MAX);

	busy = 1;

	fadeout = 0;
	fademax = FIRE_MAX * 4 + (LINES / 6);
	fadetime = fademax;

	while (busy) {
		switch (getch()) {
			case EOF:
			case KEY_RESIZE:
				break;
			default:
				if (fade_flag) {
					fadetime = 0;
					fadeout = 1;
				} else {
					busy = 0;
				}
				break;
		}

		if (fade_flag) {
			if (!fadeout && fadetime > 0) {
				fade(1);
				fadetime--;
			}
		}

		draw();

		if (fade_flag) {
			if (fadeout) {
				if (fadetime < fademax) {
					fade(0);
					fadetime++;
				} else {
					busy = 0;
				}
			}
		}
	}

	quit();
	return 0;
}
