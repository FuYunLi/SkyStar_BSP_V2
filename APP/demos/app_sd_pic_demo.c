/**
 * @file app_sd_pic_demo.c
 * @brief SD 卡图片显示自检演示实现
 * @note 对标 RocketPi 25_rocketpi_sd_pic_to_lcd。实现范围：
 *       未压缩 24 位 BMP（BITMAPINFOHEADER 40 字节头），尺寸不超过
 *       屏幕 240×240，超出尺寸直接报错（缩放属扩展功能）。
 *       逐行读取：BMP 行按 4 字节对齐存储且自下而上，读取时做行序
 *       翻转与 BGR→RGB565 转换，单行缓冲即可完成整屏绘制，无需
 *       115KB 级整帧缓冲。
 *       前置条件：SD 卡已插入（PD3 检测脚拉低）且 FatFS 已挂载。
 */

#define LOG_TAG "APP_SD_PIC"

#include "app_sd_pic_demo.h"
#include "bsp_logger.h"
#include "bsp_file.h"
#include "dev_st7789.h"
#include "shell.h"
#include <string.h>

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */

#define BMP_HEADER_SIZE       (54U)                 /* 文件头 14B + 信息头 40B */
#define BMP_ROW_ALIGN         (4U)                  /* BMP 行按 4 字节对齐 */
#define BMP_BPP_24            (24U)
#define PIC_ROW_BUF_SIZE      (736U)                /* 最大行步长 240*3 取齐 4 字节 */
#define PIC_RGB565_ROW_SIZE   (240U * 2U)

/* 行缓冲静态分配，避免栈压力 */
static uint8_t s_bmp_row_buf[PIC_ROW_BUF_SIZE];
static uint16_t s_pic_row_buf[PIC_RGB565_ROW_SIZE];

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 从字节流按小端格式读取无符号整数
 */
static uint32_t s_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief 从字节流按小端格式读取 16 位整数
 */
static uint16_t s_read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief BGR888 三元组转 RGB565
 */
static uint16_t s_bgr888_to_rgb565(const uint8_t *bgr)
{
    uint16_t r5 = (uint16_t)(bgr[2] >> 3);
    uint16_t g6 = (uint16_t)(bgr[1] >> 2);
    uint16_t b5 = (uint16_t)(bgr[0] >> 3);

    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

/**
 * @brief sd_pic Shell 指令入口：显示 SD 卡中的 24 位 BMP 图片
 */
static int shell_sd_pic(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: sd_pic <path>  (e.g. 0:/pic.bmp)");
        return -1;
    }

    /* 1. 打开文件（FatFS 挂载由 storage 模块完成） */
    bsp_file_t file = {BSP_FILE_TYPE_UNKNOWN, {0}};
    bsp_status_t ret = bsp_file_open(&file, argv[1], BSP_FILE_READ);
    if (ret != BSP_OK)
    {
        log_e("open %s failed! ret = %d (check SD card & fatfs mount)", argv[1], ret);
        return -1;
    }

    /* 2. 读取并校验 BMP 头 */
    uint8_t header[BMP_HEADER_SIZE];
    uint32_t got = 0U;
    ret = bsp_file_read(&file, header, sizeof(header), &got);
    if ((ret != BSP_OK) || (got != sizeof(header)))
    {
        log_e("read header failed! ret = %d got = %lu", ret, (unsigned long)got);
        (void)bsp_file_close(&file);
        return -1;
    }

    if ((header[0] != 'B') || (header[1] != 'M'))
    {
        log_e("not a BMP file!");
        (void)bsp_file_close(&file);
        return -1;
    }

    uint32_t data_offset = s_read_le32(&header[10]);
    int32_t width = (int32_t)s_read_le32(&header[18]);
    int32_t height = (int32_t)s_read_le32(&header[22]);
    uint16_t bpp = s_read_le16(&header[28]);

    log_i("BMP: %ld x %ld, %u bpp, data @ %lu", (long)width, (long)height, (unsigned int)bpp,
          (unsigned long)data_offset);

    if ((bpp != BMP_BPP_24) || (width <= 0) || (height <= 0) ||
        (width > (int32_t)lcd_get_width()) || (height > (int32_t)lcd_get_height()))
    {
        log_e("unsupported BMP (only 24bpp, size <= %ux%u)", (unsigned int)lcd_get_width(),
              (unsigned int)lcd_get_height());
        (void)bsp_file_close(&file);
        return -1;
    }

    /* 跳转到像素数据区（头与数据间可能有调色板等填充） */
    uint32_t pos = BMP_HEADER_SIZE;
    while (pos < data_offset)
    {
        uint8_t skip[64];
        uint32_t chunk = (data_offset - pos > sizeof(skip)) ? sizeof(skip) : (data_offset - pos);
        uint32_t skipped = 0U;
        if (bsp_file_read(&file, skip, chunk, &skipped) != BSP_OK)
        {
            log_e("seek pixel data failed!");
            (void)bsp_file_close(&file);
            return -1;
        }
        pos += skipped;
    }

    /* 3. 逐行转换绘制：BMP 自下而上存储，文件首行为画面最底行；
     *    行步长按 4 字节对齐，行尾填充字节直接丢弃 */
    uint32_t stride = ((uint32_t)width * 3U + BMP_ROW_ALIGN - 1U) & ~(BMP_ROW_ALIGN - 1U);

    for (int32_t row = 0; row < height; row++)
    {
        uint32_t br = 0U;
        ret = bsp_file_read(&file, s_bmp_row_buf, stride, &br);
        if ((ret != BSP_OK) || (br != stride))
        {
            log_e("read row %ld failed! ret = %d", (long)row, ret);
            (void)bsp_file_close(&file);
            return -1;
        }

        uint16_t *dst = s_pic_row_buf;
        const uint8_t *src = s_bmp_row_buf;
        for (int32_t col = 0; col < width; col++)
        {
            dst[col] = s_bgr888_to_rgb565(&src[(uint32_t)col * 3U]);
        }

        int32_t draw_y = height - 1 - row;
        lcd_flush(0, (uint16_t)draw_y, (uint16_t)(width - 1), (uint16_t)draw_y, s_pic_row_buf);
    }

    (void)bsp_file_close(&file);
    log_i("picture displayed.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 SD 卡图片显示演示模块
 */
bsp_status_t app_sd_pic_demo_init(void)
{
    log_i("SD Pic Demo loaded. Try: sd_pic 0:/pic.bmp");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, sd_pic, shell_sd_pic, Show 24bpp BMP (<=240x240) from SD card on LCD);
