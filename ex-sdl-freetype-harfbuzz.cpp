#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#undef main

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <string>

#include "unicode_data.hpp"

	
#define NUM_EXAMPLES 4

/* tranlations courtesy of google */
const char *texts[NUM_EXAMPLES] = {
	u8"City of saints and madmen",
	u8"من كل بلاد الدنيا من كل بقاع الأرض",
	u8"佛罗多巴金斯",
	u8"お手伝いしましょうか？"
};

const char* filenames[NUM_EXAMPLES]{
	"fonts/DejaVuSerif.ttf",
	"fonts/amiri-regular.ttf",
	"fonts/fireflysung.ttf",
	"fonts/fireflysung.ttf"
};

enum {
    ENGLISH=0, ARABIC, CHINESE, JAPANESE
};

/* google this */
#ifndef unlikely
#define unlikely
#endif


/*  This spanner is for obtaining exact bounding box for the string.
    Unfortunately this can't be done without rendering it (or pretending to).
    After this runs, we get min and max values of coordinates used.
*/
typedef struct _spanner_baton_t {
	int min_span_x;
	int max_span_x;
	int min_y;
	int max_y;
} spanner_baton_t;
void spanner_sizer(int y, int count, const FT_Span* spans, void *user) {
    spanner_baton_t *baton = (spanner_baton_t *) user;

    if (y < baton->min_y)
        baton->min_y = y;
    if (y > baton->max_y)
        baton->max_y = y;
    for (int i = 0 ; i < count; i++) {
        if (spans[i].x + spans[i].len > baton->max_span_x)
            baton->max_span_x = spans[i].x + spans[i].len;
        if (spans[i].x < baton->min_span_x)
            baton->min_span_x = spans[i].x;
    }
}

void ftfdump(FT_Face ftf) {
    for(int i=0; i<ftf->num_charmaps; i++) {
        printf("%d: %s %s %c%c%c%c plat=%hu id=%hu\n", i,
            ftf->family_name,
            ftf->style_name,
            ftf->charmaps[i]->encoding >>24,
           (ftf->charmaps[i]->encoding >>16 ) & 0xff,
           (ftf->charmaps[i]->encoding >>8) & 0xff,
           (ftf->charmaps[i]->encoding) & 0xff,
            ftf->charmaps[i]->platform_id,
            ftf->charmaps[i]->encoding_id
        );
    }
}

void hline(SDL_Surface *s, int min_x, int max_x, int y, uint32_t color) {
    uint32_t *pix = (uint32_t *)s->pixels + (y * s->pitch) / 4 + min_x;
    uint32_t *end = (uint32_t *)s->pixels + (y * s->pitch) / 4 + max_x;

    while (pix - 1 != end)
        *pix++ = color;
}

void vline(SDL_Surface *s, int min_y, int max_y, int x, uint32_t color) {
    uint32_t *pix = (uint32_t *)s->pixels + (min_y * s->pitch) / 4 + x;
    uint32_t *end = (uint32_t *)s->pixels + (max_y * s->pitch) / 4 + x;

    while (pix - s->pitch/4 != end) {
        *pix = color;
        pix += s->pitch/4;
    }
}

int main () {
    int ptSize = 50*64;
    int device_hdpi = 72;
    int device_vdpi = 72;

    /* Init freetype */
    FT_Library ft_library;
    assert(!FT_Init_FreeType(&ft_library));

    /* Load our fonts */
    FT_Face ft_face[NUM_EXAMPLES];
    assert(!FT_New_Face(ft_library, filenames[ENGLISH], 0, &ft_face[ENGLISH]));
    assert(!FT_Set_Char_Size(ft_face[ENGLISH], 0, ptSize, device_hdpi, device_vdpi ));
    ftfdump(ft_face[ENGLISH]);

    assert(!FT_New_Face(ft_library, filenames[ARABIC], 0, &ft_face[ARABIC]));
    assert(!FT_Set_Char_Size(ft_face[ARABIC], 0, ptSize, device_hdpi, device_vdpi ));
    ftfdump(ft_face[ARABIC]);

    assert(!FT_New_Face(ft_library, filenames[CHINESE], 0, &ft_face[CHINESE]));
    assert(!FT_Set_Char_Size(ft_face[CHINESE], 0, ptSize, device_hdpi, device_vdpi ));
    ftfdump(ft_face[CHINESE]);

	assert(!FT_New_Face(ft_library, filenames[JAPANESE], 0, &ft_face[JAPANESE]));
	assert(!FT_Set_Char_Size(ft_face[JAPANESE], 0, ptSize, device_hdpi, device_vdpi));
	ftfdump(ft_face[JAPANESE]);

    /* Get our harfbuzz font structs */
    hb_font_t *hb_ft_font[NUM_EXAMPLES];
    hb_ft_font[ENGLISH] = hb_ft_font_create(ft_face[ENGLISH], 0);
    hb_ft_font[ARABIC]  = hb_ft_font_create(ft_face[ARABIC] , 0);
    hb_ft_font[CHINESE] = hb_ft_font_create(ft_face[CHINESE], 0);
	hb_ft_font[JAPANESE] = hb_ft_font_create(ft_face[CHINESE], 0);

    /** Setup our SDL window **/
    int width      = 800;
    int height     = 600;
    int bpp        = 32;

	/* Initialize our SDL window */
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL");
		return -1;
	}

	SDL_Window *screen = SDL_CreateWindow("SDL2 + FreeType + HarfBuzz + FriBiDi Example",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width, height,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

	SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);

	SDL_SetWindowTitle(screen, "\"Simple\" SDL+FreeType+HarfBuzz Example");

    /* Create an SDL image surface we can draw to */
    SDL_Surface *sdl_surface = SDL_CreateRGBSurface (0, width, height, bpp, 0,0,0,0);
	SDL_Texture* screen_tex = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		width, height);


    /* Create a buffer for harfbuzz to use */
    hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_set_content_type(buf, HB_BUFFER_CONTENT_TYPE_UNICODE);

    /* Our main event/draw loop */
    int done = 0;
    int resized = 1;
    while (!done) {
        /* Clear our surface */
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

        SDL_LockSurface(sdl_surface);

		for (int i = 0; i < NUM_EXAMPLES; ++i) {
            /* Layout the text */
            hb_buffer_add_utf8(buf, texts[i], strlen(texts[i]), 0, strlen(texts[i]));
			unsigned int glyph_count = hb_buffer_get_length(buf);
			hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
			hb_buffer_set_script(buf, ucdn_get_script(glyph_info[0].codepoint));
			if (i == CHINESE)
				hb_buffer_set_direction(buf, HB_DIRECTION_TTB);
			hb_buffer_guess_segment_properties(buf);
            hb_shape(hb_ft_font[i], buf, NULL, 0);
            hb_glyph_position_t* glyph_pos    = hb_buffer_get_glyph_positions(buf, &glyph_count);

            /* set up rendering via spanners */
            spanner_baton_t stuffbaton;

            FT_Raster_Params ftr_params;
            ftr_params.target = 0;
            ftr_params.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
            ftr_params.user = &stuffbaton;
            ftr_params.black_spans = 0;
            ftr_params.bit_set = 0;
            ftr_params.bit_test = 0;

            /* Calculate string bounding box in pixels */
            ftr_params.gray_spans = spanner_sizer;

            /* See http://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html */

			float max_x = INT_MIN; // largest coordinate a pixel has been set at, or the pen was advanced to.
			float min_x = INT_MAX; // smallest coordinate a pixel has been set at, or the pen was advanced to.
			float max_y = INT_MIN; // this is max topside bearing along the string.
            float min_y = INT_MAX; // this is max value of (height - topbearing) along the string.
            /*  Naturally, the above comments swap their meaning between horizontal and vertical scripts,
                since the pen changes the axis it is advanced along.
                However, their differences still make up the bounding box for the string.
                Also note that all this is in FT coordinate system where y axis points upwards.
             */

            float sizer_x = 0;
			float sizer_y = 0; /* in FT coordinate system. */

            FT_Error fterr;
            for (unsigned j = 0; j < glyph_count; ++j) {
                if ((fterr = FT_Load_Glyph(ft_face[i], glyph_info[j].codepoint, 0))) {
                    printf("load %08x failed fterr=%d.\n",  glyph_info[j].codepoint, fterr);
                } else {
                    if (ft_face[i]->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
                        printf("glyph->format = %4s\n", (char *)&ft_face[i]->glyph->format);
                    } else {
                        float gx = sizer_x + (glyph_pos[j].x_offset/64.);
						float gy = sizer_y + (glyph_pos[j].y_offset/64.); // note how the sign differs from the rendering pass

                        stuffbaton.min_span_x = INT_MAX;
                        stuffbaton.max_span_x = INT_MIN;
                        stuffbaton.min_y = INT_MAX;
                        stuffbaton.max_y = INT_MIN;

                        if ((fterr = FT_Outline_Render(ft_library, &ft_face[i]->glyph->outline, &ftr_params)))
                            printf("FT_Outline_Render() failed err=%d\n", fterr);

                        if (stuffbaton.min_span_x != INT_MAX) {
                        /* Update values if the spanner was actually called. */
                            if (min_x > stuffbaton.min_span_x + gx)
                                min_x = stuffbaton.min_span_x + gx;

                            if (max_x < stuffbaton.max_span_x + gx)
                                max_x = stuffbaton.max_span_x + gx;

                            if (min_y > stuffbaton.min_y + gy)
                                min_y = stuffbaton.min_y + gy;

                            if (max_y < stuffbaton.max_y + gy)
                                max_y = stuffbaton.max_y + gy;
                        } else {
                        /* The spanner wasn't called at all - an empty glyph, like space. */
                            if (min_x > gx) min_x = gx;
                            if (max_x < gx) max_x = gx;
                            if (min_y > gy) min_y = gy;
                            if (max_y < gy) max_y = gy;
                        }
                    }
                }

                sizer_x += glyph_pos[j].x_advance/64.;
                sizer_y += glyph_pos[j].y_advance/64.; // note how the sign differs from the rendering pass
            }
            /* Still have to take into account last glyph's advance. Or not? */
            if (min_x > sizer_x) min_x = sizer_x;
            if (max_x < sizer_x) max_x = sizer_x;
            if (min_y > sizer_y) min_y = sizer_y;
            if (max_y < sizer_y) max_y = sizer_y;

            /* The bounding box */
            int bbox_w = max_x - min_x;
            int bbox_h = max_y - min_y;

            /* Two offsets below position the bounding box with respect to the 'origin',
                which is sort of origin of string's first glyph.

                baseline_offset - offset perpendecular to the baseline to the topmost (horizontal),
                                  or leftmost (vertical) pixel drawn.

                baseline_shift  - offset along the baseline, from the first drawn glyph's origin
                                  to the leftmost (horizontal), or topmost (vertical) pixel drawn.

                Thus those offsets allow positioning the bounding box to fit the rendered string,
                as they are in fact offsets from the point given to the renderer, to the top left
                corner of the bounding box.

                NB: baseline is defined as y==0 for horizontal and x==0 for vertical scripts.
                (0,0) here is where the first glyph's origin ended up after shaping, not taking
                into account glyph_pos[0].xy_offset (yeah, my head hurts too).
            */

            int baseline_offset;
            int baseline_shift;

            if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(buf))) {
                baseline_offset = max_y;
                baseline_shift  = min_x;
            }
            if (HB_DIRECTION_IS_VERTICAL(hb_buffer_get_direction(buf))) {
                baseline_offset = min_x;
                baseline_shift  = max_y;
            }

            if (resized)
                printf("ex %d string min_x=%d max_x=%d min_y=%d max_y=%d bbox %dx%d boffs %d,%d\n",
                    i, min_x, max_x, min_y, max_y, bbox_w, bbox_h, baseline_offset, baseline_shift);

            /* The pen/baseline start coordinates in window coordinate system
                - with those text placement in the window is controlled.
                - note that for RTL scripts pen still goes LTR */
			float x = 0;
			float y = 50 + i * 75;
            if (i == ENGLISH) { x = 20; }                  /* left justify */
            if (i == ARABIC)  { x = width - bbox_w - 20; } /* right justify */
			if (i == CHINESE) { x = width / 2 - bbox_w / 2; y -= 20; }  /* center, and for TTB script h_advance is half-width. */
			if (i == JAPANESE) { x = 20; y = 550; }                  /* left justify */

            /* Draw baseline and the bounding box */
            /* The below is complicated since we simultaneously
               convert to the window coordinate system. */
            int left, right, top, bottom;

            if (HB_DIRECTION_IS_HORIZONTAL(hb_buffer_get_direction(buf))) {
                /* bounding box in window coordinates without offsets */
                left   = x;
                right  = x + bbox_w;
                top    = y - bbox_h;
                bottom = y;

                /* apply offsets */
                left   +=  baseline_shift;
                right  +=  baseline_shift;
                top    -=  baseline_offset - bbox_h;
                bottom -=  baseline_offset - bbox_h;

                /* draw the baseline */
                hline(sdl_surface, x, x + bbox_w, y, 0x0000ff00);
            }

            if (HB_DIRECTION_IS_VERTICAL(hb_buffer_get_direction(buf))) {
                left   = x;
                right  = x + bbox_w;
                top    = y;
                bottom = y + bbox_h;

                left   += baseline_offset;
                right  += baseline_offset;
                top    -= baseline_shift;
                bottom -= baseline_shift;

                vline(sdl_surface, y, y + bbox_h, x, 0x0000ff00);
            }
            if (resized)
                printf("ex %d origin %d,%d bbox l=%d r=%d t=%d b=%d\n",
                                        i, x, y, left, right, top, bottom);

            /* +1/-1 are for the bbox borders be the next pixel outside the bbox itself */
            hline(sdl_surface, left - 1, right + 1, top - 1, 0x00ff0000);
            hline(sdl_surface, left - 1, right + 1, bottom + 1, 0x00ff0000);
            vline(sdl_surface, top - 1, bottom + 1, left - 1, 0x00ff0000);
            vline(sdl_surface, top - 1, bottom + 1, right + 1, 0x00ff0000);

			auto use_kerning = FT_HAS_KERNING(ft_face[i]);
			auto previous = 0;

            /* render */
            for (unsigned j=0; j < glyph_count; ++j) {
				auto codepoint = glyph_info[j].codepoint;
				/* convert character code to glyph index */
				auto glyph_index = FT_Get_Char_Index(ft_face[i], codepoint);
                if ((fterr = FT_Load_Glyph(ft_face[i], codepoint, FT_LOAD_DEFAULT))) {
                    printf("load %08x failed fterr=%d.\n",  codepoint, fterr);
                } else {
					auto glyph = ft_face[i]->glyph;
                    if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
                        printf("glyph->format = %4s\n", (char *)&glyph->format);
                    } else {
						/* retrieve kerning distance and move pen position */
						if (use_kerning && previous && glyph_index)
						{
							FT_Vector  delta;
							FT_Get_Kerning(ft_face[i], previous, glyph_index,
								FT_KERNING_DEFAULT, &delta);
							x += delta.x >> 6;
						}
                        float gx = x + (glyph_pos[j].x_offset/64.);
						float gy = y - (glyph_pos[j].y_offset/64.);

						FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);
						FT_Bitmap bitmap = glyph->bitmap;
						int bytes = bpp / 8;
						auto pixels = &((uint32_t*)sdl_surface->pixels)[((int)gy - glyph->bitmap_top) * sdl_surface->w + (int)gx + glyph->bitmap_left];
						for (int row = 0; row < bitmap.rows; ++row) {
							for (int col = 0; col < bitmap.width; ++col) {
								uint32_t* pixel = &pixels[row * sdl_surface->w + col];
								uint8_t color = bitmap.buffer[row * bitmap.pitch + col];
								if (color)
									*pixel = color + (color << 8) + (color << 16);
							}
						}
                    }
                }

                x += glyph_pos[j].x_advance/64.;
                y -= glyph_pos[j].y_advance/64.;
				previous = glyph_index;
            }

            /* clean up the buffer, but don't kill it just yet */
            hb_buffer_clear_contents(buf);

        }

        SDL_UnlockSurface(sdl_surface);

        /* Blit our new image to our visible screen */

		SDL_UpdateTexture(screen_tex, NULL, sdl_surface->pixels, sdl_surface->w * sizeof(Uint32));


		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, screen_tex, NULL, NULL);
		SDL_RenderPresent(renderer);


        /* Handle SDL events */
        SDL_Event event;
        resized = resized ? !resized : resized;
        while(SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        done = 1;
                    }
                    break;
                case SDL_QUIT:
                    done = 1;
                    break;
            }
        }

        SDL_Delay(150);
    }

    /* Cleanup */
    hb_buffer_destroy(buf);
    for (int i=0; i < NUM_EXAMPLES; ++i)
        hb_font_destroy(hb_ft_font[i]);

    FT_Done_FreeType(ft_library);

    SDL_FreeSurface(sdl_surface);
    SDL_Quit();

    return 0;
}
