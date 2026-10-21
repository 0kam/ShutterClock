# Out-of-tree drivers are compiled into the app target (a separate
# zephyr_library() raced the generated headers, e.g. zephyr/offsets.h).
target_sources_ifdef(CONFIG_RTC_RX8900 app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rtc/rtc_rx8900.c)
