/**************************************************
 * @file    button.c                              *
 * @brief   CPU-independent button driver         *
 * @author  Soner Yavuz                           *
 * @date    2025-05-11                            *
 *                                                *                            
 * Description:                                   *
 * This driver provides a platform-agnostic       *
 * interface for handling up to 5 buttons using   *
 * either interrupt-driven or interrupt-free      *
 * (polling) modes. Features include:             *
 *   - Debounce filtering                         *
 *   - Long press detection                       *
 *   - Single and double press counting           *
 *   - Flexible timing abstraction via user-      *
 *     provided tick functions                    *
 *   - Event callbacks for button events          *
 *                                                *
 * @warning The API must be correctly configured  *
 * for the target processor before use.           *   
 *                                                *
 **************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "button.h"

typedef enum 
{
    FAIL = -1,
    SUCCESS = 0
} init_status_t;

typedef struct
{
    uint32_t first;
    uint32_t last;
} detection_pressed_tick_t;

#define TICK_DIFF(tick) (p_api->fp_tick_elapsed(tick, p_api->fp_get_current_tick()))
#define DETECT_SINGLE_BUTTON_PRESS_IN_US    (500000) 

static button_api_t * p_api = NULL;
static init_status_t button_init_status = FAIL;
static detection_pressed_tick_t pressed_tick[BUTTON_MAX] = {{0}};

/**
 * @fn     find_pin_id
 * @brief  Locate the index of a given GPIO pin in the configured button list.
 *
 * Searches through the p_api->button_pins array for a matching pin value.
 *
 * @param  pin  GPIO pin number to search for.
 * @return Index (0..size_of_buttons-1) of the matching entry, or -1 if not found.
 */

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

/**
 * @fn     detect_the_press
 * @brief  Process and detect button press events for a specific button index.
 *
 * Checks if both the first and last press ticks are set, verifies debounce timing,
 * distinguishes long presses from valid single presses within a defined time window,
 * invokes the long‐press callback if needed, increments the press count on valid
 * short presses, resets tick counters, and returns the tick at which a short press
 * was confirmed.
 *
 * @param  index   Index of the button in the configuration array.
 * @param  p_count Pointer to the variable tracking the number of short presses.
 *                This will be incremented when a valid press is detected.
 * @return The tick count (from fp_get_current_tick) at which a short press
 *         event was last recorded, or 0 if no valid press was detected.
 */


static uint32_t detect_the_press(uint8_t index, uint8_t *p_count)
{
    uint32_t last_count_tick = 0;
    if ((0 != pressed_tick[index].first) && (0 != pressed_tick[index].last ))
    {
        if (TICK_DIFF(pressed_tick[index].last ) > p_api->debounce_us*p_api->tick_count_in_1us)
        {
            if (p_api->fp_tick_elapsed(pressed_tick[index].first, pressed_tick[index].last ) > p_api->long_press_us * p_api->tick_count_in_1us)
            {
                p_api->fp_event_callback(BUTTON_LONG_PRESS, (button_enum)index);
                pressed_tick[index].first = 0;
                pressed_tick[index].last  = 0;
            }
            else
            {
                if (TICK_DIFF(pressed_tick[index].last ) < DETECT_SINGLE_BUTTON_PRESS_IN_US*p_api->tick_count_in_1us)
                {
                    (*p_count)++;
                    pressed_tick[index].first = 0;
                    pressed_tick[index].last  = 0;
                    last_count_tick = p_api->fp_get_current_tick();
                }
            }
        }
    }
    return last_count_tick;
}

/**
 * @fn     desicion_by_pressed_count
 * @brief  Evaluate accumulated press counts and invoke the appropriate button event.
 *
 * This function calls `detect_the_press` to update the short-press count and retrieves
 * the tick at which the last valid press occurred. It uses static storage to track
 * the last press tick and count for each button index. If the elapsed time since the
 * last detected press exceeds the single-press timeout, it resets the record and:
 *   - Fires `BUTTON_NORMAL_PRESS` if exactly one press was counted.
 *   - Fires `BUTTON_DOUBLE_PRESS` if exactly two presses were counted.
 * After invoking the event callback, the press count is cleared.
 *
 * @param  index  Index of the button in the configuration array.
 */

static void desicion_by_pressed_count(uint8_t index)
{
    static uint32_t check_last_tick = 0;
    static uint32_t record_last_tick[BUTTON_MAX] = {0};
    static uint8_t press_count[BUTTON_MAX] = {0};
    check_last_tick = detect_the_press(index, &press_count[index]);
    if (0 != check_last_tick)
    {
        record_last_tick[index] = check_last_tick;   
    }

    if ((0 != record_last_tick[index]) && (press_count[index] > 0)
        && (TICK_DIFF(record_last_tick[index]) > DETECT_SINGLE_BUTTON_PRESS_IN_US*p_api->tick_count_in_1us)) 
    {
        record_last_tick[index] = 0;
        if (1 == press_count[index])
        {
            p_api->fp_event_callback(BUTTON_NORMAL_PRESS, (button_enum)index);
        }
        else
        {
            if (2 == press_count[index])
            {
                p_api->fp_event_callback(BUTTON_DOUBLE_PRESS, (button_enum)index);
            }
        }
        press_count[index] = 0;
    }    
}

/**
 * @fn     button_initialize
 * @brief  Initialize the button driver with the provided API configuration.
 *
 * Validates that the API pointer is non-NULL, that at least one button is configured,
 * and that required function pointers for retrieving the current tick and handling
 * events are set. On success, stores the API pointer internally and marks the driver
 * as initialized.
 *
 * @param  p_button_api  Pointer to a fully populated button_api_t structure.
 * @return SUCCESS (0) if initialization succeeds; FAIL (-1) otherwise.
 */
int button_initialize(button_api_t * p_button_api)
{
    button_init_status = FAIL;

    if ((NULL != p_button_api) 
        && (p_button_api->size_of_buttons > 0)
        && (NULL != p_button_api->fp_get_current_tick)
        && (NULL != p_button_api->fp_event_callback))
    {
        p_api = p_button_api;
        button_init_status = SUCCESS;
    }
    return button_init_status;
}

/**
 * @fn     button_isr
 * @brief  Handle a GPIO interrupt event for a configured button.
 *
 * This function should be registered as the ISR callback for button GPIO lines.
 * When a button interrupt occurs (rising edge, falling edge, or both), it locates
 * the corresponding button index and records the first and last press timestamps
 * based on the configured interrupt mode.
 *
 * @param  p_pin  Pointer to the pin_config_t structure describing the triggered pin
 *                and its interrupt mode.
 */

void button_isr(pin_config_t * p_pin)
{
    if ((NULL != p_pin) && (p_pin->interrupt_mode > BUTTON_INTERRUPT_MODE_NONE) && (SUCCESS == button_init_status))
    {
        int8_t inx = find_pin_id(p_pin->pin);
        if (-1 != inx)
        {
            switch (p_pin->interrupt_mode)
            {
                case BUTTON_INTERRUPT_MODE_RISING_EDGE:
                    if (0 == pressed_tick[inx].last)
                    {
                        pressed_tick[inx].last = p_api->fp_get_current_tick();
                    }
                    break;
                case BUTTON_INTERRUPT_MODE_FALLING_EDGE:
                    if (0 == pressed_tick[inx].first)
                    {
                        pressed_tick[inx].first = p_api->fp_get_current_tick();
                    }
                    break;
                case BUTTON_INTERRUPT_MODE_BOTH_EDGES:
                    if (0 == pressed_tick[inx].first)
                    {
                        pressed_tick[inx].first = p_api->fp_get_current_tick();
                    }
                    else
                    {
                        pressed_tick[inx].last = p_api->fp_get_current_tick();
                    }   
                    break;
                default:
                    break;
            }
        }
    }
}

/**
 * @fn     button_process
 * @brief  Poll and process button states, handling both interrupt-less and hybrid modes.
 *
 * This function should be called periodically (e.g., in the main loop or a dedicated task).
 * It iterates over each configured button, reads its raw logic level (applying active_high
 * inversion if needed), and updates the first/last press timestamps based on the
 * interrupt_mode:
 *   - RISING_EDGE:  records the first press timestamp when the button becomes pressed.
 *   - FALLING_EDGE: records the release timestamp after a prior press timestamp exists.
 *   - BOTH_EDGES:   managed elsewhere (interrupt-driven), so skipped here.
 *   - NONE (polling): records press and release timestamps purely by level changes.
 *
 * After timestamp updates, it calls `desicion_by_pressed_count()` to handle debounce,
 * single/double-press detection, and to fire the appropriate event callbacks.
 *
 * @note   Ensure `button_initialize()` has succeeded before calling this.
 */
void button_process()
{
    if (SUCCESS == button_init_status)
    {
        uint8_t i = 0;
        for (i=0; i<p_api->size_of_buttons; i++)
        {
            uint8_t pressed = p_api->active_high ? (1 == p_api->fp_read_button(&p_api->button_pins[i])) : (0 == p_api->fp_read_button(&p_api->button_pins[i]));
            switch (p_api->button_pins[i].interrupt_mode)
            {
                case BUTTON_INTERRUPT_MODE_RISING_EDGE:
                    if ((pressed) && (0 == pressed_tick[i].first))
                    {
                        pressed_tick[i].first = p_api->fp_get_current_tick();
                    }
                    break;
                case BUTTON_INTERRUPT_MODE_FALLING_EDGE:
                    if ((pressed) && (0 != pressed_tick[i].first))
                    {
                        pressed_tick[i].last = p_api->fp_get_current_tick();
                    }
                    break;
                case BUTTON_INTERRUPT_MODE_BOTH_EDGES:
                    break;
                default: //no interrupt
                if (pressed)
                {
                    if (0 == pressed_tick[i].first)
                    {
                        pressed_tick[i].first = p_api->fp_get_current_tick();
                    }
                    else
                    {
                        pressed_tick[i].last = p_api->fp_get_current_tick();
                    } 
                }
                    break;
            }
            desicion_by_pressed_count(i);
        }
    }
}