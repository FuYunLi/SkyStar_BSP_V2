# LVGL显示接口移植报告

## 一、移植概述

本次移植将LVGL 9.x的显示接口移植到STM32F407VETx平台，对接ST7789液晶屏驱动，实现基于DMA的异步刷新机制。

## 二、文件结构

### 2.1 核心文件

| 文件路径 | 说明 |
|---------|------|
| `Middleware/lvgl/lv_conf.h` | LVGL配置文件，包含内存池、显示配置等 |
| `Middleware/lvgl/lvgl.h` | LVGL主头文件 |
| `Middleware/lvgl/porting/lv_port_disp.c` | 显示接口实现文件 |
| `Middleware/lvgl/porting/lv_port_disp.h` | 显示接口头文件 |
| `Middleware/lvgl/src/` | LVGL核心源码目录 |

### 2.2 Keil工程配置

- **虚拟目录结构**：
  - `Middleware/LVGL`：包含所有LVGL核心源码（扁平化添加）
  - `Middleware/LVGL/Porting`：包含移植接口文件（lv_port_disp.c/h）

- **包含路径**：
  - `../Middleware/lvgl`
  - `../Middleware/lvgl/src`
  - `../Middleware/lvgl/porting`

- **宏定义**：
  - `LV_CONF_INCLUDE_SIMPLE`：使用简单的`#include "lv_conf.h"`包含方式
  - `LV_LVGL_H_INCLUDE_SIMPLE`：使用简单的`#include "lvgl.h"`包含方式

## 三、关键配置

### 3.1 lv_conf.h关键配置

```c
/* 内存配置 */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (32 * 1024U)  /* 32KB内存池 */

/* 显示配置 */
#define LV_COLOR_DEPTH 16        /* RGB565格式 */
#define LV_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
```

### 3.2 显示缓冲区配置

- **缓冲区大小**：240x10像素（240x240屏幕的1/24）
- **缓冲区数量**：单缓冲（节省内存）
- **刷新模式**：部分刷新（基于DMA异步传输）

## 四、实现细节

### 4.1 显示初始化流程

```c
void lv_port_disp_init(void)
{
    /* 1. 初始化ST7789液晶屏 */
    lcd_init();

    /* 2. 创建显示缓冲区 */
    static lv_color_t buf_1[240 * 10];
    lv_display_t * disp = lv_display_create(240, 240);
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 3. 设置分辨率 */
    lv_display_set_resolution(disp, 240, 240);
}
```

### 4.2 显示刷新回调函数

```c
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    if(disp_flush_enabled) {
        /* 计算刷新区域 */
        int32_t width = area->x2 - area->x1 + 1;
        int32_t height = area->y2 - area->y1 + 1;

        /* 参数合法性检查 */
        if (width <= 0 || height <= 0) {
            lv_display_flush_ready(disp);
            return;
        }

        /* 优先尝试DMA异步刷新 */
        bsp_status_t ret = lcd_flush_async_cb(
            (uint16_t)area->x1, (uint16_t)area->y1,
            (uint16_t)area->x2, (uint16_t)area->y2,
            (uint16_t *)px_map, lv_flush_done_cb, disp
        );

        if (ret == BSP_OK) {
            return; /* DMA传输成功，等待回调 */
        }

        /* DMA忙，回退到同步刷新 */
        lcd_flush(
            (uint16_t)area->x1, (uint16_t)area->y1,
            (uint16_t)area->x2, (uint16_t)area->y2,
            (uint16_t *)px_map
        );
        lv_display_flush_ready(disp);
    } else {
        lv_display_flush_ready(disp);
    }
}
```

### 4.3 DMA传输完成回调

```c
static void lv_flush_done_cb(void * user_data)
{
    lv_display_t * disp = (lv_display_t *)user_data;
    lv_display_flush_ready(disp);
}
```

## 五、注意事项

### 5.1 内存管理

- LVGL使用内置内存管理（LV_STDLIB_BUILTIN）
- 内存池大小为32KB，需根据实际应用调整
- 显示缓冲区占用约4.8KB（240x10x2字节）

### 5.2 DMA传输

- DMA传输期间CPU可执行其他任务，提高效率
- DMA忙时自动回退到同步刷新，确保画面正常
- DMA传输完成后通过回调通知LVGL

### 5.3 中断优先级

- DMA中断优先级应低于SysTick，避免影响LVGL心跳
- 建议DMA中断优先级设置为6或更低

## 六、编译结果

### 6.1 编译统计

- **编译状态**：成功
- **错误数量**：0
- **警告数量**：5
- **固件大小**：Flash ≈ 86.8 KB，RAM ≈ 50.1 KB

### 6.2 资源占用分析

| 模块 | Flash占用 | RAM占用 |
|------|----------|---------|
| LVGL核心 | ~70KB | ~35KB |
| 显示缓冲区 | - | ~4.8KB |
| 其他模块 | ~16KB | ~10KB |

## 七、验证步骤

### 7.1 基础验证

1. 编译工程，确认无错误
2. 烧录固件到目标板
3. 上电后液晶屏应显示LVGL界面

### 7.2 功能验证

1. 创建简单界面（如标签、按钮）
2. 观察界面刷新是否流畅
3. 测试触摸交互是否正常

## 八、后续工作

### 8.1 待完成接口

- **触控接口**（lv_port_indev）：对接触摸屏驱动
- **文件系统接口**（lv_port_fs）：对接LittleFS文件系统

### 8.2 优化方向

- 增加显示缓冲区数量，提高刷新效率
- 优化DMA传输配置，减少传输时间
- 调整内存池大小，平衡性能与资源占用

## 九、参考资料

- [LVGL官方文档](https://docs.lvgl.io/)
- [ST7789数据手册](https://www.displayfuture.com/Display/datasheet/ST7789.pdf)
- [STM32F407参考手册](https://www.st.com/resource/en/reference_manual/dm00031051.pdf)