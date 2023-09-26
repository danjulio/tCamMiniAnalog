/*
 * Video Task
 *
 * Initialize video library and render lepton data info a frame buffer
 * for PAL or NTSC video output.
 *
 * Copyright 2023 Dan Julio
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
 * along with firecam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ctrl_task.h"
#include "lepton_utilities.h"
#include "render.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "video_task.h"
#include "video.h"
#include "Philips_PM5544_320x240.h"



//
// VID Task constants
//
#define HEADER_PIXEL(data,pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}


//
// VID Task Parameter setting related
//
#define NUM_PARMS 3
#define PARM_INDEX_MARKER     0
#define PARM_INDEX_EMISSIVITY 1
#define PARM_INDEX_UNITS      2
static const char* parm_gui_name[] = {"", "Emissivity: ", "Units: "};

// Marker Parameter related
//   0 : White Hot Palette, Markers off
//   1 : White Hot Palette, Markers on
//   2 : Black Hot Palette, Markers off
//   3 : Black Hot Palette, Markers on
#define NUM_M_PARM_VALS 4
#define M_PARM_MARKER_MASK  0x01
#define M_PARM_PALETTE_MASK 0x02

// Emissivity Parameter related
#define NUM_E_PARM_VALS 23
static const int parm_e_value[] = {10, 20, 30, 40, 50, 60, 70, 80, 82, 84, 86, 88,
                                   90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};

// Temperature Display Units related
//   0 : Imperial - F
//   1 : Metric - C
#define NUM_U_PARM_VALS 2
static const char* parm_u_name[] = {"Imperial", "Metric"};

// Timeout from non-default parameter selection
//  Must be longer than button long-press
#define PARM_ENTRY_TIMEOUT_MSEC (CTRL_BTN_LONG_PRESS_MSEC + 7000)


//
// VID Task variables
//
static const char* TAG = "vid_task";

// Notifications (clear after use)
static bool notify_image_1 = false;
static bool notify_image_2 = false;
static bool notify_parm_val_change = false;
static bool notify_parm_sel_change = false;

// Rendering GUI state
static gui_state_t gui_state;

// Video Driver Frame buffer pointer
static uint8_t* drv_fbP;

// Parameter selection and modification
static int cur_parm_index;
static int cur_parm_max_index;
static int cur_parm_value;
static int prev_parm_value;
static int parm_entry_timeout;
static char parm_string[80];



//
// VID Task Forward Declarations for internal functions
//
static void _vid_handle_notifications();
static void _vid_eval_parm_update();
static void _vid_render_image_pm554(bool pal_resolution);
static void _vid_render_image(int render_buf_index);
static void _vid_display_image(int render_buf_index);
static int _vid_get_emissivity_index(int cur_e);
static const char* _vid_get_parm_string();



//
// VID Task API
//
void vid_task()
{
	int vid_format;
	
	ESP_LOGI(TAG, "Start task");
	
	// Setup a default GUI state
	cur_parm_index = 0;  // Default to Palette/Marker
	cur_parm_max_index = NUM_M_PARM_VALS - 1;
	cur_parm_value = ps_get_parm(PS_PARM_PALETTE_MARKER);
	prev_parm_value = cur_parm_value;
	parm_entry_timeout = 0;
	gui_state.black_hot_palette = (cur_parm_value & M_PARM_PALETTE_MASK) == M_PARM_PALETTE_MASK;
	gui_state.display_interp_enable = true;
	gui_state.min_max_enable = (cur_parm_value & M_PARM_MARKER_MASK) == M_PARM_MARKER_MASK;
	gui_state.spotmeter_enable = (cur_parm_value & M_PARM_MARKER_MASK) == M_PARM_MARKER_MASK;
	gui_state.temp_unit_C = ps_get_parm(PS_PARM_UNITS) != 0;
	
	// Start the video subsystem with the appropriate video format.
	ctrl_get_if_mode(&vid_format);
	if (vid_format == CTRL_VID_FORMAT_NTSC) {
		video_init(IMG_BUF_WIDTH, IMG_BUF_HEIGHT, FB_FORMAT_GREY_8BPP, VIDEO_MODE_NTSC, false);
	} else {
		video_init(IMG_BUF_WIDTH, IMG_BUF_HEIGHT, FB_FORMAT_GREY_8BPP, VIDEO_MODE_PAL, false);
	}
	
	// Setup a default image
	_vid_render_image_pm554(vid_format == CTRL_VID_FORMAT_PAL);
	
	// Get a pointer to the frame buffer
	drv_fbP = video_get_frame_buffer_address();
	
	while (1) {
		_vid_handle_notifications();
		
		_vid_eval_parm_update();
		
		// Display the previously rendered image to minimize tearing then render the 
		// current lepton data for next time
		if (notify_image_1) {
			notify_image_1 = false;
			video_wait_frame();
			_vid_display_image(1);
			_vid_render_image(0);
		}
		
		if (notify_image_2) {
			notify_image_2 = false;
			video_wait_frame();
			_vid_display_image(0);
			_vid_render_image(1);
		}
		
		vTaskDelay(pdMS_TO_TICKS(VID_EVAL_MSEC));
	}
}


//
// Internal functions
//
static void _vid_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, VID_NOTIFY_LEP_FRAME_MASK_1)) {
			notify_image_1 = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_LEP_FRAME_MASK_2)) {
			notify_image_2 = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_PARM_CHANGE_MASK)) {
			notify_parm_val_change = true;
		}
		
		if (Notification(notification_value, VID_NOTIFY_PARM_SELECT_MASK)) {
			notify_parm_sel_change = true;
		}
	}
}


static void _vid_eval_parm_update()
{
	int64_t cur_time;
	static int64_t prev_time;
	
	if (notify_parm_val_change) {
		notify_parm_val_change = false;
		
		// Select next parameter value
		if (++cur_parm_value > cur_parm_max_index) cur_parm_value = 0;
		
		// Update operating value associated with parameter
		switch (cur_parm_index) {
			case PARM_INDEX_EMISSIVITY:
				// Notify lep_task of the new emissivity
				lepton_emissivity((uint16_t) parm_e_value[cur_parm_value]);
				break;
			case PARM_INDEX_UNITS:
				// Update our display units
				gui_state.temp_unit_C = cur_parm_value != 0;
				break;
			default:
				// Update our marker enable and palette
				gui_state.black_hot_palette = (cur_parm_value & M_PARM_PALETTE_MASK) == M_PARM_PALETTE_MASK;
				gui_state.min_max_enable = (cur_parm_value & M_PARM_MARKER_MASK) == M_PARM_MARKER_MASK;
				gui_state.spotmeter_enable = (cur_parm_value & M_PARM_MARKER_MASK) == M_PARM_MARKER_MASK;
		}
		
		// Restart timer used to decide user has finished changing and time to update persistant storage
		parm_entry_timeout = PARM_ENTRY_TIMEOUT_MSEC;
		prev_time = esp_timer_get_time() / 1000;
	} else if (notify_parm_sel_change) {
		notify_parm_sel_change = false;
		
		// See if there was a change with the previous parameter
		if (cur_parm_value != prev_parm_value) {
			switch (cur_parm_index) {
				case PARM_INDEX_EMISSIVITY:
					ps_set_parm(PS_PARM_EMISSIVITY, parm_e_value[cur_parm_value]);
					break;
				case PARM_INDEX_UNITS:
					ps_set_parm(PS_PARM_UNITS, cur_parm_value);
					break;
				default:
					ps_set_parm(PS_PARM_PALETTE_MARKER, cur_parm_value);
			}
		}
		
		// Setup next parameter index to change
		if (++cur_parm_index == NUM_PARMS) {
			cur_parm_index = 0;
			parm_entry_timeout = 0;  // No timeout for default parameter
		} else {
			parm_entry_timeout = PARM_ENTRY_TIMEOUT_MSEC;
			prev_time = esp_timer_get_time() / 1000;
		}
		
		// Get starting parameter value
		switch (cur_parm_index) {
			case PARM_INDEX_EMISSIVITY:
				cur_parm_value = _vid_get_emissivity_index(ps_get_parm(PS_PARM_EMISSIVITY));
				cur_parm_max_index = NUM_E_PARM_VALS - 1;
				break;
			case PARM_INDEX_UNITS:
				cur_parm_value = ps_get_parm(PS_PARM_UNITS);
				cur_parm_max_index = NUM_U_PARM_VALS - 1;
				break;
			default:
				cur_parm_value = ps_get_parm(PS_PARM_PALETTE_MARKER);
				cur_parm_max_index = NUM_M_PARM_VALS - 1;
		}
		
		// Save current value for comparison when done with this parameter
		prev_parm_value = cur_parm_value;
	} else if (parm_entry_timeout != 0) {
		cur_time = esp_timer_get_time() / 1000;
		if ((cur_time - prev_time) >= parm_entry_timeout) {
			// Timeout entering this parameter so store any changes
			if (cur_parm_value != prev_parm_value) {
				switch (cur_parm_index) {
					case PARM_INDEX_EMISSIVITY:
						ps_set_parm(PS_PARM_EMISSIVITY, parm_e_value[cur_parm_value]);
						break;
					case PARM_INDEX_UNITS:
						ps_set_parm(PS_PARM_UNITS, cur_parm_value);
						break;
					default:
						ps_set_parm(PS_PARM_PALETTE_MARKER, cur_parm_value);
				}
			}
			
			// Go back to default display (if not there already)
			cur_parm_index = 0;
			cur_parm_max_index = NUM_M_PARM_VALS - 1;
			cur_parm_value = ps_get_parm(PS_PARM_PALETTE_MARKER);
			prev_parm_value = cur_parm_value;
			parm_entry_timeout = 0;
		}
	}
}


static void _vid_render_image_pm554(bool pal_resolution)
{
    uint8_t pixel[3];
    uint8_t grey1,grey2;
    uint8_t mask;

    const char* p= pal_resolution ? pm5544_320x240_data : pm5544_320x240_data;
    const size_t ratio = 8/g_video_signal.bits_per_pixel;

    for(unsigned int y=0; y<g_video_signal.height_pixels; y++)
    {
        for(unsigned int x=0;x<g_video_signal.width_pixels/ratio; x++)
        {
            HEADER_PIXEL(p, pixel);
            grey1 = 0.30*pixel[0] + 0.59*pixel[1] + 0.11*pixel[2];
            
            if( g_video_signal.bits_per_pixel == 4 )
            {
                HEADER_PIXEL(p, pixel);
                grey2 = 0.30*pixel[0] + 0.59*pixel[1] + 0.11*pixel[2];

                g_video_signal.frame_buffer[y*g_video_signal.width_pixels/ratio+x] = (grey2/16) << 4 | (grey1/16);

            }
            else if(g_video_signal.bits_per_pixel == 8)
            {
                g_video_signal.frame_buffer[y*g_video_signal.width_pixels+x] = grey1;
            }
            else if(g_video_signal.bits_per_pixel == 1)
            {
                const uint8_t WHITE_LEVEL = 128;
                mask = 0;
                int i=8;
                while(i--)
                {
                    // 8 pixels
                    HEADER_PIXEL(p, pixel);
                    grey1 = 0.30*pixel[0] + 0.59*pixel[1] + 0.11*pixel[2];
                    if( grey1 >= WHITE_LEVEL )
                    {
                        mask |= 1U<<i;
                    }
                }

                g_video_signal.frame_buffer[y*g_video_signal.width_pixels/ratio+x] = mask;
            }
            else
            {
                assert(false);
            }


        }
    }
}


static void _vid_render_image(int render_buf_index)
{
	lep_buffer_t* lepP = (render_buf_index == 0) ? &vid_lep_buffer[0] : &vid_lep_buffer[1];
	uint8_t* rendP = rend_fbP[render_buf_index];
	
	// Get some information from the image
	gui_state.agc_enabled = (lepton_get_tel_status(lepP->lep_telemP) & LEP_STATUS_AGC_STATE) == LEP_STATUS_AGC_STATE;
	gui_state.is_radiometric = lepton_is_radiometric();
	gui_state.rad_high_res = lepP->lep_telemP[LEP_TEL_TLIN_RES] != 0;
	
	// Render the image into the frame buffer
	render_lep_data(lepP, rendP, &gui_state);
	
	if (gui_state.min_max_enable) {
		render_min_max_markers(lepP, rendP);
	}
	
	if (gui_state.spotmeter_enable && gui_state.is_radiometric) {
		render_spotmeter(lepP, rendP, &gui_state);
	}
	
	if (cur_parm_index != PARM_INDEX_MARKER) {
		render_parm_string(_vid_get_parm_string(), rendP);
	}
}


static void _vid_display_image(int render_buf_index)
{
	uint8_t* drvP = drv_fbP;
	uint8_t* rendP = rend_fbP[render_buf_index];
	uint8_t* rendEndP = rendP + IMG_BUF_WIDTH*IMG_BUF_HEIGHT;
	
	// Quickly copy the previously rendered buffer to the video driver's buffer
	while (rendP < rendEndP) *drvP++ = *rendP++;
}


static int _vid_get_emissivity_index(int cur_e)
{
	for (int i=0; i<NUM_E_PARM_VALS; i++) {
		if (parm_e_value[i] == cur_e) return i;
	}
	
	return (NUM_E_PARM_VALS - 1);
}


static const char* _vid_get_parm_string()
{
	switch (cur_parm_index) {
		case PARM_INDEX_EMISSIVITY:
			sprintf(parm_string, "%s %d", parm_gui_name[cur_parm_index], parm_e_value[cur_parm_value]);
			break;
		case PARM_INDEX_UNITS:
			sprintf(parm_string, "%s %s", parm_gui_name[cur_parm_index], parm_u_name[cur_parm_value]);
			break;
		default:
			// Should never see this but just in case, there is nothing to display for the default index
			parm_string[0] = 0;
	}
	
	return (const char*) parm_string;
}