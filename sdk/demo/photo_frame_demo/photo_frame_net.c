#include "basic_include.h"
#include "lib/common/sysevt.h"
#include "project_config.h"
#include "brtc_agent/brtc_agent.h"
#include "album/ai_album_album_image_ai.h"
#include "ui/ai_album_chat_runtime.h"

/* 百度实时语音Agent接入(照搬旁系app_net_handler.c,本工程化简):
 * - WiFi 首次在线(DHCP完成)时 brtc_agent_start;断网时 stop
 * - 事件回调分发到 chat_runtime(AI对话/翻译/练习页面消费)
 * - 事件同时分发到 image_ai:图生图结果以 VIDEO_DATA(JPEG) 事件返回
 *
 * 音频静态组件在此前初始化(photo_frame_demo.c app_audio_init):
 *   audio_adc_init(建S_AUADC) / audio_mixer_init / audio_coder_msi_init
 *   / pcm_dec_msi_init —— brtc_agent_audio_start 前必须就绪 */

static uint8 g_brtc_started;

static void photo_frame_brtc_agent_event(const brtc_agent_event_t *event,
                                         void *user_data)
{
    (void)user_data;
    if (!event) {
        return;
    }
    /* image_ai 消费 VIDEO_DATA / ERROR / MEDIA_GENERATE_* 事件 */
    ai_album_chat_runtime_handle_event(event);
    ai_album_album_image_ai_handle_event(event);
    switch (event->type) {
        case BRTC_AGENT_EVENT_STATE_CHANGED:
            os_printf("[BRTC_APP] state=%d code=%d\r\n", event->state,
                      event->code);
            break;
        case BRTC_AGENT_EVENT_ERROR:
            os_printf("[BRTC_APP] error=%d text=%.*s\r\n", event->code,
                      (int)event->text_len, event->text ? event->text : "");
            break;
        case BRTC_AGENT_EVENT_ASR:
            os_printf("[BRTC_APP] ASR final=%d: %.*s\r\n", event->final,
                      (int)event->text_len, event->text ? event->text : "");
            break;
        case BRTC_AGENT_EVENT_ANSWER:
            os_printf("[BRTC_APP] answer final=%d: %.*s\r\n", event->final,
                      (int)event->text_len, event->text ? event->text : "");
            break;
        case BRTC_AGENT_EVENT_SPEAKING:
            os_printf("[BRTC_APP] speaking=%d\r\n", event->active);
            break;
        case BRTC_AGENT_EVENT_MEDIA_GENERATE_RESULT:
            os_printf("[BRTC_APP] media result: %.*s\r\n",
                      (int)event->text_len,
                      event->text != NULL ? event->text : "");
            break;
        default:
            break;
    }
}

int photo_frame_net_brtc_start(void)
{
    int ret;

    if (g_brtc_started) {
        return RET_OK;
    }
    ret = brtc_agent_start();
    if (ret == RET_OK || ret == 1 /* IN_PROGRESS */) {
        g_brtc_started = 1U;
    }
    return ret;
}

void photo_frame_net_brtc_stop(void)
{
    if (!g_brtc_started) {
        return;
    }
    (void)brtc_agent_stop();
    g_brtc_started = 0U;
}

static sysevt_hdl_res photo_frame_net_evt_hdl(uint32 event_id, uint32 data,
                                              uint32 priv)
{
    if (event_id == SYS_EVENT(SYS_EVENT_NETWORK, SYSEVT_LWIP_DHCPC_DONE)) {
        os_printf("[NET] DHCP Done -> start Baidu agent\r\n");
        (void)photo_frame_net_brtc_start();
    }
    return SYSEVT_CONTINUE;
}
void photo_frame_net_init(void)
{
    brtc_agent_config_t brtc_config;
    int ret;

    os_memset(&brtc_config, 0, sizeof(brtc_config));
    brtc_config.platform_url = AI_ALBUM_BRTC_PLATFORM_URL;
    brtc_config.create_url = AI_ALBUM_BRTC_CREATE_URL;
    brtc_config.stop_url = AI_ALBUM_BRTC_STOP_URL;
    brtc_config.app_id = AI_ALBUM_BRTC_APP_ID;
    brtc_config.license_key = AI_ALBUM_BRTC_LICENSE_KEY;
    brtc_config.llm = AI_ALBUM_BRTC_LLM;
    brtc_config.language = AI_ALBUM_BRTC_LANGUAGE;
    brtc_config.workflow = "VoiceChat";
    brtc_config.http_timeout_ms = 10000U;
    brtc_config.start_retries = 3U;
    brtc_config.enable_voice_interrupt = true;
    brtc_config.voice_interrupt_level = 60;
    brtc_config.enable_video = true;
    brtc_config.screen_width = AI_ALBUM_BRTC_SCREEN_WIDTH;
    brtc_config.screen_height = AI_ALBUM_BRTC_SCREEN_HEIGHT;
    brtc_config.event_cb = photo_frame_brtc_agent_event;

    ret = brtc_agent_init(&brtc_config);
    ai_album_chat_runtime_init();
    ai_album_album_image_ai_init();
    os_printf("[NET] brtc_agent_init=%d\r\n", ret);

    sys_event_take(SYS_EVENT(SYS_EVENT_NETWORK, SYSEVT_LWIP_DHCPC_DONE),
                   photo_frame_net_evt_hdl, 0);
    os_printf("[NET] photo_frame_net_init done\r\n");
}
