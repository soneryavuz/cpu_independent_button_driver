<h1 align="center">Button Driver API Documentation</h1>

This button driver is designed to operate in a processor-independent manner, either with interrupt-driven or interrupt-free (polling) methods. 
It supports up to 5 buttons, which can be managed via interrupts or polling. As a prerequisite, the API configuration must be correctly initialized for the target processor.
This driver has been tested on ESP32; codebases tested on other processors will be shared soon.

---

## 1. Overview

The driver implements platform-independent button handling using a combination of polling and optional interrupt modes. Key features include:

* **Debounce handling** to filter out mechanical bounce.
* **Long press detection** based on configurable thresholds.
* **Single and double press counting**.
* **Flexible timing abstraction** via user-provided tick functions.
* **Event callbacks** to notify application code of button events.

---

## 2. Data Structures

```c
typedef struct {
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
```

* **button\_pins**: Array of configured pins.
* **size\_of\_buttons**: Number of pins in the array.
* **active\_high**: Logic level for a "pressed" state (1 = high active, 0 = low active).
* **tick\_count\_in\_1us**: Conversion factor from microseconds to tick units.
* **debounce\_us**: Minimum stable period (in microseconds) to confirm a press or release.
* **long\_press\_us**: Threshold (in microseconds) for a long press event.
* **fp\_tick\_elapsed**: Function to compute elapsed ticks between two timestamps, handling wrap-around.
* **fp\_read\_button**: Function to read the raw logic level of a button pin.
* **fp\_get\_current\_tick**: Function to retrieve the current system tick count.
* **fp\_event\_callback**: Callback invoked with detected button events.

---

## 3. Global Variables

```c
static button_api_t * p_api = NULL;
static uint32_t last_press_tick[BUTTON_MAX] = {0};
static uint32_t first_press_tick[BUTTON_MAX] = {0};
static init_status_t button_init_status = FAIL;
static const uint32_t default_wait_us = 10000U;
```

* **p\_api**: Pointer to the driver configuration.
* **first\_press\_tick / last\_press\_tick**: Timestamps for press detection and release.
* **button\_init\_status**: Tracks whether initialization succeeded.
* **default\_wait\_us**: Default 10 ms timeout for distinguishing multi-click events.

---

## 4. Functions

### 4.1 `button_initialize`

```c
int button_initialize(button_api_t * p_button_api);
```

* Validates the provided `button_api_t`.
* Ensures non-null pointers and a positive button count.
* Stores the API pointer and returns `SUCCESS` or `FAIL`.

### 4.2 `find_pin_id`

```c
static int8_t find_pin_id(uint8_t pin);
```

* Searches the configured pins for a match.
* Returns the index or `-1` if not found.

### 4.3 `button_isr`

```c
void button_isr(pin_config_t * p_pin, uint32_t current_tick);
```

* Called on an interrupt event (if enabled).
* Uses `find_pin_id` to map the pin to an index.
* Captures the first or last press timestamp.

### 4.4 `detect_the_press`

```c
static void detect_the_press(uint8_t index, uint8_t *p_count);
```

* Checks if the elapsed time since first press exceeds **debounce\_us**.
* Differentiates between **long press** and **multi-press** based on thresholds.
* Invokes `fp_event_callback` for `BUTTON_LONG_PRESS`.

### 4.5 `button_process`

```c
void button_process(void);
```

* Main polling routine; should be called periodically.
* Reads the raw button state, applies **active\_high** logic.
* Updates timestamps for polling-only buttons.
* Calls `detect_the_press` to handle debounce and long press.
* After a multi-click timeout, invokes the callback with `BUTTON_NORMAL_PRESS` or `BUTTON_DOUBLE_PRESS`.
* Clears stale timestamps to reset the state machine.

---

## 5. Usage Example

```c
#include "button.h"

//--- Application callback ----------------
static void button_event_callback(button_pressed_types_t type, button_enum button_id){
    switch (type) {
        case BUTTON_NORMAL_PRESS:
            // single press logic
            break;
        case BUTTON_DOUBLE_PRESS:
            // double press logic
            break;
        case BUTTON_LONG_PRESS:
            // long press logic
            break;
        default:
            break;
    }
}

// -------------button api init begin------------------
button_api.button_pins[BUTTON_1].pin = BUTTON1_GPIO;
button_api.button_pins[BUTTON_1].interrupt_mode = BUTTON_INTERRUPT_MODE_BOTH_EDGES;
button_api.button_pins[BUTTON_2].pin = BUTTON2_GPIO;
button_api.button_pins[BUTTON_2].interrupt_mode = BUTTON_INTERRUPT_MODE_NONE;
button_api.size_of_buttons = 2;
button_api.active_high = 0;
button_api.tick_count_in_1us = USEC_TO_TICK(1); //40000000/1000000 * 1 40MHz
button_api.debounce_us = 10000; //10ms
button_api.long_press_us = 1000000; //1s
button_api.fp_tick_elapsed = tick_elapsed;
button_api.fp_read_button = read_button;
button_api.fp_get_current_tick = get_current_tick;
button_api.fp_event_callback = button_event_callback;
int ret_value = button_initialize(&button_api);
// -------------button api init end------------------

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

//--- Main loop --------------------------
while (1) {
    button_process();      // must be called periodically
}
```

---

**End of README**
