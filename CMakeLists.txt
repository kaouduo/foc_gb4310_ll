cmake_minimum_required(VERSION 3.22)

project(foc_gb4310_ll LANGUAGES C CXX ASM)

add_executable(${PROJECT_NAME}
    # application 应用层
    project/application/motor/app_motor.c
    project/application/motor/app_motor_diagnostics.c
    project/application/main.c
    project/application/state/app_device_state.c
    project/application/led/app_led.c
    project/application/task/app_led_task.c
    project/application/task/app_usb_task.c
    project/application/shell/app_shell.c
    project/application/shell/commands/cmd_console.c
    project/application/shell/commands/cmd_system.c
    project/application/shell/commands/cmd_test.c

    # bsp 板级支持层
    project/bsp/motor/bsp_motor.c
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
    project/porting/stm32g4/system_bootloader.c
    project/porting/stm32g4/system_bootloader_jump.S
    project/porting/stm32g4/runtime.c
    project/porting/stm32g4/dwt.c
    project/porting/internal_flash/internal_flash.c
    project/porting/pwm_output/pwm_output_stm32.c
    project/porting/mt6826s/mt6826s_spi.c
    project/porting/uart/uart_stm32.c
    project/porting/motor_sampling/motor_sampling_stm32.c
    project/porting/stm32g4/motor_fault_shutdown.S
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
    FOC_ENABLE_MOTOR_DIAGNOSTICS=$<IF:$<CONFIG:Debug>,1,0>
    FOC_HW_REV=6
    USER_VECT_TAB_ADDRESS
)

target_include_directories(${PROJECT_NAME} PRIVATE
    project/application/motor
    project/middleware/control
    project/middleware/foc
    project/bsp/motor
    project/porting/motor_sampling
    project/application
    project/application/task
    project/application/shell
    project/application/log
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
    project/porting/led
    project/middleware/third_party/CMSIS/Include
    project/middleware/third_party/CMSIS/Device/ST/STM32G4xx/Include
    project/middleware/third_party/STM32G4xx_LL_Driver/Inc
)

include(cmake/freertos.cmake)
include(cmake/cherryusb.cmake)
# 获取并校验固定依赖，固件在下方链接核心与本工程适配。
include(cmake/cherrysh.cmake)
include(cmake/easylogger.cmake)
# 上游帮助只编译函数，由项目命令检查参数后调用，避免重复注册。
set_source_files_properties(${CHERRYSH_SOURCE_DIR}/builtin/help.c PROPERTIES
    COMPILE_DEFINITIONS CONFIG_CSH_SYMTAB=0
    COMPILE_OPTIONS -Wno-sign-compare
)
target_sources(${PROJECT_NAME} PRIVATE ${CHERRYSH_SOURCE_DIR}/builtin/help.c)
# BSP 在启动任务完成断开时序和描述符注册后启动设备栈。
target_sources(${PROJECT_NAME} PRIVATE
    project/application/log/app_log.c
    project/porting/easylogger/elog_port.c
    $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/project/application/shell/commands/cmd_log.c>
)
target_link_libraries(${PROJECT_NAME} PRIVATE freertos_kernel cherryusb_device cherrysh easylogger ${TOOLCHAIN_LINK_LIBRARIES})

# 输出文件生成、链接脚本依赖及基础库集中在 cmake 目录。
include(cmake/firmware.cmake)

# 本工程独立控制算法与模式，禁止引入 BSP/LL/RTOS。
target_sources(${PROJECT_NAME} PRIVATE
    project/middleware/control/pi.c
    project/middleware/control/ramp.c
    project/middleware/foc/foc_math.c
    project/middleware/foc/foc_transform.c
    project/middleware/foc/foc_svpwm.c
    project/middleware/foc/foc_base.c
    project/middleware/foc/foc_open_voltage.c
    project/middleware/foc/foc_encoder_voltage.c
    project/middleware/foc/foc_current.c
)

# 实板证据：O0 实时全链路超过 50 us。控制路径在 Debug 仍保留 g3/日志，但用 O2 保证截止时间。
set_source_files_properties(
    project/application/motor/app_motor.c
    project/application/motor/app_motor_diagnostics.c
    project/porting/stm32g4/dwt.c
    project/bsp/encoder/bsp_encoder.c
    project/driver/mt6826s/mt6826s_drv.c
    project/porting/mt6826s/mt6826s_spi.c
    project/porting/motor_sampling/motor_sampling_stm32.c
    project/porting/pwm_output/pwm_output_stm32.c
    project/middleware/control/pi.c
    project/middleware/control/ramp.c
    project/middleware/foc/foc_math.c
    project/middleware/foc/foc_transform.c
    project/middleware/foc/foc_svpwm.c
    project/middleware/foc/foc_base.c
    project/middleware/foc/foc_open_voltage.c
    project/middleware/foc/foc_encoder_voltage.c
    project/middleware/foc/foc_current.c
    PROPERTIES COMPILE_OPTIONS -O2
)
