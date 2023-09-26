/*
 * Control Interface Task
 *
 * Determine video output format.
 *
 * Manage user controls:
 *   Mode Button
 *   Red/Green Dual Status LED
 *
 * Copyright 2020-2022 Dan Julio
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
#ifndef CTRL_TASK_H
#define CTRL_TASK_H

#include <stdbool.h>


//
// Control Task Constants
//

// Control Task evaluation interval
#define CTRL_EVAL_MSEC             50

// Timeouts (multiples of CTRL_EVAL_MSEC)
#define CTRL_BTN_LONG_PRESS_MSEC   3000
#define CTRL_FAULT_BLINK_ON_MSEC   200
#define CTRL_FAULT_BLINK_OFF_MSEC  300
#define CTRL_FAULT_IDLE_MSEC       2000

// Fault Types - sets blink count too
#define CTRL_FAULT_NONE           0
#define CTRL_FAULT_ESP32_INIT     1
#define CTRL_FAULT_PERIPH_INIT    2
#define CTRL_FAULT_MEM_INIT       3
#define CTRL_FAULT_LEP_CCI        4
#define CTRL_FAULT_LEP_VOSPI      5
#define CTRL_FAULT_LEP_SYNC       6

// Video formats
#define CTRL_VID_FORMAT_NTSC      0
#define CTRL_VID_FORMAT_PAL       1

// Control Task notifications
#define CTRL_NOTIFY_STARTUP_DONE      0x00000001
#define CTRL_NOTIFY_FAULT             0x00000002
#define CTRL_NOTIFY_FAULT_CLEAR       0x00000004


//
// Control Task API
//
void ctrl_task();
void ctrl_get_if_mode(int* vid_format);
void ctrl_set_fault_type(int f);

#endif /* CTRL_TASK_H */