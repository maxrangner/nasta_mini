#include "button_driver.h"

#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_err.h>
#include <esp_timer.h>

static void IRAM_ATTR button_isr(void *arg)
{
    button_t* handle = (button_t*)arg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    handle->current_edge = (gpio_num_t)gpio_get_level(handle->gpio_num);
    xTimerResetFromISR(handle->timer, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_timer_cb(TimerHandle_t xTimer)
{
    button_t* handle = (button_t*)pvTimerGetTimerID(xTimer);
    button_event_t event;

    if ((handle->current_edge) == handle->pressed_level) {
        handle->press_time = esp_timer_get_time();
    } else {
        uint64_t now = esp_timer_get_time();
        if (handle->press_time == 0) {
            return;
        }
        uint64_t pressed_time = (now - handle->press_time) / 1000;
        if (pressed_time >= handle->long_press_dur) {
            event = BTN_LONG_PRESS;
            handle->btn_callback(event, handle->gpio_num, handle->user_data);
        } else {
            event = BTN_SHORT_PRESS;
            handle->btn_callback(event, handle->gpio_num, handle->user_data);
        }
        handle->press_time = 0;
    }
}

void button_service_init()
{
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
}

void button_init(const button_cfg_t* cfg, button_t* button_handle)
{
    gpio_config_t io_config = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << cfg->gpio_num),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = cfg->hasPullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_config);
    
    button_handle->gpio_num = cfg->gpio_num;
    button_handle->pressed_level = cfg->hasPullup ? 0 : 1;
    button_handle->current_edge = (gpio_num_t)gpio_get_level(button_handle->gpio_num);
    button_handle->long_press_dur = cfg->long_press_dur;
    button_handle->btn_callback = cfg->btn_callback;
    button_handle->user_data = cfg->user_data;
    button_handle->timer = xTimerCreate(
        "btn_timer",
        pdMS_TO_TICKS(cfg->debounce),
        pdFALSE,
        button_handle,
        button_timer_cb
    );
    button_handle->press_time = 0;

    gpio_isr_handler_add(cfg->gpio_num, button_isr, button_handle);
}
