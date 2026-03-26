idf_component_register(SRCS "main.cpp"
                    "lwiperf.c"
                    "socket_bridge.cpp"
                    "toe_iperf.cpp"
                    "w5500_base.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino-esp32 arduino_tinyusb) # Add other dependencies as needed
