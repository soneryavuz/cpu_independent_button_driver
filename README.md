# Button Driver API Documentation

This document provides an overview and explanation of the button driver code structure. It is intended for developers integrating or maintaining the driver.

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
// Application-level callback
void on_button_event(button_pressed_types_t type, button_enum id) {
    switch(type) {
        case BUTTON_NORMAL_PRESS:
            // handle single press
            break;
        case BUTTON_DOUBLE_PRESS:
            // handle double press
            break;
        case BUTTON_LONG_PRESS:
            // handle long press
            break;
    }
}

// Setup
button_api_t cfg = { /* fill members */ };
button_initialize(&cfg);

// In main loop (e.g., every 5 ms)
while (1) {
    button_process();
    delay_ms(5);
}
```

---

**End of Documentation**
