/**
 *  @file   runtime.c
 *  @brief  提供 STM32 裸机启动所需的 Newlib 运行库钩子。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / stm32g4
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

/**
 *  @brief  提供 Newlib 初始化数组调用所需的空初始化钩子。
 *  @param  无。
 *  @return 无返回值。
 *  @note   由 __libc_init_array 在 main 前调用；RAM 初始化由 STM32 启动文件完成，此处无额外副作用或失败状态。
 */
void _init(void) {}

/**
 *  @brief  提供 Newlib 运行库所需的空终止钩子。
 *  @param  无。
 *  @return 无返回值。
 *  @note   供运行库终止路径调用；当前固件主循环不退出，此处不释放资源且无失败状态返回。
 */
void _fini(void) {}
