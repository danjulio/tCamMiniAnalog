/*
 * tCam-Mini Analog Output Main
 *
 * This program implements a thermal imaging camera based around a Flir 
 * Lepton 3.X module and outputs a NTSC or PAL analog video signal.
 * It is designed to operate on tCam-Mini hardware with the video signal
 * output on the DAC output capable GPIO26.  See below for hardware notes.
 * It operates radiometric Leptons in AGC mode and can display a spotmeter
 * temperature and min/max points on the display.  It displays min/max
 * points for non-radiometric Leptons without the spotmeter display.
 * Output is monochrome with the option of white-hot or black-hot palettes.
 * The tCam-Mini push button is used to set operating parameters (stored
 * in Flash NV memory).  A long press (more than 3 seconds) moves between
 * configuration items.  A short press changes the selected configuration
 * item.
 *
 *   Configuration Items
 *     1. (Default) : Toggle between palette and marker on/off combinations
 *        (4 selections)
 *     2. Emissivity : Select from a list of various emissivity values.  The
 *        current value is displayed overlaying the image.
 *     3. Units : Select between Imperial and Metric units for temperature
 *        display.  The current value is displayed overlaying the image.
 *
 * Resolution is 320x240 pixels which is slightly vertically over-scanned on a
 * NTSC monitor.
 *
 * Hardware note
 * -------------
 * By default the firmware is configured to use a DAC output range of 0 - 2.36V
 * allowing 128 intensity levels.  This requires an external 180 ohm resistor in
 * series with the video device input as shown below to ensure a video signal of
 * 0 - 1V into a 75 ohm input.
 *
 *                100 ohm
 *    GPIO26 -----/\/\/\/\------- Video Input
 *    GND    -------------------- Video Shield
 *
 * Go to menuconfig->Component Config->Enable fullscale (2.36V) DAC output
 * and set CONFIG_VIDEO_USE_FS_DC to false to configure the DAC for an output
 * voltage range of 0 - 1V and reduced level of intensity levels which can be
 * driven directly into a video input.
 *
 *
 * Copyright 2020-2023 Dan Julio
 *
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
 */
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ctrl_task.h"
#include "lep_task.h"
#include "mon_task.h"
#include "video_task.h"
#include "system_config.h"
#include "sys_utilities.h"



static const char* TAG = "main";

void app_main(void)
{
	ESP_LOGI(TAG, "tCamMiniAnalog starting");
    
    // Start the control task to light the red light immediately
    // and to determine what type of video we will be generating
    xTaskCreatePinnedToCore(&ctrl_task, "ctrl_task", 2176, NULL, 1, &task_handle_ctrl, 0);
    
    // Initialize the SPI and I2C drivers
    if (!system_esp_io_init()) {
    	ESP_LOGE(TAG, "ESP32 init failed");
    	ctrl_set_fault_type(CTRL_FAULT_ESP32_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Initialize the camera's peripheral devices
    if (!system_peripheral_init()) {
    	ESP_LOGE(TAG, "Peripheral init failed");
    	ctrl_set_fault_type(CTRL_FAULT_PERIPH_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Pre-allocate big buffers
    if (!system_buffer_init()) {
    	ESP_LOGE(TAG, "Memory allocate failed");
    	ctrl_set_fault_type(CTRL_FAULT_MEM_INIT);
    	while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
    }
    
    // Notify control task that we've successfully started up
    xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_STARTUP_DONE, eSetBits);
    
    // Start tasks
    //  Core 0 : PRO - video task
    xTaskCreatePinnedToCore(&vid_task, "vid_task",  2816, NULL, 2, &task_handle_vid,  0);
    
    // Delay for Lepton internal initialization on power-on (max 950 mSec) and to let
    // test image display for a bit
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    //  Core 1 : APP - lepton task
    xTaskCreatePinnedToCore(&lep_task, "lep_task",  2304, NULL, 2, &task_handle_lep,  1);

#ifdef INCLUDE_SYS_MON
	xTaskCreatePinnedToCore(&mon_task, "mon_task",  2048, NULL, 1, &task_handle_mon,  0);
#endif
}
