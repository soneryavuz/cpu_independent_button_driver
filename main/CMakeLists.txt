idf_component_register(
  SRCS         "example_main.c"
  INCLUDE_DIRS "."            # app’s headers
  REQUIRES     esp_driver_gpio
               esp_driver_gptimer
               button_module    # link in your driver module
)
