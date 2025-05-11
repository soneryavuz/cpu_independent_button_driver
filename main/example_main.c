#include <stdio.h>
#include <stdint.h>
#include "../button_module/button.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"

#define TICK_DIFF(tick)         (tick_elapsed(tick, get_current_tick()))
#define USEC_TO_TICK(us)        (us * (SYSTEM_FREQUENCY / 1000000))
#define MSEC_TO_TICK(ms)        (ms * (SYSTEM_FREQUENCY / 1000))
#define GET_CURRENT_TIMER_TICK  (gptimer_get_raw_count(gptimer, &current_tick)) 
#define BUTTON1_GPIO        (33)
#define BUTTON2_GPIO        (32)
#define SYSTEM_FREQUENCY    (40000000U)

static gptimer_handle_t gptimer = NULL;
static button_api_t button_api;
static uint64_t current_tick = 0;
static const char * tag = "app_main";
static uint32_t last_press_tick = 0;

static uint32_t get_current_tick(void)
{
    GET_CURRENT_TIMER_TICK;
    return (uint32_t)current_tick;
}

static void gpio_isr_handler(void *arg)
{
    pin_config_t * p_pin = (pin_config_t *)arg;
    button_isr(p_pin);
    last_press_tick = get_current_tick();
}

static uint32_t tick_elapsed(uint32_t start, uint32_t end)
{
    uint32_t diff = 0;
    if(end>start)
    {
        diff = (end - start);
    }
    else
    {
        diff = (UINT32_MAX - start ) +end ;
    }
    return diff;
}

static int32_t read_button(pin_config_t * p_pin)
{
    return (int32_t)gpio_get_level(p_pin->pin);
}

static void button_event_callback(button_pressed_types_t type, button_enum button_id)
{
    switch(type)
    {
        case BUTTON_NORMAL_PRESS:
            ESP_LOGI("BUTTON_NORMAL_PRESS", "Button normally %d pressed\n", button_id);
            break;
        case BUTTON_LONG_PRESS:
            ESP_LOGI("BUTTON_LONG_PRESS", "Button LONG %d pressed\n", button_id);
            break;
        case BUTTON_DOUBLE_PRESS:
            ESP_LOGI("BUTTON_DOUBLE_PRESS", "Button DOUBLE %d pressed\n", button_id);
            break;
        default:
            break;
    }
}

static void system_init(void)
{
    // -------------button api init begin------------------
    button_api.button_pins[BUTTON_1].pin = BUTTON1_GPIO;
    button_api.button_pins[BUTTON_1].interrupt_mode = BUTTON_INTERRUPT_MODE_BOTH_EDGES;
    button_api.button_pins[BUTTON_2].pin = BUTTON2_GPIO;
    button_api.button_pins[BUTTON_2].interrupt_mode = BUTTON_INTERRUPT_MODE_NONE;
    button_api.size_of_buttons = 2;
    button_api.active_high = 0;
    button_api.tick_count_in_1us = USEC_TO_TICK(1); //40MHz
    button_api.debounce_us = 10000; //10ms
    button_api.long_press_us = 1000000; //1s
    button_api.fp_tick_elapsed = tick_elapsed;
    button_api.fp_read_button = read_button;
    button_api.fp_get_current_tick = get_current_tick;
    button_api.fp_event_callback = button_event_callback;
    int ret_value = button_initialize(&button_api);
    ESP_LOGI(tag, "button init ret value: %d", ret_value);
    // -------------button api init end------------------

    // -------------general purpose timer initialization begin------------------
    gptimer_config_t timer_config =
    {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction= GPTIMER_COUNT_UP,
        .resolution_hz = SYSTEM_FREQUENCY, 
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer,0));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
    // -------------general purpose timer initialization end--------------------

    // -------------IO Configuration start------------------
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (1ULL << BUTTON1_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = (1ULL << BUTTON2_GPIO);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON1_GPIO, gpio_isr_handler, (void *)&button_api.button_pins[0]);
    // -------------IO Configuration end--------------------
}

void app_main(void)
{
    system_init();

    while (1)
    {
        button_process();
    }
}