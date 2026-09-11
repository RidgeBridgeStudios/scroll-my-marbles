#include "scroll_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CLAMP_MIN(val, min) (((val) < (min)) ? (min) : (val))

void scroll_engine_init(ScrollEngine *engine, const AppConfig *cfg) {
    if (!engine) return;
    memset(engine, 0, sizeof(*engine));
    scroll_engine_update_config(engine, cfg);
}

void scroll_engine_update_config(ScrollEngine *engine, const AppConfig *cfg) {
    if (!engine || !cfg) return;
    engine->config = *cfg;

    if (cfg->smooth_scroll) {
        engine->effective_sens_x = CLAMP_MIN(cfg->h_sensitivity / CONFIG_SMOOTH_FACTOR, 1);
        engine->effective_sens_y = CLAMP_MIN(cfg->v_sensitivity / CONFIG_SMOOTH_FACTOR, 1);
        engine->effective_delta = CONFIG_STANDARD_WHEEL_DELTA / CONFIG_SMOOTH_FACTOR;
    } else {
        engine->effective_sens_x = CLAMP_MIN(cfg->h_sensitivity, 1);
        engine->effective_sens_y = CLAMP_MIN(cfg->v_sensitivity, 1);
        engine->effective_delta = 1;
    }
}

static bool is_scroll_modifier(const ScrollEngine *engine, uint16_t code) {
    if (code == engine->config.scroll_button) return true;

    if (code == BTN_SIDE && engine->config.btn_side_action == BUTTON_ACTION_SCROLL_MODIFIER)
        return true;
    if (code == BTN_EXTRA && engine->config.btn_extra_action == BUTTON_ACTION_SCROLL_MODIFIER)
        return true;
    if (code == BTN_MIDDLE && engine->config.btn_middle_action == BUTTON_ACTION_SCROLL_MODIFIER)
        return true;
    if (code == BTN_RIGHT && engine->config.btn_right_action == BUTTON_ACTION_SCROLL_MODIFIER)
        return true;

    return false;
}

static ButtonAction get_button_action(const ScrollEngine *engine, uint16_t code) {
    if (code == BTN_SIDE) return engine->config.btn_side_action;
    if (code == BTN_EXTRA) return engine->config.btn_extra_action;
    if (code == BTN_MIDDLE) return engine->config.btn_middle_action;
    if (code == BTN_RIGHT) return engine->config.btn_right_action;
    return BUTTON_ACTION_PASSTHROUGH;
}

void scroll_engine_process_event(ScrollEngine *engine, DeviceContext *ctx, const struct input_event *ev) {
    if (!engine || !ctx || !ev) return;

    if (ev->type == EV_KEY) {
        if (is_scroll_modifier(engine, ev->code)) {
            if (ev->value == 1) {
                /* Button pressed down: enter scroll intercept mode */
                engine->button_pressed = true;
                engine->scrolling_active = false;
                engine->accum_x = 0;
                engine->accum_y = 0;
                return;
            } else if (ev->value == 0) {
                /* Button released */
                engine->button_pressed = false;

                if (!engine->scrolling_active && engine->config.emulate_click) {
                    /* Emulate click because no scrolling occurred */
                    uint16_t click_btn = engine->config.emulated_click_button;
                    device_emit_event(ctx, EV_KEY, click_btn, 1);
                    device_emit_syn(ctx);
                    device_emit_event(ctx, EV_KEY, click_btn, 0);
                    device_emit_syn(ctx);
                }
                return;
            } else if (ev->value == 2) {
                /* Ignore autorepeat for modifier */
                return;
            }
        }

        /* Not scroll modifier: check remapping */
        ButtonAction action = get_button_action(engine, ev->code);
        switch (action) {
            case BUTTON_ACTION_DISABLED:
                /* Drop event */
                return;
            case BUTTON_ACTION_LEFT_CLICK:
                device_emit_event(ctx, EV_KEY, BTN_LEFT, ev->value);
                device_emit_syn(ctx);
                return;
            case BUTTON_ACTION_RIGHT_CLICK:
                device_emit_event(ctx, EV_KEY, BTN_RIGHT, ev->value);
                device_emit_syn(ctx);
                return;
            case BUTTON_ACTION_MIDDLE_CLICK:
                device_emit_event(ctx, EV_KEY, BTN_MIDDLE, ev->value);
                device_emit_syn(ctx);
                return;
            case BUTTON_ACTION_BACK:
                device_emit_event(ctx, EV_KEY, BTN_SIDE, ev->value);
                device_emit_syn(ctx);
                return;
            case BUTTON_ACTION_FORWARD:
                device_emit_event(ctx, EV_KEY, BTN_EXTRA, ev->value);
                device_emit_syn(ctx);
                return;
            case BUTTON_ACTION_SCROLL_MODIFIER:
                /* Already handled above */
                return;
            case BUTTON_ACTION_PASSTHROUGH:
            default:
                device_emit_event(ctx, EV_KEY, ev->code, ev->value);
                return;
        }
    } else if (ev->type == EV_REL) {
        if (engine->button_pressed) {
            /* Suppress cursor motion and calculate scrolling */
            if (ev->code == REL_Y) {
                int dy = ev->value;
                engine->accum_y += engine->config.reverse_scroll ? -dy : dy;

                if (abs(engine->accum_y) >= engine->effective_sens_y) {
                    int steps = engine->accum_y / engine->effective_sens_y;
                    engine->accum_y -= steps * engine->effective_sens_y;
                    engine->scrolling_active = true;

                    /* Linux evdev convention:
                     * Moving trackball down (positive dy) -> scroll down (-steps)
                     * Moving trackball up (negative dy) -> scroll up (+steps)
                     */
                    int wheel_val = -steps;
                    device_emit_event(ctx, EV_REL, REL_WHEEL, wheel_val);

#ifdef REL_WHEEL_HI_RES
                    if (engine->config.smooth_scroll) {
                        device_emit_event(ctx, EV_REL, REL_WHEEL_HI_RES, -steps * engine->effective_delta);
                    }
#endif
                    device_emit_syn(ctx);
                }
                return;
            } else if (ev->code == REL_X) {
                int dx = ev->value;
                engine->accum_x += engine->config.reverse_scroll ? -dx : dx;

                if (abs(engine->accum_x) >= engine->effective_sens_x) {
                    int steps = engine->accum_x / engine->effective_sens_x;
                    engine->accum_x -= steps * engine->effective_sens_x;
                    engine->scrolling_active = true;

                    if (!engine->config.smooth_scroll) {
                        /* Cancel out perpendicular movement in detent mode */
                        engine->accum_y = 0;
                    }

                    /* Linux evdev convention:
                     * Moving trackball right (positive dx) -> scroll right (+steps)
                     * Moving trackball left (negative dx) -> scroll left (-steps)
                     */
                    int hwheel_val = steps;
                    device_emit_event(ctx, EV_REL, REL_HWHEEL, hwheel_val);

#ifdef REL_HWHEEL_HI_RES
                    if (engine->config.smooth_scroll) {
                        device_emit_event(ctx, EV_REL, REL_HWHEEL_HI_RES, steps * engine->effective_delta);
                    }
#endif
                    device_emit_syn(ctx);
                }
                return;
            }
            /* Suppress other relative motion while scrolling */
            return;
        } else {
            /* Normal mouse motion */
            device_emit_event(ctx, EV_REL, ev->code, ev->value);
            return;
        }
    } else if (ev->type == EV_SYN) {
        if (engine->button_pressed) {
            /* Handled per emitted event above */
            return;
        } else {
            device_emit_syn(ctx);
            return;
        }
    } else {
        /* Forward any other events untouched */
        device_emit_event(ctx, ev->type, ev->code, ev->value);
    }
}
