#include "sdkconfig.h"

#if CONFIG_TINYUSB_ENABLED && CONFIG_TINYUSB_HID_ENABLED

#include "managers/payloads/click_high_speed.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#define CLICK_HIGH_SPEED_RELEASE_TIMEOUT_MS 100
#define CLICK_HIGH_SPEED_TASK_STACK 4096
#define CLICK_HIGH_SPEED_TASK_PRIORITY 6

static uint8_t s_mouse_interface = 0;
static uint8_t s_mouse_button = 0;
static volatile bool *s_stop_requested = NULL;
static volatile bool s_running = false;
static volatile bool s_failed = false;
static TaskHandle_t s_task_handle = NULL;

static bool click_high_speed_should_continue(void) {
    return (s_stop_requested == NULL) || !*s_stop_requested;
}

static bool click_high_speed_wait_ready(void) {
    while (!tud_hid_n_ready(s_mouse_interface) && click_high_speed_should_continue()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return tud_hid_n_ready(s_mouse_interface) && click_high_speed_should_continue();
}

static bool click_high_speed_wait_ready_timeout(uint32_t timeout_ms) {
    uint32_t waited_ms = 0;
    while (!tud_hid_n_ready(s_mouse_interface) && waited_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(1));
        waited_ms++;
    }

    return tud_hid_n_ready(s_mouse_interface);
}

static bool click_high_speed_send_report(uint8_t buttons) {
    if (!click_high_speed_wait_ready()) {
        return false;
    }

    return tud_hid_n_mouse_report(s_mouse_interface, 0, buttons, 0, 0, 0, 0);
}

static bool click_high_speed_release(void) {
    if (!click_high_speed_wait_ready_timeout(CLICK_HIGH_SPEED_RELEASE_TIMEOUT_MS)) {
        return false;
    }

    return tud_hid_n_mouse_report(s_mouse_interface, 0, 0, 0, 0, 0, 0);
}

static bool click_high_speed_click_once(void) {
    if (!click_high_speed_should_continue()) {
        return false;
    }

    if (!click_high_speed_send_report(s_mouse_button)) {
        return false;
    }

    if (!click_high_speed_should_continue()) {
        (void)click_high_speed_release();
        return false;
    }

    return click_high_speed_release();
}

static void click_high_speed_task(void *arg) {
    (void)arg;

    s_running = true;
    s_failed = false;

    (void)click_high_speed_release();

    while (click_high_speed_should_continue()) {
        if (!click_high_speed_click_once()) {
            taskYIELD();
        }
    }

    if (!click_high_speed_release()) {
        s_failed = true;
    }

    s_running = false;
    s_stop_requested = NULL;
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

bool click_high_speed_start(uint8_t mouse_interface, uint8_t mouse_button, volatile bool *stop_requested) {
    if (s_task_handle != NULL || s_running) {
        return false;
    }

    s_mouse_interface = mouse_interface;
    s_mouse_button = mouse_button;
    s_stop_requested = stop_requested;
    s_failed = false;

    if (xTaskCreate(click_high_speed_task,
                    "click_high",
                    CLICK_HIGH_SPEED_TASK_STACK,
                    NULL,
                    CLICK_HIGH_SPEED_TASK_PRIORITY,
                    &s_task_handle) != pdPASS) {
        s_task_handle = NULL;
        return false;
    }

    return true;
}

bool click_high_speed_wait(void) {
    while (s_task_handle != NULL || s_running) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return !s_failed;
}

void click_high_speed_stop(void) {
    (void)click_high_speed_release();
}

#else

#include "managers/payloads/click_high_speed.h"

bool click_high_speed_start(uint8_t mouse_interface, uint8_t mouse_button, volatile bool *stop_requested) {
    (void)mouse_interface;
    (void)mouse_button;
    (void)stop_requested;
    return false;
}

bool click_high_speed_wait(void) {
    return false;
}

void click_high_speed_stop(void) {}

#endif
