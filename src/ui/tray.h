#ifndef TRAY_H
#define TRAY_H

#include <stdbool.h>

typedef struct {
    void (*on_settings)(void *user_data);
    void (*on_about)(void *user_data);
    void (*on_quit)(void *user_data);
} TrayCallbacks;

typedef struct Tray Tray;

Tray *tray_init(const TrayCallbacks *callbacks, void *user_data);
void tray_destroy(Tray *t);
void tray_set_connected_status(Tray *t, bool connected, const char *device_name);

#endif /* TRAY_H */
