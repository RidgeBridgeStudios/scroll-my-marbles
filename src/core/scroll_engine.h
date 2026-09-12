#ifndef SCROLL_ENGINE_H
#define SCROLL_ENGINE_H

#include "../config/config.h"
#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef struct {
    AppConfig config;
    bool button_pressed;
    bool scrolling_active;
    bool btn_side_down;
    bool btn_extra_down;
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
