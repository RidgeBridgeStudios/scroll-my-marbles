#ifndef SCROLL_ENGINE_H
#define SCROLL_ENGINE_H

#include "../config/config.h"
#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>

typedef struct DeviceContext DeviceContext;
void device_emit_event(DeviceContext *ctx, uint16_t type, uint16_t code, int32_t value);
void device_emit_syn(DeviceContext *ctx);

typedef struct {
    AppConfig config;
    bool button_pressed;
    bool scrolling_active;
    int accum_x;
    int accum_y;
    int effective_sens_x;
    int effective_sens_y;
    int effective_delta;
} ScrollEngine;

void scroll_engine_init(ScrollEngine *engine, const AppConfig *cfg);
void scroll_engine_update_config(ScrollEngine *engine, const AppConfig *cfg);
void scroll_engine_process_event(ScrollEngine *engine, DeviceContext *ctx, const struct input_event *ev);

#endif /* SCROLL_ENGINE_H */
