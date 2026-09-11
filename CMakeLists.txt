cmake_minimum_required(VERSION 3.16)
project(lvgl_pc_sim C)
set(CMAKE_C_STANDARD 11)

add_subdirectory(lvgl)

add_executable(sim
    main.c
    config.c
    app.c
    ui_common.c
    "screen_page1.c"
    "screen_page2.c"
    "screen_page3.c"
    "screen_page4.c"
    "screen_page5.c"
    "screen_page6.c"
    cJSON.c
    time_service.c

    fonts/quartz_60.c
    fonts/quartz_90.c
    fonts/quartz_140.c
    fonts/quartz_200.c
    fonts/lv_montserrat_90.c

    pictures/img_solar.c
    pictures/img_house.c
    pictures/img_grid.c
    pictures/img_battery.c

    pictures/img_00_clear_day.c
    pictures/img_00_clear_night.c
    pictures/img_01_mainly_clear_day.c
    pictures/img_01_mainly_clear_night.c
    pictures/img_02_partly_clear_day.c
    pictures/img_02_partly_clear_night.c
    pictures/img_03_overcast.c
    pictures/img_45_fog.c
    pictures/img_61_rain_light_day.c
    pictures/img_61_rain_light_night.c
    pictures/img_63_rain_medium.c
    pictures/img_65_rain_heavy.c
    pictures/img_66_sleet.c
    pictures/img_71_snow.c
    pictures/img_95_thunderstorm.c
    pictures/img_96_thunderstorm_hail.c
    pictures/img_unknown.c
	
    pictures/img_off.c
    pictures/img_hot.c
    pictures/img_frost.c

    pictures/img_cpu.c
    pictures/img_ram.c
    pictures/img_hdd.c
    pictures/img_net.c
)

if(ESP_PLATFORM)
    target_sources(sim PRIVATE gree_backend.c)
else()
    target_sources(sim PRIVATE gree_backend_stub.c)
endif()

target_include_directories(sim PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lvgl
    ${CMAKE_SOURCE_DIR}/lvgl/src
    ${CMAKE_SOURCE_DIR}/lvgl/src/drivers/windows
    ${CMAKE_SOURCE_DIR}/fonts
    ${CMAKE_SOURCE_DIR}/pictures
)

target_compile_definitions(sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LV_USE_WINDOWS=1
)

target_link_libraries(sim PRIVATE
    lvgl
    winhttp
    ws2_32
)