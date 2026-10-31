cmake_minimum_required(VERSION 3.22)

project(foc_gb4310_ll LANGUAGES C CXX ASM)

add_executable(${PROJECT_NAME}
    # application 应用层
    project/application/main.c
    project/application/state/app_device_state.c
    project/application/led/app_led.c
    project/application/task/app_led_task.c

    # bsp 板级支持层
    project/bsp/platform/bsp_platform.c
    project/bsp/platform/bsp_internal_flash.c
    project/bsp/encoder/bsp_encoder.c
    project/bsp/led/bsp_led.c
    project/bsp/usb/bsp_usb_cdc.c

    # driver 器件驱动层
    project/driver/mt6826s/mt6826s_drv.c
    project/driver/led/led_base.c

    # porting 平台移植层
    project/porting/stm32g4/system_stm32g4xx.c
    project/porting/stm32g4/system_clock.c
    project/porting/stm32g4/system_control.c
    project/porting/stm32g4/runtime.c
    project/porting/stm32g4/dwt.c
    project/porting/internal_flash/internal_flash.c
    project/porting/pwm_output/pwm_output_stm32.c
    project/porting/mt6826s/mt6826s_spi.c
    project/porting/uart/uart_stm32.c
    project/porting/current_sampling/current_sampling_stm32.c
    project/porting/adc_regular/bus_voltage_stm32.c
    project/porting/led/led_gpio.c
    project/porting/led/led_pwm.c
    project/porting/freertos/freertos_hooks.c
    project/porting/freertos/freertos_time.c
    project/porting/cherryusb/usb_io.c
    project/porting/cherryusb/usb_dc_port.c
    project/porting/cherryusb/usb_cdc.c
    project/porting/cherryusb/usb_descriptors.c

    # 芯片启动（入口，非分层）
    startup_stm32g431xx.s
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32G431xx
    FOC_HW_REV=6
    USER_VECT_TAB_ADDRESS
)

target_include_directories(${PROJECT_NAME} PRIVATE
    project/application
    project/application/task
    project/application/led
    project/application/state
    project/bsp/led
    project/bsp/usb
    project/bsp
    project/bsp/platform
    project/common
    project/driver
    project/driver/led
    project/porting
    project/porting/stm32g4
    project/porting/internal_flash
    project/porting/pwm_output
    project/driver/mt6826s
    project/porting/mt6826s
    project/bsp/encoder
    project/porting/uart
    project/porting/current_sampling
    project/porting/adc_regular
    project/porting/led
    project/middleware/third_party/CMSIS/Include
    project/middleware/third_party/CMSIS/Device/ST/STM32G4xx/Include
    project/middleware/third_party/STM32G4xx_LL_Driver/Inc
)

include(cmake/freertos.cmake)
include(cmake/cherryusb.cmake)
# BSP 在启动任务完成断开时序和描述符注册后启动设备栈。
target_link_libraries(${PROJECT_NAME} PRIVATE freertos_kernel cherryusb_device ${TOOLCHAIN_LINK_LIBRARIES})

# 输出文件生成、链接脚本依赖及基础库集中在 cmake 目录。
include(cmake/firmware.cmake)
