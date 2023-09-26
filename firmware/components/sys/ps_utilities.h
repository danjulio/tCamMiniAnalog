/*
 * Persistent Storage Module
 *
 * Manage the persistent storage kept in the ESP32 NVS and provide access
 * routines to it.
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
 * along with tCam.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef PS_UTILITIES_H
#define PS_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// PS Utilities Constants
//

// Parameters
#define PS_NUM_PARMS           3

#define PS_PARM_PALETTE_MARKER 0
#define PS_PARM_EMISSIVITY     1
#define PS_PARM_UNITS          2

// Default values
#define PS_PARAM_PALETTE_MARKER_DEF 0
#define PS_PARAM_EMISSIVITY_DEF     97
#define PS_PARAM_UNITS_DEF          0



//
// PS Utilities API
//
bool ps_init();
int ps_get_parm(int index);
void ps_set_parm(int index, int val);

#endif /* PS_UTILITIES_H */