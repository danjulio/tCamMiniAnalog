/*
 * System related utilities
 *
 * Contains functions to initialize the system, other utility functions and a set
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
#include "ctrl_task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "ps_utilities.h"
#include "sys_utilities.h"
#include "i2c.h"
#include "render.h"
#include "vospi.h"
#include <string.h>



//
// System Utilities internal constants
//



//
// System Utilities variables
//
static const char* TAG = "sys";


//
// Task handle externs for use by tasks to communicate with each other
//
TaskHandle_t task_handle_ctrl;
TaskHandle_t task_handle_lep;
TaskHandle_t task_handle_vid;
#ifdef INCLUDE_SYS_MON
TaskHandle_t task_handle_mon;
#endif



//
// Global buffer pointers for memory allocated in the external SPIRAM
//

// Shared memory data structures
lep_buffer_t vid_lep_buffer[2];   // Ping-pong buffer loaded by lep_task for vid_task
uint8_t* rend_fbP[2];      // Ping-pong rendering buffers for vid_task



//
// System Utilities API
//

/**
 * Initialize the ESP32 GPIO and internal peripherals
 */
bool system_esp_io_init()
{
	esp_err_t ret;
	spi_bus_config_t spi_buscfg;
	
	ESP_LOGI(TAG, "ESP32 Peripheral Initialization");	
	
	// Attempt to initialize the I2C Master
	ret = i2c_master_init(BRD_I2C_MASTER_SCL_IO, BRD_I2C_MASTER_SDA_IO);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "I2C Master initialization failed");
		return false;
	}
	
	// Attempt to initialize the SPI Master used by the lep_task
	memset(&spi_buscfg, 0, sizeof(spi_bus_config_t));
	spi_buscfg.miso_io_num=BRD_LEP_MISO_IO;
	spi_buscfg.mosi_io_num=-1;
	spi_buscfg.sclk_io_num=BRD_LEP_SCK_IO;
	spi_buscfg.max_transfer_sz=LEP_PKT_LENGTH;
	spi_buscfg.quadwp_io_num=-1;
	spi_buscfg.quadhd_io_num=-1;
	
	if (spi_bus_initialize(LEP_SPI_HOST, &spi_buscfg, LEP_DMA_NUM) != ESP_OK) {
		ESP_LOGE(TAG, "Lepton Master initialization failed");
		return false;
	}
	
	return true;
}


/**
 * Initialize the board-level peripheral subsystems
 */
bool system_peripheral_init()
{
	ESP_LOGI(TAG, "System Peripheral Initialization");
	
	if (!ps_init()) {
		ESP_LOGE(TAG, "Persistent Storage initialization failed");
		return false;
	}
	
	// Initialize the Lepton GPIO and then reset the Lepton
	// (reset handles potential external crystal oscillator slow start-up)
	gpio_set_direction(BRD_LEP_VSYNC_IO, GPIO_MODE_INPUT);
	gpio_set_direction(BRD_LEP_RESET_IO, GPIO_MODE_OUTPUT);
	gpio_set_level(BRD_LEP_RESET_IO, 1);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(BRD_LEP_RESET_IO, 0);
	
	return true;
}


/**
 * Allocate shared buffers for use by tasks for image data in the external RAM
 */
bool system_buffer_init()
{
	ESP_LOGI(TAG, "Buffer Allocation");
	
	// Allocate the LEP/RSP task lepton frame and telemetry ping-pong buffers
	vid_lep_buffer[0].lep_bufferP = heap_caps_malloc(LEP_NUM_PIXELS*2, MALLOC_CAP_SPIRAM);
	if (vid_lep_buffer[0].lep_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc VID lepton shared image buffer 0 failed");
		return false;
	}
	vid_lep_buffer[0].lep_telemP = heap_caps_malloc(LEP_TEL_WORDS*2, MALLOC_CAP_SPIRAM);
	if (vid_lep_buffer[0].lep_telemP == NULL) {
		ESP_LOGE(TAG, "malloc VID lepton shared telemetry buffer 0 failed");
		return false;
	}
	vid_lep_buffer[1].lep_bufferP = heap_caps_malloc(LEP_NUM_PIXELS*2, MALLOC_CAP_SPIRAM);
	if (vid_lep_buffer[1].lep_bufferP == NULL) {
		ESP_LOGE(TAG, "malloc VID lepton shared image buffer 1 failed");
		return false;
	}
	vid_lep_buffer[1].lep_telemP = heap_caps_malloc(LEP_TEL_WORDS*2, MALLOC_CAP_SPIRAM);
	if (vid_lep_buffer[1].lep_telemP == NULL) {
		ESP_LOGE(TAG, "malloc VID lepton shared telemetry buffer 1 failed");
		return false;
	}
	
	// Create the ping-pong buffer access mutexes
	vid_lep_buffer[0].lep_mutex = xSemaphoreCreateMutex();
	if (vid_lep_buffer[0].lep_mutex == NULL) {
		ESP_LOGE(TAG, "create VID lepton mutex 0 failed");
		return false;
	}
	vid_lep_buffer[1].lep_mutex = xSemaphoreCreateMutex();
	if (vid_lep_buffer[1].lep_mutex == NULL) {
		ESP_LOGE(TAG, "create VID lepton mutex 1 failed");
		return false;
	}
	
	// Create the ping-pong rendering buffers
	rend_fbP[0] = heap_caps_malloc(IMG_BUF_WIDTH*IMG_BUF_HEIGHT, MALLOC_CAP_SPIRAM);
	if (rend_fbP[0] == NULL) {
		ESP_LOGE(TAG, "create rendering buffer 0 failed");
		return false;
	}
	rend_fbP[1] = heap_caps_malloc(IMG_BUF_WIDTH*IMG_BUF_HEIGHT, MALLOC_CAP_SPIRAM);
	if (rend_fbP[1] == NULL) {
		ESP_LOGE(TAG, "create rendering buffer 1 failed");
		return false;
	}
	
	return true;
}
