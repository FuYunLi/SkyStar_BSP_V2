/**
 * @file app_buzzer_demo.c
 * @brief 蜂鸣器曲目播放自检演示实现
 * @note 对标 RocketPi 16_rocketpi_pwm_passive_buzzer 的序列播放机制，
 *       架构上按本框架"全回调 + 单向数据流"规范重写：
 *       Shell 指令只投递播放请求（输入层），MultiTimer 回调按音符时值
 *       逐拍调度 dev_buzzer（输出层），播放期间不阻塞任何任务。
 *
 *       与常驻状态机定时器"回调末尾无条件续期"的铁律不同，播放定时器
 *       属于一次性任务：曲目播完或被手动停止时有意不再续期，定时器自然
 *       终止；再次播放时由 Shell 指令重新启动。
 */

#define LOG_TAG "APP_BUZZER"

#include "app_buzzer_demo.h"
#include "bsp_logger.h"
#include "dev_buzzer.h"
#include "MultiTimer.h"
#include "shell.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 音符与曲谱定义
 * ================================================================ */

/**
 * @brief 单个音符描述
 * @note 三字段零值均有默认兜底：freq_hz=0 表示休止符，volume=0 使用
 *       默认音量，duration_ms=0 使用默认时值——曲谱定义得以最简
 */
typedef struct
{
    uint16_t freq_hz;     /* 发声频率（Hz），0 为休止符 */
    uint8_t  volume;      /* 音量 (0-100)，0 使用默认音量 */
    uint16_t duration_ms; /* 时值（ms），0 使用默认时值 */
} app_buzzer_note_t;

/* 音符频率表：十二平均律取整，f = 440 × 2^((n-69)/12)，A4=440Hz */
#define NOTE_REST  (0U)
#define NOTE_C4    (262U)
#define NOTE_D4    (294U)
#define NOTE_E4    (330U)
#define NOTE_F4    (349U)
#define NOTE_G4    (392U)
#define NOTE_A4    (440U)
#define NOTE_B4    (494U)
#define NOTE_C5    (523U)
#define NOTE_G5    (784U)

/* 欢乐颂时值：单位 × 8ms。25 单位 ≈ 四分音符(200ms)，36 ≈ 附点，
 * 22 ≈ 半拍收尾，调小 ODE_TEMPO_MS 即整体加速 */
#define ODE_TEMPO_MS (8U)
#define ODE_DUR(units) ((uint16_t)((units) * ODE_TEMPO_MS))

/* 音阶上行（do re mi...） */
static const app_buzzer_note_t s_song_scale[] =
{
    { NOTE_C4, 0U, 200U }, { NOTE_D4, 0U, 200U }, { NOTE_E4, 0U, 200U },
    { NOTE_F4, 0U, 200U }, { NOTE_G4, 0U, 200U }, { NOTE_A4, 0U, 200U },
    { NOTE_B4, 0U, 200U }, { NOTE_C5, 0U, 400U },
};

/* 贝多芬《欢乐颂》选段 */
static const app_buzzer_note_t s_song_ode_to_joy[] =
{
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_F4, 0U, ODE_DUR(25U) }, { NOTE_G4, 0U, ODE_DUR(25U) },
    { NOTE_G4, 0U, ODE_DUR(25U) }, { NOTE_F4, 0U, ODE_DUR(25U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_D4, 0U, ODE_DUR(36U) },
    { NOTE_C4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_E4, 0U, ODE_DUR(22U) }, { NOTE_D4, 0U, ODE_DUR(22U) },
    { NOTE_D4, 0U, ODE_DUR(42U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_F4, 0U, ODE_DUR(25U) }, { NOTE_G4, 0U, ODE_DUR(25U) },
    { NOTE_G4, 0U, ODE_DUR(25U) }, { NOTE_F4, 0U, ODE_DUR(25U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_D4, 0U, ODE_DUR(25U) },
    { NOTE_C4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(22U) }, { NOTE_C4, 0U, ODE_DUR(22U) },
    { NOTE_C4, 0U, ODE_DUR(42U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_D4, 0U, ODE_DUR(25U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(36U) },
    { NOTE_F4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_G4, 0U, ODE_DUR(40U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_F4, 0U, ODE_DUR(25U) }, { NOTE_G4, 0U, ODE_DUR(25U) },
    { NOTE_G4, 0U, ODE_DUR(25U) }, { NOTE_F4, 0U, ODE_DUR(25U) },
    { NOTE_E4, 0U, ODE_DUR(25U) }, { NOTE_D4, 0U, ODE_DUR(25U) },
    { NOTE_C4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_E4, 0U, ODE_DUR(25U) },
    { NOTE_D4, 0U, ODE_DUR(25U) }, { NOTE_C4, 0U, ODE_DUR(25U) },
    { NOTE_C4, 0U, ODE_DUR(48U) },
};

/* 提示音（双音 + 休止） */
static const app_buzzer_note_t s_song_chime[] =
{
    { NOTE_G5, 60U, 120U }, { NOTE_REST, 0U, 80U }, { NOTE_C5, 60U, 200U },
};

/* 曲库表 */
typedef struct
{
    const char *name;                 /* 曲目名（Shell 检索键） */
    const app_buzzer_note_t *notes;   /* 音符数组 */
    uint32_t len;                     /* 音符数量 */
} app_buzzer_song_t;

static const app_buzzer_song_t s_songs[] =
{
    { "ode",   s_song_ode_to_joy, ARRAY_SIZE(s_song_ode_to_joy) },
    { "scale", s_song_scale,      ARRAY_SIZE(s_song_scale) },
    { "chime", s_song_chime,      ARRAY_SIZE(s_song_chime) },
};

/* ================================================================
 * 播放器状态
 * ================================================================ */

#define PLAYER_DEFAULT_VOLUME  (50U)   /* 音符音量缺省值 */
#define PLAYER_DEFAULT_NOTE_MS (200U)  /* 音符时值缺省值 */
#define PLAYER_NOTE_GAP_MS     (20U)   /* 音符间隔，防振膜粘连 */

static MultiTimer s_player_timer;                  /* 音符调度定时器 */
static const app_buzzer_song_t *s_current_song;   /* 正在播放的曲目 */
static uint32_t s_note_idx;                       /* 下一音符下标 */
static bool s_playing;                            /* 播放使能标志 */
static bool s_loop;                               /* 循环播放标志 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 音符调度回调：按曲目顺序逐拍发声
 * @note 每次到期只发出一个音符并以该音符时值重新调度自身，
 *       回调即刻返回，不阻塞调度循环
 */
static void s_player_timer_cb(MultiTimer *timer, void *arg)
{
    (void)arg;

    /* 被 buzzer_stop 终止后最后一次挂起到期，直接结束 */
    if (!s_playing || (s_current_song == NULL))
    {
        return;
    }

    /* 曲目播完：循环则回卷，否则关闭蜂鸣器并终止调度 */
    if (s_note_idx >= s_current_song->len)
    {
        if (s_loop)
        {
            s_note_idx = 0U;
        }
        else
        {
            (void)dev_buzzer_off();
            s_playing = false;
            log_i("Playback finished.");
            return;
        }
    }

    const app_buzzer_note_t *note = &s_current_song->notes[s_note_idx];
    s_note_idx++;

    if (note->freq_hz == NOTE_REST)
    {
        (void)dev_buzzer_off();
    }
    else
    {
        uint8_t volume = (note->volume != 0U) ? note->volume : PLAYER_DEFAULT_VOLUME;
        (void)dev_buzzer_tone(note->freq_hz, volume);
    }

    /* 音符时值 + 音符间隔一并纳入下次调度 */
    uint16_t duration = (note->duration_ms != 0U) ? note->duration_ms : PLAYER_DEFAULT_NOTE_MS;
    (void)multiTimerStart(timer, (uint64_t)duration + PLAYER_NOTE_GAP_MS, s_player_timer_cb, NULL);
}

/**
 * @brief 按曲目名检索曲库
 * @return 命中的曲目表项，未命中返回 NULL
 */
static const app_buzzer_song_t *s_find_song(const char *name)
{
    for (uint32_t i = 0U; i < ARRAY_SIZE(s_songs); i++)
    {
        if (strcmp(name, s_songs[i].name) == 0)
        {
            return &s_songs[i];
        }
    }

    return NULL;
}

/**
 * @brief buzzer_play Shell 指令入口：启动曲目播放
 */
static int shell_buzzer_play(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_i("usage: buzzer_play <ode|scale|chime> [loop]");
        return -1;
    }

    const app_buzzer_song_t *song = s_find_song(argv[1]);
    if (song == NULL)
    {
        log_e("unknown song: %s", argv[1]);
        return -1;
    }

    s_current_song = song;
    s_note_idx = 0U;
    s_loop = ((argc >= 3) && (strcmp(argv[2], "loop") == 0));
    s_playing = true;

    log_i("Playing %s (%lu notes%s)...", song->name, (unsigned long)song->len, s_loop ? ", loop" : "");

    /* 立即挂起首个音符，后续由回调自行续期 */
    (void)multiTimerStart(&s_player_timer, 1U, s_player_timer_cb, NULL);

    return 0;
}

/**
 * @brief buzzer_stop Shell 指令入口：终止播放并静音
 */
static int shell_buzzer_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    s_playing = false;
    (void)dev_buzzer_off();
    log_i("Playback stopped.");

    return 0;
}

/**
 * @brief buzzer_beep Shell 指令入口：阻塞式单音自检
 */
static int shell_buzzer_beep(int argc, char *argv[])
{
    if (argc != 4)
    {
        log_i("usage: buzzer_beep <freq_hz> <volume> <duration_ms>");
        return -1;
    }

    uint32_t freq = (uint32_t)atoi(argv[1]);
    uint8_t volume = (uint8_t)atoi(argv[2]);
    uint32_t duration = (uint32_t)atoi(argv[3]);

    bsp_status_t ret = dev_buzzer_beep((uint16_t)freq, volume, duration);
    if (ret != BSP_OK)
    {
        log_e("beep failed! ret = %d", ret);
        return -1;
    }

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化蜂鸣器曲目播放演示模块
 */
bsp_status_t app_buzzer_demo_init(void)
{
    log_i("Buzzer Demo loaded. Try: buzzer_play ode");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, buzzer_play, shell_buzzer_play, Play a song by name (ode/scale/chime) with optional loop);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, buzzer_stop, shell_buzzer_stop, Stop song playback immediately);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, buzzer_beep, shell_buzzer_beep, Blocking single beep (freq volume ms));
