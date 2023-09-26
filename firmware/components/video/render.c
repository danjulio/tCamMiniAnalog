/*
 * Renderers for lepton images, spot meter and min/max markers
 *
 * Copyright 2020, 2023 Dan Julio
 *
 * This file is part of tCamMiniAnalog.
 *
 * tCamMiniAnalog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tCamMiniAnalog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tCam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <math.h>
#include <string.h>
#include "render.h"
#include "font.h"
#include "digits8x16.h"
#include "font7x10.h"
#include "lepton_utilities.h"
#include "sys_utilities.h"



//
// Variables
//
static uint8_t render_palette_mod;    // Either 0x00 or 0xFF, used to invert image (white-hot -> black-hot)



//
// Forward declarations for internal functions
//
static void render_double_rad_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g);
static void render_double_agc_data(lep_buffer_t* lep, uint8_t* img);
static void render_interp_rad_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g);
static void render_interp_agc_data(uint16_t* buf, uint8_t* img);
static void render_min_marker(lep_buffer_t* lep, uint8_t* img);
static void render_max_marker(lep_buffer_t* lep, uint8_t* img);
static void interp_set_pixel(uint16_t src, uint8_t* img, int x, int y);
static void interp_set_outer_row(uint16_t* src, uint8_t* img, bool first_row);
static void interp_set_outer_col(uint16_t* src, uint8_t* img, bool first_col);
static void interp_set_inner(uint16_t* src, uint8_t* img);
static void draw_hline(uint8_t* img, int16_t x1, int16_t x2, int16_t y, uint8_t c);
static void draw_vline(uint8_t* img, int16_t x, int16_t y1, int16_t y2, uint8_t c);
static void draw_line(uint8_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t c);
static void draw_fill_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c);
static int16_t draw_char(uint8_t* img, int16_t x, int16_t y, uint8_t c, const Font_TypeDef *Font);
static void draw_string(uint8_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font);
static __inline__ void draw_pixel(uint8_t* img, int16_t x, int16_t y, uint8_t c);
static uint16_t get_string_width(const char *str, const Font_TypeDef *Font);
static float lep_to_disp_temp(uint16_t v, gui_state_t* g);


//
// Render API
//
void render_lep_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g)
{
	// Setup the global palette modifier
	render_palette_mod = (g->black_hot_palette) ? 0xFF : 0x00;
	
	if (g->display_interp_enable) {
		if (g->agc_enabled) {
			render_interp_agc_data(lep->lep_bufferP, img);
		} else {
			render_interp_rad_data(lep, img, g);
		}
	} else {
		if (g->agc_enabled) {
			render_double_agc_data(lep, img);
		} else {
			render_double_rad_data(lep, img, g);
		}
	}
}


void render_spotmeter(lep_buffer_t* lep, uint8_t* img, gui_state_t* g)
{
	char temp_str[8];
	int16_t x1, x2, y1, y2;
	uint16_t c1, c2, r1, r2;
	uint16_t dw, dh;
	uint16_t w, h;
	
	c1 = lep->lep_telemP[LEP_TEL_SPOT_X1] * IMG_BUF_MULT_FACTOR;
	r1 = lep->lep_telemP[LEP_TEL_SPOT_Y1] * IMG_BUF_MULT_FACTOR;
	c2 = lep->lep_telemP[LEP_TEL_SPOT_X2] * IMG_BUF_MULT_FACTOR;
	r2 = lep->lep_telemP[LEP_TEL_SPOT_Y2] * IMG_BUF_MULT_FACTOR;
	
	// Spotmeter sense area dimensions
	dw = c2 - c1;
	dh = r2 - r1;
	
	// Center
	c1 = c1 + dw/2;
	r1 = r1 + dh/2;
	
	// Bounding box dimensions
	w = (dw > IMG_SPOT_MIN_SIZE) ? dw : IMG_SPOT_MIN_SIZE;
	h = (dh > IMG_SPOT_MIN_SIZE) ? dh : IMG_SPOT_MIN_SIZE;
	
	// Bounding box coordinates
	x1 = c1 - w/2;
	x2 = x1 + w;
	y1 = r1 - h/2;
	y2 = y1 + h;
	
	// Draw a white bounding box surrounded by a black bounding box for contrast
	// on all color palettes
	draw_hline(img, x1, x2, y1, 0xFF);
	draw_hline(img, x1, x2, y2, 0xFF);
	draw_vline(img, x1, y1, y2, 0xFF);
	draw_vline(img, x2, y1, y2, 0xFF);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	draw_hline(img, x1, x2, y1, 0x00);
	draw_hline(img, x1, x2, y2, 0x00);
	draw_vline(img, x1, y1, y2, 0x00);
	draw_vline(img, x2, y1, y2, 0x00);
	
	// Get the temperature string
	sprintf(temp_str, "%d", (int16_t) round(lep_to_disp_temp(lep->lep_telemP[LEP_TEL_SPOT_MEAN], g)));
	
	// Compute upper left corner for text string
	dw = get_string_width(temp_str, &Digits8x16);
	dh = Digits8x16.font_Height;
	x1 = c1 - dw/2;
	y1 = (c1 <= (IMG_BUF_HEIGHT/2)) ? y1 - dh - 2 : y2 + 2;
	
	// Blank an area and the draw the text
	draw_fill_rect(img, x1-1, y1-1, dw+2, dh+2, TEXT_BG_COLOR);
	draw_string(img, x1, y1, temp_str, &Digits8x16);	
}


void render_min_max_markers(lep_buffer_t* lep, uint8_t* img)
{
	render_min_marker(lep, img);
	render_max_marker(lep, img);
}


void render_parm_string(const char* s, uint8_t* img)
{
	uint16_t w, h;
	uint16_t x, y;
	
	// Do nothing for zero-length strings
	if (s[0] == 0) return;
	
	// Compute the width and height of the string
	w = get_string_width(s, &Font7x10);
	h = Font7x10.font_Height;
	
	// Compute the starting location
	x = (IMG_BUF_WIDTH - w) / 2;
	y = IMG_BUF_HEIGHT/3;
	
	// Blank an area and draw the text
	draw_fill_rect(img, x-1, y-1, w+2, h+2, TEXT_BG_COLOR);
	draw_string(img, x, y, s, &Font7x10);
}



//
// Internal functions
//
static void render_double_rad_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g)
{
	int src_y;
	uint32_t t32;
	uint32_t diff;
	uint16_t* ptr = lep->lep_bufferP;
	uint16_t min_val, max_val;
	uint8_t t8;
	
	// Dynamic range from image
	min_val = lep->lep_min_val;
	max_val = lep->lep_max_val;
	diff = max_val - min_val;
	if (diff == 0) diff = 1;
	
	for (src_y=0; src_y<LEP_HEIGHT; src_y++) {
		// Linearly scale then double each pixel in a source line into the destination buffer
		while (ptr < (lep->lep_bufferP + ((src_y+1)*LEP_WIDTH))) {
			if (*ptr < min_val) {
				t8 = 0;
				ptr++;
			} else {
				t32 = ((uint32_t)(*ptr++ - min_val) * 255) / diff;
				t8 = (t32 > 255) ? 255 : (uint8_t) t32;
			}
			
			*img++ = t8 ^ render_palette_mod;
			*img++ = t8 ^ render_palette_mod;
		}
		
		// Duplicate the destination buffer line
		memcpy(img, img - 2*LEP_WIDTH, 2*LEP_WIDTH);
		img += 2*LEP_WIDTH;
	}
}


static void render_double_agc_data(lep_buffer_t* lep, uint8_t* img)
{
	int src_y;
	uint16_t* ptr = lep->lep_bufferP;
	
	for (src_y=0; src_y<LEP_HEIGHT; src_y++) {
		// Double each pixel in a source line into the destination buffer
		while (ptr < (lep->lep_bufferP + ((src_y+1)*LEP_WIDTH))) {
			*img++ = ((uint8_t) (*ptr   & 0xFF)) ^ render_palette_mod;
			*img++ = ((uint8_t) (*ptr++ & 0xFF)) ^ render_palette_mod;
		}
		
		// Duplicate the destination buffer line
		memcpy(img, img - 2*LEP_WIDTH, 2*LEP_WIDTH);
		img += 2*LEP_WIDTH;
	}
}


static void render_interp_rad_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g)
{
	uint16_t* lepP = lep->lep_bufferP;
	uint32_t t32;
	uint16_t min_val, max_val;
	uint32_t diff;
	
	// Dynamic range from image
	min_val = lep->lep_min_val;
	max_val = lep->lep_max_val;
	diff = max_val - min_val;
	if (diff == 0) diff = 1;
	 
	// Linearize the 16-bit data into 8-bits in the lep buffer
	do {
		if (*lepP < min_val) {
			*lepP = 0;
		} else {
			t32 = ((uint32_t)(*lepP - min_val) * 255) / diff;
			*lepP = (t32 > 255) ? 255 : (uint8_t) t32;
		}
	} while (++lepP < (lep->lep_bufferP + LEP_WIDTH*LEP_HEIGHT));
	
	// Render 8-bit data
	render_interp_agc_data(lep->lep_bufferP, img);
}


static void render_interp_agc_data(uint16_t* buf, uint8_t* img)
{
	// Corner pixels
	interp_set_pixel(*buf, img, 0, 0);
	interp_set_pixel(*(buf + (LEP_WIDTH-1)), img, 2*LEP_WIDTH-1, 0);
	interp_set_pixel(*(buf + LEP_WIDTH*(LEP_HEIGHT-1)), img, 0, 2*LEP_HEIGHT-1);
	interp_set_pixel(*(buf + LEP_WIDTH*(LEP_HEIGHT-1) + (LEP_WIDTH-1)), img, 2*LEP_WIDTH-1, 2*LEP_HEIGHT-1);
	
	// Top/Bottom rows
	interp_set_outer_row(buf, img, true);
	interp_set_outer_row(buf, img, false);
	
	// Left/Right columns
	interp_set_outer_col(buf, img, true);
	interp_set_outer_col(buf, img, false);
	
	// Inner pixels
	interp_set_inner(buf, img);
}


static void render_min_marker(lep_buffer_t* lep, uint8_t* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	x1 = lep->lep_min_x * IMG_BUF_MULT_FACTOR - (IMG_MM_MARKER_SIZE/2);
	xm = lep->lep_min_x * IMG_BUF_MULT_FACTOR;
	x2 = x1 + IMG_MM_MARKER_SIZE;
	y1 = lep->lep_min_y * IMG_BUF_MULT_FACTOR - (IMG_MM_MARKER_SIZE/2);
	y2 = y1 + IMG_MM_MARKER_SIZE;
	
	// Draw a white downward facing triangle surrounded by a black triangle for contrast
	draw_hline(img, x1, x2, y1, 0xFF);
	draw_line(img, x1, y1, xm, y2, 0xFF);
	draw_line(img, xm, y2, x2, y1, 0xFF);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	draw_hline(img, x1, x2, y1, 0x00);
	draw_line(img, x1, y1, xm, y2, 0x00);
	draw_line(img, xm, y2, x2, y1, 0x00);
}


static void render_max_marker(lep_buffer_t* lep, uint8_t* img)
{
	int16_t x1, xm, x2, y1, y2;
	
	// Compute a bounding box around the marker triangle
	x1 = lep->lep_max_x * IMG_BUF_MULT_FACTOR - (IMG_MM_MARKER_SIZE/2);
	xm = lep->lep_max_x * IMG_BUF_MULT_FACTOR;
	x2 = x1 + IMG_MM_MARKER_SIZE;
	y1 = lep->lep_max_y * IMG_BUF_MULT_FACTOR - (IMG_MM_MARKER_SIZE/2);
	y2 = y1 + IMG_MM_MARKER_SIZE;
	
	// Draw a white upward facing triangle surrounded by a black triangle for contrast
	draw_hline(img, x1, x2, y2, 0xFF);
	draw_line(img, x1, y2, xm, y1, 0xFF);
	draw_line(img, xm, y1, x2, y2, 0xFF);
	
	x1--;
	y1--;
	x2++;
	y2++;
	
	draw_hline(img, x1, x2, y2, 0x00);
	draw_line(img, x1, y2, xm, y1, 0x00);
	draw_line(img, xm, y1, x2, y2, 0x00);
}



/******
 *
 * Linear Interpolation Pixel Doubler
 *
 * Each source pixel is broken into 4 sub-pixels (a-d), shown below.  Each sub-
 * pixel value is based primarily by its source pixel value plus contribution from 
 * surrounding source pixels.  Four source pixels (A-D) are shown.
 *
 *      +---+---+ +---+---+
 *      | a | b | | a | b |
 *      +---A---+ +---B---+
 *      | c | d | | c | d |
 *      +---+---+ +---+---+
 *
 *      +---+---+ +---+---+
 *      | a | b | | a | b |
 *      +---C---+ +---D---+
 *      | c | d | | c | d |
 *      +---+---+ +---+---+
 * 
 * There are three cases to calculate:
 *   1. The four corners of the source array (Aa, Bb, Cc, and Dd here)
 *      - These are simply set to the value of the source pixel (A, B, C and D)
 *   2. The outer edges of the source array (Ab, Ba, Ac, Bd, Ca, Db, Dc, Dc)
 *      - The sub-pixel value is based on two source pixels (the owning pixel and its neighbor)
 *      - The owning pixel contributes more to the sub-pixel by a Scale Factor (SF)
 *      - The sub-pixel = (SF * Owning Pixel + Neighbor Pixel) / DIV
 *      - The divisor scales the sum back to 8-bits = SF + 1
 *   3. The inner sub-pixels (Ad, Bc, Cb, Da)
 *      - The sub-pixel value is based on four source pixels (the owning pixel and its neighbors)
 *      - The owning pixel contributes more to the sub-pixel by a Scale Factor (SF)
 *      - The sub-pixel = (SF * Owning Pixel + 3 Neighbor Pixels) / DIV
 *      - The divisor scales the sum back to 8-bits = SF + 3
 *
 */
 
 
/**
 * Set a single pixel in the segment buffer
 *   d contains source buffer 8-bit value
 *   img points to display buffer
 *   x, y specify position in display buffer
 */
static void interp_set_pixel(uint16_t src, uint8_t* img, int x, int y)
{
	*(img + y*2*LEP_WIDTH + x) = ((uint8_t)(src & 0xFF)) ^ render_palette_mod;
}
 

/**
 * Process either the top or bottom row in the destination buffer where each pixel
 * only depends on contributions from two source locations.
 *   src points to the lepton source buffer
 *   img points to the display buffer
 *   first_row indicates top or bottom
 */
static void interp_set_outer_row(uint16_t* src, uint8_t* img, bool first_row)
{
	int x;
	uint8_t A, B, sub_pixel;
	
	// Set the pointers to the start of the row to load
	if (first_row) {
		// Top row starting 1 pixel in (dest)
		img += 1;
	} else {
		// Bottom row starting 1 pixel in (dest)
		src += (LEP_HEIGHT-1)*LEP_WIDTH;
		img += (2*LEP_HEIGHT-1)*LEP_WIDTH*2 + 1;
	}
	
	// Inner pixels
	B = *src;
	for (x=0; x<LEP_WIDTH-1; x++) {
		A = B;
		B = *++src;
		
		// Left sub-pixel Ab (top) / Ad (bottom)
		sub_pixel = (SF_DS*A + B) / DIV_DS;
		*img++ = sub_pixel ^ render_palette_mod;
		
		// Right sub-pixel Ba (top) / Bc (bottom)
		sub_pixel = (A + SF_DS*B) / DIV_DS;
		*img++ = sub_pixel ^ render_palette_mod;
	}
}


/**
 * Process either the left or right column in the destination buffer where each pixel
 * only depends on contributions from two source locations.
 *   src points to the lepton source buffer
 *   img points to the display buffer
 *   first_col indicates left or right
 */
static void interp_set_outer_col(uint16_t* src, uint8_t* img, bool first_col)
{
	int y;
	uint8_t A, B, sub_pixel;
	
	// Set the pointers to the start of the column to load
	if (first_col) {
		// Left column starting 1 pixel down (dest)
		img += 2*LEP_WIDTH;
	} else {
		// Right column starting 1 pixel down (dest)
		src += LEP_WIDTH - 1;
		img += 2*LEP_WIDTH + (2*LEP_WIDTH-1);
	}
		
	// Inner pixels
	B = *src;
	for (y=0; y<LEP_HEIGHT-1; y++) {
		A = B;
		src += LEP_WIDTH;
		B = *src;
	
		// Top sub-pixel Ac (left) / Ad (right)
		sub_pixel = (SF_DS*A + B) / DIV_DS;
		*img = sub_pixel ^ render_palette_mod;
		img += 2*LEP_WIDTH;
		
		// Bottom sub-pixel Ba (left) / Bb (right)
		sub_pixel = (A + SF_DS*B) / DIV_DS;
		*img = sub_pixel ^ render_palette_mod;
		img += 2*LEP_WIDTH;
	}
}


/**
 * Process inner destination pixels that depend on the contribution from
 * four source pixels.
 *   src points to the lepton source buffer
 *   img points to the display buffer
 */
static void interp_set_inner(uint16_t* src, uint8_t* img)
{
	int x, y;
	uint8_t A, B, C, D, sub_pixel;
	
	// Set the destination pointer to the start of the first inner row
	img += 2*LEP_WIDTH + 1;

	// Loop over inner lines (LEP_HEIGHT-1 lines of LEP_WIDTH-1 pixels)
	for (y=0; y<LEP_HEIGHT-1; y++) {	
		// Compute all four sub-pixels in the inner section
		B = *src;
		D = *(src + LEP_WIDTH);
		for (x=0; x<LEP_WIDTH-1; x++) {
			A = B;
			C = D;
			src++;
			B = *src;
			D = *(src + LEP_WIDTH);
			
			// Lower right sub-pixel Ad
			sub_pixel = (SF_QS*A + B + C + D) / DIV_QS;
			*img = sub_pixel ^ render_palette_mod;
			
			// Upper right sub-pixel Cb
			sub_pixel = (A + B + SF_QS*C + D) / DIV_QS;
			*(img + 2*LEP_WIDTH) = sub_pixel ^ render_palette_mod;
			img++;
			
			// Lower left sub-pixel Bc
			sub_pixel = (A + SF_QS*B + C + D) / DIV_QS;
			*img = sub_pixel ^ render_palette_mod;
			
			// Upper left sub-pixel Da
  			sub_pixel = (A + B + C + SF_QS*D) / DIV_QS;
			*(img + 2*LEP_WIDTH) = sub_pixel ^ render_palette_mod;
			img++;
		}

		// Next source line, 2 dest lines down, 1-pixel in
		src++;
		img += 2*LEP_WIDTH + 2;
	}
}


static void draw_hline(uint8_t* img, int16_t x1, int16_t x2, int16_t y, uint8_t c)
{
	uint8_t* imgP;
	
	if ((y < 0) || (y >= IMG_BUF_HEIGHT)) return;
	
	imgP = img + y*IMG_BUF_WIDTH + x1;
	
	while (x1 <= x2) {
		if ((x1 >= 0) && (x1 < IMG_BUF_WIDTH)) {
			*imgP = c;
		}
		imgP++;
		x1++;
	}
}


static void draw_vline(uint8_t* img, int16_t x, int16_t y1, int16_t y2, uint8_t c)
{
	uint8_t* imgP;
	
	if ((x < 0) || (x >= IMG_BUF_WIDTH)) return;
	
	imgP = img + y1*IMG_BUF_WIDTH + x;
	
	while (y1 <= y2) {
		if ((y1 >= 0) && (y1 < IMG_BUF_HEIGHT)) {
			*imgP = c;
		}
		imgP += IMG_BUF_WIDTH;
		y1++;
	}
}


static void draw_line(uint8_t* img, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t c)
{
	int16_t dx = abs(x2 - x1);
	int16_t dy = -abs(y2 - y1);
	int16_t err = dx + dy;
	int16_t e2;
	int16_t sx = (x1 < x2) ? 1 : -1;
	int16_t sy = (y1 < y2) ? 1 : -1;
	
	for (;;) {
		if ((x1 >= 0) && (x1 < IMG_BUF_WIDTH) && (y1 >= 0) && (y1 < IMG_BUF_HEIGHT)) {
			draw_pixel(img, x1, y1, c);
		}
		
		if ((x1 == x2) && (y1 == y2)) break;
		
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x1 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y1 += sy;
		}
	}
}


static void draw_fill_rect(uint8_t* img, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c)
{
	int16_t y1 = y;
	
	while (y1 < (y+h)) {
		draw_hline(img, x, x+w-1, y1, c);
		y1++;
	}
}


static int16_t draw_char(uint8_t* img, int16_t x, int16_t y, uint8_t c, const Font_TypeDef *Font)
{
	uint16_t pX;
	uint16_t pY;
	uint8_t tmpCh;
	uint8_t bL;
	const uint8_t *pCh;

	// If the specified character code is out of bounds should substitute the code of the "unknown" character
	if ((c < Font->font_MinChar) || (c > Font->font_MaxChar)) c = Font->font_UnknownChar;

	// Pointer to the first byte of character in font data array
	pCh = &Font->font_Data[(c - Font->font_MinChar) * Font->font_BPC];

	// Draw character
	if (Font->font_Scan == FONT_V) {
		// Vertical pixels order
		if (Font->font_Height < 9) {
			// Height is 8 pixels or less (one byte per column)
			pX = x;
			while (pX < x + Font->font_Width) {
				pY = y;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
					tmpCh >>= 1;
					pY++;
				}
				pX++;
			}
		} else {
			// Height is more than 8 pixels (several bytes per column)
			pX = x;
			while (pX < x + Font->font_Width) {
				pY = y;
				while (pY < y + Font->font_Height) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
							tmpCh >>= 1;
							if (tmpCh) {
								pY++;
								bL--;
							} else {
								pY += bL;
								break;
							}
						}
					} else {
						pY += bL;
					}
				}
				pX++;
			}
		}
	} else {
		// Horizontal pixels order
		if (Font->font_Width < 9) {
			// Width is 8 pixels or less (one byte per row)
			pY = y;
			while (pY < y + Font->font_Height) {
				pX = x;
				tmpCh = *pCh++;
				while (tmpCh) {
					if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
					tmpCh >>= 1;
					pX++;
				}
				pY++;
			}
		} else {
			// Width is more than 8 pixels (several bytes per row)
			pY = y;
			while (pY < y + Font->font_Height) {
				pX = x;
				while (pX < x + Font->font_Width) {
					bL = 8;
					tmpCh = *pCh++;
					if (tmpCh) {
						while (bL) {
							if (tmpCh & 0x01) draw_pixel(img, pX, pY, TEXT_COLOR);
							tmpCh >>= 1;
							if (tmpCh) {
								pX++;
								bL--;
							} else {
								pX += bL;
								break;
							}
						}
					} else {
						pX += bL;
					}
				}
				pY++;
			}
		}
	}

	return Font->font_Width + 1;
}


static void draw_string(uint8_t* img, int16_t x, int16_t y, const char *str, const Font_TypeDef *Font)
{
	uint16_t pX = x;
	uint16_t eX = IMG_BUF_WIDTH - Font->font_Width - 1;

	while (*str) {
		pX += draw_char(img, pX, y, *str++, Font);
		if (pX > eX) break;
	}
}


static __inline__ void draw_pixel(uint8_t* img, int16_t x, int16_t y, uint8_t c)
{
	if ((x < 0) || (x >= IMG_BUF_WIDTH)) return;
	if ((y < 0) || (y >= IMG_BUF_HEIGHT)) return;
	*(img + x + y*IMG_BUF_WIDTH) = c;
}


static uint16_t get_string_width(const char *str, const Font_TypeDef *Font)
{
	int n = strlen(str);
	
	return (int16_t) n * (Font->font_Width + 1);
}


static float lep_to_disp_temp(uint16_t v, gui_state_t* g)
{
	float t;
	
	if (g->rad_high_res) {
		t = lepton_kelvin_to_C(v, 0.01);
	} else {
		t = lepton_kelvin_to_C(v, 0.1);
	}

	// Convert to F if required
	if (!g->temp_unit_C) {
		t = t * 9.0 / 5.0 + 32.0;
	}
	
	return t;
}

