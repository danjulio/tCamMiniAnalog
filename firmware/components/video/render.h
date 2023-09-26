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
#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "lepton_utilities.h"
#include "sys_utilities.h"
#include "vospi.h"


//
// Render Constants
//

// Image buffer dimensions
#define IMG_BUF_MULT_FACTOR 2
#define IMG_BUF_WIDTH       (IMG_BUF_MULT_FACTOR * LEP_WIDTH)
#define IMG_BUF_HEIGHT      (IMG_BUF_MULT_FACTOR * LEP_HEIGHT)

// Spot meter
#define IMG_SPOT_MIN_SIZE   10

// Min/Max markers
#define IMG_MM_MARKER_SIZE  10

// Text intensity
#define TEXT_COLOR          250

// Text background intensity
#define TEXT_BG_COLOR       120


// Linear Interpolation Scale Factors
//  DS = Dual Source Pixel case (SF_DS is typically 2 or 3)
//  QS = Quad Source Pixel case (SF_QS is typically 3 or 5)
//
#define SF_DS 3
#define SF_QS 5
#define DIV_DS (SF_DS + 1)
#define DIV_QS (SF_QS + 3)


//
// Render Typedefs
//
// GUI state - state shared between screens
typedef struct {
	bool agc_enabled;            // Set by telemetry from Lepton to indicate image state
	bool black_hot_palette;      // Set for black_hot, clear for white_hot
	bool display_interp_enable;
	bool is_radiometric;         // Set by telemetry from Lepton to indicate if the lepton is radiometric
	bool min_max_enable;
	bool spotmeter_enable;
	bool temp_unit_C;
	bool rad_high_res;           // Set by telem when radiometric resolution is 0.01, clear when 0.1
} gui_state_t;


//
// Render API
//
void render_lep_data(lep_buffer_t* lep, uint8_t* img, gui_state_t* g);
void render_spotmeter(lep_buffer_t* lep, uint8_t* img, gui_state_t* g);
void render_min_max_markers(lep_buffer_t* lep, uint8_t* img);
void render_parm_string(const char* s, uint8_t* img);

#endif /* RENDER_H */