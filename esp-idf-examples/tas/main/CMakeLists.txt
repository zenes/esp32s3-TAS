idf_component_register(SRCS "main.cpp"
                    "lwiperf.c"
                    "socket_bridge.cpp"
                    "toe_iperf.cpp"
                    "w5500_base.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=format"
                                              "-Wno-error=unused-variable"
                                              "-Wno-error=unused-function")
