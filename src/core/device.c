#define _GNU_SOURCE
#include "device.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

bool device_is_trackman_name(const char *name) {
    if (!name) return false;
    if (strcasestr(name, "TrackMan") ||
        strcasestr(name, "Trackman") ||
        strcasestr(name, "Marble") ||
        strcasestr(name, "Marble FX")) {
        return true;
    }
    return false;
}

static int compare_device_info(const void *a, const void *b) {
    const DeviceInfo *da = (const DeviceInfo *)a;
    const DeviceInfo *db = (const DeviceInfo *)b;

    /* Put TrackMan/Marble devices at the top */
    if (da->is_trackman && !db->is_trackman) return -1;
    if (!da->is_trackman && db->is_trackman) return 1;

    return strcmp(da->name, db->name);
}

int device_scan_pointers(DeviceInfo **out_list) {
    if (!out_list) return 0;
    *out_list = NULL;

    const char *input_dir = "/dev/input";
    DIR *dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Failed to open %s: %s\n", input_dir, strerror(errno));
        return 0;
    }

    struct dirent *entry;
    int capacity = 16;
    int count = 0;
    DeviceInfo *list = malloc(capacity * sizeof(DeviceInfo));
    if (!list) {
        closedir(dir);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0) {
            close(fd);
            continue;
        }

        const char *name = libevdev_get_name(dev);
        if (!name) name = "Unknown Device";

        /* Skip virtual device created by ourselves to avoid loops */
        if (strcasestr(name, "Scroll My Marbles Virtual")) {
            libevdev_free(dev);
            close(fd);
            continue;
        }

        /* Check if device has pointer capabilities (relative motion or mouse buttons) */
        bool has_rel_xy = libevdev_has_event_type(dev, EV_REL) &&
                          (libevdev_has_event_code(dev, EV_REL, REL_X) ||
                           libevdev_has_event_code(dev, EV_REL, REL_Y));
        bool has_mouse_btn = libevdev_has_event_type(dev, EV_KEY) &&
                             (libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) ||
                              libevdev_has_event_code(dev, EV_KEY, BTN_MOUSE));

        if (has_rel_xy || has_mouse_btn) {
            if (count >= capacity) {
                capacity *= 2;
                DeviceInfo *new_list = realloc(list, capacity * sizeof(DeviceInfo));
                if (!new_list) {
                    libevdev_free(dev);
                    close(fd);
                    break;
                }
                list = new_list;
            }

            DeviceInfo *info = &list[count++];
            strncpy(info->path, path, sizeof(info->path) - 1);
            info->path[sizeof(info->path) - 1] = '\0';
            strncpy(info->name, name, sizeof(info->name) - 1);
            info->name[sizeof(info->name) - 1] = '\0';
            info->is_trackman = device_is_trackman_name(name);
            info->is_pointer = true;
        }

        libevdev_free(dev);
        close(fd);
    }

    closedir(dir);

    if (count > 1) {
        qsort(list, count, sizeof(DeviceInfo), compare_device_info);
    }

    *out_list = list;
    return count;
}

void device_free_scan_list(DeviceInfo *list, int count) {
    (void)count;
    if (list) {
        free(list);
    }
}

DeviceContext *device_open_and_grab(const char *preferred_path, const char *preferred_name) {
    char target_path[PATH_MAX] = {0};
    char target_name[256] = {0};

    if (preferred_path && preferred_path[0] != '\0' && access(preferred_path, R_OK | W_OK) == 0) {
        strncpy(target_path, preferred_path, sizeof(target_path) - 1);
    } else {
        /* Scan and locate device */
        DeviceInfo *list = NULL;
        int count = device_scan_pointers(&list);
        if (count == 0) {
            fprintf(stderr, "No pointer input devices found in /dev/input\n");
            device_free_scan_list(list, count);
            return NULL;
        }

        int selected_idx = -1;

        /* Try exact or substring name match first */
        if (preferred_name && preferred_name[0] != '\0') {
            for (int i = 0; i < count; i++) {
                if (strcasestr(list[i].name, preferred_name)) {
                    selected_idx = i;
                    break;
                }
            }
        }

        /* If not found, pick first TrackMan / Marble device */
        if (selected_idx < 0) {
            for (int i = 0; i < count; i++) {
                if (list[i].is_trackman) {
                    selected_idx = i;
                    break;
                }
            }
        }

        /* If still not found and no specific preferred name was requested, pick first pointer */
        if (selected_idx < 0 && (!preferred_name || preferred_name[0] == '\0')) {
            selected_idx = 0;
        }

        if (selected_idx >= 0) {
            strncpy(target_path, list[selected_idx].path, sizeof(target_path) - 1);
            strncpy(target_name, list[selected_idx].name, sizeof(target_name) - 1);
        }

        device_free_scan_list(list, count);
    }

    if (target_path[0] == '\0') {
        fprintf(stderr, "Could not find matching device for '%s'\n",
                preferred_name ? preferred_name : "default trackball");
        return NULL;
    }

    int fd = open(target_path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        /* Try read-only if read-write failed */
        fd = open(target_path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            fprintf(stderr, "Failed to open device %s: %s\n", target_path, strerror(errno));
            if (errno == EACCES) {
                fprintf(stderr, "Permission denied. Ensure udev rule 99-scroll-my-marbles.rules is installed,\n"
                                "or current user is in 'input' group.\n");
            }
            return NULL;
        }
    }

    struct libevdev *dev = NULL;
    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "Failed to initialize libevdev on %s: %s\n", target_path, strerror(-rc));
        close(fd);
        return NULL;
    }

    if (target_name[0] == '\0') {
        const char *name = libevdev_get_name(dev);
        if (name) strncpy(target_name, name, sizeof(target_name) - 1);
    }

    /* Grab device exclusively */
    rc = libevdev_grab(dev, LIBEVDEV_GRAB);
    if (rc < 0) {
        fprintf(stderr, "Warning: failed to grab device %s exclusively: %s\n", target_path, strerror(-rc));
    }

    /* Create virtual uinput device */
    struct libevdev *uidev_desc = libevdev_new();
    if (!uidev_desc) {
        fprintf(stderr, "Failed to create libevdev uinput descriptor\n");
        libevdev_grab(dev, LIBEVDEV_UNGRAB);
        libevdev_free(dev);
        close(fd);
        return NULL;
    }

    libevdev_set_name(uidev_desc, "Scroll My Marbles Virtual Trackball");
    libevdev_set_id_bustype(uidev_desc, BUS_USB);
    libevdev_set_id_vendor(uidev_desc, 0x046d);  /* Logitech */
    libevdev_set_id_product(uidev_desc, 0xc401); /* TrackMan Marble FX id */
    libevdev_set_id_version(uidev_desc, 1);

    /* Enable standard synchronization */
    libevdev_enable_event_type(uidev_desc, EV_SYN);
    libevdev_enable_event_code(uidev_desc, EV_SYN, SYN_REPORT, NULL);

    /* Enable relative axes (including wheels & high-res wheels) */
    libevdev_enable_event_type(uidev_desc, EV_REL);
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_X, NULL);
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_Y, NULL);
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_WHEEL, NULL);
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_HWHEEL, NULL);
#ifdef REL_WHEEL_HI_RES
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_WHEEL_HI_RES, NULL);
#endif
#ifdef REL_HWHEEL_HI_RES
    libevdev_enable_event_code(uidev_desc, EV_REL, REL_HWHEEL_HI_RES, NULL);
#endif

    /* Enable standard mouse buttons */
    libevdev_enable_event_type(uidev_desc, EV_KEY);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_MIDDLE, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_SIDE, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_EXTRA, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_FORWARD, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_BACK, NULL);
    libevdev_enable_event_code(uidev_desc, EV_KEY, BTN_TASK, NULL);

    /* Also copy any additional capabilities from physical device */
    for (int type = 0; type < EV_CNT; type++) {
        if (!libevdev_has_event_type(dev, type)) continue;
        int max_code = libevdev_event_type_get_max(type);
        for (int code = 0; code <= max_code; code++) {
            if (libevdev_has_event_code(dev, type, code)) {
                if (!libevdev_has_event_type(uidev_desc, type)) {
                    libevdev_enable_event_type(uidev_desc, type);
                }
                libevdev_enable_event_code(uidev_desc, type, code, NULL);
            }
        }
    }

    struct libevdev_uinput *uidev = NULL;
    rc = libevdev_uinput_create_from_device(uidev_desc, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    libevdev_free(uidev_desc);

    if (rc < 0) {
        fprintf(stderr, "Failed to create uinput virtual device: %s\n", strerror(-rc));
        fprintf(stderr, "Ensure /dev/uinput is accessible by the current user.\n");
        libevdev_grab(dev, LIBEVDEV_UNGRAB);
        libevdev_free(dev);
        close(fd);
        return NULL;
    }

    DeviceContext *ctx = calloc(1, sizeof(DeviceContext));
    if (!ctx) {
        libevdev_uinput_destroy(uidev);
        libevdev_grab(dev, LIBEVDEV_UNGRAB);
        libevdev_free(dev);
        close(fd);
        return NULL;
    }

    ctx->fd = fd;
    ctx->dev = dev;
    ctx->uidev = uidev;
    strncpy(ctx->current_path, target_path, sizeof(ctx->current_path) - 1);
    strncpy(ctx->current_name, target_name, sizeof(ctx->current_name) - 1);
    ctx->grabbed = true;

    printf("Attached and grabbed: %s (%s)\n", ctx->current_name, ctx->current_path);
    return ctx;
}

void device_close_and_ungrab(DeviceContext *ctx) {
    if (!ctx) return;

    if (ctx->dev && ctx->grabbed) {
        libevdev_grab(ctx->dev, LIBEVDEV_UNGRAB);
        ctx->grabbed = false;
    }

    if (ctx->uidev) {
        libevdev_uinput_destroy(ctx->uidev);
        ctx->uidev = NULL;
    }

    if (ctx->dev) {
        libevdev_free(ctx->dev);
        ctx->dev = NULL;
    }

    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    printf("Released device: %s\n", ctx->current_name);
    free(ctx);
}

void device_emit_event(DeviceContext *ctx, uint16_t type, uint16_t code, int32_t value) {
    if (!ctx || !ctx->uidev) return;
    libevdev_uinput_write_event(ctx->uidev, type, code, value);
}

void device_emit_syn(DeviceContext *ctx) {
    if (!ctx || !ctx->uidev) return;
    libevdev_uinput_write_event(ctx->uidev, EV_SYN, SYN_REPORT, 0);
}
