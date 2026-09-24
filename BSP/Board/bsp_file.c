/**
 * @file bsp_file.c
 * @brief 板级虚拟文件系统 (VFS) 胶水层实现
 * @note 支持将文件操作动态路由至 FatFS (SD卡) 或 LittleFS (W25Q128 Flash)
 */

#include "bsp_file.h"
#include "bsp_lfs.h"
#include <string.h>

/* =========================================================================
 * DMA 对齐契约的 VFS 统一兜底（契约见 ARCHITECTURE.md 第 4 节第 4 条）
 *
 * 为什么要在这拦：SDIO IDMA 只按 32 位取指、会丢弃地址低 2 位，而 FatFS 对整扇区
 * 读写是把用户指针直达 disk_read/disk_write（ff.c 的 direct 路径，无中间拷贝）。
 * 非 4 字节对齐的缓冲一旦走上该路径，整块数据从对齐下界开始搬运，位移却不报任何错
 * （CRC 由外设对实际发出的字节生成）——实测代价是文件头被帧头覆盖、内容整体错位。
 *
 * 本层是全部文件读写的唯一入口，因此在这里兜底一次即覆盖所有上层调用方；
 * 更底层 sd_diskio 另有一道拒绝守卫作为红线（只拒绝不中转），专拦绕过 VFS 直接操作
 * FatFs 的调用。LittleFS 后端走轮询 SPI 与自带缓存，不受此约束，故不中转。
 * ========================================================================= */
#define BSP_FILE_ALIGN_STAGE_SIZE   (1024U) /* 暂存区大小：必须是扇区大小的整数倍，且不应为 512——
                                              * 实测连续两笔单扇区 DMA 写会确定性错开 2 字节（见 §9），
                                              * 拆成单扇区反而诱发旧缺陷；保留多扇区一笔下发的形状 */

/* 以 uint32_t 为底，天然 4 字节对齐（工程惯例，不依赖编译器对齐关键字） */
static uint32_t s_align_stage[BSP_FILE_ALIGN_STAGE_SIZE / 4U];
static volatile uint32_t s_align_stage_count;

/**
 * @brief 取回非对齐请求经中转落地的次数
 * @note  用于自检区分"本来就合规"与"靠中转才正确"，不靠推断下结论
 */
uint32_t bsp_file_get_align_stage_count(void)
{
    return s_align_stage_count;
}

/* 扇区大小：本配置 _MIN_SS == _MAX_SS == 512（ssize 字段被编译掉），直接取宏；
 * 若将来启用可变扇区，则改从 FATFS::ssize 取 */
static uint32_t s_fs_sector_size(const bsp_file_t *file)
{
#if (_MAX_SS != _MIN_SS)
    const FATFS *fs = file->handle.fat_file.obj.fs;

    return ((fs != NULL) && (fs->ssize != 0U)) ? (uint32_t)fs->ssize : BSP_FILE_ALIGN_STAGE_SIZE;
#else
    (void)file;
    return (uint32_t)_MIN_SS;
#endif
}

/**
 * @brief FatFS 错误码 → 统一状态码（保留“为什么失败”，供上层区分可重试与真失败）
 * @param fr f_* 返回值
 * @return bsp_status_t BSP_OK / BSP_EIO(介质或底层 IO 错) / BSP_ENODEV(文件或卡不在) /
 *         BSP_EINVAL(存在只读等策略拒绝) / BSP_ENOMEM(空间或核心不够) / BSP_ETIMEOUT
 * @note  以前一律压成 BSP_ERROR：卡满、卡被拔、名字非法、总线被占四种病因完全无法区分，
 *       曾直接把排查带到错方向；卡未挂载与文件不存在均归 BSP_ENODEV
 */
static bsp_status_t s_ff_err_to_bsp(FRESULT fr)
{
    switch (fr)
    {
    case FR_OK:                return BSP_OK;
    case FR_DISK_ERR:          return BSP_EIO;
    case FR_INT_ERR:           return BSP_EIO;
    case FR_NOT_READY:         return BSP_ENODEV;
    case FR_NO_FILE:
    case FR_NO_PATH:
    case FR_INVALID_NAME:
    case FR_INVALID_DRIVE:
    case FR_NOT_ENABLED:
    case FR_NO_FILESYSTEM:     return BSP_ENODEV;
    case FR_DENIED:
    case FR_EXIST:
    case FR_WRITE_PROTECTED:
    case FR_LOCKED:
    case FR_INVALID_OBJECT:
    case FR_INVALID_PARAMETER: return BSP_EINVAL;
    case FR_NOT_ENOUGH_CORE:
    case FR_TOO_MANY_OPEN_FILES: return BSP_ENOMEM;
    case FR_TIMEOUT:           return BSP_ETIMEOUT;
    default:                   return BSP_ERROR;
    }
}

/**
 * @brief LittleFS 错误码 → 统一状态码；本版本无“设备忙”错误码，故叠加总线门闩判断
 * @param err lfs_* 返回值（>=0 视为成功）
 * @return bsp_status_t 执行结果；总线被音频持有时返回 BSP_BUSY（可重试语义）
 */
static bsp_status_t s_lfs_err_to_bsp(int err)
{
    bsp_status_t st;

    if (err >= 0)
    {
        return BSP_OK;
    }

    switch (err)
    {
    case LFS_ERR_IO:
    case LFS_ERR_CORRUPT:      st = BSP_EIO; break;
    case LFS_ERR_NOENT:        st = BSP_ENODEV; break;
    case LFS_ERR_NOSPC:
    case LFS_ERR_NOMEM:        st = BSP_ENOMEM; break;
    case LFS_ERR_INVAL:
    case LFS_ERR_EXIST:
    case LFS_ERR_NOTDIR:
    case LFS_ERR_ISDIR:
    case LFS_ERR_NOTEMPTY:
    case LFS_ERR_BADF:
    case LFS_ERR_FBIG:
    case LFS_ERR_NOATTR:
    case LFS_ERR_NAMETOOLONG: st = BSP_EINVAL; break;
    default:                   st = BSP_ERROR; break;
    }

    /* 块设备回调把“总线被 I2S2 占用”只能上报成 LFS_ERR_IO，这里用门闩把它还原成可重试语义 */
    if ((st == BSP_EIO) && (bsp_lfs_get_last_error() == BSP_BUSY))
    {
        st = BSP_BUSY;
    }

    return st;
}

/**
 * @brief 判定本次请求是否必须经对齐中转
 * @note  条件不是“用户缓冲 4 字节对齐”，而是 buf 与当前文件位置同余 mod 4：
 *       FatFS 的 direct 路径会先把用户指针推进到扇区边界（推进量 = ssz - fptr%ssz，
 *       且 ssz 是 4 的倍数），故交给 disk_* 的地址 ≡ buf - fptr (mod 4)。
 *       实测反例：data 块起始 78 字节的 WAV，半区缓冲本身 4 字节对齐，FatFS 仍交出 buf+434。
 */
static uint8_t s_need_align_stage(const bsp_file_t *file, const void *buf)
{
    if (file->type != BSP_FILE_TYPE_FATFS)
    {
        return 0U;
    }

    return ((((uint32_t)(uintptr_t)buf) ^ file->handle.fat_file.fptr) & 0x3U) != 0U;
}

/**
 * @brief 经对齐中转分段写：段首/段尾走 FatFS 窗口，段体批量整扇区经对齐暂存区
 * @note  拆成三类分段的原因：只有“文件位置在扇区边界 + 整扇区长度”的请求才走 direct 路径，
 *       此时目标地址是 stage + k*ssz（ssz 是 4 的倍数）→ 恒 4 对齐；不足一扇区的头/尾由 FatFS
 *       逐字节拷进自己的窗口再 RMW，不会把用户指针交给 DMA。短写（卡满等）不猜测，按实际已写数返回。
 */
static bsp_status_t s_fatfs_write_staged(bsp_file_t *file, const uint8_t *buf, uint32_t len,
                                        uint32_t *written)
{
    uint8_t *stage = (uint8_t *)s_align_stage;
    const uint32_t ssz = s_fs_sector_size(file);
    uint32_t done = 0U;
    FRESULT fr = FR_OK;
    UINT bw = 0U;

    s_align_stage_count++;

    while (done < len)
    {
        uint32_t in_sec = (uint32_t)(file->handle.fat_file.fptr % ssz);
        const uint8_t *src = buf + done;
        uint32_t n;

        if (in_sec != 0U)
        {
            /* 段首：先补到扇区边界，不足一扇区 → FatFS 走窗口，不碰用户指针 */
            uint32_t to_bound = ssz - in_sec;

            n = ((len - done) < to_bound) ? (len - done) : to_bound;
        }
        else if ((len - done) >= ssz)
        {
            /* 段体：一次下发尽可能多的整扇区（不拆成单扇区连续写，那会错 2 字节） */
            n = ((len - done) > BSP_FILE_ALIGN_STAGE_SIZE) ? BSP_FILE_ALIGN_STAGE_SIZE : (len - done);
            n -= n % ssz;
            memcpy(stage, src, n);
            src = stage;
        }
        else
        {
            n = len - done;   /* 段尾：不足一扇区 */
        }

        fr = f_write(&file->handle.fat_file, (const BYTE *)src, n, &bw);
        if (fr != FR_OK)
        {
            if (written != NULL)
            {
                *written = done;
            }
            return s_ff_err_to_bsp(fr);
        }

        done += (uint32_t)bw;
        if (bw != n)
        {
            break;
        }
    }

    if (written != NULL)
    {
        *written = done;
    }
    return BSP_OK;
}

/**
 * @brief 读侧同理：段首/段尾走 FatFS 窗口，段体批量整扇区经对齐暂存区拷出
 */
static bsp_status_t s_fatfs_read_staged(bsp_file_t *file, uint8_t *buf, uint32_t len,
                                       uint32_t *readed)
{
    uint8_t *stage = (uint8_t *)s_align_stage;
    const uint32_t ssz = s_fs_sector_size(file);
    uint32_t done = 0U;
    FRESULT fr = FR_OK;
    UINT br = 0U;

    s_align_stage_count++;

    while (done < len)
    {
        uint32_t in_sec = (uint32_t)(file->handle.fat_file.fptr % ssz);
        uint8_t *dst = buf + done;
        uint32_t n;

        if ((in_sec != 0U) || ((len - done) < ssz))
        {
            /* 段首或段尾：不足一个扇区边界，FatFS 走内部窗口，不碰用户指针的 DMA 约束 */
            n = len - done;
            if (in_sec != 0U)
            {
                uint32_t to_bound = ssz - in_sec;

                if (n > to_bound)
                {
                    n = to_bound;
                }
            }
        }
        else
        {
            /* 段体：一次读尽可能多的整扇区到对齐暂存区，再拷给用户 */
            n = ((len - done) > BSP_FILE_ALIGN_STAGE_SIZE) ? BSP_FILE_ALIGN_STAGE_SIZE : (len - done);
            n -= n % ssz;
            dst = stage;
        }

        fr = f_read(&file->handle.fat_file, dst, n, &br);
        if (fr != FR_OK)
        {
            if (readed != NULL)
            {
                *readed = done;
            }
            return s_ff_err_to_bsp(fr);
        }

        if (dst == stage)
        {
            memcpy(buf + done, stage, br);
        }

        done += (uint32_t)br;
        if (br != n)
        {
            break;   /* 文件尾：属正常结束，由调用方比对 readed 与 len 判定 */
        }
    }

    if (readed != NULL)
    {
        *readed = done;
    }
    return BSP_OK;
}

/* =========================================================================
 * 导出 API 接口
 * ========================================================================= */

/**
 * @brief 打开或创建文件
 */
bsp_status_t bsp_file_open(bsp_file_t *file, const char *path, uint8_t flags)
{
    if (file == NULL || path == NULL)
    {
        return BSP_EINVAL;
    }

    /* 1. 路径路由：以 "0:" 开头则分发至 FatFS (SD卡) */
    if (strncmp(path, "0:", 2) == 0)
    {
        file->type = BSP_FILE_TYPE_FATFS;

        BYTE mode = 0;
        if (flags & BSP_FILE_READ)
        {
            mode |= FA_READ;
        }
        if (flags & BSP_FILE_WRITE)
        {
            mode |= FA_WRITE;
        }
        if (flags & BSP_FILE_CREATE)
        {
            if (flags & BSP_FILE_TRUNC)
            {
                mode |= FA_CREATE_ALWAYS;
            }
            else
            {
                mode |= FA_OPEN_ALWAYS;
            }
        }

        FRESULT fr = f_open(&file->handle.fat_file, path, mode);
        return s_ff_err_to_bsp(fr);
    }
    /* 2. 路径路由：以 "flash/" 开头则分发至 LittleFS (板载 Flash) */
    else if (strncmp(path, "flash/", 6) == 0)
    {
        file->type = BSP_FILE_TYPE_LITTLEFS;

        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int mode = 0;
        if (flags & BSP_FILE_READ)
        {
            mode |= LFS_O_RDONLY;
        }
        if (flags & BSP_FILE_WRITE)
        {
            mode |= LFS_O_WRONLY;
        }
        if (flags & BSP_FILE_CREATE)
        {
            mode |= LFS_O_CREAT;
        }
        if (flags & BSP_FILE_TRUNC)
        {
            mode |= LFS_O_TRUNC;
        }

        /* 剥离 "flash/" 前缀，直接将相对路径传给 LittleFS */
        const char *lfs_path = path + 6;

        int err = lfs_file_open(lfs, &file->handle.lfs_file, lfs_path, mode);
        return s_lfs_err_to_bsp(err);
    }

    file->type = BSP_FILE_TYPE_UNKNOWN;
    return BSP_EINVAL;
}

/**
 * @brief 写入文件数据
 */
bsp_status_t bsp_file_write(bsp_file_t *file, const void *buf, uint32_t len, uint32_t *written)
{
    if (file == NULL || buf == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        if (s_need_align_stage(file, buf) != 0U)
        {
            return s_fatfs_write_staged(file, (const uint8_t *)buf, len, written);
        }

        UINT bw = 0;
        FRESULT fr = f_write(&file->handle.fat_file, buf, len, &bw);
        if (written != NULL)
        {
            *written = (uint32_t)bw;
        }
        return s_ff_err_to_bsp(fr);
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        lfs_ssize_t res = lfs_file_write(lfs, &file->handle.lfs_file, buf, len);
        if (res >= 0)
        {
            if (written != NULL)
            {
                *written = (uint32_t)res;
            }
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 读取文件数据（从当前读写位置继续）
 */
bsp_status_t bsp_file_read(bsp_file_t *file, void *buf, uint32_t len, uint32_t *readed)
{
    if (file == NULL || buf == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        if (s_need_align_stage(file, buf) != 0U)
        {
            return s_fatfs_read_staged(file, (uint8_t *)buf, len, readed);
        }

        UINT br = 0;
        FRESULT fr = f_read(&file->handle.fat_file, buf, len, &br);
        if (readed != NULL)
        {
            *readed = (uint32_t)br;
        }
        return s_ff_err_to_bsp(fr);
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        lfs_ssize_t res = lfs_file_read(lfs, &file->handle.lfs_file, buf, len);
        if (res >= 0)
        {
            if (readed != NULL)
            {
                *readed = (uint32_t)res;
            }
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 获取文件总大小（字节）
 */
bsp_status_t bsp_file_size(bsp_file_t *file, uint32_t *size)
{
    if (file == NULL || size == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        *size = (uint32_t)f_size(&file->handle.fat_file);
        return BSP_OK;
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        *size = (uint32_t)lfs_file_size(bsp_lfs_get_handle(), &file->handle.lfs_file);
        return BSP_OK;
    }

    return BSP_EINVAL;
}

/**
 * @brief 关闭文件并释放资源
 */
bsp_status_t bsp_file_close(bsp_file_t *file)
{
    if (file == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        FRESULT fr = f_close(&file->handle.fat_file);
        file->type = BSP_FILE_TYPE_UNKNOWN;
        return s_ff_err_to_bsp(fr);
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int err = lfs_file_close(lfs, &file->handle.lfs_file);
        file->type = BSP_FILE_TYPE_UNKNOWN;
        return s_lfs_err_to_bsp(err);
    }

    return BSP_EINVAL;
}

/**
 * @brief 同步文件数据到物理介质
 */
bsp_status_t bsp_file_sync(bsp_file_t *file)
{
    if (file == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        FRESULT fr = f_sync(&file->handle.fat_file);
        return s_ff_err_to_bsp(fr);
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int err = lfs_file_sync(lfs, &file->handle.lfs_file);
        return s_lfs_err_to_bsp(err);
    }

    return BSP_EINVAL;
}

/**
 * @brief 创建单个目录
 */
bsp_status_t bsp_file_mkdir(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    if (strncmp(path, "0:", 2) == 0)
    {
        FRESULT fr = f_mkdir(path);
        if (fr == FR_OK || fr == FR_EXIST)
        {
            return BSP_OK;
        }
        return BSP_ERROR;
    }
    else if (strncmp(path, "flash/", 6) == 0)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }
        const char *lfs_path = path + 6;
        int err = lfs_mkdir(lfs, lfs_path);
        if (err >= 0 || err == LFS_ERR_EXIST)
        {
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 递归创建目录（若父目录不存在则自动级联创建）
 */
bsp_status_t bsp_file_mkdir_rec(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    char tmp_path[128];
    uint32_t len = strlen(path);
    if (len >= sizeof(tmp_path))
    {
        return BSP_EINVAL;
    }

    strncpy(tmp_path, path, sizeof(tmp_path) - 1);
    tmp_path[sizeof(tmp_path) - 1] = '\0';

    /* 逐层提取目录并创建 */
    char *p = tmp_path;
    
    /* 区分路径类型，跳过根前缀 */
    if (strncmp(tmp_path, "0:", 2) == 0)
    {
        p += 2;
        /* 跳过可能包含的 "0:/" */
        if (*p == '/' || *p == '\\')
        {
            p++;
        }
    }
    else if (strncmp(tmp_path, "flash/", 6) == 0)
    {
        p += 6;
    }
    else
    {
        return BSP_EINVAL;
    }

    /* 循环遍历，遇到分隔符就截断并创建目录 */
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
        {
            char backup = *p;
            *p = '\0';  /* 截断字符串得到当前层级路径 */
            
            bsp_status_t status = bsp_file_mkdir(tmp_path);
            (void)status; /* 忽略错误，因为父目录可能已存在 */
            
            *p = backup; /* 还原字符 */
        }
        p++;
    }

    /* 最后再创建最深的一层目录（如果路径不是以斜杠结尾的话） */
    if (len > 0 && tmp_path[len - 1] != '/' && tmp_path[len - 1] != '\\')
    {
        (void)bsp_file_mkdir(tmp_path);
    }

    return BSP_OK;
}

bsp_status_t bsp_file_remove(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    if (strncmp(path, "0:", 2) == 0)
    {
        FRESULT fr = f_unlink(path);

        if (fr == FR_OK)
        {
            return BSP_OK;
        }
        if (fr == FR_NO_FILE || fr == FR_NO_PATH)
        {
            return BSP_ENODEV;
        }
        return BSP_ERROR;
    }
    else if (strncmp(path, "flash/", 6) == 0)
    {
        lfs_t *lfs = bsp_lfs_get_handle();

        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int err = lfs_remove(lfs, path + 6);

        if (err == LFS_ERR_OK)
        {
            return BSP_OK;
        }
        if (err == LFS_ERR_NOENT)
        {
            return BSP_ENODEV;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

bsp_status_t bsp_file_rename(const char *old_path, const char *new_path)
{
    bool old_fatfs;
    bool new_fatfs;

    if (old_path == NULL || new_path == NULL)
    {
        return BSP_EINVAL;
    }

    old_fatfs = (strncmp(old_path, "0:", 2) == 0);
    new_fatfs = (strncmp(new_path, "0:", 2) == 0);

    /* 后端不一致属于误用：底层无法跨 FatFS/LittleFS 搬运，当场拒绝 */
    if (old_fatfs != new_fatfs)
    {
        return BSP_EINVAL;
    }

    if (old_fatfs)
    {
        /* FatFS 的目标已存在时会返回 FR_EXIST，先按覆盖语义删掉目标 */
        (void)f_unlink(new_path);

        FRESULT fr = f_rename(old_path, new_path);

        if (fr == FR_OK)
        {
            return BSP_OK;
        }
        if (fr == FR_NO_FILE || fr == FR_NO_PATH)
        {
            return BSP_ENODEV;
        }
        return BSP_ERROR;
    }

    lfs_t *lfs = bsp_lfs_get_handle();

    if (lfs == NULL)
    {
        return BSP_ENODEV;
    }

    /* LittleFS 的 rename 本身具备目标存在时的替换语义 */
    int err = lfs_rename(lfs, old_path + 6, new_path + 6);

    if (err == LFS_ERR_OK)
    {
        return BSP_OK;
    }
    if (err == LFS_ERR_NOENT)
    {
        return BSP_ENODEV;
    }
    return BSP_ERROR;
}
