#include "brtc_agent_internal.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <stdlib.h>

/* HTTP传输适配(本SDK无curl):百度平台URL为明文http(旁系同款
 * http://host:port/api/v1/aiagent),用lwip BSD socket实现同步POST。
 * 仅支持http://;JSON解析在brt层手写(os_strstr),不依赖本文件。 */

#define BRTC_AGENT_HTTP_RESPONSE_MAX 1024U
#define BRTC_AGENT_HTTP_PAYLOAD_MAX  1536U
#define BRTC_AGENT_HTTP_RECV_TOTAL   4096U

typedef struct brtc_agent_http_response {
    char *data;
    size_t capacity;
    size_t size;
    long status_code;
    int socket_error;
} brtc_agent_http_response_t;

static size_t brtc_agent_http_write(const char *contents, size_t bytes,
                                    void *user_data)
{
    brtc_agent_http_response_t *response =
        (brtc_agent_http_response_t *)user_data;

    if (!response || !contents || bytes == 0U) {
        return 0U;
    }
    if (response->size + bytes + 1U > response->capacity) {
        return 0U;
    }
    os_memcpy(response->data + response->size, contents, bytes);
    response->size += bytes;
    response->data[response->size] = '\0';
    return bytes;
}

/* 解析 http://host[:port]/path;返回0成功。host/port/path 均为输出缓冲 */
static int brtc_agent_http_parse_url(const char *url, char *host,
                                     size_t host_size, char *port_text,
                                     size_t port_size, const char **path)
{
    const char *cursor;
    const char *host_begin;
    const char *host_end;
    size_t length;

    if (!url || !host || !port_text || !path) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    if (os_strncmp(url, "http://", 7U) != 0) {
        os_printf("[BRTC_AGENT] only http:// supported: %s\r\n", url);
        return BRTC_AGENT_ERR_CONFIG;
    }
    host_begin = url + 7U;
    cursor = host_begin;
    while (*cursor && *cursor != '/' && *cursor != ':') {
        cursor++;
    }
    host_end = cursor;
    length = (size_t)(host_end - host_begin);
    if (length == 0U || length >= host_size) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    os_memcpy(host, host_begin, length);
    host[length] = '\0';

    if (*cursor == ':') {
        cursor++;
        const char *port_begin = cursor;
        while (*cursor >= '0' && *cursor <= '9') {
            cursor++;
        }
        length = (size_t)(cursor - port_begin);
        if (length == 0U || length >= port_size) {
            return BRTC_AGENT_ERR_CONFIG;
        }
        os_memcpy(port_text, port_begin, length);
        port_text[length] = '\0';
    } else {
        os_memcpy(port_text, "80", 3U);
    }
    if (*cursor != '/') {
        cursor = "/";
    }
    *path = cursor;
    return BRTC_AGENT_OK;
}

static int brtc_agent_http_post(const char *url, const char *payload,
                                uint32_t timeout_ms, char *response_data,
                                size_t response_capacity)
{
    brtc_agent_http_response_t response;
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    char host[96];
    char port_text[8];
    char request_head[512];
    const char *path;
    int written;
    int sock = -1;
    int ret_code;
    size_t header_len;
    size_t payload_len;
    size_t sent;
    struct timeval recv_timeout;
    int ret;

    if (!url || !url[0] || !payload || !response_data ||
        response_capacity < 2U) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }

    os_memset(&response, 0, sizeof(response));
    response.data = response_data;
    response.capacity = response_capacity;
    response_data[0] = '\0';

    ret = brtc_agent_http_parse_url(url, host, sizeof(host), port_text,
                                    sizeof(port_text), &path);
    if (ret != BRTC_AGENT_OK) {
        return ret;
    }

    os_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    ret_code = getaddrinfo(host, port_text, &hints, &result);
    if (ret_code != 0 || !result) {
        os_printf("[BRTC_AGENT] dns fail %s:%s\r\n", host, port_text);
        return BRTC_AGENT_ERR_HTTP;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return BRTC_AGENT_ERR_HTTP;
    }

    recv_timeout.tv_sec = timeout_ms / 1000U;
    recv_timeout.tv_usec = (timeout_ms % 1000U) * 1000U;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout,
               sizeof(recv_timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &recv_timeout,
               sizeof(recv_timeout));

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        os_printf("[BRTC_AGENT] connect fail %s:%s\r\n", host, port_text);
        close(sock);
        freeaddrinfo(result);
        return BRTC_AGENT_ERR_HTTP;
    }
    freeaddrinfo(result);
    result = NULL;

    payload_len = os_strlen(payload);
    written = os_snprintf(request_head, sizeof(request_head),
                          "POST %s HTTP/1.1\r\n"
                          "Host: %s\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %u\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          path, host, (unsigned)payload_len);
    if (written < 0 || (size_t)written >= sizeof(request_head)) {
        close(sock);
        return BRTC_AGENT_ERR_CONFIG;
    }
    header_len = (size_t)written;

    /* 头与体分开计数:两段长度偶合相等时也不能漏判发送失败 */
    sent = 0U;
    while (sent < header_len) {
        ret_code = send(sock, request_head + sent, header_len - sent, 0);
        if (ret_code <= 0) {
            break;
        }
        sent += (size_t)ret_code;
    }
    if (sent == header_len) {
        size_t body_sent = 0U;
        while (body_sent < payload_len) {
            ret_code = send(sock, payload + body_sent, payload_len - body_sent, 0);
            if (ret_code <= 0) {
                break;
            }
            body_sent += (size_t)ret_code;
        }
        if (body_sent != payload_len) {
            sent = body_sent + 1U; /* 标记失败 */
        }
    }
    if (sent != header_len) {
        os_printf("[BRTC_AGENT] send incomplete\r\n");
        close(sock);
        return BRTC_AGENT_ERR_HTTP;
    }

    /* 收完整响应到栈缓冲,再拆 status line / body */
    {
        char raw[BRTC_AGENT_HTTP_RECV_TOTAL];
        size_t raw_size = 0U;
        const char *header_end;
        const char *body;
        size_t body_len;

        for (;;) {
            ret_code = recv(sock, raw + raw_size,
                            sizeof(raw) - 1U - raw_size, 0);
            if (ret_code <= 0) {
                break;
            }
            raw_size += (size_t)ret_code;
            if (raw_size >= sizeof(raw) - 1U) {
                break;
            }
        }
        close(sock);
        raw[raw_size] = '\0';

        if (raw_size < 12U || os_strncmp(raw, "HTTP/", 5U) != 0) {
            os_printf("[BRTC_AGENT] bad http response\r\n");
            return BRTC_AGENT_ERR_HTTP;
        }
        response.status_code = (long)os_atoi(raw + 9U);
        header_end = os_strstr(raw, "\r\n\r\n");
        if (!header_end) {
            return BRTC_AGENT_ERR_HTTP;
        }
        body = header_end + 4U;
        body_len = raw_size - (size_t)(body - raw);

        /* 百度平台响应为 chunked 编码(<hex size>\r\n<data>\r\n...
         * <0>\r\n):按块解包成纯净JSON,否则引擎JSON解析失败→401。
         * 判据:首行是十六进制长度(且非数字JSON开头)时按chunked解 */
        if (body_len > 2U && os_strncmp(body, "{\"", 2U) != 0) {
            size_t in_pos = 0U;
            size_t out_pos = 0U;
            for (;;) {
                size_t line_end = in_pos;
                unsigned long chunk_size;

                while (line_end + 1U < body_len &&
                       !(body[line_end] == '\r' &&
                         body[line_end + 1U] == '\n')) {
                    line_end++;
                }
                if (line_end + 1U >= body_len) {
                    break;
                }
                chunk_size = (unsigned long)strtoul(body + in_pos, NULL, 16);
                in_pos = line_end + 2U;
                if (chunk_size == 0UL) {
                    break;
                }
                if (in_pos + chunk_size > body_len) {
                    chunk_size = body_len - in_pos;
                }
                if (out_pos + chunk_size >= response.capacity) {
                    chunk_size = response.capacity - 1U - out_pos;
                }
                os_memcpy(response.data + out_pos, body + in_pos,
                          chunk_size);
                out_pos += chunk_size;
                in_pos += chunk_size;
                if (in_pos + 2U <= body_len) {
                    in_pos += 2U;
                }
                if (in_pos >= body_len || out_pos + 1U >= response.capacity) {
                    break;
                }
            }
            response.data[out_pos] = '\0';
            response.size = out_pos;
        } else {
            brtc_agent_http_write(body, body_len, &response);
        }
    }

    if (response.status_code >= 200L && response.status_code < 300L) {
        ret_code = BRTC_AGENT_OK;
    } else {
        os_printf("[BRTC_AGENT] HTTP status %ld for %s\r\n",
                  response.status_code, url);
        ret_code = BRTC_AGENT_ERR_HTTP;
    }
    return ret_code;
}

static int brtc_agent_extract_instance_id(const char *json, char *output,
                                          size_t output_size)
{
    const char *cursor;
    const char *end;
    size_t length;

    if (!json || !output || output_size < 2U) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    output[0] = '\0';
    cursor = os_strstr(json, "\"ai_agent_instance_id\"");
    if (!cursor) {
        return BRTC_AGENT_ERR_HTTP;
    }
    cursor = os_strchr(cursor, ':');
    if (!cursor) {
        return BRTC_AGENT_ERR_HTTP;
    }
    cursor++;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\"') {
        cursor++;
    }
    end = cursor;
    while ((*end >= '0' && *end <= '9') || *end == '-') {
        end++;
    }
    length = (size_t)(end - cursor);
    if (length == 0U || length >= output_size) {
        return BRTC_AGENT_ERR_HTTP;
    }
    os_memcpy(output, cursor, length);
    output[length] = '\0';
    return BRTC_AGENT_OK;
}

static int brtc_agent_format_screen_config(
    const brtc_agent_runtime_config_t *config, char *output,
    size_t output_size)
{
    int written;

    if (!config || !output || output_size == 0U) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    output[0] = '\0';
    if (config->screen_width == 0U && config->screen_height == 0U) {
        return BRTC_AGENT_OK;
    }
    if (config->screen_width == 0U || config->screen_height == 0U) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    written = os_snprintf(
        output, output_size,
        "\\\"screen_width\\\":\\\"%u\\\","
        "\\\"screen_height\\\":\\\"%u\\\",",
        (unsigned)config->screen_width, (unsigned)config->screen_height);
    if (written < 0 || (size_t)written >= output_size) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    return BRTC_AGENT_OK;
}

int brtc_agent_http_create(const brtc_agent_runtime_config_t *config,
                           const char *device_id, char *remote_params,
                           size_t remote_params_size, char *instance_id,
                           size_t instance_id_size)
{
    char payload[BRTC_AGENT_HTTP_PAYLOAD_MAX];
    char screen_config[96];
    const char *visual_flag;
    int written;
    int ret;

    if (!config || !device_id || !remote_params || !instance_id) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    ret = brtc_agent_format_screen_config(config, screen_config,
                                          sizeof(screen_config));
    if (ret != BRTC_AGENT_OK) {
        return ret;
    }

    visual_flag = config->enable_video ? "true" : "false";

    /* Keep the server session voice-only unless the application explicitly
     * enables video; image generation is enabled later by a session event. */
    written = os_snprintf(
        payload, sizeof(payload),
        "{\"app_id\":\"%s\",\"appid\":\"%s\","
        "\"license\":\"%s\",\"license_key\":\"%s\","
        "\"device_id\":\"%s\",\"user_id\":\"%s\","
        "\"config\":\"{\\\"llm\\\":\\\"%s\\\","
        "\\\"llm_token\\\":\\\"no\\\","
        "\\\"emotionRecognitionCfg\\\":{\\\"enable\\\":true},"
        "\\\"enable_visual\\\":\\\"%s\\\","
        "%s"
        "\\\"media_generate_mode\\\":\\\"false\\\","
        "\\\"remote_music_player\\\":\\\"false\\\","
        "\\\"rtc_ac\\\":\\\"pcmu\\\","
        "\\\"lang\\\":\\\"%s\\\","
        "\\\"user_id\\\":\\\"%s\\\"}\","
        "\"quick_start\":true}",
        config->app_id, config->app_id, config->license_key,
        config->license_key, device_id, device_id, config->llm,
        visual_flag, screen_config, config->language, device_id);
    if (written < 0 || (size_t)written >= sizeof(payload)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    if (config->screen_width != 0U) {
        os_printf("[BRTC_AGENT] create screen=%ux%u\r\n",
                  (unsigned)config->screen_width,
                  (unsigned)config->screen_height);
    }

    ret = brtc_agent_http_post(config->create_url, payload,
                               config->http_timeout_ms, remote_params,
                               remote_params_size);
    if (ret != BRTC_AGENT_OK) {
        return ret;
    }
    /* 打印服务器响应,便于诊断鉴权类失败(401等) */
    os_printf("[BRTC_AGENT] create resp: %.256s\r\n", remote_params);
    ret = brtc_agent_extract_instance_id(remote_params, instance_id,
                                         instance_id_size);
    if (ret != BRTC_AGENT_OK) {
        os_printf("[BRTC_AGENT] create response has no exact instance id\r\n");
        /* The full response is still valid remote_params for the SDK. */
        instance_id[0] = '\0';
    }
    return BRTC_AGENT_OK;
}

int brtc_agent_http_stop(const brtc_agent_runtime_config_t *config,
                         const char *device_id, const char *instance_id)
{
    char payload[768];
    char response[BRTC_AGENT_HTTP_RESPONSE_MAX];
    int written;

    if (!config || !device_id || !instance_id || !instance_id[0]) {
        return BRTC_AGENT_ERR_INVALID_ARG;
    }
    written = os_snprintf(
        payload, sizeof(payload),
        "{\"app_id\":\"%s\",\"appid\":\"%s\","
        "\"license\":\"%s\",\"license_key\":\"%s\","
        "\"device_id\":\"%s\",\"user_id\":\"%s\","
        "\"ai_agent_instance_id\":\"%s\"}",
        config->app_id, config->app_id, config->license_key,
        config->license_key, device_id, device_id, instance_id);
    if (written < 0 || (size_t)written >= sizeof(payload)) {
        return BRTC_AGENT_ERR_CONFIG;
    }
    return brtc_agent_http_post(config->stop_url, payload,
                                config->http_timeout_ms, response,
                                sizeof(response));
}
