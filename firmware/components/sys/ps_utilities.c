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
#include "ps_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"


//
// PS Utilities internal constants
//

// Uncomment the following directive to force an NVS memory erase (but only do it for one execution)
//#define PS_ERASE_NVS

// NVS namespace
#define STORAGE_NAMESPACE "tCamMiniAnalog"


//
// PS Utilities Internal variables
//
static const char* TAG = "ps_utilities";

// NVS namespace handle
static nvs_handle_t ps_handle;

// NVS Keys
static const char* palette_marker_info_key = "palette_marker";
static const char* emissivity_info_key = "emissivity";
static const char* units_info_key = "units";

// Local values
static int val_palette_marker;
static int val_emissivity;
static int val_units;



//
// PS Utilities API
//
bool ps_init()
{
	esp_err_t err;
	
	ESP_LOGI(TAG, "Init Persistant Storage");
	
	// Initialize the NVS Storage system
	err = nvs_flash_init();
#ifdef PS_ERASE_NVS
	ESP_LOGE(TAG, "NVS Erase");
	err = nvs_flash_erase();	
	err = nvs_flash_init();
#endif

	if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
		// NVS partition was truncated and needs to be erased
		err = nvs_flash_erase();
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "NVS Erase failed with err %d", err);
			return false;
		}
		
		// Retry init
		err = nvs_flash_init();
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "NVS Init failed with err %d", err);
			return false;
		}
	}
	
	// Open NVS Storage
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &ps_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "NVS Open %s failed with err %d", STORAGE_NAMESPACE, err);
		return false;
	}
	
	// Initialize our local copies
	err = nvs_get_i32(ps_handle, palette_marker_info_key, &val_palette_marker);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		// Not found : create it (first time)
		val_palette_marker = PS_PARAM_PALETTE_MARKER_DEF;
		err = nvs_set_i32(ps_handle, palette_marker_info_key, val_palette_marker);
		ESP_LOGI(TAG, "Creating NVS entry %s = %d", palette_marker_info_key, val_palette_marker);
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error accessing NVS entry %s (%d)", palette_marker_info_key, err);
	}
	
	err = nvs_get_i32(ps_handle, emissivity_info_key, &val_emissivity);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		// Not found : create it (first time)
		val_emissivity = PS_PARAM_EMISSIVITY_DEF;
		err = nvs_set_i32(ps_handle, emissivity_info_key, val_emissivity);
		ESP_LOGI(TAG, "Creating NVS entry %s = %d", emissivity_info_key, val_emissivity);
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error accessing NVS entry %s (%d)", emissivity_info_key, err);
	}
	
	err = nvs_get_i32(ps_handle, units_info_key, &val_units);
	if (err == ESP_ERR_NVS_NOT_FOUND) {
		// Not found : create it (first time)
		val_units = PS_PARAM_UNITS_DEF;
		err = nvs_set_i32(ps_handle, units_info_key, val_units);
		ESP_LOGI(TAG, "Creating NVS entry %s = %d", units_info_key, val_units);
	}
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error accessing NVS entry %s (%d)", units_info_key, err);
	}
	
	return true;
}


int ps_get_parm(int index)
{
	int ret = 0;
	
	switch (index) {
		case PS_PARM_PALETTE_MARKER:
			ret = val_palette_marker;
			break;
		case PS_PARM_EMISSIVITY:
			ret = val_emissivity;
			break;
		case PS_PARM_UNITS:
			ret = val_units;
			break;
		default:
			ESP_LOGE(TAG, "Read NVS index %d not supported", index);
	}
	
	return ret;
}


void ps_set_parm(int index, int val)
{
	esp_err_t err = ESP_OK;
	
	switch (index) {
		case PS_PARM_PALETTE_MARKER:
			val_palette_marker = val;
			err = nvs_set_i32(ps_handle, palette_marker_info_key, val);
			break;
		case PS_PARM_EMISSIVITY:
			val_emissivity = val;
			err = nvs_set_i32(ps_handle, emissivity_info_key, val);
			break;
		case PS_PARM_UNITS:
			val_units = val;
			err = nvs_set_i32(ps_handle, units_info_key, val);
			break;
		default:
			ESP_LOGE(TAG, "Write NVS index %d not supported", index);
	}
	
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error writing NVS index %d (%d)", index, err);
	}
}
