#include "settings_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SettingsWindow {
    AdwPreferencesWindow *pref_win;
    Worker *worker;
    AppConfig cfg;

    AdwComboRow *device_combo;
    AdwActionRow *status_row;
    AdwSpinRow *v_sens_spin;
    AdwSpinRow *h_sens_spin;
    AdwSwitchRow *reverse_switch;
    AdwSwitchRow *smooth_switch;
    AdwComboRow *scroll_btn_combo;
    AdwSwitchRow *emulate_click_switch;
    AdwComboRow *emulated_click_btn_combo;
    AdwComboRow *btn4_action_combo;
    AdwComboRow *btn5_action_combo;
    AdwSwitchRow *autostart_switch;

    DeviceInfo *scanned_devices;
    int scanned_count;
    bool updating_ui;
};

static const char *scroll_btn_names[] = {
    "Middle Button (BTN_MIDDLE)",
    "Button 4 / Back (BTN_SIDE)",
    "Button 5 / Forward (BTN_EXTRA)",
    "Right Button (BTN_RIGHT)",
    NULL
};

static const uint16_t scroll_btn_codes[] = {
    BTN_MIDDLE,
    BTN_SIDE,
    BTN_EXTRA,
    BTN_RIGHT
};

static const char *click_btn_names[] = {
    "Middle Click (BTN_MIDDLE)",
    "Left Click (BTN_LEFT)",
    "Right Click (BTN_RIGHT)",
    "Button 4 / Back (BTN_SIDE)",
    "Button 5 / Forward (BTN_EXTRA)",
    NULL
};

static const uint16_t click_btn_codes[] = {
    BTN_MIDDLE,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_SIDE,
    BTN_EXTRA
};

static const char *button_action_names[] = {
    "Pass-through (Default)",
    "Middle Click (Send MMB)",
    "Scroll Modifier",
    "Disabled",
    NULL
};

static const ButtonAction button_actions[] = {
    BUTTON_ACTION_PASSTHROUGH,
    BUTTON_ACTION_MIDDLE_CLICK,
    BUTTON_ACTION_SCROLL_MODIFIER,
    BUTTON_ACTION_DISABLED
};

static void apply_and_save(SettingsWindow *win) {
    if (win->updating_ui) return;

    win->cfg.v_sensitivity = (int)adw_spin_row_get_value(win->v_sens_spin);
    win->cfg.h_sensitivity = (int)adw_spin_row_get_value(win->h_sens_spin);
    win->cfg.reverse_scroll = adw_switch_row_get_active(win->reverse_switch);
    win->cfg.smooth_scroll = adw_switch_row_get_active(win->smooth_switch);
    win->cfg.emulate_click = adw_switch_row_get_active(win->emulate_click_switch);
    win->cfg.autostart = adw_switch_row_get_active(win->autostart_switch);

    guint s_idx = adw_combo_row_get_selected(win->scroll_btn_combo);
    if (s_idx < G_N_ELEMENTS(scroll_btn_codes)) {
        win->cfg.scroll_button = scroll_btn_codes[s_idx];
    }

    guint c_idx = adw_combo_row_get_selected(win->emulated_click_btn_combo);
    if (c_idx < G_N_ELEMENTS(click_btn_codes)) {
        win->cfg.emulated_click_button = click_btn_codes[c_idx];
    }

    guint b4_idx = adw_combo_row_get_selected(win->btn4_action_combo);
    if (b4_idx < G_N_ELEMENTS(button_actions)) {
        win->cfg.btn_side_action = button_actions[b4_idx];
    }

    guint b5_idx = adw_combo_row_get_selected(win->btn5_action_combo);
    if (b5_idx < G_N_ELEMENTS(button_actions)) {
        win->cfg.btn_extra_action = button_actions[b5_idx];
    }

    guint dev_idx = adw_combo_row_get_selected(win->device_combo);
    if (dev_idx == 0) {
        /* Auto-detect */
        strncpy(win->cfg.device_name, CONFIG_DEFAULT_DEVICE_NAME, sizeof(win->cfg.device_name) - 1);
        win->cfg.device_path[0] = '\0';
    } else if (dev_idx - 1 < (guint)win->scanned_count) {
        DeviceInfo *dev = &win->scanned_devices[dev_idx - 1];
        strncpy(win->cfg.device_name, dev->name, sizeof(win->cfg.device_name) - 1);
        strncpy(win->cfg.device_path, dev->path, sizeof(win->cfg.device_path) - 1);
    }

    config_set_copy(&win->cfg);
    if (win->worker) {
        worker_reload_config(win->worker, &win->cfg);
    }
}

static void on_setting_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
    (void)obj; (void)pspec;
    SettingsWindow *win = (SettingsWindow *)user_data;
    apply_and_save(win);
}

static void populate_devices(SettingsWindow *win) {
    if (win->scanned_devices) {
        device_free_scan_list(win->scanned_devices, win->scanned_count);
        win->scanned_devices = NULL;
        win->scanned_count = 0;
    }

    win->scanned_count = device_scan_pointers(&win->scanned_devices);

    GtkStringList *list = gtk_string_list_new(NULL);
    gtk_string_list_append(list, "Auto-detect (Logitech TrackMan Marble FX)");

    int selected_idx = 0;
    for (int i = 0; i < win->scanned_count; i++) {
        char label[320];
        snprintf(label, sizeof(label), "%s (%s)", win->scanned_devices[i].name, win->scanned_devices[i].path);
        gtk_string_list_append(list, label);

        if (win->cfg.device_path[0] != '\0' && strcmp(win->cfg.device_path, win->scanned_devices[i].path) == 0) {
            selected_idx = i + 1;
        } else if (win->cfg.device_path[0] == '\0' && win->scanned_devices[i].is_trackman && selected_idx == 0) {
            selected_idx = i + 1;
        }
    }

    adw_combo_row_set_model(win->device_combo, G_LIST_MODEL(list));
    adw_combo_row_set_selected(win->device_combo, selected_idx);
}

void settings_window_update_device_status(SettingsWindow *win, bool connected, const char *device_name, const char *device_path) {
    if (!win || !win->status_row) return;

    if (connected && device_name) {
        char subtitle[320];
        snprintf(subtitle, sizeof(subtitle), "Grabbed %s on %s", device_name, device_path ? device_path : "/dev/input");
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->status_row), "Status: Active &amp; Scrolling Ready");
        adw_action_row_set_subtitle(win->status_row, subtitle);
        adw_action_row_set_icon_name(win->status_row, "emblem-ok-symbolic");
    } else {
        adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->status_row), "Status: Waiting for Device");
        adw_action_row_set_subtitle(win->status_row, "Searching for Logitech TrackMan Marble FX in /dev/input...");
        adw_action_row_set_icon_name(win->status_row, "process-working-symbolic");
    }
}

static void on_rescan_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SettingsWindow *win = (SettingsWindow *)user_data;
    win->updating_ui = true;
    populate_devices(win);
    win->updating_ui = false;
}

static void on_reset_defaults_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SettingsWindow *win = (SettingsWindow *)user_data;
    win->updating_ui = true;

    config_set_defaults(&win->cfg);

    adw_spin_row_set_value(win->v_sens_spin, win->cfg.v_sensitivity);
    adw_spin_row_set_value(win->h_sens_spin, win->cfg.h_sensitivity);
    adw_switch_row_set_active(win->reverse_switch, win->cfg.reverse_scroll);
    adw_switch_row_set_active(win->smooth_switch, win->cfg.smooth_scroll);
    adw_switch_row_set_active(win->emulate_click_switch, win->cfg.emulate_click);
    adw_switch_row_set_active(win->autostart_switch, win->cfg.autostart);

    adw_combo_row_set_selected(win->device_combo, 0);
    adw_combo_row_set_selected(win->scroll_btn_combo, 0);
    adw_combo_row_set_selected(win->emulated_click_btn_combo, 0);
    adw_combo_row_set_selected(win->btn4_action_combo, 0);
    adw_combo_row_set_selected(win->btn5_action_combo, 0);

    win->updating_ui = false;
    apply_and_save(win);
}

void settings_window_show_about(GtkWindow *parent) {
    const char *developers[] = { "RidgeBridgeStudios", "Spitfire_x86 (original TBScroll)", NULL };

    adw_show_about_dialog(
        GTK_WIDGET(parent),
        "application-name", "Scroll My Marbles",
        "application-icon", "scroll-my-marbles",
        "developer-name", "Scroll My Marbles Team",
        "version", "1.0.0",
        "copyright", "© 2026 RidgeBridgeStudios",
        "license-type", GTK_LICENSE_MIT_X11,
        "website", "https://github.com/RidgeBridgeStudios/scroll-my-marbles",
        "issue-url", "https://github.com/RidgeBridgeStudios/scroll-my-marbles/issues",
        "comments", "Native Linux scroll emulation and button remapping for pointing devices "
                    "without a dedicated scroll wheel, targeting the Logitech TrackMan Marble FX.",
        "developers", developers,
        NULL
    );
}

static void on_about_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SettingsWindow *win = (SettingsWindow *)user_data;
    settings_window_show_about(GTK_WINDOW(win->pref_win));
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data) {
    (void)window; (void)user_data;
    gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
    return TRUE; /* Keep window alive in background for tray */
}

SettingsWindow *settings_window_new(GtkApplication *app, Worker *worker) {
    SettingsWindow *win = calloc(1, sizeof(SettingsWindow));
    win->worker = worker;
    config_get_copy(&win->cfg);

    win->pref_win = ADW_PREFERENCES_WINDOW(adw_preferences_window_new());
    gtk_window_set_application(GTK_WINDOW(win->pref_win), app);
    gtk_window_set_title(GTK_WINDOW(win->pref_win), "Scroll My Marbles Settings");
    gtk_window_set_default_size(GTK_WINDOW(win->pref_win), 650, 720);
    g_signal_connect(win->pref_win, "close-request", G_CALLBACK(on_close_request), win);

    /* Preferences Page */
    AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    adw_preferences_page_set_title(page, "Settings");
    adw_preferences_page_set_icon_name(page, "preferences-system-symbolic");
    adw_preferences_window_add(win->pref_win, page);

    /* --- GROUP 1: Device Configuration --- */
    AdwPreferencesGroup *dev_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(dev_group, "Target Device");
    adw_preferences_group_set_description(dev_group, "Select the trackball or pointing device to capture and emulate scrolling on.");
    adw_preferences_page_add(page, dev_group);

    win->status_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->status_row), "Status");
    adw_preferences_group_add(dev_group, GTK_WIDGET(win->status_row));

    win->device_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->device_combo), "Input Device");
    adw_preferences_row_set_use_underline(ADW_PREFERENCES_ROW(win->device_combo), TRUE);
    adw_preferences_group_add(dev_group, GTK_WIDGET(win->device_combo));

    GtkWidget *rescan_btn = gtk_button_new_with_label("Rescan Devices");
    gtk_widget_set_valign(rescan_btn, GTK_ALIGN_CENTER);
    g_signal_connect(rescan_btn, "clicked", G_CALLBACK(on_rescan_clicked), win);
    adw_action_row_add_suffix(ADW_ACTION_ROW(win->device_combo), rescan_btn);

    /* --- GROUP 2: Scrolling Options --- */
    AdwPreferencesGroup *scroll_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(scroll_group, "Scroll Sensitivities");
    adw_preferences_group_set_description(scroll_group, "Configure motion thresholds required to generate scroll events.");
    adw_preferences_page_add(page, scroll_group);

    win->v_sens_spin = ADW_SPIN_ROW(adw_spin_row_new_with_range(1, 500, 1));
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->v_sens_spin), "Vertical Sensitivity");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->v_sens_spin), "Lower values scroll faster (default: 20)");
    adw_spin_row_set_value(win->v_sens_spin, win->cfg.v_sensitivity);
    adw_preferences_group_add(scroll_group, GTK_WIDGET(win->v_sens_spin));

    win->h_sens_spin = ADW_SPIN_ROW(adw_spin_row_new_with_range(1, 500, 1));
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->h_sens_spin), "Horizontal Sensitivity");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->h_sens_spin), "Lower values scroll faster (default: 120)");
    adw_spin_row_set_value(win->h_sens_spin, win->cfg.h_sensitivity);
    adw_preferences_group_add(scroll_group, GTK_WIDGET(win->h_sens_spin));

    win->reverse_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->reverse_switch), "Reverse / Natural Scrolling");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->reverse_switch), "Inverts the vertical and horizontal scroll direction");
    adw_switch_row_set_active(win->reverse_switch, win->cfg.reverse_scroll);
    adw_preferences_group_add(scroll_group, GTK_WIDGET(win->reverse_switch));

    win->smooth_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->smooth_switch), "Smooth High-Resolution Scrolling");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->smooth_switch), "Emits sub-divided high-resolution wheel events for modern desktops");
    adw_switch_row_set_active(win->smooth_switch, win->cfg.smooth_scroll);
    adw_preferences_group_add(scroll_group, GTK_WIDGET(win->smooth_switch));

    /* --- GROUP 3: Scroll Modifier & Click Emulation --- */
    AdwPreferencesGroup *btn_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(btn_group, "Scroll Button &amp; Click Emulation");
    adw_preferences_group_set_description(btn_group, "Hold this button and move the ball to scroll. Release without motion to click.");
    adw_preferences_page_add(page, btn_group);

    win->scroll_btn_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->scroll_btn_combo), "Scroll Modifier Button");
    GtkStringList *s_model = gtk_string_list_new(scroll_btn_names);
    adw_combo_row_set_model(win->scroll_btn_combo, G_LIST_MODEL(s_model));
    guint sel_btn = 0;
    for (guint i = 0; i < G_N_ELEMENTS(scroll_btn_codes); i++) {
        if (scroll_btn_codes[i] == win->cfg.scroll_button) { sel_btn = i; break; }
    }
    adw_combo_row_set_selected(win->scroll_btn_combo, sel_btn);
    adw_preferences_group_add(btn_group, GTK_WIDGET(win->scroll_btn_combo));

    win->emulate_click_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->emulate_click_switch), "Emulate Click on Release");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->emulate_click_switch), "Emits a click event if the scroll button is tapped without scrolling");
    adw_switch_row_set_active(win->emulate_click_switch, win->cfg.emulate_click);
    adw_preferences_group_add(btn_group, GTK_WIDGET(win->emulate_click_switch));

    win->emulated_click_btn_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->emulated_click_btn_combo), "Click Action to Emulate");
    GtkStringList *c_model = gtk_string_list_new(click_btn_names);
    adw_combo_row_set_model(win->emulated_click_btn_combo, G_LIST_MODEL(c_model));
    guint sel_clk = 0;
    for (guint i = 0; i < G_N_ELEMENTS(click_btn_codes); i++) {
        if (click_btn_codes[i] == win->cfg.emulated_click_button) { sel_clk = i; break; }
    }
    adw_combo_row_set_selected(win->emulated_click_btn_combo, sel_clk);
    adw_preferences_group_add(btn_group, GTK_WIDGET(win->emulated_click_btn_combo));

    /* --- GROUP 4: Button Remapping --- */
    AdwPreferencesGroup *remap_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(remap_group, "Auxiliary Button Remapping");
    adw_preferences_group_set_description(remap_group, "Customize extra buttons on your TrackMan (e.g. Button 4/5 sending Middle Click).");
    adw_preferences_page_add(page, remap_group);

    win->btn4_action_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->btn4_action_combo), "Button 4 / Back (BTN_SIDE)");
    GtkStringList *b4_model = gtk_string_list_new(button_action_names);
    adw_combo_row_set_model(win->btn4_action_combo, G_LIST_MODEL(b4_model));
    guint sel_b4 = 0;
    for (guint i = 0; i < G_N_ELEMENTS(button_actions); i++) {
        if (button_actions[i] == win->cfg.btn_side_action) { sel_b4 = i; break; }
    }
    adw_combo_row_set_selected(win->btn4_action_combo, sel_b4);
    adw_preferences_group_add(remap_group, GTK_WIDGET(win->btn4_action_combo));

    win->btn5_action_combo = ADW_COMBO_ROW(adw_combo_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->btn5_action_combo), "Button 5 / Forward (BTN_EXTRA)");
    GtkStringList *b5_model = gtk_string_list_new(button_action_names);
    adw_combo_row_set_model(win->btn5_action_combo, G_LIST_MODEL(b5_model));
    guint sel_b5 = 0;
    for (guint i = 0; i < G_N_ELEMENTS(button_actions); i++) {
        if (button_actions[i] == win->cfg.btn_extra_action) { sel_b5 = i; break; }
    }
    adw_combo_row_set_selected(win->btn5_action_combo, sel_b5);
    adw_preferences_group_add(remap_group, GTK_WIDGET(win->btn5_action_combo));

    /* --- GROUP 5: Interactive Test Area --- */
    AdwPreferencesGroup *test_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(test_group, "Interactive Scroll Test Area");
    adw_preferences_group_set_description(test_group, "Hold your scroll button and roll the trackball here to test sensitivity and feel.");
    adw_preferences_page_add(page, test_group);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 140);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scrolled), 180);
    gtk_widget_add_css_class(scrolled, "card");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);

    for (int i = 1; i <= 25; i++) {
        char item_text[96];
        snprintf(item_text, sizeof(item_text), "Sample Item %02d ── Hold scroll modifier and move trackball to scroll", i);
        GtkWidget *label = gtk_label_new(item_text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_box_append(GTK_BOX(box), label);
    }
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), box);
    adw_preferences_group_add(test_group, scrolled);

    /* --- GROUP 6: General & Autostart --- */
    AdwPreferencesGroup *gen_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(gen_group, "System Integration");
    adw_preferences_page_add(page, gen_group);

    win->autostart_switch = ADW_SWITCH_ROW(adw_switch_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(win->autostart_switch), "Start Minimized at Login");
    adw_action_row_set_subtitle(ADW_ACTION_ROW(win->autostart_switch), "Automatically launches the tray service when you log in");
    adw_switch_row_set_active(win->autostart_switch, win->cfg.autostart);
    adw_preferences_group_add(gen_group, GTK_WIDGET(win->autostart_switch));

    /* Actions Row with Reset & About buttons */
    AdwActionRow *action_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(action_row), "Actions &amp; Information");

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset to Defaults");
    gtk_widget_set_valign(reset_btn, GTK_ALIGN_CENTER);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_defaults_clicked), win);
    adw_action_row_add_suffix(action_row, reset_btn);

    GtkWidget *about_btn = gtk_button_new_with_label("About");
    gtk_widget_set_valign(about_btn, GTK_ALIGN_CENTER);
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), win);
    adw_action_row_add_suffix(action_row, about_btn);

    adw_preferences_group_add(gen_group, GTK_WIDGET(action_row));

    /* Initial device scan & status */
    win->updating_ui = true;
    populate_devices(win);
    win->updating_ui = false;

    char dev_name[256] = {0};
    char dev_path[PATH_MAX] = {0};
    bool connected = worker_is_connected(worker);
    worker_get_device_info(worker, dev_name, sizeof(dev_name), dev_path, sizeof(dev_path));
    settings_window_update_device_status(win, connected, dev_name, dev_path);

    /* Connect change signals */
    g_signal_connect(win->device_combo, "notify::selected", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->v_sens_spin, "notify::value", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->h_sens_spin, "notify::value", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->reverse_switch, "notify::active", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->smooth_switch, "notify::active", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->scroll_btn_combo, "notify::selected", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->emulate_click_switch, "notify::active", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->emulated_click_btn_combo, "notify::selected", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->btn4_action_combo, "notify::selected", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->btn5_action_combo, "notify::selected", G_CALLBACK(on_setting_changed), win);
    g_signal_connect(win->autostart_switch, "notify::active", G_CALLBACK(on_setting_changed), win);

    return win;
}

void settings_window_present(SettingsWindow *win) {
    if (!win || !win->pref_win) return;
    gtk_window_present(GTK_WINDOW(win->pref_win));
}

void settings_window_hide(SettingsWindow *win) {
    if (!win || !win->pref_win) return;
    gtk_widget_set_visible(GTK_WIDGET(win->pref_win), FALSE);
}
