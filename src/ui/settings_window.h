#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../config/config.h"
#include "../core/worker.h"

typedef struct SettingsWindow SettingsWindow;

SettingsWindow *settings_window_new(GtkApplication *app, Worker *worker);
void settings_window_present(SettingsWindow *win);
void settings_window_hide(SettingsWindow *win);
GtkWindow *settings_window_get_window(SettingsWindow *win);
void settings_window_show_about(GtkWindow *parent);
void settings_window_update_device_status(SettingsWindow *win, bool connected, const char *device_name, const char *device_path);

#endif /* SETTINGS_WINDOW_H */
