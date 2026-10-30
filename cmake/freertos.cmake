include(FetchContent)

# 固定稳定版及归档校验值；库本体不放入工程格式化范围。
set(FREERTOS_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/FreeRTOS-Kernel")
if(NOT EXISTS "${FREERTOS_KERNEL_DIR}/tasks.c")
    FetchContent_Declare(freertos_source
        URL "https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/zip/refs/tags/V11.3.2"
        URL_HASH SHA256=d840bdc153ad08e1fc5c915ea71643b58173fa33129c3b390e4ab1de3bee65f6
        SOURCE_DIR "${FREERTOS_KERNEL_DIR}"
        SOURCE_SUBDIR source_only
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(freertos_source)
endif()

file(READ "${FREERTOS_KERNEL_DIR}/include/task.h" FREERTOS_TASK_HEADER)
if(NOT FREERTOS_TASK_HEADER MATCHES "tskKERNEL_VERSION_NUMBER[ \t]+\"V11.3.2\"")
    message(FATAL_ERROR "FreeRTOS source version differs from the pinned V11.3.2")
endif()

add_library(freertos_kernel STATIC
    ${FREERTOS_KERNEL_DIR}/tasks.c
    ${FREERTOS_KERNEL_DIR}/queue.c
    ${FREERTOS_KERNEL_DIR}/list.c
    ${FREERTOS_KERNEL_DIR}/timers.c
    ${FREERTOS_KERNEL_DIR}/event_groups.c
    ${FREERTOS_KERNEL_DIR}/stream_buffer.c
    ${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c
)
target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_KERNEL_DIR}/include
    ${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM4F
    ${CMAKE_CURRENT_SOURCE_DIR}/project/porting/freertos
    ${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/CMSIS/Device/ST/STM32G4xx/Include
)
target_compile_definitions(freertos_kernel PUBLIC STM32G431xx)

# 即使当前任务仍使用静态分配，也保留动态分配入口和配置的堆空间。
target_link_options(freertos_kernel INTERFACE "-Wl,--undefined=pvPortMalloc")
