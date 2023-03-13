// 

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/buf-size/buf-size.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/options/options.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

#ifdef __MOD_DEVICES__
#define MAX_BUFFER_SIZE 256
#define SEMAPHORE_PSHARED 0
#else
#define MAX_BUFFER_SIZE 8192
#define SEMAPHORE_PSHARED 1
#endif

//=====================================================================================================================

typedef struct {
    const float* left;
    const float* right;
    const float* enabled;
    float* status;
} Sink;

typedef struct {
    float* left;
    float* right;
    const float* enabled;
    float* status;
} Source;

typedef struct {
    float* buffer_left;
    float* buffer_right;
    atomic_uint counter_sink;
    atomic_uint counter_source;
    pthread_mutex_t mutex;
    sem_t sem;
} Portal;

//=====================================================================================================================

static Portal* portal_init(const int prioceiling)
{
    Portal* const portal = (Portal*)malloc(sizeof(Portal));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#ifndef __APPLE__
    if (prioceiling != 0)
    {
        if (pthread_mutexattr_setprioceiling(&attr, prioceiling + 1) != 0)
            goto error;
        if (pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_PROTECT) != 0)
            goto error;
    }
    else
#endif
    {
        if (pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT) != 0)
            goto error;
    }
    pthread_mutex_init(&portal->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    atomic_init(&portal->counter_sink, 0);
    atomic_init(&portal->counter_source, 0);

    portal->buffer_left = calloc(MAX_BUFFER_SIZE, sizeof(float));
    portal->buffer_right = calloc(MAX_BUFFER_SIZE, sizeof(float));

    // NOTE initial value 1 (lock by default)
    sem_init(&portal->sem, SEMAPHORE_PSHARED, 1);
    return portal;

#ifndef __APPLE__
error:
    pthread_mutexattr_destroy(&attr);
    free(portal);
    return NULL;
#endif
}

static void portal_destroy(Portal* const portal)
{
    sem_destroy(&portal->sem);
    pthread_mutex_destroy(&portal->mutex);
    free(portal->buffer_left);
    free(portal->buffer_right);
    free(portal);
}

//=====================================================================================================================

static int get_lv2_prio(LV2_Options_Option* const options, LV2_URID_Map* const uridMap)
{
    if (options == NULL || uridMap == NULL)
        return 0;

    const LV2_URID uridAtomInt = uridMap->map(uridMap->handle, LV2_ATOM__Int);
    const LV2_URID uridPriority = uridMap->map(uridMap->handle, "http://ardour.org/lv2/threads/#schedPriority");

    for (int i=0; options[i].key != 0 && options[i].subject != 0; ++i)
    {
        // find our wanted key
        if (options[i].key != uridPriority)
            continue;

        // everything must match, otherwise invalid
        if (options[i].context != LV2_OPTIONS_INSTANCE)
            break;
        if (options[i].subject != 0)
            break;
        if (options[i].size != sizeof(int32_t))
            break;
        if (options[i].type != uridAtomInt)
            break;

        const int32_t* const valueptr = options[i].value;
        return *valueptr;
    }

    return 0;
}

//=====================================================================================================================

static Portal* g_portal = NULL;
static bool g_sink_loaded = false;
static bool g_source_loaded = false;

static LV2_Handle instantiate_sink(const LV2_Descriptor* const descriptor,
                                   const double samplerate,
                                   const char* const bundle_path,
                                   const LV2_Feature* const* const features)
{
    bool fixedBlockLength = false;
    LV2_Log_Log* log;
    LV2_Options_Option* options;
    LV2_URID_Map* uridMap;

    for (int i=0; features[i] != NULL; ++i)
    {
        if (!strcmp(features[i]->URI, LV2_BUF_SIZE__fixedBlockLength))
            fixedBlockLength = true;
        else if (!strcmp(features[i]->URI, LV2_LOG__log))
            log = features[i]->data;
        else if (!strcmp(features[i]->URI, LV2_OPTIONS__options))
            options = features[i]->data;
        else if (!strcmp(features[i]->URI, LV2_URID__map))
            uridMap = features[i]->data;
    }

    LV2_Log_Logger logger;
    lv2_log_logger_init(&logger, uridMap, log);

    if (!fixedBlockLength)
    {
        lv2_log_error(&logger, "Host does not support lv2plug.in/ns/ext/buf-size#fixedBlockLength, cannot continue");
        return NULL;
    }

    if (g_sink_loaded)
    {
        lv2_log_error(&logger, "A portal sink is already loaded");
        return NULL;
    }

    if (!g_source_loaded)
    {
        lv2_log_note(&logger, "Creating global portal instance");
        const int prio = get_lv2_prio(options, uridMap);
        if (prio == 0)
            lv2_log_note(&logger, "NOTICE: Host does not support ardour.org/lv2/threads/#schedPriority");
        g_portal = portal_init(prio);
    }

    if (g_portal == NULL)
    {
        lv2_log_error(&logger, "Failed to fetch global portal instance");
        return NULL;
    }

    g_sink_loaded = true;
    return malloc(sizeof(Sink));
}

static LV2_Handle instantiate_source(const LV2_Descriptor* const descriptor,
                                     const double samplerate,
                                     const char* const bundle_path,
                                     const LV2_Feature* const* const features)
{
    bool fixedBlockLength = false;
    LV2_Log_Log* log;
    LV2_Options_Option* options;
    LV2_URID_Map* uridMap;

    for (int i=0; features[i] != NULL; ++i)
    {
        if (!strcmp(features[i]->URI, LV2_BUF_SIZE__fixedBlockLength))
            fixedBlockLength = true;
        else if (!strcmp(features[i]->URI, LV2_LOG__log))
            log = features[i]->data;
        else if (!strcmp(features[i]->URI, LV2_OPTIONS__options))
            options = features[i]->data;
        else if (!strcmp(features[i]->URI, LV2_URID__map))
            uridMap = features[i]->data;
    }

    LV2_Log_Logger logger;
    lv2_log_logger_init(&logger, uridMap, log);

    if (!fixedBlockLength)
    {
        lv2_log_error(&logger, "Host does not support lv2plug.in/ns/ext/buf-size#fixedBlockLength, cannot continue");
        return NULL;
    }

    if (g_source_loaded)
    {
        lv2_log_error(&logger, "A portal source is already loaded");
        return NULL;
    }

    if (!g_sink_loaded)
    {
        lv2_log_note(&logger, "Creating global portal instance");
        const int prio = get_lv2_prio(options, uridMap);
        if (prio == 0)
            lv2_log_note(&logger, "NOTICE: Host does not support ardour.org/lv2/threads/#schedPriority");
        g_portal = portal_init(prio);
    }

    if (g_portal == NULL)
    {
        lv2_log_error(&logger, "Failed to fetch global portal instance");
        return NULL;
    }

    g_source_loaded = true;
    return malloc(sizeof(Source));
}

//=====================================================================================================================

static void cleanup_sink(const LV2_Handle instance)
{
    free(instance);
    g_sink_loaded = false;

    if (!g_source_loaded)
    {
        portal_destroy(g_portal);
        g_portal = NULL;
    }
}

static void cleanup_source(const LV2_Handle instance)
{
    free(instance);
    g_source_loaded = false;

    if (!g_sink_loaded)
    {
        portal_destroy(g_portal);
        g_portal = NULL;
    }
}

//=====================================================================================================================

static void run_sink(const LV2_Handle instance, const uint32_t samples)
{
    Sink* const sink = instance;

    if (samples > MAX_BUFFER_SIZE)
    {
        *sink->status = 2.f;
        return;
    }

    Portal* const portal = g_portal;

    const uint counter_sink = atomic_load(&portal->counter_sink);
    const uint counter_source = atomic_load(&portal->counter_source);

    // if there is no source active yet, do nothing
    if (counter_source == 0)
    {
        *sink->status = 0.f;
        return;
    }

    const bool processing = counter_sink != 1 && counter_source != 1;
    *sink->status = processing ? 1.f : 0.f;

    if (samples == 0)
        return;

    // if sink and source are processing, make sure source side always goes before us
    if (processing)
        sem_wait(&portal->sem);

    pthread_mutex_lock(&portal->mutex);
    memcpy(portal->buffer_left, sink->left, sizeof(float)*samples);
    memcpy(portal->buffer_right, sink->right, sizeof(float)*samples);
    pthread_mutex_unlock(&portal->mutex);

    // increment sink counter
    atomic_fetch_add(&portal->counter_sink, 1);
}

static void run_source(const LV2_Handle instance, const uint32_t samples)
{
    Source* const source = instance;

    if (samples > MAX_BUFFER_SIZE)
    {
        *source->status = 2.f;
        goto clear;
    }

    Portal* const portal = g_portal;

    // if there is no sink processing yet, do nothing
    if (atomic_load(&portal->counter_sink) <= 1)
    {
        *source->status = 0.f;
        goto clear;
    }

    *source->status = 1.f;

    if (samples == 0)
        return;

    pthread_mutex_lock(&portal->mutex);
    memcpy(source->left, portal->buffer_left, sizeof(float)*samples);
    memcpy(source->right, portal->buffer_right, sizeof(float)*samples);
    pthread_mutex_unlock(&portal->mutex);

    // increment source counter
    atomic_fetch_add(&portal->counter_source, 1);

    // notify sink so it goes after us
    sem_post(&portal->sem);
    return;

clear:
    memset(source->left, 0, sizeof(float)*samples);
    memset(source->right, 0, sizeof(float)*samples);
}

//=====================================================================================================================

static void activate_sink(const LV2_Handle instance)
{
    Portal* const portal = g_portal;
    atomic_store(&portal->counter_sink, 1);
}

static void activate_source(const LV2_Handle instance)
{
    Portal* const portal = g_portal;
    atomic_store(&portal->counter_source, 1);
}

//=====================================================================================================================

static void deactivate_sink(const LV2_Handle instance)
{
    Portal* const portal = g_portal;
    atomic_store(&portal->counter_sink, 0);
}

static void deactivate_source(const LV2_Handle instance)
{
    Portal* const portal = g_portal;

    const int old_counter_source = atomic_exchange(&portal->counter_source, 0);

    // handle case of sink waiting for source
    if (atomic_load(&portal->counter_sink) != 1 && old_counter_source != 1)
        sem_post(&portal->sem);
}

//=====================================================================================================================

static void connect_port(const LV2_Handle instance, const uint32_t port, void* const data)
{
    Sink* const shared = instance;

    switch (port)
    {
    case 0:
        shared->left = data;
        break;
    case 1:
        shared->right = data;
        break;
    case 2:
        shared->enabled = data;
        break;
    case 3:
        shared->status = data;
        break;
    }
}

//=====================================================================================================================

static const void* extension_data(const char* const uri)
{
    return NULL;
}

//=====================================================================================================================

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(const uint32_t index)
{
    static const LV2_Descriptor descriptor_sink = {
        "https://falktx.com/plugins/portal#sink",
        instantiate_sink,
        connect_port,
        activate_sink,
        run_sink,
        deactivate_sink,
        cleanup_sink,
        extension_data
    };

    static const LV2_Descriptor descriptor_source = {
        "https://falktx.com/plugins/portal#source",
        instantiate_source,
        connect_port,
        activate_source,
        run_source,
        deactivate_source,
        cleanup_source,
        extension_data
    };

    switch (index)
    {
    case 0: return &descriptor_sink;
    case 1: return &descriptor_source;
    default: return NULL;
    }
}

//=====================================================================================================================

