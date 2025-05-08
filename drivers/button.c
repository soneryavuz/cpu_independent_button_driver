#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "button.h"


#define TICK_DIFF(tick) (p_api->fp_tick_elapsed(tick, p_api->fp_get_current_tick()))

typedef enum 
{
    FAIL = -1,
    SUCCESS = 0
} init_status_t;

static button_api_t * p_api = NULL;
static uint32_t last_press_tick[BUTTON_MAX] = {0};
static uint32_t first_press_tick[BUTTON_MAX] = {0};
static init_status_t button_init_status = FAIL;
static const uint32_t default_wait_us = 10000U; //10ms detect press


static int8_t find_pin_id(uint8_t pin)
{
    int8_t index = -1;
    uint8_t i = 0;
    while (i <= p_api->size_of_buttons-1)
    {
        if (p_api->button_pins[i].pin == pin)
        {
            index = i;
            break;
        }
        i++;
    }
    return index;
}

static void detect_the_press(uint8_t index, uint8_t *p_count)
{
    if ((TICK_DIFF(first_press_tick[index]) > p_api->debounce_us*p_api->tick_count_in_1us)
        && (0 != last_press_tick[index]))
    {
        if (p_api->fp_tick_elapsed(first_press_tick[index], last_press_tick[index]) > p_api->long_press_us * p_api->tick_count_in_1us)
        {
            p_api->fp_event_callback(BUTTON_LONG_PRESS, (button_enum)index);
            first_press_tick[index] = 0;
        }
        else 
        {
            if (TICK_DIFF(last_press_tick[index]) > default_wait_us*p_api->tick_count_in_1us)
            {
                (*p_count)++;
                first_press_tick[index] = 0;
            }
        }
    }
}


int button_initialize(button_api_t * p_button_api)
{
    button_init_status = FAIL;

    if ((NULL != p_button_api) 
        && (p_button_api->size_of_buttons > 0)
        && (p_button_api->fp_get_current_tick != NULL)
        && (p_button_api->fp_event_callback != NULL))
    {
        p_api = p_button_api;
        button_init_status = SUCCESS;
    }
    return button_init_status;
}

void button_isr(pin_config_t * p_pin, uint32_t current_tick)
{
    if ((NULL != p_pin) && (1 == p_pin->interrupt_mode) && (SUCCESS == button_init_status))
    {
        int8_t inx = find_pin_id(p_pin->pin);
        if (-1 != inx)
        {
            if (0 == first_press_tick[inx])
            {
                first_press_tick[inx] = current_tick;
            }
            else
            {
                last_press_tick[inx] = current_tick;
            }
        }
    }
}

void button_process()
{
    //todo: It must be applied debounce on rising edge
    static uint8_t press_count[BUTTON_MAX] = {0};
    if (SUCCESS == button_init_status)
    {
        uint8_t i = 0;
        for (i=0; i<p_api->size_of_buttons; i++)
        {
            uint8_t pressed = p_api->active_high ? (1 == p_api->fp_read_button(&p_api->button_pins[i])) : (0 == p_api->fp_read_button(&p_api->button_pins[i]));
            if ((pressed) && (0 == p_api->button_pins[i].interrupt_mode))
            {
                if (0 == first_press_tick[i])
                {
                    first_press_tick[i] = p_api->fp_get_current_tick();
                }
                else
                {
                    last_press_tick[i] = p_api->fp_get_current_tick();
                }             
            }
            if (0 != first_press_tick[i])
            {
                detect_the_press(i, &press_count[i]);
            }

            if ((TICK_DIFF(last_press_tick[i]) > default_wait_us*10*p_api->tick_count_in_1us)
                && (0 != press_count[i]) && (0 != last_press_tick[i]))
            {
                if (1 == press_count[i])
                {
                    p_api->fp_event_callback(BUTTON_NORMAL_PRESS, (button_enum)i);
                }
                else
                {
                    if (2 == press_count[i])
                    {
                        p_api->fp_event_callback(BUTTON_DOUBLE_PRESS, (button_enum)i);
                    }
                }
                press_count[i] = 0;
            }
            if ((TICK_DIFF(first_press_tick[i]) > p_api->long_press_us*2*p_api->tick_count_in_1us)) //protect
            {
                //todo: It will add a timeout func
                first_press_tick[i] = 0;
            }
        }
    }
}