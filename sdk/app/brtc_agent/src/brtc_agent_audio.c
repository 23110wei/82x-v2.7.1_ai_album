#include "brtc_agent_internal.h"

#include "audio_msi/audio_adc.h"
#include "lib/multimedia/framebuff.h"
#include "lib/multimedia/msi.h"
#include "lib/multimedia/msi_names.h"
#include "lib/multimedia/media_types.h"
#include "lib/multimedia/audio.h"
#include "osal/sleep.h"
#include "osal/task.h"

/* 音频链适配(本SDK 2.7.1.7原生组件,替代旁系S_AUADC/R_AUDAC私有链):
 * 麦克风: vendor audio_adc.c 建 S_AUADC,auadc_msi_add_output 推到
 *         本组件的 R_BRTC_MIC,brtc_mic 任务取帧喂引擎(PTT期间)。
 * TTS:   引擎回传PCM -> S_BRTC_TTS -> pcmdec动态通道(type=PCM_S16LE)
 *        -> audio_mixer -> DAC。pcmdec/mixer 均为本SDK源码组件
 *        (coder/pcm_msi.c, multimedia/audio/mixer.c),由 app 侧
 *        audio_mixer_init/audio_coder_msi_init/pcm_dec_msi_init 建立静态注册。
 * TTS PCM 规格: 16kHz/16bit/单声道(msi_recv_fb 按 mtype<<8|stype 匹配
 * pcmdec 的 type 字段,codec_info 提供采样率给 mixer 重采样)。 */

#define BRTC_AGENT_MIC_MSI_NAME       "R_BRTC_MIC"
#define BRTC_AGENT_TTS_MSI_NAME       "S_BRTC_TTS"
#define BRTC_AGENT_MIC_QUEUE_DEPTH    4U
#define BRTC_AGENT_MIC_STACK_SIZE     4096U
/* 引擎TTS输出实测20ms/320B = 8kHz/16bit/mono(与旁系tts_track.samplerate
 * 8000一致);推帧块大小按8k的20ms计算 */
#define BRTC_AGENT_TTS_CHUNK_BYTES    320U
#define BRTC_AGENT_TTS_SAMPLERATE     8000U

typedef struct brtc_agent_audio_context {
    struct msi *mic_msi;
    struct msi *tts_msi;
    struct msi *pcmdec_msi;
    struct msi *mixer_msi;
    void *mic_task;
    void *mic_stack;
    txAudioInfo_t pcm_info;
    volatile bool active;
    bool mic_bound;
    bool tts_bound;
    bool tts_pcm_seen;
} brtc_agent_audio_context_t;

static brtc_agent_audio_context_t g_brtc_audio;

static bool brtc_agent_audio_msi_present(const char *name)
{
    struct msi *msi = msi_find(name, 1);

    if (!msi) {
        return false;
    }
    msi_put(msi);
    return true;
}

static int32 brtc_agent_mic_action(struct msi *msi, uint32 command,
                                   uint32 param1, uint32 param2)
{
    struct framebuff *frame;
    (void)msi;
    (void)param2;

    if (command != MSI_CMD_TRANS_FB) {
        return RET_OK;
    }
    frame = (struct framebuff *)param1;
    return (frame && frame->mtype == MEDIA_DATA_AUDIO) ? RET_OK : RET_ERR;
}

static void brtc_agent_mic_task(void *arg)
{
    struct framebuff *frame;
    struct framebuff *node;
    (void)arg;

    for (;;) {
        frame = msi_get_fb(g_brtc_audio.mic_msi, 100U);
        if (!frame) {
            continue;
        }
        if (g_brtc_audio.active && brtc_agent_internal_ptt_active()) {
            node = frame;
            while (node) {
                if (!node->clone && node->data && node->len) {
                    brtc_agent_internal_send_audio(node->data, node->len);
                }
                node = node->next;
            }
        }
        msi_delete_fb(NULL, frame);
    }
}

static int brtc_agent_audio_create_nodes(void)
{
    uint8 is_new = 0U;

    if (!g_brtc_audio.mic_msi) {
        g_brtc_audio.mic_msi =
            msi_new(BRTC_AGENT_MIC_MSI_NAME, BRTC_AGENT_MIC_QUEUE_DEPTH,
                    &is_new);
        if (!g_brtc_audio.mic_msi) {
            return BRTC_AGENT_ERR_AUDIO;
        }
        g_brtc_audio.mic_msi->action = brtc_agent_mic_action;
        g_brtc_audio.mic_msi->enable = 1;
        g_brtc_audio.mic_stack = os_malloc_psram(BRTC_AGENT_MIC_STACK_SIZE);
        if (!g_brtc_audio.mic_stack) {
            msi_destroy(g_brtc_audio.mic_msi);
            g_brtc_audio.mic_msi = NULL;
            return BRTC_AGENT_ERR_NO_MEMORY;
        }
        g_brtc_audio.mic_task = os_task_create(
            "brtc_mic", brtc_agent_mic_task, NULL,
            OS_TASK_PRIORITY_ABOVE_NORMAL, 0, g_brtc_audio.mic_stack,
            BRTC_AGENT_MIC_STACK_SIZE);
        if (!g_brtc_audio.mic_task) {
            os_free_psram(g_brtc_audio.mic_stack);
            g_brtc_audio.mic_stack = NULL;
            msi_destroy(g_brtc_audio.mic_msi);
            g_brtc_audio.mic_msi = NULL;
            return BRTC_AGENT_ERR_NO_MEMORY;
        }
    }

    if (!g_brtc_audio.tts_msi) {
        is_new = 0U;
        /* 第二参=fb_limits(可同时在途的帧数上限),传0会导致
         * msi_alloc_fb永远失败(日志:drop TTS PCM: no frame memory)。
         * 16帧×20ms=320ms深度,覆盖pcmdec/mixer的消费抖动 */
        g_brtc_audio.tts_msi = msi_new(BRTC_AGENT_TTS_MSI_NAME, 16U, &is_new);
        if (!g_brtc_audio.tts_msi) {
            return BRTC_AGENT_ERR_AUDIO;
        }
        /* msi_new并不初始化fb_limits(构造时memset恒0),而msi_alloc_fb
         * 按额度递减分配:必须照dac_msg.c手动设额度,否则永远分配失败 */
        g_brtc_audio.tts_msi->fb_limits.counter = 16;
        g_brtc_audio.tts_msi->enable = 1;
        g_brtc_audio.tts_msi->type =
            (uint16)((MEDIA_DATA_AUDIO << 8) | AUDIO_CODEC_PCM_S16LE);
    }
    return BRTC_AGENT_OK;
}

int brtc_agent_audio_start(void)
{
    int ret;

    /* 麦克风供给侧: vendor audio_adc.c 在 audio_adc_init 时建 S_AUADC。
     * 播放供给侧: pcmdec/mixer 由 app 侧 pcm_dec_msi_init/audio_mixer_init
     * 建立静态注册;coder 任务由 audio_coder_msi_init 建立。 */
    if (!brtc_agent_audio_msi_present("S_AUADC") ||
        !brtc_agent_audio_msi_present(PCMDEC_MSI) ||
        !brtc_agent_audio_msi_present(MIXER_MSI)) {
        os_printf("[BRTC_AGENT] provider audio MSI is not ready\r\n");
        return BRTC_AGENT_ERR_AUDIO;
    }
    ret = brtc_agent_audio_create_nodes();
    if (ret != BRTC_AGENT_OK) {
        return ret;
    }

    if (!g_brtc_audio.pcmdec_msi) {
        os_memset(&g_brtc_audio.pcm_info, 0, sizeof(g_brtc_audio.pcm_info));
        g_brtc_audio.pcm_info.codec_id = AUDIO_CODEC_PCM_S16LE;
        g_brtc_audio.pcm_info.sample_rate = BRTC_AGENT_TTS_SAMPLERATE;
        g_brtc_audio.pcm_info.channels = 1;
        g_brtc_audio.pcm_info.bits_per_coded_sample = 16;
        g_brtc_audio.pcm_info.frame_size =
            BRTC_AGENT_TTS_CHUNK_BYTES / 2U;
        /* 必须按名字找 pcmdec 管理器(动态通道由 find2 经 NEW_CHANNEL 创建,
         * arg=pcm_info 提供采样率)。不能按 type 找:msi_find_lock 的 type
         * 扫描只排除动态通道(mgr!=NULL),不排除同 type 的静态源组件,而
         * 本模块的 S_BRTC_TTS type 恰好也是 PCM_S16LE 且在链表头(后进先
         * 出),会自匹配→tts 对自己 add_output 成自环,帧被自己 fbQ 永久
         * 持有,16 帧额度耗尽后全部 drop(2026-09-16 板测日志根因)。 */
        g_brtc_audio.pcmdec_msi = msi_find2(
            PCMDEC_MSI, 0, 1, &g_brtc_audio.pcm_info);
        if (!g_brtc_audio.pcmdec_msi) {
            os_printf("[BRTC_AGENT] pcmdec channel create failed\r\n");
            return BRTC_AGENT_ERR_AUDIO;
        }
        g_brtc_audio.mixer_msi = msi_find2(MIXER_MSI, 0, 0, 0);
        if (!g_brtc_audio.mixer_msi) {
            msi_put(g_brtc_audio.pcmdec_msi);
            g_brtc_audio.pcmdec_msi = NULL;
            os_printf("[BRTC_AGENT] mixer not found\r\n");
            return BRTC_AGENT_ERR_AUDIO;
        }
        /* 注意:find2创建的mixer动态通道(mixer#N)引用**保留到stop**,
         * 此处立即put会导致通道被框架回收(上一版日志:mixer#1创建
         * 后立刻destroy,pcmdec输出落空) */
        ret = msi_add_output(g_brtc_audio.pcmdec_msi, NULL,
                             g_brtc_audio.mixer_msi, NULL);
        if (ret != RET_OK) {
            os_printf("[BRTC_AGENT] pcmdec->mixer bind failed: %d\r\n", ret);
            msi_put(g_brtc_audio.mixer_msi);
            g_brtc_audio.mixer_msi = NULL;
            msi_put(g_brtc_audio.pcmdec_msi);
            g_brtc_audio.pcmdec_msi = NULL;
            return BRTC_AGENT_ERR_AUDIO;
        }
        msi_do_cmd(g_brtc_audio.pcmdec_msi, MSI_CMD_START, 0, 0);
    }

    if (!g_brtc_audio.mic_bound) {
        ret = auadc_msi_add_output(AUSYS_AUAD, BRTC_AGENT_MIC_MSI_NAME);
        if (ret != RET_OK) {
            os_printf("[BRTC_AGENT] AUADC bind failed: %d\r\n", ret);
            return BRTC_AGENT_ERR_AUDIO;
        }
        g_brtc_audio.mic_bound = true;
    }
    if (!g_brtc_audio.tts_bound) {
        ret = msi_add_output(g_brtc_audio.tts_msi, NULL,
                             g_brtc_audio.pcmdec_msi, NULL);
        if (ret != RET_OK) {
            (void)auadc_msi_del_output(AUSYS_AUAD,
                                       BRTC_AGENT_MIC_MSI_NAME);
            g_brtc_audio.mic_bound = false;
            os_printf("[BRTC_AGENT] pcmdec bind failed: %d\r\n", ret);
            return BRTC_AGENT_ERR_AUDIO;
        }
        g_brtc_audio.tts_bound = true;
    }

    g_brtc_audio.active = true;
    g_brtc_audio.tts_pcm_seen = false;
    os_printf("[BRTC_AGENT] audio attached: S_AUADC -> %s, %s -> pcmdec -> mixer\r\n",
              BRTC_AGENT_MIC_MSI_NAME, BRTC_AGENT_TTS_MSI_NAME);
    return BRTC_AGENT_OK;
}

void brtc_agent_audio_stop(void)
{
    g_brtc_audio.active = false;
    if (g_brtc_audio.mic_bound) {
        (void)auadc_msi_del_output(AUSYS_AUAD, BRTC_AGENT_MIC_MSI_NAME);
        g_brtc_audio.mic_bound = false;
    }
    if (g_brtc_audio.tts_bound) {
        (void)msi_del_output(g_brtc_audio.tts_msi, NULL,
                             g_brtc_audio.pcmdec_msi, NULL);
        g_brtc_audio.tts_bound = false;
    }
    if (g_brtc_audio.pcmdec_msi) {
        msi_do_cmd(g_brtc_audio.pcmdec_msi, MSI_CMD_STOP, 0, 0);
        /* 解除 pcmdec->mixer 连接并释放两个动态通道引用 */
        if (g_brtc_audio.mixer_msi) {
            (void)msi_del_output(g_brtc_audio.pcmdec_msi, NULL,
                                 g_brtc_audio.mixer_msi, NULL);
            msi_put(g_brtc_audio.mixer_msi);
            g_brtc_audio.mixer_msi = NULL;
        }
        msi_put(g_brtc_audio.pcmdec_msi);
        g_brtc_audio.pcmdec_msi = NULL;
    }
}

void brtc_agent_audio_play_pcm(const uint8_t *data, size_t len)
{
    struct framebuff *frame;
    size_t chunk;

    if (!data || len == 0U || !g_brtc_audio.active ||
        !g_brtc_audio.tts_msi || !g_brtc_audio.tts_bound) {
        return;
    }

    if (!g_brtc_audio.tts_pcm_seen) {
        g_brtc_audio.tts_pcm_seen = true;
        os_printf("[BRTC_AGENT] first TTS PCM len=%u\r\n", (unsigned)len);
    }

    while (len > 0U) {
        chunk = len > BRTC_AGENT_TTS_CHUNK_BYTES
                    ? BRTC_AGENT_TTS_CHUNK_BYTES
                    : len;
        /* msi_alloc_fb 统一分配帧+数据,消费端经 fb_put/FREE_FB 归还 */
        frame = msi_alloc_fb(g_brtc_audio.tts_msi, NULL, NULL,
                             (uint32)chunk, 0, 0);
        if (!frame) {
            os_printf("[BRTC_AGENT] drop TTS PCM: no frame memory\r\n");
            return;
        }
        os_memcpy(frame->data, data, chunk);
        frame->len = (uint32)chunk;
        frame->time = os_jiffies();
        frame->mtype = MEDIA_DATA_AUDIO;
        frame->stype = AUDIO_CODEC_PCM_S16LE;
        frame->codec_info = &g_brtc_audio.pcm_info;
        /* msi_output_fb always consumes the caller's frame reference, including
         * the queue-full/no-output path (care=0: silent release). */
        if (msi_output_fb(g_brtc_audio.tts_msi, frame, 0) <= 0) {
            os_printf("[BRTC_AGENT] TTS PCM output failed\r\n");
        }
        data += chunk;
        len -= chunk;
    }
}
