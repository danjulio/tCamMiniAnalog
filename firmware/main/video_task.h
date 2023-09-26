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
#ifndef VID_TASK_H
#define VID_TASK_H

#include <stdbool.h>
#include <stdint.h>

//
// VID Task Constants
//

// Evaluation rate (mSec)
#define VID_EVAL_MSEC  20

//
// VID Task notifications
//
// From lep_task
#define VID_NOTIFY_LEP_FRAME_MASK_1         0x00000001
#define VID_NOTIFY_LEP_FRAME_MASK_2         0x00000002

// From ctrl_task
#define VID_NOTIFY_PARM_CHANGE_MASK         0x00000010
#define VID_NOTIFY_PARM_SELECT_MASK         0x00000020



//
// VID Task API
//
void vid_task();

#endif /* VID_TASK_H */