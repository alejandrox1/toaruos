#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H

#include "lib/window.h"
#include "lib/graphics.h"

sprite_t * sprites[128];
sprite_t alpha_tmp;

FT_Library   library;
FT_Face      face;
FT_Face      face_bold;
FT_Face      face_italic;
FT_Face      face_bold_italic;
FT_Face      face_extra;
FT_GlyphSlot slot;
FT_UInt      glyph_index;


uint16_t win_width;
uint16_t win_height;

int center_x(int x) {
	return (win_width - x) / 2;
}

int center_y(int y) {
	return (win_height - y) / 2;
}


void init_sprite(int i, char * filename, char * alpha) {
	sprites[i] = malloc(sizeof(sprite_t));
	load_sprite(sprites[i], filename);
	if (alpha) {
		sprites[i]->alpha = 1;
		load_sprite(&alpha_tmp, alpha);
		sprites[i]->masks = alpha_tmp.bitmap;
	} else {
		sprites[i]->alpha = 0;
	}
	sprites[i]->blank = 0x0;
}

int32_t min(int32_t a, int32_t b) {
	return (a < b) ? a : b;
}

int32_t max(int32_t a, int32_t b) {
	return (a > b) ? a : b;
}


/* Bilinear filtering from Wikipedia */
uint32_t getBilinearFilteredPixelColor(sprite_t * tex, double u, double v) {
	u *= tex->width;
	v *= tex->height;
	int x = floor(u);
	int y = floor(v);
	if (x >= tex->width)  return 0;
	if (y >= tex->height) return 0;
	double u_ratio = u - x;
	double v_ratio = v - y;
	double u_o = 1 - u_ratio;
	double v_o = 1 - v_ratio;
	double r_ALP = 256;
	if (tex->alpha) {
		if (x == tex->width - 1 || y == tex->height - 1) return (SPRITE(tex,x,y) | 0xFF000000) & (0xFFFFFF + _RED(SMASKS(tex,x,y)) * 0x1000000);
		r_ALP = (_RED(SMASKS(tex,x,y)) * u_o + _RED(SMASKS(tex,x+1,y)) * u_ratio) * v_o + (_RED(SMASKS(tex,x,y+1)) * u_o  + _RED(SMASKS(tex,x+1,y+1)) * u_ratio) * v_ratio;
	}
	if (x == tex->width - 1 || y == tex->height - 1) return SPRITE(tex,x,y);
	double r_RED = (_RED(SPRITE(tex,x,y)) * u_o + _RED(SPRITE(tex,x+1,y)) * u_ratio) * v_o + (_RED(SPRITE(tex,x,y+1)) * u_o  + _RED(SPRITE(tex,x+1,y+1)) * u_ratio) * v_ratio;
	double r_BLU = (_BLU(SPRITE(tex,x,y)) * u_o + _BLU(SPRITE(tex,x+1,y)) * u_ratio) * v_o + (_BLU(SPRITE(tex,x,y+1)) * u_o  + _BLU(SPRITE(tex,x+1,y+1)) * u_ratio) * v_ratio;
	double r_GRE = (_GRE(SPRITE(tex,x,y)) * u_o + _GRE(SPRITE(tex,x+1,y)) * u_ratio) * v_o + (_GRE(SPRITE(tex,x,y+1)) * u_o  + _GRE(SPRITE(tex,x+1,y+1)) * u_ratio) * v_ratio;

	return rgb(r_RED,r_GRE,r_BLU) & (0xFFFFFF + (int)r_ALP * 0x1000000);
}

void draw_sprite_scaled(sprite_t * sprite, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
	int32_t _left   = max(x, 0);
	int32_t _top    = max(y, 0);
	int32_t _right  = min(x + width,  graphics_width - 1);
	int32_t _bottom = min(y + height, graphics_height - 1);
	for (uint16_t _y = 0; _y < height; ++_y) {
		for (uint16_t _x = 0; _x < width; ++_x) {
			if (x + _x < _left || x + _x > _right || y + _y < _top || y + _y > _bottom)
				continue;
			if (sprite->alpha) {
				uint32_t n_color = getBilinearFilteredPixelColor(sprite, (double)_x / (double)width, (double)_y/(double)height);
				uint32_t f_color = rgb(_ALP(n_color), 0, 0);
				GFX(x + _x, y + _y) = alpha_blend(GFX(x + _x, y + _y), n_color, f_color);
			} else {
				if (SPRITE(sprite,_x,_y) != sprite->blank) {
					GFX(x + _x, y + _y) = getBilinearFilteredPixelColor(sprite, (double)_x / (double)width, (double)_y/(double)height);
				}
			}
		}
	}
}

void draw_char(FT_Bitmap * bitmap, int x, int y, uint32_t fg) {
	int i, j, p, q;
	int x_max = x + bitmap->width;
	int y_max = y + bitmap->rows;
	for (j = y, q = 0; j < y_max; j++, q++) {
		for ( i = x, p = 0; i < x_max; i++, p++) {
			GFX(i,j) = alpha_blend(GFX(i,j),fg,rgb(bitmap->buffer[q * bitmap->width + p],0,0));
			//term_set_point(i,j, alpha_blend(bg, fg, rgb(bitmap->buffer[q * bitmap->width + p],0,0)));
		}
	}
}

void draw_string(int x, int y, uint32_t fg, char * string) {
	slot = face->glyph;
	int pen_x = x, pen_y = y, i = 0;
	int len = strlen(string);
	int error;

	for (i = 0; i < len; ++i) {
		FT_UInt glyph_index;

		glyph_index = FT_Get_Char_Index( face, string[i]);
		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
		if (error) {
			printf("Error loading glyph for '%c'\n", string[i]);
			continue;
		}
		slot = (face)->glyph;
		if (slot->format == FT_GLYPH_FORMAT_OUTLINE) {
			error = FT_Render_Glyph((face)->glyph, FT_RENDER_MODE_NORMAL);
			if (error) {
				printf("Error rendering glyph for '%c'\n", string[i]);
				continue;
			}
		}

		draw_char(&slot->bitmap, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top, fg);
		pen_x += slot->advance.x >> 6;
		pen_y += slot->advance.y >> 6;
	}
}

#define FONT_SIZE 14
#define FONT_BYTE_SIZE 720012

void _loadDejavu() {
	char * font;
	size_t s = FONT_BYTE_SIZE;
	int error;
	font = (char *)syscall_shm_obtain(WINS_SERVER_IDENTIFIER ".fonts.sans-serif", s);
	error = FT_New_Memory_Face(library, font, s, 0, &face);
	if (error) {
		printf("Oh dear, this is bad.\n");
	}
	error = FT_Set_Pixel_Sizes(face, FONT_SIZE, FONT_SIZE);
	if (error) {
		printf("Oh my.\n");
	}
}


#define INPUT_SIZE 1024
char input_buffer[1024];
uint32_t input_collected = 0;

int buffer_put(char c) {
	if (c == 8) {
		/* Backspace */
		if (input_collected > 0) {
			input_collected--;
			input_buffer[input_collected] = '\0';
		}
		return 0;
	}
	if (c < 10 || (c > 10 && c < 32) || c > 126) {
		return 0;
	}
	input_buffer[input_collected] = c;
	if (input_buffer[input_collected] == '\n') {
		input_collected++;
		return 1;
	}
	input_collected++;
	if (input_collected == INPUT_SIZE) {
		return 1;
	}
	return 0;
}

int main (int argc, char ** argv) {
	if (argc < 3) {
		printf("usage: %s width height\n", argv[0]);
		return -1;
	}

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);

	win_width = width;
	win_height = height;

	setup_windowing();

	FT_Init_FreeType(&library);
	_loadDejavu();

	/* Do something with a window */
	window_t * wina = window_create(0,0, width, height);
	assert(wina);
	window_fill(wina, rgb(100,100,100));
	init_graphics_window_double_buffer(wina);
#if 0
	window_redraw_full(wina);
#endif

#if 1
	printf("Loading background...\n");
	init_sprite(0, "/usr/share/login-background.bmp", NULL);
	printf("Background loaded.\n");
	draw_sprite_scaled(sprites[0], 0, 0, width, height);
#endif

	init_sprite(1, "/usr/share/bs.bmp", "/usr/share/bs-alpha.bmp");
	draw_sprite_scaled(sprites[1], center_x(sprites[1]->width), center_y(sprites[1]->height), sprites[1]->width, sprites[1]->height);

	flip();

	size_t buf_size = wina->width * wina->height * sizeof(uint32_t);
	char * buf = malloc(buf_size);
	memcpy(buf, wina->buffer, buf_size);

	uint32_t i = 0;

	while (1) {
		w_keyboard_t * kbd = poll_keyboard();
		if (kbd != NULL) {
			buffer_put(kbd->key);
			free(kbd);
		}

		double scale = 2.0 + 1.5 * sin((double)i * 0.02);

		/* Redraw the background by memcpy (super speedy) */
		memcpy(frame_mem, buf, buf_size);

		draw_string(50 + scale * 30,50 + scale * 30, rgb(255,0,0), input_buffer);


		flip();
		window_redraw_full(wina);
		++i;
	}

	//window_destroy(window); // (will close on exit)
	teardown_windowing();

	return 0;
}