/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions, a set
 * of globally available handles for the various tasks (to use for task notifications).
 *
 * Copyright 2020-2023 Dan Julio
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
#ifndef SYS_UTILITIES_H
#define SYS_UTILITIES_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "system_config.h"
#include <stdbool.h>
#include <stdint.h>


//
// System Utilities constants
//

// Gain mode
#define SYS_GAIN_HIGH 0
#define SYS_GAIN_LOW  1
#define SYS_GAIN_AUTO 2



//
// System Utilities typedefs
//
typedef struct {
	bool telem_valid;
	uint16_t lep_min_val;
	uint16_t lep_min_x;
	uint16_t lep_min_y;
	uint16_t lep_max_val;
	uint16_t lep_max_x;
	uint16_t lep_max_y;
	uint16_t* lep_bufferP;
	uint16_t* lep_telemP;
	SemaphoreHandle_t lep_mutex;
} lep_buffer_t;



//
// System Utilities macros
//
#define Notification(var, mask) ((var & mask) == mask)



//
// Task handle externs for use by tasks to communicate with each other
//
extern TaskHandle_t task_handle_ctrl;
extern TaskHandle_t task_handle_lep;
extern TaskHandle_t task_handle_vid;
#ifdef INCLUDE_SYS_MON
extern TaskHandle_t task_handle_mon;
#endif



//
// Global buffer pointers for allocated memory
//

// Shared memory data structures
extern lep_buffer_t vid_lep_buffer[2];   // Ping-pong buffer loaded by lep_task for vid_task
extern uint8_t* rend_fbP[2];      // Ping-pong rendering buffers for vid_task



//
// System Utilities API
//
bool system_esp_io_init();
bool system_peripheral_init();
bool system_buffer_init();
 
#endif /* SYS_UTILITIES_H */