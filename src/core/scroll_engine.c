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

static bool is_scroll_modifier_pressed(const ScrollEngine *e) {
    if (e->config.scroll_mod_mode == SCROLL_MOD_CHORD_BOTH_SIDE_BUTTONS) {
        return e->btn_side_down && e->btn_extra_down;
    } else {
        if (e->config.scroll_button == BTN_SIDE) {
            return e->btn_side_down;
        } else if (e->config.scroll_button == BTN_EXTRA) {
            return e->btn_extra_down;
        }
        return false;
    }
}

static bool should_swallow_button(const ScrollEngine *engine, uint16_t code) {
    if (engine->button_pressed) {
        if (code == BTN_SIDE || code == BTN_EXTRA) return true;
    }
    if (engine->config.scroll_mod_mode == SCROLL_MOD_CHORD_BOTH_SIDE_BUTTONS) {
        if (code == BTN_SIDE || code == BTN_EXTRA) return true;
    } else {
        if (code == engine->config.scroll_button) return true;
        if (code == BTN_SIDE && engine->config.btn_side_action == BUTTON_ACTION_SCROLL_MODIFIER) return true;
        if (code == BTN_EXTRA && engine->config.btn_extra_action == BUTTON_ACTION_SCROLL_MODIFIER) return true;
    }
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
        if (ev->code == BTN_SIDE || ev->code == BTN_EXTRA) {
            bool was_armed = engine->button_pressed;

            if (ev->value == 1) {
                if (ev->code == BTN_SIDE) engine->btn_side_down = true;
                else if (ev->code == BTN_EXTRA) engine->btn_extra_down = true;

                if (is_scroll_modifier_pressed(engine)) {
                    if (!engine->button_pressed) {
                        engine->button_pressed = true;
                        engine->scrolling_active = false;
                        engine->accum_x = 0;
                        engine->accum_y = 0;
                    }
                }

                if (should_swallow_button(engine, ev->code) || engine->button_pressed) {
                    return;
                }
            } else if (ev->value == 0) {
                if (ev->code == BTN_SIDE) engine->btn_side_down = false;
                else if (ev->code == BTN_EXTRA) engine->btn_extra_down = false;

                if (engine->config.scroll_mod_mode == SCROLL_MOD_CHORD_BOTH_SIDE_BUTTONS) {
                    if (engine->button_pressed) {
                        if (!engine->btn_side_down && !engine->btn_extra_down) {
                            if (!engine->scrolling_active && engine->config.emulate_click) {
                                uint16_t click_btn = engine->config.emulated_click_button;
                                device_emit_event(ctx, EV_KEY, click_btn, 1);
                                device_emit_syn(ctx);
                                device_emit_event(ctx, EV_KEY, click_btn, 0);
                                device_emit_syn(ctx);
                            }
                            engine->button_pressed = false;
                            engine->scrolling_active = false;
                        }
                    }
                } else {
                    /* Single button mode */
                    if (engine->button_pressed && !is_scroll_modifier_pressed(engine)) {
                        if (!engine->scrolling_active && engine->config.emulate_click) {
                            uint16_t click_btn = engine->config.emulated_click_button;
                            device_emit_event(ctx, EV_KEY, click_btn, 1);
                            device_emit_syn(ctx);
                            device_emit_event(ctx, EV_KEY, click_btn, 0);
                            device_emit_syn(ctx);
                        }
                        engine->button_pressed = false;
                        engine->scrolling_active = false;
                    }
                }

                if (was_armed || should_swallow_button(engine, ev->code)) {
                    return;
                }
            } else if (ev->value == 2) {
                if (should_swallow_button(engine, ev->code) || engine->button_pressed) {
                    return;
                }
            }
        }

        /* Remapping and pass-through for other buttons or non-swallowed events */
        ButtonAction action = get_button_action(engine, ev->code);
        switch (action) {
            case BUTTON_ACTION_DISABLED:
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
                engine->accum_y += dy;

                if (abs(engine->accum_y) >= engine->effective_sens_y) {
                    int steps = engine->accum_y / engine->effective_sens_y;
                    engine->accum_y -= steps * engine->effective_sens_y;
                    engine->scrolling_active = true;

                    if (!engine->config.smooth_scroll) {
                        engine->accum_x = 0;
                    }

                    int wheel_val = -steps;
                    if (engine->config.reverse_scroll) {
                        wheel_val = -wheel_val;
                    }
                    device_emit_event(ctx, EV_REL, REL_WHEEL, wheel_val);

#ifdef REL_WHEEL_HI_RES
                    if (engine->config.smooth_scroll) {
                        int hi_res_val = -steps * engine->effective_delta;
                        if (engine->config.reverse_scroll) {
                            hi_res_val = -hi_res_val;
                        }
                        device_emit_event(ctx, EV_REL, REL_WHEEL_HI_RES, hi_res_val);
                    }
#endif
                    device_emit_syn(ctx);
                }
                return;
            } else if (ev->code == REL_X) {
                int dx = ev->value;
                engine->accum_x += dx;

                if (abs(engine->accum_x) >= engine->effective_sens_x) {
                    int steps = engine->accum_x / engine->effective_sens_x;
                    engine->accum_x -= steps * engine->effective_sens_x;
                    engine->scrolling_active = true;

                    if (!engine->config.smooth_scroll) {
                        engine->accum_y = 0;
                    }

                    int hwheel_val = steps;
                    if (engine->config.reverse_scroll) {
                        hwheel_val = -hwheel_val;
                    }
                    device_emit_event(ctx, EV_REL, REL_HWHEEL, hwheel_val);

#ifdef REL_HWHEEL_HI_RES
                    if (engine->config.smooth_scroll) {
                        int hi_res_val = steps * engine->effective_delta;
                        if (engine->config.reverse_scroll) {
                            hi_res_val = -hi_res_val;
                        }
                        device_emit_event(ctx, EV_REL, REL_HWHEEL_HI_RES, hi_res_val);
                    }
#endif
                    device_emit_syn(ctx);
                }
                return;
            }
            return;
        } else {
            /* Normal mouse motion */
            device_emit_event(ctx, EV_REL, ev->code, ev->value);
            return;
        }
    } else if (ev->type == EV_SYN) {
        if (engine->button_pressed) {
            return;
        } else {
            device_emit_syn(ctx);
            return;
        }
    } else {
        device_emit_event(ctx, ev->type, ev->code, ev->value);
    }
}
