cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(temp_sensor_test)

# Only builds temp_sensor — no max30102 involved
target_sources(app PRIVATE
    src/main.c
    ../temp_sensor/src/temp_sensor.c
)

target_include_directories(app PRIVATE
    src
    ../temp_sensor/src
)

target_link_libraries(app PRIVATE m)
