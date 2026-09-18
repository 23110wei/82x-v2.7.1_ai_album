#ifndef BRTC_AGENT_H
#define BRTC_AGENT_H

#include "brtc_agent/brtc_agent_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The callback strings are borrowed and valid only for the callback duration. */
int brtc_agent_init(const brtc_agent_config_t *config);
int brtc_agent_start(void);
int brtc_agent_stop(void);
/* Returns IN_PROGRESS when the active Agent is restarting asynchronously. */
int brtc_agent_switch_language(const char *language);
int brtc_agent_get_language(char *language, size_t capacity);
int brtc_agent_ptt_start(void);
int brtc_agent_ptt_stop(void);
int brtc_agent_send_text(const char *text);
int brtc_agent_speak_text(const char *text);
/* Submit one JPEG and a style prompt while the media-generation mode is active. */
int brtc_agent_send_image_generation(const uint8_t *jpeg_data,
                                     size_t jpeg_len,
                                     const char *prompt);
/* Return the active session to the normal voice-chat mode. */
int brtc_agent_exit_image_generation(void);
/* Add persistent context around subsequent user queries. */
int brtc_agent_set_query_enhancement(const char *pre_query,
                                     const char *post_query);
int brtc_agent_clear_query_enhancement(void);
/* Constrain the agent to return a direct translation for subsequent queries. */
int brtc_agent_set_translation_languages(const char *source_language,
                                         const char *target_language);
/* Restore the normal VoiceChat query behavior. */
int brtc_agent_clear_translation(void);
int brtc_agent_interrupt(void);
brtc_agent_state_t brtc_agent_get_state(void);

#ifdef __cplusplus
}
#endif

#endif
