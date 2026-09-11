#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

#include "core/scroll_engine.h"
#include "config/config.h"

struct DeviceContext {
    int dummy;
};

/* Mock device context for testing event emission */
typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} EmittedEvent;

#define MAX_EMITTED 64
static EmittedEvent g_events[MAX_EMITTED];
static int g_event_count = 0;

/* Override device_emit_event and device_emit_syn for unit test harness */
void device_emit_event(DeviceContext *ctx, uint16_t type, uint16_t code, int32_t value) {
    (void)ctx;
    if (g_event_count < MAX_EMITTED) {
        g_events[g_event_count].type = type;
        g_events[g_event_count].code = code;
        g_events[g_event_count].value = value;
        g_event_count++;
    }
}

void device_emit_syn(DeviceContext *ctx) {
    (void)ctx;
    device_emit_event(ctx, EV_SYN, SYN_REPORT, 0);
}

void config_set_defaults(AppConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->device_name, CONFIG_DEFAULT_DEVICE_NAME, sizeof(cfg->device_name) - 1);
    cfg->device_path[0] = '\0';
    cfg->v_sensitivity = CONFIG_DEFAULT_V_SENSITIVITY;
    cfg->h_sensitivity = CONFIG_DEFAULT_H_SENSITIVITY;
    cfg->reverse_scroll = false;
    cfg->smooth_scroll = false;
    cfg->scroll_button = BTN_MIDDLE;
    cfg->emulate_click = true;
    cfg->emulated_click_button = BTN_MIDDLE;
    cfg->autostart = true;
    cfg->btn_side_action = BUTTON_ACTION_PASSTHROUGH;
    cfg->btn_extra_action = BUTTON_ACTION_PASSTHROUGH;
    cfg->btn_middle_action = BUTTON_ACTION_SCROLL_MODIFIER;
    cfg->btn_right_action = BUTTON_ACTION_PASSTHROUGH;
}

static void reset_test_events(void) {
    memset(g_events, 0, sizeof(g_events));
    g_event_count = 0;
}

static void test_initialization(void) {
    printf("Running test_initialization...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.v_sensitivity = 40;
    cfg.h_sensitivity = 100;
    cfg.smooth_scroll = false;

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);

    assert(engine.effective_sens_y == 40);
    assert(engine.effective_sens_x == 100);
    assert(engine.effective_delta == 1);
    assert(engine.button_pressed == false);
    assert(engine.scrolling_active == false);

    /* Test smooth mode scaling */
    cfg.smooth_scroll = true;
    scroll_engine_update_config(&engine, &cfg);
    assert(engine.effective_sens_y == 4);
    assert(engine.effective_sens_x == 10);
    assert(engine.effective_delta == 12);
    printf(" -> Passed!\n");
}

static void test_click_emulation_on_release(void) {
    printf("Running test_click_emulation_on_release...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.scroll_button = BTN_MIDDLE;
    cfg.emulate_click = true;
    cfg.emulated_click_button = BTN_MIDDLE;

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* 1. Press middle button down */
    struct input_event ev_down = { .type = EV_KEY, .code = BTN_MIDDLE, .value = 1 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev_down);

    assert(engine.button_pressed == true);
    assert(engine.scrolling_active == false);
    /* Should suppress press event from desktop */
    assert(g_event_count == 0);

    /* 2. Release middle button without any movement */
    struct input_event ev_up = { .type = EV_KEY, .code = BTN_MIDDLE, .value = 0 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev_up);

    assert(engine.button_pressed == false);
    /* Should have emitted: BTN_MIDDLE(1), SYN, BTN_MIDDLE(0), SYN */
    assert(g_event_count == 4);
    assert(g_events[0].type == EV_KEY && g_events[0].code == BTN_MIDDLE && g_events[0].value == 1);
    assert(g_events[1].type == EV_SYN && g_events[1].code == SYN_REPORT);
    assert(g_events[2].type == EV_KEY && g_events[2].code == BTN_MIDDLE && g_events[2].value == 0);
    assert(g_events[3].type == EV_SYN && g_events[3].code == SYN_REPORT);
    printf(" -> Passed!\n");
}

static void test_vertical_scrolling(void) {
    printf("Running test_vertical_scrolling...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.scroll_button = BTN_MIDDLE;
    cfg.v_sensitivity = 20;
    cfg.reverse_scroll = false;
    cfg.smooth_scroll = false;

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* Press modifier */
    struct input_event ev = { .type = EV_KEY, .code = BTN_MIDDLE, .value = 1 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    reset_test_events();

    /* Move trackball down by 15 units (threshold is 20, so no scroll event yet) */
    ev.type = EV_REL; ev.code = REL_Y; ev.value = 15;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(engine.accum_y == 15);
    assert(engine.scrolling_active == false);
    assert(g_event_count == 0);

    /* Move another 10 units (total 25 >= 20 -> 1 step down) */
    ev.value = 10;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(engine.scrolling_active == true);
    assert(engine.accum_y == 5); /* 25 - 20 = 5 */

    /* Should have emitted REL_WHEEL = -1 (down) and SYN */
    assert(g_event_count == 2);
    assert(g_events[0].type == EV_REL && g_events[0].code == REL_WHEEL && g_events[0].value == -1);
    assert(g_events[1].type == EV_SYN && g_events[1].code == SYN_REPORT);

    reset_test_events();

    /* Release modifier button: since scrolling occurred, NO click should be emulated! */
    ev.type = EV_KEY; ev.code = BTN_MIDDLE; ev.value = 0;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(engine.button_pressed == false);
    assert(g_event_count == 0);
    printf(" -> Passed!\n");
}

static void test_horizontal_scrolling(void) {
    printf("Running test_horizontal_scrolling...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.scroll_button = BTN_MIDDLE;
    cfg.h_sensitivity = 50;
    cfg.reverse_scroll = false;
    cfg.smooth_scroll = false;

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* Press modifier */
    struct input_event ev = { .type = EV_KEY, .code = BTN_MIDDLE, .value = 1 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    reset_test_events();

    /* Move right by 60 units (threshold 50 -> 1 step right = +1) */
    ev.type = EV_REL; ev.code = REL_X; ev.value = 60;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(engine.scrolling_active == true);
    assert(engine.accum_x == 10); /* 60 - 50 = 10 */

    assert(g_event_count == 2);
    assert(g_events[0].type == EV_REL && g_events[0].code == REL_HWHEEL && g_events[0].value == 1);
    assert(g_events[1].type == EV_SYN);
    printf(" -> Passed!\n");
}

static void test_reverse_scrolling(void) {
    printf("Running test_reverse_scrolling...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.scroll_button = BTN_MIDDLE;
    cfg.v_sensitivity = 20;
    cfg.reverse_scroll = true;

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* Press modifier */
    struct input_event ev = { .type = EV_KEY, .code = BTN_MIDDLE, .value = 1 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    reset_test_events();

    /* Move trackball down by 25 units. With reverse_scroll=true, accum_y = -25.
     * steps = -25 / 20 = -1. wheel_val = -steps = +1 (wheel up).
     */
    ev.type = EV_REL; ev.code = REL_Y; ev.value = 25;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(engine.scrolling_active == true);
    assert(g_events[0].type == EV_REL && g_events[0].code == REL_WHEEL && g_events[0].value == 1);
    printf(" -> Passed!\n");
}

static void test_button_remapping(void) {
    printf("Running test_button_remapping...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);
    cfg.btn_side_action = BUTTON_ACTION_MIDDLE_CLICK; /* Button 4 sends MMB */
    cfg.btn_extra_action = BUTTON_ACTION_DISABLED;    /* Button 5 disabled */

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* Press BTN_SIDE */
    struct input_event ev = { .type = EV_KEY, .code = BTN_SIDE, .value = 1 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(g_event_count == 2);
    assert(g_events[0].type == EV_KEY && g_events[0].code == BTN_MIDDLE && g_events[0].value == 1);

    reset_test_events();
    /* Press BTN_EXTRA (disabled) */
    ev.code = BTN_EXTRA; ev.value = 1;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    /* Should be completely dropped */
    assert(g_event_count == 0);
    printf(" -> Passed!\n");
}

static void test_passthrough_when_not_held(void) {
    printf("Running test_passthrough_when_not_held...\n");
    AppConfig cfg;
    config_set_defaults(&cfg);

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);
    DeviceContext dummy_ctx;
    reset_test_events();

    /* Move mouse without modifier pressed */
    struct input_event ev = { .type = EV_REL, .code = REL_X, .value = 42 };
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(g_event_count == 1);
    assert(g_events[0].type == EV_REL && g_events[0].code == REL_X && g_events[0].value == 42);

    reset_test_events();
    /* Left click */
    ev.type = EV_KEY; ev.code = BTN_LEFT; ev.value = 1;
    scroll_engine_process_event(&engine, &dummy_ctx, &ev);
    assert(g_event_count == 1);
    assert(g_events[0].type == EV_KEY && g_events[0].code == BTN_LEFT && g_events[0].value == 1);
    printf(" -> Passed!\n");
}

int main(void) {
    printf("===========================================\n");
    printf("  Scroll My Marbles - Scroll Engine Unit Tests\n");
    printf("===========================================\n");
    test_initialization();
    test_click_emulation_on_release();
    test_vertical_scrolling();
    test_horizontal_scrolling();
    test_reverse_scrolling();
    test_button_remapping();
    test_passthrough_when_not_held();
    printf("===========================================\n");
    printf("  ALL TESTS PASSED SUCCESSFULLY!\n");
    printf("===========================================\n");
    return 0;
}
