/*
 * System Configuration File
 *
 * Contains system definition and configurable items.
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
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "esp_system.h"



// ======================================================================================
// System debug
//

// Undefine to include the system monitoring task (included only for debugging/tuning)
//#define INCLUDE_SYS_MON



// ======================================================================================
// System hardware definitions
//

//
// IO Pins
//
#define BRD_VID_SENSE_IO      32

#define BRD_BTN_IO            0
#define BRD_RED_LED_IO        14
#define BRD_GREEN_LED_IO      15

#define BRD_LEP_SCK_IO        5
#define BRD_LEP_CSN_IO        12
#define BRD_LEP_VSYNC_IO      13
#define BRD_LEP_MISO_IO       19
#define BRD_LEP_RESET_IO      21

#define BRD_I2C_MASTER_SDA_IO 23
#define BRD_I2C_MASTER_SCL_IO 22

#define BRD_VID_OUT_IO        26



//
// Hardware Configuration
//

// I2C
#define I2C_MASTER_NUM     1
#define I2C_MASTER_FREQ_HZ 100000

// SPI
//   Lepton uses HSPI (no MOSI)
#define LEP_SPI_HOST    HSPI_HOST
#define LEP_DMA_NUM     2
#define LEP_SPI_FREQ_HZ 16000000



// ======================================================================================
// System configuration
//


#endif // SYSTEM_CONFIG_H