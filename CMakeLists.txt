cmake_minimum_required(VERSION 3.22)

project(foc_gb4310_ll LANGUAGES C CXX ASM)

add_executable(${PROJECT_NAME}
    # application 应用层
    project/application/main.c

    # porting 平台移植层
    project/porting/stm32g4/system_stm32g4xx.c
    project/porting/stm32g4/system_clock.c
    project/porting/stm32g4/system_control.c
    project/porting/stm32g4/runtime.c
    project/porting/stm32g4/dwt.c

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
    project/bsp
    project/common
    project/porting
    project/porting/stm32g4
    project/middleware/third_party/CMSIS/Include
    project/middleware/third_party/CMSIS/Device/ST/STM32G4xx/Include
    project/middleware/third_party/STM32G4xx_LL_Driver/Inc
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

# 输出文件生成、链接脚本依赖及基础库集中在 cmake 目录。
include(cmake/firmware.cmake)
