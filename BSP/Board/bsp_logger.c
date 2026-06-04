#include "bsp_logger.h"
#include "elog.h"

void bsp_logger_init(void)
{
    /* 1. 初始化核心引擎 */
    elog_init();

    /* 2. 启用颜色输出 */
    elog_set_text_color_enabled(true);

    /* 3. 设定业务策略：输出格式 */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~ELOG_FMT_FUNC);
    /* 4. 启动 */
    elog_start();
}
