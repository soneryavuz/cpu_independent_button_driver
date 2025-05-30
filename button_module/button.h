#ifndef BUTTON_H
#define BUTTON_H

typedef enum
{
    BUTTON_1,
    BUTTON_2,
    BUTTON_3,
    BUTTON_4,
    BUTTON_5,
    BUTTON_MAX,
} button_enum;

typedef enum
{
    BUTTON_NORMAL_PRESS,
    BUTTON_LONG_PRESS,
    BUTTON_DOUBLE_PRESS,
} button_pressed_types_t;

typedef enum
{
    BUTTON_INTERRUPT_MODE_NONE,
    BUTTON_INTERRUPT_MODE_RISING_EDGE,
    BUTTON_INTERRUPT_MODE_FALLING_EDGE,
    BUTTON_INTERRUPT_MODE_BOTH_EDGES,
} button_interrupt_mode_t;

typedef struct
{
    uint8_t pin;
    button_interrupt_mode_t interrupt_mode;
    uint8_t * p_reg;
} pin_config_t;

typedef struct
{
    pin_config_t button_pins[BUTTON_MAX];
    uint8_t size_of_buttons;
    uint8_t active_high;
    uint32_t tick_count_in_1us;
    uint32_t debounce_us;
    uint32_t long_press_us;
    uint32_t (* fp_tick_elapsed)(uint32_t start, uint32_t end);
    int32_t (* fp_read_button)(pin_config_t * p_pin);
    uint32_t (* fp_get_current_tick)(void);
    void (* fp_event_callback)(button_pressed_types_t type, button_enum button_id);
} button_api_t;

extern int button_initialize(button_api_t * p_button_api);
extern void button_isr(pin_config_t * p_pin);
extern void button_process();


#endif // BUTTON_H