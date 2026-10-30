#ifndef BSP_BOARD_CONFIG_H
#define BSP_BOARD_CONFIG_H /* 板级配置头文件保护。 */

#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"

/* 主控模块：REV6 板卡，STM32G431CBU6。 */
#define BSP_BOARD_MCU_NAME "STM32G431CBU6" /* 主控器件型号。 */
#define BSP_BOARD_HW_REV 6U                /* 硬件版本号，仅支持 REV6。 */

/* 三相 PWM 模块：TIM1 主输出及互补输出，按物理通道编号排列。 */
#define BSP_BOARD_PWM_TIMER TIM1              /* 三相主输出与互补输出共用的定时器。 */
#define BSP_BOARD_PWM_CH1_PORT GPIOA          /* 第 1 相主输出端口。 */
#define BSP_BOARD_PWM_CH1_PIN LL_GPIO_PIN_8   /* 第 1 相主输出引脚。 */
#define BSP_BOARD_PWM_CH1_AF LL_GPIO_AF_6     /* 第 1 相主输出复用编号。 */
#define BSP_BOARD_PWM_CH2_PORT GPIOA          /* 第 2 相主输出端口。 */
#define BSP_BOARD_PWM_CH2_PIN LL_GPIO_PIN_9   /* 第 2 相主输出引脚。 */
#define BSP_BOARD_PWM_CH2_AF LL_GPIO_AF_6     /* 第 2 相主输出复用编号。 */
#define BSP_BOARD_PWM_CH3_PORT GPIOA          /* 第 3 相主输出端口。 */
#define BSP_BOARD_PWM_CH3_PIN LL_GPIO_PIN_10  /* 第 3 相主输出引脚。 */
#define BSP_BOARD_PWM_CH3_AF LL_GPIO_AF_6     /* 第 3 相主输出复用编号。 */
#define BSP_BOARD_PWM_CH1N_PORT GPIOB         /* 第 1 相互补输出端口。 */
#define BSP_BOARD_PWM_CH1N_PIN LL_GPIO_PIN_13 /* 第 1 相互补输出引脚。 */
#define BSP_BOARD_PWM_CH1N_AF LL_GPIO_AF_6    /* 第 1 相互补输出复用编号。 */
#define BSP_BOARD_PWM_CH2N_PORT GPIOB         /* 第 2 相互补输出端口。 */
#define BSP_BOARD_PWM_CH2N_PIN LL_GPIO_PIN_14 /* 第 2 相互补输出引脚。 */
#define BSP_BOARD_PWM_CH2N_AF LL_GPIO_AF_6    /* 第 2 相互补输出复用编号。 */
/* CH3N 的 PB15 使用 AF4，CH1N/CH2N 使用 AF6。 */
#define BSP_BOARD_PWM_CH3N_PORT GPIOB         /* 第 3 相互补输出端口。 */
#define BSP_BOARD_PWM_CH3N_PIN LL_GPIO_PIN_15 /* 第 3 相互补输出引脚。 */
#define BSP_BOARD_PWM_CH3N_AF LL_GPIO_AF_4    /* 第 3 相互补输出复用编号。 */

/* 编码器模块：SPI1 通信，PC4 软件片选，低电平选中。 */
#define BSP_BOARD_ENCODER_SPI SPI1               /* 磁编码器使用的 SPI 外设。 */
#define BSP_BOARD_ENCODER_SCK_PORT GPIOA         /* 编码器SPI 时钟端口。 */
#define BSP_BOARD_ENCODER_SCK_PIN LL_GPIO_PIN_5  /* 编码器SPI 时钟引脚。 */
#define BSP_BOARD_ENCODER_SCK_AF LL_GPIO_AF_5    /* 编码器SPI 时钟复用编号。 */
#define BSP_BOARD_ENCODER_MISO_PORT GPIOA        /* 编码器SPI 从机输出端口。 */
#define BSP_BOARD_ENCODER_MISO_PIN LL_GPIO_PIN_6 /* 编码器SPI 从机输出引脚。 */
#define BSP_BOARD_ENCODER_MISO_AF LL_GPIO_AF_5   /* 编码器SPI 从机输出复用编号。 */
#define BSP_BOARD_ENCODER_MOSI_PORT GPIOA        /* 编码器SPI 主机输出端口。 */
#define BSP_BOARD_ENCODER_MOSI_PIN LL_GPIO_PIN_7 /* 编码器SPI 主机输出引脚。 */
#define BSP_BOARD_ENCODER_MOSI_AF LL_GPIO_AF_5   /* 编码器SPI 主机输出复用编号。 */
#define BSP_BOARD_ENCODER_CS_PORT GPIOC          /* 编码器低有效片选端口。 */
#define BSP_BOARD_ENCODER_CS_PIN LL_GPIO_PIN_4   /* 编码器低有效片选引脚。 */

/* 调试串口模块：USART3，PC10 发送、PC11 接收。 */
#define BSP_BOARD_DEBUG_UART USART3           /* 调试串口外设。 */
#define BSP_BOARD_DEBUG_TX_PORT GPIOC         /* 调试串口发送端口。 */
#define BSP_BOARD_DEBUG_TX_PIN LL_GPIO_PIN_10 /* 调试串口发送引脚。 */
#define BSP_BOARD_DEBUG_TX_AF LL_GPIO_AF_7    /* 调试串口发送复用编号。 */
#define BSP_BOARD_DEBUG_RX_PORT GPIOC         /* 调试串口接收端口。 */
#define BSP_BOARD_DEBUG_RX_PIN LL_GPIO_PIN_11 /* 调试串口接收引脚。 */
#define BSP_BOARD_DEBUG_RX_AF LL_GPIO_AF_7    /* 调试串口接收复用编号。 */

/* 电流采样模块：PA2 对应 ADC1_IN3，PA4 对应 ADC2_IN17。 */
#define BSP_BOARD_CURRENT_1_ADC ADC1                  /* 电流采样 1 的ADC 外设。 */
#define BSP_BOARD_CURRENT_1_PORT GPIOA                /* 电流采样 1 的输入端口。 */
#define BSP_BOARD_CURRENT_1_PIN LL_GPIO_PIN_2         /* 电流采样 1 的模拟引脚。 */
#define BSP_BOARD_CURRENT_1_CHANNEL LL_ADC_CHANNEL_3  /* 电流采样 1 的ADC 采样通道。 */
#define BSP_BOARD_CURRENT_2_ADC ADC2                  /* 电流采样 2 的ADC 外设。 */
#define BSP_BOARD_CURRENT_2_PORT GPIOA                /* 电流采样 2 的输入端口。 */
#define BSP_BOARD_CURRENT_2_PIN LL_GPIO_PIN_4         /* 电流采样 2 的模拟引脚。 */
#define BSP_BOARD_CURRENT_2_CHANNEL LL_ADC_CHANNEL_17 /* 电流采样 2 的ADC 采样通道。 */

/* 母线电压模块：PB11 对应 ADC1_IN14。 */
#define BSP_BOARD_BUS_VOLTAGE_ADC ADC1                  /* 母线电压采样的ADC 外设。 */
#define BSP_BOARD_BUS_VOLTAGE_PORT GPIOB                /* 母线电压采样的输入端口。 */
#define BSP_BOARD_BUS_VOLTAGE_PIN LL_GPIO_PIN_11        /* 母线电压采样的模拟引脚。 */
#define BSP_BOARD_BUS_VOLTAGE_CHANNEL LL_ADC_CHANNEL_14 /* 母线电压采样的ADC 采样通道。 */

/* LED 模块：红灯 PB6、绿灯 PB7，低电平点亮，高电平熄灭。 */
#define BSP_BOARD_LED_GPIO_CLOCK LL_AHB2_GRP1_PERIPH_GPIOB /* 红绿 LED 端口的时钟使能掩码。 */
#define BSP_BOARD_LED_TIMER_CLOCK LL_APB1_GRP1_PERIPH_TIM4 /* LED PWM 定时器的时钟使能掩码。 */
#define BSP_BOARD_LED_ACTIVE_LOW 1U                        /* 取 1 表示 LED 低电平点亮。 */
#define BSP_BOARD_LED_R_TIMER TIM4                         /* 红灯呼吸亮度使用的定时器。 */
#define BSP_BOARD_LED_R_CHANNEL LL_TIM_CHANNEL_CH1         /* 红灯 PWM 输出通道。 */
#define BSP_BOARD_LED_R_AF LL_GPIO_AF_2                    /* 红灯 PWM 引脚复用编号。 */
#define BSP_BOARD_LED_R_PORT GPIOB                         /* 红灯输出端口。 */
#define BSP_BOARD_LED_R_PIN LL_GPIO_PIN_6                  /* 红灯输出引脚。 */
#define BSP_BOARD_LED_G_PORT GPIOB                         /* 绿灯输出端口。 */
#define BSP_BOARD_LED_G_PIN LL_GPIO_PIN_7                  /* 绿灯输出引脚。 */

#endif /* BSP_BOARD_CONFIG_H */
