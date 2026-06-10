/**
 * @file dev_st7789.c
 * @brief ST7789 LCD 驱动实现
 * @note 对外接口使用 lcd_ 前缀，内部实现使用 st7789_ 前缀。
 *       完全基于 port_spi, port_gpio, port_tick 物理接口隔离层。
 */

#include "dev_st7789.h"
#include "port_spi.h"
#include "port_gpio.h"
#include "port_tick.h"
#include "bsp_backlight.h"

/* 默认使用的物理 SPI 通道 */
#define ST7789_SPI_BUS PORT_SPI_1

/* 物理控制引脚映射 */
#define LCD_PIN_CS     PORT_GPIO_LCD_CS
#define LCD_PIN_DC     PORT_GPIO_LCD_DC
#define LCD_PIN_RST    PORT_GPIO_LCD_RST

/* 片选控制宏 */
#define LCD_CS_SELECT()    port_gpio_write(LCD_PIN_CS, PORT_GPIO_LOW)
#define LCD_CS_UNSELECT()  port_gpio_write(LCD_PIN_CS, PORT_GPIO_HIGH)

/* 数据/命令选择宏 */
#define LCD_DC_CMD()       port_gpio_write(LCD_PIN_DC, PORT_GPIO_LOW)
#define LCD_DC_DATA()      port_gpio_write(LCD_PIN_DC, PORT_GPIO_HIGH)

/* 复位控制宏 */
#define LCD_RST_LOW()      port_gpio_write(LCD_PIN_RST, PORT_GPIO_LOW)
#define LCD_RST_HIGH()     port_gpio_write(LCD_PIN_RST, PORT_GPIO_HIGH)

/* ST7789 常用控制命令定义 */
#define ST7789_CMD_NOP              0x00
#define ST7789_CMD_SOFT_RESET       0x01
#define ST7789_CMD_SLEEP_IN         0x10
#define ST7789_CMD_SLEEP_OUT        0x11
#define ST7789_CMD_NORMAL_ON        0x13
#define ST7789_CMD_INVERSION_OFF    0x20
#define ST7789_CMD_INVERSION_ON     0x21
#define ST7789_CMD_DISPLAY_OFF      0x28
#define ST7789_CMD_DISPLAY_ON       0x29
#define ST7789_CMD_COL_ADDR_SET     0x2A
#define ST7789_CMD_ROW_ADDR_SET     0x2B
#define ST7789_CMD_MEMORY_WRITE     0x2C
#define ST7789_CMD_MEM_ACCESS_CTRL  0x36
#define ST7789_CMD_PIXEL_FORMAT     0x3A

/* MADCTL 方向控制寄存器位定义 */
#define ST7789_MADCTL_MY            0x80
#define ST7789_MADCTL_MX            0x40
#define ST7789_MADCTL_MV            0x20
#define ST7789_MADCTL_ML            0x10
#define ST7789_MADCTL_BGR           0x08
#define ST7789_MADCTL_MH            0x04

#define ST7789_CMD_FRAME_RATE_CTRL2 0xB2
#define ST7789_CMD_POWER_CTRL3      0xC2
#define ST7789_CMD_POWER_CTRL4      0xC3
#define ST7789_CMD_POWER_CTRL5      0xC4
#define ST7789_CMD_POS_GAMMA_CTRL   0xE0
#define ST7789_CMD_NEG_GAMMA_CTRL   0xE1

/* 屏幕尺寸及方向变量 */
static uint16_t s_lcd_width = ST7789_CFG_DEFAULT_WIDTH;
static uint16_t s_lcd_height = ST7789_CFG_DEFAULT_HEIGHT;
static uint8_t s_lcd_rotation = 0;

/* 内部异步 DMA 回调上下文 */
typedef struct
{
    lcd_dma_cb_t done_cb;
    void *user_data;
} lcd_dma_ctx_t;

static lcd_dma_ctx_t s_lcd_dma_ctx = {NULL, NULL};

/* 大端颜色字节序转换辅助宏 */
#define SWAP_BYTES(color) ((uint16_t)(((color) >> 8) | ((color) << 8)))

/* =========================================================================
 * 内部私有物理层发送函数
 * ========================================================================= */

/**
 * @brief 发送单字节指令
 * @param cmd 指令代码
 */
static void st7789_write_cmd(uint8_t cmd)
{
    LCD_CS_SELECT();
    LCD_DC_CMD();
    port_spi_write(ST7789_SPI_BUS, &cmd, 1, BSP_WAIT_FOREVER);
    LCD_CS_UNSELECT();
}

/**
 * @brief 发送单字节数据
 * @param data 参数数据
 */
static void st7789_write_data8(uint8_t data)
{
    LCD_CS_SELECT();
    LCD_DC_DATA();
    port_spi_write(ST7789_SPI_BUS, &data, 1, BSP_WAIT_FOREVER);
    LCD_CS_UNSELECT();
}

/**
 * @brief 发送双字节数据
 * @param data 16位数据
 */
static void st7789_write_data16(uint16_t data)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(data >> 8);
    buf[1] = (uint8_t)(data & 0xFF);
    
    LCD_CS_SELECT();
    LCD_DC_DATA();
    port_spi_write(ST7789_SPI_BUS, buf, 2, BSP_WAIT_FOREVER);
    LCD_CS_UNSELECT();
}

/**
 * @brief 发送多字节数据流
 * @param data 数据缓冲区指针
 * @param len 发送字节长度
 */
static void st7789_write_datas(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return;
    }
    
    LCD_CS_SELECT();
    LCD_DC_DATA();
    port_spi_write(ST7789_SPI_BUS, data, len, BSP_WAIT_FOREVER);
    LCD_CS_UNSELECT();
}

/**
 * @brief 液晶屏硬件复位流程
 */
static void st7789_hw_reset(void)
{
    LCD_RST_LOW();
    port_tick_delay_ms(50);
    LCD_RST_HIGH();
    port_tick_delay_ms(120);
}

/**
 * @brief 配置显示控制器内部寄存器偏置参数
 * @note 依据屏幕模组手册进行电气特性参数初始化
 */
static void st7789_reg_init(void)
{
    // 配置像素格式为 16-bit/pixel (RGB565)
    st7789_write_cmd(ST7789_CMD_PIXEL_FORMAT);
    st7789_write_data8(0x55);

    // 配置帧率参数 (Porch Setting)
    st7789_write_cmd(ST7789_CMD_FRAME_RATE_CTRL2);
    {
        const uint8_t seq[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
        st7789_write_datas(seq, sizeof(seq));
    }

    // 设置闸极驱动偏压 (Gate Control)
    st7789_write_cmd(0xB7);
    st7789_write_data8(0x35);

    // 设置共模电压偏压 (VCOM Setting)
    st7789_write_cmd(0xBB);
    st7789_write_data8(0x32);

    // 设置正偏置电压控制 (VRH Set)
    st7789_write_cmd(ST7789_CMD_POWER_CTRL3);
    st7789_write_data8(0x01);

    // 设置负偏置电压控制 (VDV Set)
    st7789_write_cmd(ST7789_CMD_POWER_CTRL4);
    st7789_write_data8(0x15);

    // 启用偏置电压动态调整 (VDV/VRH Command Enable)
    st7789_write_cmd(ST7789_CMD_POWER_CTRL5);
    st7789_write_data8(0x20);

    // 配置正常显示下的刷新帧率 (约 60Hz)
    st7789_write_cmd(0xC6);
    st7789_write_data8(0x0F);

    // 设置驱动核心供电参数 (Power Control 1)
    st7789_write_cmd(0xD0);
    {
        const uint8_t seq[] = {0xA4, 0xA1};
        st7789_write_datas(seq, sizeof(seq));
    }

    // 配置正极性伽马校正曲线
    st7789_write_cmd(ST7789_CMD_POS_GAMMA_CTRL);
    {
        const uint8_t seq[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34};
        st7789_write_datas(seq, sizeof(seq));
    }

    // 配置负极性伽马校正曲线
    st7789_write_cmd(ST7789_CMD_NEG_GAMMA_CTRL);
    {
        const uint8_t seq[] = {0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34};
        st7789_write_datas(seq, sizeof(seq));
    }

    // 启用液晶色彩反转 (Inversion ON)
    st7789_write_cmd(ST7789_CMD_INVERSION_ON);

    // 打开显示通道 (Display ON)
    st7789_write_cmd(ST7789_CMD_DISPLAY_ON);

    // 预备显存写入序列
    st7789_write_cmd(ST7789_CMD_MEMORY_WRITE);
}

/**
 * @brief 设置显存读写视口窗口
 * @note 寄存器手册原理：通过写入列地址 (CASET: 2Ah) 及行地址 (RASET: 2Bh) 定义物理显存颗粒起始与终止边界。
 * @param x0 起始 X 坐标
 * @param y0 起始 Y 坐标
 * @param x1 终止 X 坐标
 * @param y1 终止 Y 坐标
 */
static void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t xs = x0 + ST7789_CFG_X_OFFSET;
    uint16_t xe = x1 + ST7789_CFG_X_OFFSET;
    uint16_t ys = y0 + ST7789_CFG_Y_OFFSET;
    uint16_t ye = y1 + ST7789_CFG_Y_OFFSET;

    st7789_write_cmd(ST7789_CMD_COL_ADDR_SET);
    st7789_write_data8(xs >> 8);
    st7789_write_data8(xs & 0xFF);
    st7789_write_data8(xe >> 8);
    st7789_write_data8(xe & 0xFF);

    st7789_write_cmd(ST7789_CMD_ROW_ADDR_SET);
    st7789_write_data8(ys >> 8);
    st7789_write_data8(ys & 0xFF);
    st7789_write_data8(ye >> 8);
    st7789_write_data8(ye & 0xFF);

    st7789_write_cmd(ST7789_CMD_MEMORY_WRITE);
}

/**
 * @brief 底层物理层总线异步传输完毕回调
 */
static void lcd_dma_internal_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)user_ctx;

    // 释放液晶显示屏的物理片选线
    LCD_CS_UNSELECT();

    // 链式唤醒上层逻辑，如通知图形库当前缓冲区已就绪
    if (s_lcd_dma_ctx.done_cb != NULL)
    {
        s_lcd_dma_ctx.done_cb(result == BSP_OK, s_lcd_dma_ctx.user_data);
    }
}

/* =========================================================================
 * 导出 API 函数实现
 * ========================================================================= */

/**
 * @brief 初始化 ST7789 液晶控制器并拉高背光引脚
 * @details 优先拉低复位引脚进行硬件清零，然后退出睡眠模式并发送厂商配置序列。
 * @note 使用示例:
 *        lcd_init();
 */
void lcd_init(void)
{
    // 初始化物理通道总线
    port_spi_init(ST7789_SPI_BUS);

    // 触发硬件低电平复位
    st7789_hw_reset();

    // 唤醒液晶屏 (Sleep Out)
    st7789_write_cmd(ST7789_CMD_SLEEP_OUT);
    port_tick_delay_ms(120);

    // 装载扫描方向偏置 (默认为竖屏 0 度)
    lcd_set_rotation(0);

    // 装载电气偏压寄存器序列
    st7789_reg_init();

    // 先行将整个屏幕物理清屏为黑色，规避背光开启时的白屏或花屏闪烁
    lcd_fill_color(0, 0, lcd_get_width(), lcd_get_height(), LCD_COLOR_BLACK);

    // 初始化屏幕背光 PWM 驱动
    bsp_backlight_init();
}

/**
 * @brief 设置显示视口旋转角度与扫描方向
 * @note 寄存器手册原理：通过写入参数控制 MADCTL (36h) 寄存器实现刷新扫描行列交换。
 * @param rotation 旋转角度标识 (0:0度竖屏, 1:90度横屏, 2:180度竖屏, 3:270度横屏)
 */
void lcd_set_rotation(uint8_t rotation)
{
    s_lcd_rotation = rotation % 4;
    st7789_write_cmd(ST7789_CMD_MEM_ACCESS_CTRL);

    switch (s_lcd_rotation)
    {
        case 0:
        {
            s_lcd_width = ST7789_CFG_DEFAULT_WIDTH;
            s_lcd_height = ST7789_CFG_DEFAULT_HEIGHT;
            st7789_write_data8(0x00);
            break;
        }
        case 1:
        {
            s_lcd_width = ST7789_CFG_DEFAULT_HEIGHT;
            s_lcd_height = ST7789_CFG_DEFAULT_WIDTH;
            st7789_write_data8(ST7789_MADCTL_MX | ST7789_MADCTL_MV);
            break;
        }
        case 2:
        {
            s_lcd_width = ST7789_CFG_DEFAULT_WIDTH;
            s_lcd_height = ST7789_CFG_DEFAULT_HEIGHT;
            st7789_write_data8(ST7789_MADCTL_MX | ST7789_MADCTL_MY);
            break;
        }
        case 3:
        {
            s_lcd_width = ST7789_CFG_DEFAULT_HEIGHT;
            s_lcd_height = ST7789_CFG_DEFAULT_WIDTH;
            st7789_write_data8(ST7789_MADCTL_MY | ST7789_MADCTL_MV);
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief 获取当前显示视口方向值
 * @retval 旋转角度标识 (0~3)
 */
uint8_t lcd_get_rotation(void)
{
    return s_lcd_rotation;
}

/**
 * @brief 打开显示输出
 */
void lcd_display_on(void)
{
    st7789_write_cmd(ST7789_CMD_DISPLAY_ON);
}

/**
 * @brief 关闭显示输出
 */
void lcd_display_off(void)
{
    st7789_write_cmd(ST7789_CMD_DISPLAY_OFF);
}

/**
 * @brief 获取显示视口当前物理宽度
 * @retval 像素宽度
 */
uint16_t lcd_get_width(void)
{
    return s_lcd_width;
}

/**
 * @brief 获取显示视口当前物理高度
 * @retval 像素高度
 */
uint16_t lcd_get_height(void)
{
    return s_lcd_height;
}

/**
 * @brief 在视口指定坐标绘制单像素
 * @param x 横坐标
 * @param y 纵坐标
 * @param color RGB565 颜色值
 */
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (port_spi_is_busy(ST7789_SPI_BUS))
    {
        return;
    }
    if (x >= s_lcd_width || y >= s_lcd_height)
    {
        return;
    }
    
    st7789_set_window(x, y, x, y);
    st7789_write_data16(color);
}

/**
 * @brief 以阻塞模式将指定矩形颜色区域填充为固定色彩值
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param w 宽度
 * @param h 高度
 * @param color RGB565 颜色值
 */
void lcd_fill_color(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (port_spi_is_busy(ST7789_SPI_BUS))
    {
        return;
    }
    if (x >= s_lcd_width || y >= s_lcd_height || w == 0 || h == 0)
    {
        return;
    }

    if (x + w > s_lcd_width)
    {
        w = s_lcd_width - x;
    }
    if (y + h > s_lcd_height)
    {
        h = s_lcd_height - y;
    }

    st7789_set_window(x, y, x + w - 1, y + h - 1);

    uint32_t pixels = (uint32_t)w * h;
    const uint16_t color_swapped = SWAP_BYTES(color);

    // 采用批缓冲以加速总线连续数据传输并降低多重开销
    uint16_t buffer[64];
    for (int i = 0; i < 64; i++)
    {
        buffer[i] = color_swapped;
    }

    LCD_CS_SELECT();
    LCD_DC_DATA();
    while (pixels > 0)
    {
        const uint32_t batch = (pixels > 64) ? 64 : pixels;
        port_spi_write(ST7789_SPI_BUS, (const uint8_t *)buffer, batch * 2, BSP_WAIT_FOREVER);
        pixels -= batch;
    }
    LCD_CS_UNSELECT();
}

/**
 * @brief 阻塞模式下，将 RAM 数据块全量推送至 LCD 指定区域
 * @param x1 起始 X 坐标
 * @param y1 起始 Y 坐标
 * @param x2 终止 X 坐标
 * @param y2 终止 Y 坐标
 * @param p_data 待刷新的大端色彩缓冲区首地址指针
 */
void lcd_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data)
{
    if (p_data == NULL)
    {
        return;
    }

    lcd_wait_done();
    st7789_set_window(x1, y1, x2, y2);

    const uint32_t pixels = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    LCD_CS_SELECT();
    LCD_DC_DATA();
    port_spi_write(ST7789_SPI_BUS, (const uint8_t *)p_data, pixels * 2, BSP_WAIT_FOREVER);
    LCD_CS_UNSELECT();
}

/**
 * @brief 异步非阻塞 DMA 模式下，将 RAM 数据块推送至 LCD 窗口（无完成回调）
 * @param x1 起始 X 坐标
 * @param y1 起始 Y 坐标
 * @param x2 终止 X 坐标
 * @param y2 终止 Y 坐标
 * @param p_data 待刷新的大端色彩缓冲区首地址指针
 * @retval bsp_status_t 异步流开启状态码
 */
bsp_status_t lcd_flush_async(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data)
{
    return lcd_flush_async_cb(x1, y1, x2, y2, p_data, NULL, NULL);
}

/**
 * @brief 异步非阻塞 DMA 模式下，将 RAM 数据块推送至 LCD 窗口，完成后调用完成回调
 * @note 必须确保在 DMA 激活期间，用户显存缓冲区 p_data 内容不能发生覆写。
 * @param x1 起始 X 坐标
 * @param y1 起始 Y 坐标
 * @param x2 终止 X 坐标
 * @param y2 终止 Y 坐标
 * @param p_data 待刷新的大端色彩缓冲区首地址指针
 * @param done_cb 异步写入完毕后被链式调用的中断安全应用层回调指针
 * @param user_data 传递给回调函数的应用层句柄上下文
 * @retval bsp_status_t 异步流开启状态码
 */
bsp_status_t lcd_flush_async_cb(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data,
                                lcd_dma_cb_t done_cb, void *user_data)
{
    if (p_data == NULL)
    {
        return BSP_EINVAL;
    }
    if (port_spi_is_busy(ST7789_SPI_BUS))
    {
        return BSP_BUSY;
    }

    s_lcd_dma_ctx.done_cb = done_cb;
    s_lcd_dma_ctx.user_data = user_data;

    st7789_set_window(x1, y1, x2, y2);

    const uint32_t pixels = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    LCD_CS_SELECT();
    LCD_DC_DATA();

    const bsp_status_t ret = port_spi_write_dma(ST7789_SPI_BUS, (const uint8_t *)p_data, pixels * 2, lcd_dma_internal_cb, NULL);

    if (ret != BSP_OK)
    {
        LCD_CS_UNSELECT();
    }

    return ret;
}

/**
 * @brief 查询当前 DMA 物理通道是否处于忙绿阶段
 * @retval true 总线忙碌
 * @retval false 总线空闲
 */
bool lcd_is_busy(void)
{
    return port_spi_is_busy(ST7789_SPI_BUS);
}

/**
 * @brief 同步等待上一轮 DMA 传输完成并释放显示屏总线
 */
void lcd_wait_done(void)
{
    if (!port_spi_is_busy(ST7789_SPI_BUS))
    {
        return;
    }
    port_spi_wait_complete(ST7789_SPI_BUS, BSP_WAIT_FOREVER);
}
