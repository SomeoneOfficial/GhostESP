#include "sdkconfig.h"

#ifdef CONFIG_HAS_BADUSB

#include "managers/payloads/click_high_speed.h"

#include "managers/badusb_manager.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CLICK_HIGH_SPEED_PRESS_HOLD_US 1200
#define CLICK_HIGH_SPEED_ARM_HOLD_US 15000
#define CLICK_HIGH_SPEED_ARM_GAP_MS 8
#define CLICK_HIGH_SPEED_ARM_CLICKS 3
#define CLICK_HIGH_SPEED_TASK_STACK 4096
#define CLICK_HIGH_SPEED_TASK_PRIORITY 7

static const char *TAG = "click_high";

static volatile bool *s_stop_requested = NULL;
static volatile bool s_running = false;
static volatile bool s_failed = false;
static TaskHandle_t s_task_handle = NULL;

static bool click_high_speed_should_continue(void) {
    return (s_stop_requested == NULL) || !*s_stop_requested;
}

static bool click_high_speed_click_once(void) {
    if (!click_high_speed_should_continue()) {
        return false;
    }

    if (!badusb_hid_mouse_buttons_send(0x01)) {
        ESP_LOGW(TAG, "Mouse press send failed");
        return false;
    }

    esp_rom_delay_us(CLICK_HIGH_SPEED_PRESS_HOLD_US);

    if (!click_high_speed_should_continue()) {
        (void)badusb_hid_mouse_buttons_send(0x00);
        return false;
    }

    if (!badusb_hid_mouse_buttons_send(0x00)) {
        ESP_LOGW(TAG, "Mouse release send failed");
        return false;
    }

    return true;
}

static bool click_high_speed_arm_device(void) {
    for (int i = 0; i < CLICK_HIGH_SPEED_ARM_CLICKS && click_high_speed_should_continue(); i++) {
        if (!badusb_hid_mouse_buttons_send(0x01)) {
            ESP_LOGW(TAG, "Mouse arm press send failed");
            return false;
        }

        esp_rom_delay_us(CLICK_HIGH_SPEED_ARM_HOLD_US);

        if (!badusb_hid_mouse_buttons_send(0x00)) {
            ESP_LOGW(TAG, "Mouse arm release send failed");
            return false;
        }

        if (i + 1 < CLICK_HIGH_SPEED_ARM_CLICKS) {
            vTaskDelay(pdMS_TO_TICKS(CLICK_HIGH_SPEED_ARM_GAP_MS));
        }
    }

    return click_high_speed_should_continue();
}

static void click_high_speed_task(void *arg) {
    (void)arg;

    s_running = true;
    s_failed = false;

    // Give the USB stack a moment to settle before the first press/release
    // pair, then arm the target with a few visible clicks so browsers and
    // games reliably capture the device before the fast loop begins.
    vTaskDelay(pdMS_TO_TICKS(20));
    if (!click_high_speed_arm_device()) {
        s_failed = true;
        s_running = false;
        s_stop_requested = NULL;
        s_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Mouse armed, starting fast click loop");

    while (click_high_speed_should_continue()) {
        if (!click_high_speed_click_once()) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    if (!badusb_hid_mouse_buttons_send(0x00)) {
        s_failed = true;
    }

    s_running = false;
    s_stop_requested = NULL;
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

bool click_high_speed_start(volatile bool *stop_requested) {
    if (s_task_handle != NULL || s_running) {
        return false;
    }

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
    if (s_stop_requested) {
        *s_stop_requested = true;
    }
}

#else

#include "managers/payloads/click_high_speed.h"

bool click_high_speed_start(volatile bool *stop_requested) {
    (void)stop_requested;
    return false;
}

bool click_high_speed_wait(void) {
    return false;
}

void click_high_speed_stop(void) {}

#endif
