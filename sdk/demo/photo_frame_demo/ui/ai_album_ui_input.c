#include "ui/ai_album_ui_input.h"

#include "basic_include.h"
#include "osal/msgqueue.h"
#include "ui/ai_album_ui_input_internal.h"

#define AI_ALBUM_UI_INPUT_QUEUE_DEPTH 8

static os_msgqueue_t g_ui_input_queue;
static uint8_t g_ui_input_ready;

int ai_album_ui_input_init(void)
{
    if (g_ui_input_ready) {
        return RET_OK;
    }
    memset(&g_ui_input_queue, 0, sizeof(g_ui_input_queue));
    if (os_msgq_init(&g_ui_input_queue, AI_ALBUM_UI_INPUT_QUEUE_DEPTH) !=
        RET_OK) {
        return RET_ERR;
    }
    g_ui_input_ready = 1U;
    return RET_OK;
}

void ai_album_ui_input_deinit(void)
{
    if (!g_ui_input_ready) {
        return;
    }
    (void)os_msgq_del(&g_ui_input_queue);
    g_ui_input_ready = 0U;
}

int ai_album_ui_input_post(ai_album_ui_action_t action)
{
    if (!g_ui_input_ready || action < AI_ALBUM_UI_ACTION_LEFT ||
        action >= AI_ALBUM_UI_ACTION_COUNT) {
        return RET_ERR;
    }
    return os_msgq_put(&g_ui_input_queue, (uint32)action, 0);
}

int ai_album_ui_input_try_pop(ai_album_ui_action_t *action)
{
    int32 queue_result = RET_ERR;
    uint32 value;

    if (!g_ui_input_ready || action == NULL) {
        return RET_ERR;
    }
    value = os_msgq_get2(&g_ui_input_queue, 0, &queue_result);
    if (queue_result != RET_OK || value >= AI_ALBUM_UI_ACTION_COUNT) {
        return RET_ERR;
    }
    *action = (ai_album_ui_action_t)value;
    return RET_OK;
}
