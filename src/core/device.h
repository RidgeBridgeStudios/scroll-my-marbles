#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>
#include <linux/limits.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

typedef struct {
    char path[PATH_MAX];
    char name[256];
    bool is_trackman;
    bool is_pointer;
} DeviceInfo;

typedef struct DeviceContext {
    int fd;
    struct libevdev *dev;
    struct libevdev_uinput *uidev;
    char current_path[PATH_MAX];
    char current_name[256];
    bool grabbed;
} DeviceContext;

/* Device enumeration and discovery */
int device_scan_pointers(DeviceInfo **out_list);
void device_free_scan_list(DeviceInfo *list, int count);
bool device_is_trackman_name(const char *name);

/* Device grab and uinput creation */
DeviceContext *device_open_and_grab(const char *preferred_path, const char *preferred_name);
void device_close_and_ungrab(DeviceContext *ctx);

/* Input forwarding */
void device_emit_event(DeviceContext *ctx, uint16_t type, uint16_t code, int32_t value);
void device_emit_syn(DeviceContext *ctx);

#endif /* DEVICE_H */
