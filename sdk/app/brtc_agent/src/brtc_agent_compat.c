#include "brtc_agent_internal.h"

#include "lwip/netdb.h"
#include "osal/mutex.h"
#include "osal/task.h"

/* The TXW81x BRTC archive was built with TXWSDK_POSIX and therefore imports
 * the POSIX netdb symbol names.  Component lwIP exposes the same ABI as
 * lwip_getaddrinfo/lwip_freeaddrinfo and maps the names only at compile time,
 * which cannot rewrite imports in a prebuilt archive.  Keep these aliases
 * weak so an SDK configuration that already supplies strong POSIX wrappers
 * remains authoritative. */
#ifdef getaddrinfo
#undef getaddrinfo
#endif
#ifdef freeaddrinfo
#undef freeaddrinfo
#endif

__attribute__((weak)) int
getaddrinfo(const char *nodename, const char *servname,
            const struct addrinfo *hints, struct addrinfo **res)
{
    return lwip_getaddrinfo(nodename, servname, hints, res);
}

__attribute__((weak)) void freeaddrinfo(struct addrinfo *ai)
{
    lwip_freeaddrinfo(ai);
}

#define BRTC_AGENT_PSRAM_STACK_THRESHOLD (16U * 1024U)
#define BRTC_AGENT_TRACKED_STACKS        16U
#define BRTC_AGENT_ENGINE_TASK_NAME      "brtc_task"

typedef struct brtc_agent_stack_record {
    void *task;
    void *stack;
    uint32 size;
    bool occupied;
} brtc_agent_stack_record_t;

static os_mutex_t g_stack_lock;
static bool g_stack_lock_ready;
static brtc_agent_stack_record_t
    g_stack_records[BRTC_AGENT_TRACKED_STACKS];

void *__real_os_task_create(const char *name, os_task_func_t func, void *args,
                            uint32 prio, uint32 time, void *stack,
                            uint32 stack_size);
int32 __real_os_task_destroy(void *task);

void *custom_malloc_psram(size_t size)
{
    return os_malloc_psram(size);
}

void custom_free_psram(void *pointer)
{
    os_free_psram(pointer);
}

int32 os_task_set_stacksize(struct os_task *task, uint32 stack_size)
{
    if (!task) {
        return RET_ERR;
    }
    task->stack_size = stack_size;
    return RET_OK;
}

int brtc_agent_compat_init(void)
{
    if (g_stack_lock_ready) {
        return BRTC_AGENT_OK;
    }
    os_memset(g_stack_records, 0, sizeof(g_stack_records));
    if (os_mutex_init(&g_stack_lock) != RET_OK) {
        return BRTC_AGENT_ERR_NO_MEMORY;
    }
    g_stack_lock_ready = true;
    return BRTC_AGENT_OK;
}

static int brtc_agent_track_stack(void *task, void *stack, uint32 stack_size)
{
    uint32 i;

    if (!g_stack_lock_ready) {
        return BRTC_AGENT_ERR_INVALID_STATE;
    }
    os_mutex_lock(&g_stack_lock, osWaitForever);
    for (i = 0U; i < BRTC_AGENT_TRACKED_STACKS; ++i) {
        if (!g_stack_records[i].occupied) {
            g_stack_records[i].task = task;
            g_stack_records[i].stack = stack;
            g_stack_records[i].size = stack_size;
            g_stack_records[i].occupied = true;
            os_mutex_unlock(&g_stack_lock);
            return BRTC_AGENT_OK;
        }
    }
    os_mutex_unlock(&g_stack_lock);
    return BRTC_AGENT_ERR_NO_MEMORY;
}

void *__wrap_os_task_create(const char *name, os_task_func_t func, void *args,
                            uint32 prio, uint32 time, void *stack,
                            uint32 stack_size)
{
    void *owned_stack = NULL;
    void *task;

    /* The inspected vendor archive has one direct os_task_create import:
     * src_main.o creates "brtc_task" with a 96 KiB automatic stack.  Linker
     * wrapping is firmware-wide, so match that task exactly instead of moving
     * unrelated SDK/application task stacks to PSRAM. */
    if (g_stack_lock_ready && !stack && name &&
        os_strcmp(name, BRTC_AGENT_ENGINE_TASK_NAME) == 0 &&
        stack_size >= BRTC_AGENT_PSRAM_STACK_THRESHOLD) {
        owned_stack = os_malloc_psram(stack_size);
        if (!owned_stack) {
            os_printf("[BRTC_AGENT] no PSRAM for task %s stack=%u\r\n",
                      name ? name : "?", (unsigned)stack_size);
            return NULL;
        }
        stack = owned_stack;
    }

    task = __real_os_task_create(name, func, args, prio, time, stack,
                                 stack_size);
    if (!task && owned_stack) {
        os_free_psram(owned_stack);
        return NULL;
    }
    if (task && owned_stack &&
        brtc_agent_track_stack(task, owned_stack, stack_size) !=
            BRTC_AGENT_OK) {
        (void)__real_os_task_destroy(task);
        os_free_psram(owned_stack);
        os_printf("[BRTC_AGENT] PSRAM task stack tracker is full\r\n");
        return NULL;
    }
    if (owned_stack) {
        os_printf("[BRTC_AGENT] task %s uses PSRAM stack=%u\r\n",
                  name ? name : "?", (unsigned)stack_size);
    }
    return task;
}

int32 __wrap_os_task_destroy(void *task)
{
    void *stack = NULL;
    uint32 i;
    bool current = (task == os_task_current());
    int32 ret;

    if (g_stack_lock_ready) {
        os_mutex_lock(&g_stack_lock, osWaitForever);
        for (i = 0U; i < BRTC_AGENT_TRACKED_STACKS; ++i) {
            if (g_stack_records[i].occupied &&
                g_stack_records[i].task == task) {
                if (!current) {
                    stack = g_stack_records[i].stack;
                    os_memset(&g_stack_records[i], 0,
                              sizeof(g_stack_records[i]));
                } else {
                    /* A task cannot free the stack it is currently executing
                     * on. Engine shutdown reclaims it after all SDK tasks exit. */
                    g_stack_records[i].task = NULL;
                }
                break;
            }
        }
        os_mutex_unlock(&g_stack_lock);
    }

    ret = __real_os_task_destroy(task);
    if (stack) {
        os_free_psram(stack);
    }
    return ret;
}

void brtc_agent_compat_reclaim_engine_stacks(void)
{
    void *stacks[BRTC_AGENT_TRACKED_STACKS];
    uint32 count = 0U;
    uint32 i;

    if (!g_stack_lock_ready) {
        return;
    }
    os_memset(stacks, 0, sizeof(stacks));
    os_mutex_lock(&g_stack_lock, osWaitForever);
    for (i = 0U; i < BRTC_AGENT_TRACKED_STACKS; ++i) {
        if (g_stack_records[i].occupied && g_stack_records[i].stack) {
            stacks[count++] = g_stack_records[i].stack;
            os_memset(&g_stack_records[i], 0, sizeof(g_stack_records[i]));
        }
    }
    os_mutex_unlock(&g_stack_lock);

    for (i = 0U; i < count; ++i) {
        os_free_psram(stacks[i]);
    }
    if (count) {
        os_printf("[BRTC_AGENT] reclaimed %u BRTC PSRAM task stacks\r\n",
                  (unsigned)count);
    }
}
