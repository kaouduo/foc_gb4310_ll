/**
 *  @file   led_base.h
 *  @brief  提供 LED 基类、虚函数表和统一操作。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  driver / led
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 按向上转型组织 LED，分离对象绑定与驱动完整硬件初始化。
 */

#ifndef LED_BASE_H
#define LED_BASE_H

#include <stdint.h>

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct LED LED;

/** @brief LED 基类虚函数表，由各子类提供静态常量实现。 */
typedef struct {
    void (*init)(LED *self); /*!< 外设初始化；GPIO 类初始化普通输出，PWM 类配置定时器及引脚复用。 */
    void (*on)(LED *self);   /*!< 点亮，不向业务暴露有效电平。 */
    void (*off)(LED *self);  /*!< 熄灭。 */
    void (*setBrightness)(LED *self, uint16_t brightness); /*!< PWM 类支持 0 至 1000，GPIO 类只使用 0 或 1000。 */
} LED_Ops;

/** @brief LED 通用基类，必须作为各硬件子类的首成员。 */
struct LED {
    const LED_Ops *ops; /*!< 构造时绑定的静态虚函数表，运行中不替换。 */
    const char *name;   /*!< 静态名称，仅用于标识对象。 */
};

/**
 *  @brief  构造 LED 基类并绑定子类虚函数表。
 *  @param  self 生命周期覆盖对象使用期的基类首成员。
 *  @param ops 子类私有的静态常量虚函数表。
 *  @param name 生命周期覆盖对象使用期的名称字符串。
 *  @return 无返回值。
 *  @note   只完成对象构造，不操作 GPIO 或定时器；参数由板级绑定代码保证，不在运行中替换虚表。
 */
void LED_Base_Bind(LED *self, const LED_Ops *ops, const char *name);

/**
 *  @brief  通过基类调用具体子类的外设初始化。
 *  @param  self 已构造的基类句柄。
 *  @return 无返回值。
 *  @note   启动任务中调用一次；GPIO 子类配置普通输出，PWM 子类配置 GPIO、定时器并切换 AF；不判断指针或构造状态。
 */
void LED_Init(LED *self);

/**
 *  @brief  通过基类点亮 LED。
 *  @param  self 已完成构造和外设初始化的句柄。
 *  @return 无返回值。
 *  @note   有效电平由具体子类处理；同一对象只允许一个任务持有输出权。
 */
void LED_On(LED *self);

/**
 *  @brief  通过基类熄灭 LED。
 *  @param  self 已完成构造和外设初始化的句柄。
 *  @return 无返回值。
 *  @note   GPIO/PWM 差异由子类处理；不重新配置外设或改变对象绑定。
 */
void LED_Off(LED *self);

/**
 *  @brief  通过基类设置具体 LED 的亮度。
 *  @param  self 已初始化的基类句柄。
 *  @param brightness PWM 子类范围为 0 至 1000；GPIO 子类仅使用 0 或 1000。
 *  @return 无返回值。
 *  @note   调用方选择具备所需能力的子类；当前红灯为 PWM 类，可连续调光，绿灯为 GPIO 类，仅亮灭。
 */
void LED_SetBrightness(LED *self, uint16_t brightness);

#ifdef __cplusplus
}
#endif

#endif
