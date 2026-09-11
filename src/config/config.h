#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>

#define CONFIG_APP_NAME "scroll-my-marbles"
#define CONFIG_DEFAULT_DEVICE_NAME "Logitech TrackMan Marble FX"
#define CONFIG_DEFAULT_V_SENSITIVITY 20
#define CONFIG_DEFAULT_H_SENSITIVITY 120
#define CONFIG_SMOOTH_FACTOR 10
#define CONFIG_STANDARD_WHEEL_DELTA 120

typedef enum {
    BUTTON_ACTION_PASSTHROUGH = 0,
    BUTTON_ACTION_SCROLL_MODIFIER,
    BUTTON_ACTION_LEFT_CLICK,
    BUTTON_ACTION_RIGHT_CLICK,
    BUTTON_ACTION_MIDDLE_CLICK,
    BUTTON_ACTION_BACK,
    BUTTON_ACTION_FORWARD,
    BUTTON_ACTION_DISABLED
} ButtonAction;

typedef struct {
    char device_name[256];
    char device_path[256];
    int v_sensitivity;
    int h_sensitivity;
    bool reverse_scroll;
    bool smooth_scroll;
    uint16_t scroll_button;
    bool emulate_click;
    uint16_t emulated_click_button;
    bool autostart;

    /* Quick button assignments matching TBScroll options */
    ButtonAction btn_side_action;   /* Button 4 / BTN_SIDE */
    ButtonAction btn_extra_action;  /* Button 5 / BTN_EXTRA */
    ButtonAction btn_middle_action; /* Button 3 / BTN_MIDDLE */
    ButtonAction btn_right_action;  /* Button 2 / BTN_RIGHT */
} AppConfig;

void config_set_defaults(AppConfig *cfg);
bool config_load(AppConfig *cfg);
bool config_save(const AppConfig *cfg);

void config_get_copy(AppConfig *dest);
void config_set_copy(const AppConfig *src);

/* Autostart configuration */
bool config_update_autostart(bool enable);
bool config_is_autostart_enabled(void);

/* Helper button name / code conversions */
const char *config_button_code_to_name(uint16_t code);
uint16_t config_button_name_to_code(const char *name);
const char *config_button_action_to_name(ButtonAction action);
ButtonAction config_button_name_to_action(const char *name);

#endif /* CONFIG_H */
