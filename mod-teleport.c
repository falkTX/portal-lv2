/*
 * MOD Teleport
*/

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

/**********************************************************************************************************************************************************/

typedef struct {
    const float* left;
    const float* right;
    const float* enabled;
} Sink;

typedef struct {
    float* left;
    float* right;
    const float* enabled;
} Source;

/**********************************************************************************************************************************************************/

static bool global_init = false;
static float* global_left;
static float* global_right;
static pthread_mutex_t global_mutex;

// single instance for now, for testing
static bool has_sink = false;
static bool has_source = false;

static void global_init_if_needed()
{
    if (global_init)
        return;

    global_init = true;
    global_left = calloc(256, sizeof(float));
    global_right = calloc(256, sizeof(float));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&global_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

/**********************************************************************************************************************************************************/
static LV2_Handle instantiate_sink(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
    if (has_sink)
        return NULL;

    has_sink = true;
    Sink* self = (Sink*)malloc(sizeof(Sink));
    global_init_if_needed();
    return self;
}

static LV2_Handle instantiate_source(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features)
{
    if (has_source)
        return NULL;

    has_source = true;
    Source* self = (Source*)malloc(sizeof(Source));
    global_init_if_needed();
    return self;
}

/**********************************************************************************************************************************************************/
static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Sink* self = (Sink*)instance;

    switch (port)
    {
    case 0:
        self->left = (const float*)data;
        break;
    case 1:
        self->right = (const float*)data;
        break;
    case 2:
        self->enabled = (const float*)data;
        break;
    }
}

/**********************************************************************************************************************************************************/
void run_sink(LV2_Handle instance, uint32_t samples)
{
    Sink* self = (Sink*)instance;

    pthread_mutex_lock(&global_mutex);
    memcpy(global_left, self->left, sizeof(float)*samples);
    memcpy(global_right, self->right, sizeof(float)*samples);
    pthread_mutex_unlock(&global_mutex);
}

void run_source(LV2_Handle instance, uint32_t samples)
{
    Source* self = (Source*)instance;

    pthread_mutex_lock(&global_mutex);
    memcpy(self->left, global_left, sizeof(float)*samples);
    memcpy(self->right, global_right, sizeof(float)*samples);
    pthread_mutex_unlock(&global_mutex);
}

/**********************************************************************************************************************************************************/
void activate(LV2_Handle instance)
{
    // TODO: include the activate function code here
}

/**********************************************************************************************************************************************************/
void deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}

/**********************************************************************************************************************************************************/
void cleanup_sink(LV2_Handle instance)
{
    has_sink = false;
    free(instance);
}

void cleanup_source(LV2_Handle instance)
{
    has_source = false;
    free(instance);
}

/**********************************************************************************************************************************************************/
const void* extension_data(const char* uri)
{
    return NULL;
}

/**********************************************************************************************************************************************************/
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    static const LV2_Descriptor descriptor_sink = {
        "https://mod.audio/plugins/teleport#sink",
        instantiate_sink,
        connect_port,
        activate,
        run_sink,
        deactivate,
        cleanup_sink,
        extension_data
    };

    static const LV2_Descriptor descriptor_source = {
        "https://mod.audio/plugins/teleport#source",
        instantiate_source,
        connect_port,
        activate,
        run_source,
        deactivate,
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

/**********************************************************************************************************************************************************/
