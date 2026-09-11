#include "config.h"
#include <glib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GROUP_GENERAL "General"
#define GROUP_BUTTONS "Buttons"

#define KEY_DEVICE_NAME "DeviceName"
#define KEY_DEVICE_PATH "DevicePath"
#define KEY_V_SENSITIVITY "VSensitivity"
#define KEY_H_SENSITIVITY "HSensitivity"
#define KEY_REVERSE_SCROLL "ReverseScroll"
#define KEY_SMOOTH_SCROLL "SmoothScroll"
#define KEY_SCROLL_BUTTON "ScrollButton"
#define KEY_EMULATE_CLICK "EmulateClick"
#define KEY_EMULATED_CLICK_BTN "EmulatedClickButton"
#define KEY_AUTOSTART "Autostart"

#define KEY_BTN_SIDE_ACTION "Button4Action"
#define KEY_BTN_EXTRA_ACTION "Button5Action"
#define KEY_BTN_MIDDLE_ACTION "Button3Action"
#define KEY_BTN_RIGHT_ACTION "Button2Action"

static AppConfig g_active_config;
static pthread_mutex_t g_config_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *get_config_path(void) {
    const char *config_dir = g_get_user_config_dir();
    char *app_dir = g_build_filename(config_dir, CONFIG_APP_NAME, NULL);
    g_mkdir_with_parents(app_dir, 0755);
    char *path = g_build_filename(app_dir, "config.ini", NULL);
    g_free(app_dir);
    return path;
}

static char *get_autostart_path(void) {
    const char *config_dir = g_get_user_config_dir();
    char *autostart_dir = g_build_filename(config_dir, "autostart", NULL);
    g_mkdir_with_parents(autostart_dir, 0755);
    char *path = g_build_filename(autostart_dir, "scroll-my-marbles.desktop", NULL);
    g_free(autostart_dir);
    return path;
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

const char *config_button_code_to_name(uint16_t code) {
    switch (code) {
        case BTN_LEFT: return "BTN_LEFT";
        case BTN_RIGHT: return "BTN_RIGHT";
        case BTN_MIDDLE: return "BTN_MIDDLE";
        case BTN_SIDE: return "BTN_SIDE";
        case BTN_EXTRA: return "BTN_EXTRA";
        case BTN_FORWARD: return "BTN_FORWARD";
        case BTN_BACK: return "BTN_BACK";
        default: return "UNKNOWN";
    }
}

uint16_t config_button_name_to_code(const char *name) {
    if (!name) return BTN_MIDDLE;
    if (g_ascii_strcasecmp(name, "BTN_LEFT") == 0 || g_ascii_strcasecmp(name, "Left") == 0)
        return BTN_LEFT;
    if (g_ascii_strcasecmp(name, "BTN_RIGHT") == 0 || g_ascii_strcasecmp(name, "Right") == 0)
        return BTN_RIGHT;
    if (g_ascii_strcasecmp(name, "BTN_MIDDLE") == 0 || g_ascii_strcasecmp(name, "Middle") == 0)
        return BTN_MIDDLE;
    if (g_ascii_strcasecmp(name, "BTN_SIDE") == 0 || g_ascii_strcasecmp(name, "Button 4") == 0 || g_ascii_strcasecmp(name, "Side") == 0)
        return BTN_SIDE;
    if (g_ascii_strcasecmp(name, "BTN_EXTRA") == 0 || g_ascii_strcasecmp(name, "Button 5") == 0 || g_ascii_strcasecmp(name, "Extra") == 0)
        return BTN_EXTRA;
    if (g_ascii_strcasecmp(name, "BTN_FORWARD") == 0 || g_ascii_strcasecmp(name, "Forward") == 0)
        return BTN_FORWARD;
    if (g_ascii_strcasecmp(name, "BTN_BACK") == 0 || g_ascii_strcasecmp(name, "Back") == 0)
        return BTN_BACK;
    return BTN_MIDDLE;
}

const char *config_button_action_to_name(ButtonAction action) {
    switch (action) {
        case BUTTON_ACTION_SCROLL_MODIFIER: return "ScrollModifier";
        case BUTTON_ACTION_LEFT_CLICK: return "LeftClick";
        case BUTTON_ACTION_RIGHT_CLICK: return "RightClick";
        case BUTTON_ACTION_MIDDLE_CLICK: return "MiddleClick";
        case BUTTON_ACTION_BACK: return "Back";
        case BUTTON_ACTION_FORWARD: return "Forward";
        case BUTTON_ACTION_DISABLED: return "Disabled";
        case BUTTON_ACTION_PASSTHROUGH:
        default:
            return "PassThrough";
    }
}

ButtonAction config_button_name_to_action(const char *name) {
    if (!name) return BUTTON_ACTION_PASSTHROUGH;
    if (g_ascii_strcasecmp(name, "ScrollModifier") == 0)
        return BUTTON_ACTION_SCROLL_MODIFIER;
    if (g_ascii_strcasecmp(name, "LeftClick") == 0)
        return BUTTON_ACTION_LEFT_CLICK;
    if (g_ascii_strcasecmp(name, "RightClick") == 0)
        return BUTTON_ACTION_RIGHT_CLICK;
    if (g_ascii_strcasecmp(name, "MiddleClick") == 0 || g_ascii_strcasecmp(name, "SendMMB") == 0)
        return BUTTON_ACTION_MIDDLE_CLICK;
    if (g_ascii_strcasecmp(name, "Back") == 0)
        return BUTTON_ACTION_BACK;
    if (g_ascii_strcasecmp(name, "Forward") == 0)
        return BUTTON_ACTION_FORWARD;
    if (g_ascii_strcasecmp(name, "Disabled") == 0)
        return BUTTON_ACTION_DISABLED;
    return BUTTON_ACTION_PASSTHROUGH;
}

bool config_load(AppConfig *cfg) {
    if (!cfg) return false;
    config_set_defaults(cfg);

    char *path = get_config_path();
    GKeyFile *kf = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &error)) {
        /* File doesn't exist yet, save defaults */
        g_clear_error(&error);
        g_key_file_free(kf);
        config_save(cfg);
        g_free(path);
        return true;
    }

    /* General Group */
    char *str_val = g_key_file_get_string(kf, GROUP_GENERAL, KEY_DEVICE_NAME, NULL);
    if (str_val) {
        strncpy(cfg->device_name, str_val, sizeof(cfg->device_name) - 1);
        g_free(str_val);
    }

    str_val = g_key_file_get_string(kf, GROUP_GENERAL, KEY_DEVICE_PATH, NULL);
    if (str_val) {
        strncpy(cfg->device_path, str_val, sizeof(cfg->device_path) - 1);
        g_free(str_val);
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_V_SENSITIVITY, NULL)) {
        cfg->v_sensitivity = g_key_file_get_integer(kf, GROUP_GENERAL, KEY_V_SENSITIVITY, NULL);
        if (cfg->v_sensitivity <= 0) cfg->v_sensitivity = CONFIG_DEFAULT_V_SENSITIVITY;
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_H_SENSITIVITY, NULL)) {
        cfg->h_sensitivity = g_key_file_get_integer(kf, GROUP_GENERAL, KEY_H_SENSITIVITY, NULL);
        if (cfg->h_sensitivity <= 0) cfg->h_sensitivity = CONFIG_DEFAULT_H_SENSITIVITY;
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_REVERSE_SCROLL, NULL)) {
        cfg->reverse_scroll = g_key_file_get_boolean(kf, GROUP_GENERAL, KEY_REVERSE_SCROLL, NULL);
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_SMOOTH_SCROLL, NULL)) {
        cfg->smooth_scroll = g_key_file_get_boolean(kf, GROUP_GENERAL, KEY_SMOOTH_SCROLL, NULL);
    }

    str_val = g_key_file_get_string(kf, GROUP_GENERAL, KEY_SCROLL_BUTTON, NULL);
    if (str_val) {
        cfg->scroll_button = config_button_name_to_code(str_val);
        g_free(str_val);
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_EMULATE_CLICK, NULL)) {
        cfg->emulate_click = g_key_file_get_boolean(kf, GROUP_GENERAL, KEY_EMULATE_CLICK, NULL);
    }

    str_val = g_key_file_get_string(kf, GROUP_GENERAL, KEY_EMULATED_CLICK_BTN, NULL);
    if (str_val) {
        cfg->emulated_click_button = config_button_name_to_code(str_val);
        g_free(str_val);
    }

    if (g_key_file_has_key(kf, GROUP_GENERAL, KEY_AUTOSTART, NULL)) {
        cfg->autostart = g_key_file_get_boolean(kf, GROUP_GENERAL, KEY_AUTOSTART, NULL);
    }

    /* Buttons Group */
    str_val = g_key_file_get_string(kf, GROUP_BUTTONS, KEY_BTN_SIDE_ACTION, NULL);
    if (str_val) {
        cfg->btn_side_action = config_button_name_to_action(str_val);
        g_free(str_val);
    }

    str_val = g_key_file_get_string(kf, GROUP_BUTTONS, KEY_BTN_EXTRA_ACTION, NULL);
    if (str_val) {
        cfg->btn_extra_action = config_button_name_to_action(str_val);
        g_free(str_val);
    }

    str_val = g_key_file_get_string(kf, GROUP_BUTTONS, KEY_BTN_MIDDLE_ACTION, NULL);
    if (str_val) {
        cfg->btn_middle_action = config_button_name_to_action(str_val);
        g_free(str_val);
    }

    str_val = g_key_file_get_string(kf, GROUP_BUTTONS, KEY_BTN_RIGHT_ACTION, NULL);
    if (str_val) {
        cfg->btn_right_action = config_button_name_to_action(str_val);
        g_free(str_val);
    }

    g_key_file_free(kf);
    g_free(path);

    pthread_mutex_lock(&g_config_mutex);
    g_active_config = *cfg;
    pthread_mutex_unlock(&g_config_mutex);

    return true;
}

bool config_save(const AppConfig *cfg) {
    if (!cfg) return false;

    char *path = get_config_path();
    GKeyFile *kf = g_key_file_new();

    g_key_file_set_string(kf, GROUP_GENERAL, KEY_DEVICE_NAME, cfg->device_name);
    g_key_file_set_string(kf, GROUP_GENERAL, KEY_DEVICE_PATH, cfg->device_path);
    g_key_file_set_integer(kf, GROUP_GENERAL, KEY_V_SENSITIVITY, cfg->v_sensitivity);
    g_key_file_set_integer(kf, GROUP_GENERAL, KEY_H_SENSITIVITY, cfg->h_sensitivity);
    g_key_file_set_boolean(kf, GROUP_GENERAL, KEY_REVERSE_SCROLL, cfg->reverse_scroll);
    g_key_file_set_boolean(kf, GROUP_GENERAL, KEY_SMOOTH_SCROLL, cfg->smooth_scroll);
    g_key_file_set_string(kf, GROUP_GENERAL, KEY_SCROLL_BUTTON, config_button_code_to_name(cfg->scroll_button));
    g_key_file_set_boolean(kf, GROUP_GENERAL, KEY_EMULATE_CLICK, cfg->emulate_click);
    g_key_file_set_string(kf, GROUP_GENERAL, KEY_EMULATED_CLICK_BTN, config_button_code_to_name(cfg->emulated_click_button));
    g_key_file_set_boolean(kf, GROUP_GENERAL, KEY_AUTOSTART, cfg->autostart);

    g_key_file_set_string(kf, GROUP_BUTTONS, KEY_BTN_SIDE_ACTION, config_button_action_to_name(cfg->btn_side_action));
    g_key_file_set_string(kf, GROUP_BUTTONS, KEY_BTN_EXTRA_ACTION, config_button_action_to_name(cfg->btn_extra_action));
    g_key_file_set_string(kf, GROUP_BUTTONS, KEY_BTN_MIDDLE_ACTION, config_button_action_to_name(cfg->btn_middle_action));
    g_key_file_set_string(kf, GROUP_BUTTONS, KEY_BTN_RIGHT_ACTION, config_button_action_to_name(cfg->btn_right_action));

    GError *error = NULL;
    char *data = g_key_file_to_data(kf, NULL, &error);
    bool ok = false;
    if (data) {
        ok = g_file_set_contents(path, data, -1, &error);
        g_free(data);
    }

    if (!ok && error) {
        g_warning("Failed to save configuration to %s: %s", path, error->message);
        g_clear_error(&error);
    }

    g_key_file_free(kf);
    g_free(path);

    if (ok) {
        pthread_mutex_lock(&g_config_mutex);
        g_active_config = *cfg;
        pthread_mutex_unlock(&g_config_mutex);
    }

    config_update_autostart(cfg->autostart);
    return ok;
}

void config_get_copy(AppConfig *dest) {
    if (!dest) return;
    pthread_mutex_lock(&g_config_mutex);
    *dest = g_active_config;
    pthread_mutex_unlock(&g_config_mutex);
}

void config_set_copy(const AppConfig *src) {
    if (!src) return;
    pthread_mutex_lock(&g_config_mutex);
    g_active_config = *src;
    pthread_mutex_unlock(&g_config_mutex);
    config_save(src);
}

bool config_update_autostart(bool enable) {
    char *path = get_autostart_path();
    bool result = true;

    if (enable) {
        const char *desktop_content =
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=Scroll My Marbles\n"
            "Comment=TrackMan Marble FX Scroll Emulation\n"
            "Exec=/usr/bin/scroll-my-marbles --tray\n"
            "Icon=scroll-my-marbles\n"
            "Terminal=false\n"
            "Categories=Utility;\n"
            "X-GNOME-Autostart-enabled=true\n";

        GError *error = NULL;
        result = g_file_set_contents(path, desktop_content, -1, &error);
        if (!result && error) {
            g_warning("Failed to create autostart entry: %s", error->message);
            g_clear_error(&error);
        }
    } else {
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            result = (g_remove(path) == 0);
        }
    }

    g_free(path);
    return result;
}

bool config_is_autostart_enabled(void) {
    char *path = get_autostart_path();
    bool exists = g_file_test(path, G_FILE_TEST_EXISTS);
    g_free(path);
    return exists;
}
