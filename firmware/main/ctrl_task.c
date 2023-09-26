/*
 * Control Interface Task
 *
 * Determine video output format.
 *
 * Manage user controls:
 *   Mode Button
 *   Red/Green Dual Status LED
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
#include <stdbool.h>
#include "ctrl_task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_config.h"
#include "sys_utilities.h"
#include "video_task.h"



//
// Control Task Constants
//

// LED Colors
#define CTRL_LED_OFF 0
#define CTRL_LED_RED 1
#define CTRL_LED_YEL 2
#define CTRL_LED_GRN 3

// LED State Machine states
#define CTRL_LED_ST_SOLID      0
#define CTRL_LED_ST_FLT_ON     1
#define CTRL_LED_ST_FLT_OFF    2
#define CTRL_LED_ST_FLT_IDLE   3

// Control State Machine states
#define CTRL_ST_STARTUP        0
#define CTRL_ST_RUN            1
#define CTRL_ST_FAULT          2


//
// Control Task variables
//
static const char* TAG = "ctrl_task";

// Board/IO related
static int ctrl_vid_format;
static const int ctrl_pin_btn = BRD_BTN_IO;
static const int ctrl_pin_r_led = BRD_RED_LED_IO;
static const int ctrl_pin_g_led = BRD_GREEN_LED_IO;

// State
static int ctrl_state;
static int ctrl_pre_activity_state;
static int ctrl_led_state;
//static int ctrl_action_timer;
static int ctrl_led_timer;
static int ctrl_fault_led_count;
static int ctrl_fault_type = CTRL_FAULT_NONE;



//
// Forward Declarations for internal functions
//
static void ctrl_task_init();
static void ctrl_debounce_button(bool* short_p, bool* long_p);
static void ctrl_set_led(int color);
static void ctrl_eval_sm();
static void ctrl_set_state(int new_st);
static void ctrl_eval_led_sm();
static void ctrl_set_led_state(int new_st);
static void ctrl_handle_notifications();



//
// API
//
void ctrl_task()
{
	ESP_LOGI(TAG, "Start task");
	
	ctrl_task_init();
	
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(CTRL_EVAL_MSEC));
		ctrl_handle_notifications();
		ctrl_eval_led_sm();
		ctrl_eval_sm();
	}
}

void ctrl_get_if_mode(int* vid_format)
{
	*vid_format = ctrl_vid_format;
}


// Not protected by semaphore since it won't be accessed until after subsequent notification
void ctrl_set_fault_type(int f)
{
	ctrl_fault_type = f;
	
	if (f == CTRL_FAULT_NONE) {
		// Asynchronously notify this task to return to the previous state
		xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_FAULT_CLEAR, eSetBits);
	} else {
		// Save the existing state to return to when the fault is cleared
		ctrl_pre_activity_state = ctrl_state;
		
		// Asynchronously notify this task
		xTaskNotify(task_handle_ctrl, CTRL_NOTIFY_FAULT, eSetBits);
	}
}



//
// Internal functions
//
static void ctrl_task_init()
{
	// Determine the video mode
	gpio_reset_pin(BRD_VID_SENSE_IO);
	gpio_set_direction(BRD_VID_SENSE_IO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BRD_VID_SENSE_IO, GPIO_PULLUP_ONLY);
	if (gpio_get_level(BRD_VID_SENSE_IO) == 1) {
		ctrl_vid_format = CTRL_VID_FORMAT_NTSC;
	} else {
		ctrl_vid_format = CTRL_VID_FORMAT_PAL;
	}
	
	// Setup the GPIO
	gpio_reset_pin((gpio_num_t) ctrl_pin_btn);
	gpio_set_direction((gpio_num_t) ctrl_pin_btn, GPIO_MODE_INPUT);
	gpio_pullup_en((gpio_num_t) ctrl_pin_btn);
	
	gpio_reset_pin((gpio_num_t) ctrl_pin_r_led);
	gpio_set_direction((gpio_num_t) ctrl_pin_r_led, GPIO_MODE_OUTPUT);
	gpio_set_drive_capability((gpio_num_t) ctrl_pin_r_led, GPIO_DRIVE_CAP_3);
	
	gpio_reset_pin((gpio_num_t) ctrl_pin_g_led);
	gpio_set_direction((gpio_num_t) ctrl_pin_g_led, GPIO_MODE_OUTPUT);
	gpio_set_drive_capability((gpio_num_t) ctrl_pin_g_led, GPIO_DRIVE_CAP_3);
	
	// Initialize state
	ctrl_set_state(CTRL_ST_STARTUP);
}


static void ctrl_debounce_button(bool* short_p, bool* long_p)
{
	// Button press state
	static bool prev_btn = false;
	static bool btn_down = false;
	static int btn_timer;
	
	// Dynamic variables
	bool cur_btn;
	bool btn_released = false;
	
	// Outputs will be set as necessary
	*short_p = false;
	*long_p = false;
	
	// Get current button value
	cur_btn = gpio_get_level(ctrl_pin_btn) == 0;
	
	// Evaluate button logic
	if (cur_btn && prev_btn && !btn_down) {
		// Button just pressed
		btn_down = true;
		btn_timer = CTRL_BTN_LONG_PRESS_MSEC / CTRL_EVAL_MSEC;
	}
	if (!cur_btn && !prev_btn && btn_down) {
		btn_down = false;
		btn_released = true;
	}
	prev_btn = cur_btn;
	
	// Evaluate timer for long press detection
	if (btn_down) {
		if (btn_timer != 0) {
			if (--btn_timer == 0) {
				// Long press detected
				*long_p = true;
			}
		}
	}
	
	if (btn_released && (btn_timer != 0)) {
		// Short press detected
		*short_p = true;
	}
}


static void ctrl_set_led(int color)
{
	switch (color) {
		case CTRL_LED_OFF:
			gpio_set_level((gpio_num_t) ctrl_pin_r_led, 0);
			gpio_set_level((gpio_num_t) ctrl_pin_g_led, 0);
			break;
		
		case CTRL_LED_RED:
			gpio_set_level((gpio_num_t) ctrl_pin_r_led, 1);
			gpio_set_level((gpio_num_t) ctrl_pin_g_led, 0);
			break;
		
		case CTRL_LED_YEL:
			gpio_set_level((gpio_num_t) ctrl_pin_r_led, 1);
			gpio_set_level((gpio_num_t) ctrl_pin_g_led, 1);
			break;
		
		case CTRL_LED_GRN:
			gpio_set_level((gpio_num_t) ctrl_pin_r_led, 0);
			gpio_set_level((gpio_num_t) ctrl_pin_g_led, 1);
			break;
	}
}


static void ctrl_eval_sm()
{
	bool btn_short_press;
	bool btn_long_press;
	
	// Look for button presses
	ctrl_debounce_button(&btn_short_press, &btn_long_press);
	
	switch (ctrl_state) {
		case CTRL_ST_STARTUP:
			// Wait to be taken out of this state by a notification
			break;

		case CTRL_ST_RUN:
			// Notify video task of button presses
			if (btn_short_press) {
				xTaskNotify(task_handle_vid, VID_NOTIFY_PARM_CHANGE_MASK, eSetBits);
			}
			if (btn_long_press) {
				xTaskNotify(task_handle_vid, VID_NOTIFY_PARM_SELECT_MASK, eSetBits);
			}
			break;
		
		case CTRL_ST_FAULT:
			// Remain here until taken out if the fault is cleared
			break;
		
		default:
			ctrl_set_state(CTRL_ST_STARTUP);
	}
}


static void ctrl_eval_led_sm()
{
	switch (ctrl_led_state) {
		case CTRL_LED_ST_SOLID:
			// Wait to be taken out of this state
			break;
		
		case CTRL_LED_ST_FLT_ON:
			if (--ctrl_led_timer == 0) {
				ctrl_set_led_state(CTRL_LED_ST_FLT_OFF);
			}
			break;
		
		case CTRL_LED_ST_FLT_OFF:
			if (--ctrl_led_timer == 0) {
				if (--ctrl_fault_led_count == 0) {
					ctrl_set_led_state(CTRL_LED_ST_FLT_IDLE);
				} else {
					ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
				}
			}
			break;
		
		case CTRL_LED_ST_FLT_IDLE:
			if (--ctrl_led_timer == 0) {
				ctrl_fault_led_count = ctrl_fault_type;
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
			}
			break;
	}
}


static void ctrl_set_state(int new_st)
{
	ctrl_state = new_st;
	
	switch (new_st) {
		case CTRL_ST_STARTUP:
			ctrl_set_led(CTRL_LED_YEL);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
		
		case CTRL_ST_RUN:
			ctrl_set_led(CTRL_LED_GRN);
			ctrl_set_led_state(CTRL_LED_ST_SOLID);
			break;
			
		case CTRL_ST_FAULT:
			if (ctrl_fault_type != CTRL_FAULT_NONE) {
				// Setup to blink fault code
				ctrl_fault_led_count = ctrl_fault_type;
				ctrl_set_led_state(CTRL_LED_ST_FLT_ON);
			}
			break;
	}
}


static void ctrl_set_led_state(int new_st)
{
	ctrl_led_state = new_st;
	
	switch (new_st) {
		case CTRL_LED_ST_SOLID:
			// LED Color set outside this call
			break;
		
		case CTRL_LED_ST_FLT_ON:
			ctrl_led_timer = CTRL_FAULT_BLINK_ON_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_RED);
			break;
		
		case CTRL_LED_ST_FLT_OFF:
			ctrl_led_timer = CTRL_FAULT_BLINK_OFF_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
			
		case CTRL_LED_ST_FLT_IDLE:
			ctrl_led_timer = CTRL_FAULT_IDLE_MSEC / CTRL_EVAL_MSEC;
			ctrl_set_led(CTRL_LED_OFF);
			break;
	}
}


static void ctrl_handle_notifications()
{
	uint32_t notification_value;
	
	notification_value = 0;
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, CTRL_NOTIFY_STARTUP_DONE)) {
			if (ctrl_state != CTRL_ST_FAULT) {
				ctrl_set_state(CTRL_ST_RUN);
			}
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FAULT)) {
			ctrl_set_state(CTRL_ST_FAULT);
		}
		
		if (Notification(notification_value, CTRL_NOTIFY_FAULT_CLEAR)) {
			// Return to the pre-fault state
			ctrl_set_state(ctrl_pre_activity_state);
		}
	}
}
