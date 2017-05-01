#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdl.h"
#include "keys.h"
#include "keysyms.h"

// settings
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define SCROLL_LINES 4



#define FONT_WIDTH 10
#define FONT_HEIGHT 20

#define CHARS_PER_LINE (SCREEN_WIDTH / FONT_WIDTH)

SDL_Surface *font;
SDL_Rect font_src = { 0, 0, FONT_WIDTH, FONT_HEIGHT };
SDL_Rect font_dest = { 0, 0, FONT_WIDTH, FONT_HEIGHT };
SDL_Rect font_scroll = { 0, SCROLL_LINES * FONT_HEIGHT };
SDL_Rect font_scrolled = { 0 };
SDL_Rect font_cr = { 0, 0, 0, FONT_HEIGHT };
Uint32 colors[8];
Uint32 color;
Uint32 background_color;


int quit = 0;
static SDL_Surface *screen;

SDL_Surface *
load_image(char *filename)
{
	SDL_Surface *t = 0, *i = 0;
	t = IMG_Load(filename); if(!t) return t;
	i = SDL_DisplayFormat(t);
	SDL_FreeSurface(t);
	return i;
}

void
init_colors() {
	colors[0] = SDL_MapRGB(screen->format, 0, 0, 0);
	colors[1] = SDL_MapRGB(screen->format, 180, 0, 0);
	colors[2] = SDL_MapRGB(screen->format, 0, 200, 0);
	colors[3] = SDL_MapRGB(screen->format, 180, 180, 0);
	colors[4] = SDL_MapRGB(screen->format, 0, 0, 255);
	colors[5] = SDL_MapRGB(screen->format, 180, 0, 180);
	colors[6] = SDL_MapRGB(screen->format, 0, 180, 180);
	colors[7] = SDL_MapRGB(screen->format, 200, 200, 200);

	color = colors[7];
	background_color = colors[0];
}

void
font_color(int c) {
	font_src.y = (c % 8) * FONT_HEIGHT;
	background_color = colors[(c >> 4) & 7];
}

void
sdl_term_move(int pos) {
	font_dest.x = (pos % CHARS_PER_LINE) * FONT_WIDTH;
	font_dest.y = (pos / CHARS_PER_LINE) * FONT_HEIGHT;
}

void
init_font() {
	font = load_image("fovium_font.png");
	if(!font) font = load_image(FONT_PATH);
	if(!font) exit(5);

	font_color(7);

	font_scroll.w = screen->w;
	font_scroll.h = screen->h - (SCROLL_LINES * FONT_HEIGHT) - (screen->h % FONT_HEIGHT);

	font_scrolled.y = screen->h - (SCROLL_LINES * FONT_HEIGHT) - (screen->h % FONT_HEIGHT);
	font_scrolled.w = screen->w;
	font_scrolled.h = screen->h - (screen->h % FONT_HEIGHT);
}
	

void
init_gfx()
{
	if(SDL_Init(SDL_INIT_EVERYTHING) == -1) exit(1);
	if(atexit(SDL_Quit)) { SDL_Quit(); exit(2); }
	SDL_EnableUNICODE(1);

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 0, SDL_HWSURFACE);
	if(!screen) exit(3);

	init_font();
	init_colors();

	// SDL_Flip(screen); 
}

void emit_cr() {
		font_dest.x = 0;
		font_dest.y += FONT_HEIGHT;
		if(font_dest.y + FONT_HEIGHT > screen->h) {
			// scroll
			SDL_BlitSurface(screen, &font_scroll, screen, 0);
			// blank area revealed
			SDL_FillRect(screen, &font_scrolled, background_color);

			SDL_Flip(screen);
			font_dest.y -= SCROLL_LINES * FONT_HEIGHT;
		}
}

void sdl_emit(char c) {
	if(c == 10) {
		font_cr.x = font_dest.x;
		font_cr.y = font_dest.y;
		font_cr.w = screen->w - font_dest.x;
		SDL_FillRect(screen, &font_cr, background_color);
		SDL_UpdateRect(screen, font_cr.x, font_cr.y, font_cr.w, font_cr.h);
		emit_cr();
		return;
	}
	
	if(c < ' ' || c > '~') {
		c = 127;
	}
	font_src.x = (c - ' ') * FONT_WIDTH;

	if(font_dest.x + FONT_WIDTH > screen->w) {
		emit_cr();
	}

	SDL_FillRect(screen, &font_dest, background_color);
	SDL_BlitSurface(font, &font_src, screen, &font_dest);
	SDL_UpdateRect(screen, font_dest.x, font_dest.y, FONT_WIDTH, FONT_HEIGHT);

	font_dest.x += FONT_WIDTH;
}

int _t_ret3[3];
int *t_ret3 = &(_t_ret3[0]);

int* ret3(int zero, int one, int two) {
	t_ret3[0] = zero;
	t_ret3[1] = one;
	t_ret3[2] = two;
	return t_ret3;
}

int *
sdl_key(int microsec) {
	SDL_Event event;
	unsigned int start;
	int gotone;

	if(microsec == -1) {
		gotone = SDL_WaitEvent(&event);
	} else {
		if(microsec == 0) {
			gotone = SDL_PollEvent(&event);
		} else {
			start = SDL_GetTicks();
			for(;;) {
				gotone = SDL_PollEvent(&event);
				if(gotone) {
					break;
				} else {
					if((SDL_GetTicks() - start) * 1000 >= microsec) {
						break;
					}
					SDL_Delay(1);
				}
			}
		}
	}

	if(!gotone) {
		return 0;
	}

	switch (event.type) {
		case SDL_KEYDOWN:
			return ret3(0, 1, sdlk_to_fovk(event.key.keysym.sym));
		case SDL_KEYUP:
			return ret3(0, 0, sdlk_to_fovk(event.key.keysym.sym));
		case SDL_MOUSEBUTTONDOWN:
			if(event.button.button == SDL_BUTTON_LEFT) {
				return ret3(0, 1, FOVK_MOUSE0);
			}
			if(event.button.button == SDL_BUTTON_RIGHT) {
				return ret3(0, 1, FOVK_MOUSE1);
			}
			if(event.button.button == SDL_BUTTON_MIDDLE) {
				return ret3(0, 1, FOVK_MOUSE2);
			}
		case SDL_MOUSEBUTTONUP:
			if(event.button.button == SDL_BUTTON_LEFT) {
				return ret3(0, 0, FOVK_MOUSE0);
			}
			if(event.button.button == SDL_BUTTON_RIGHT) {
				return ret3(0, 0, FOVK_MOUSE1);
			}
			if(event.button.button == SDL_BUTTON_MIDDLE) {
				return ret3(0, 0, FOVK_MOUSE2);
			}
		case SDL_QUIT: 
			exit(0);
		default:
			// FIXME what the crap should I do?
			return sdl_key(microsec);
	}
}
