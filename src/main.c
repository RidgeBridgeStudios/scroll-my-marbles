#define _GNU_SOURCE
#include <adwaita.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config/config.h"
#include "core/device.h"
#include "core/scroll_engine.h"
#include "core/worker.h"
#include "ui/settings_window.h"
#include "ui/tray.h"

#define APP_ID "org.ridgebridgestudios.scrollmymarbles"
/* APP_VERSION is supplied by the build system (meson project version). */

typedef struct {
    AdwApplication *app;
    Worker worker;
    Tray *tray;
    SettingsWindow *settings_win;
    AppConfig config;
    bool show_settings_on_start;
} AppContext;

static AppContext g_app_ctx;

typedef struct {
    AppContext *ctx;
    bool connected;
    char *device_name;
    char *device_path;
} WorkerStatusPayload;

static void on_worker_status_idle(gpointer user_data) {
    WorkerStatusPayload *p = (WorkerStatusPayload *)user_data;
    if (p->ctx->tray) {
        tray_set_connected_status(p->ctx->tray, p->connected, p->device_name);
    }
    if (p->ctx->settings_win) {
        settings_window_update_device_status(p->ctx->settings_win, p->connected, p->device_name, p->device_path);
    }
    g_free(p->device_name);
    g_free(p->device_path);
    g_free(p);
}

static void on_worker_status_changed(bool connected, const char *device_name, const char *device_path, void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    WorkerStatusPayload *payload = g_new0(WorkerStatusPayload, 1);
    payload->ctx = ctx;
    payload->connected = connected;
    payload->device_name = device_name ? g_strdup(device_name) : NULL;
    payload->device_path = device_path ? g_strdup(device_path) : NULL;

    g_idle_add_once(on_worker_status_idle, payload);
}

static void on_tray_settings(void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    if (ctx->settings_win) {
        settings_window_present(ctx->settings_win);
    }
}

static void on_tray_about(void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    if (ctx->settings_win) {
        settings_window_show_about_page(ctx->settings_win);
    }
}

static void on_tray_quit(void *user_data) {
    AppContext *ctx = (AppContext *)user_data;
    if (ctx->app) {
        g_application_quit(G_APPLICATION(ctx->app));
    }
}

static void on_app_startup(GApplication *app, gpointer user_data) {
    (void)user_data;
    /* Hold application refcount so the process remains running even with no visible windows */
    g_application_hold(app);
}

static void on_app_activate(GApplication *app, gpointer user_data) {
    (void)app;
    AppContext *ctx = (AppContext *)user_data;

    if (!ctx->settings_win) {
        ctx->settings_win = settings_window_new(GTK_APPLICATION(ctx->app), &ctx->worker);
    }

    if (ctx->show_settings_on_start) {
        settings_window_present(ctx->settings_win);
    }
}

static void on_app_shutdown(GApplication *app, gpointer user_data) {
    (void)app;
    AppContext *ctx = (AppContext *)user_data;

    if (ctx->tray) {
        tray_destroy(ctx->tray);
        ctx->tray = NULL;
    }

    worker_stop(&ctx->worker);
    printf("Scroll My Marbles stopped successfully.\n");
}

/* Console test mode to verify evdev reading and uinput forwarding without GUI */
static int run_test_mode(const char *override_device) {
    printf("=== Scroll My Marbles - Diagnostics & Test Mode ===\n");
    printf("Scanning for pointer input devices...\n\n");

    DeviceInfo *list = NULL;
    int count = device_scan_pointers(&list);

    if (count == 0) {
        printf("No pointer devices found in /dev/input!\n");
        bool trackball_in_proc = false;
        char tb_name[256] = {0};
        int perm_errors = device_check_permissions(&trackball_in_proc, tb_name, sizeof(tb_name));
        if (perm_errors > 0 && trackball_in_proc) {
            printf("\nDIAGNOSIS: Permission Denied!\n");
            printf("A %s was detected in /proc/bus/input/devices,\n", tb_name[0] ? tb_name : "Logitech Trackball");
            printf("but %d device node(s) in /dev/input/ could not be opened due to lack of user permissions.\n\n", perm_errors);
            printf("To fix this, activate 70-scroll-my-marbles.rules:\n");
            printf("  sudo mv /lib/udev/rules.d/99-scroll-my-marbles.rules /lib/udev/rules.d/70-scroll-my-marbles.rules 2>/dev/null || \\\n");
            printf("  sudo cp data/70-scroll-my-marbles.rules /lib/udev/rules.d/\n");
            printf("  sudo udevadm control --reload-rules && sudo udevadm trigger\n\n");
        } else if (perm_errors > 0) {
            printf("\nDIAGNOSIS: Permission Denied on %d device node(s) in /dev/input/.\n", perm_errors);
            printf("Ensure 70-scroll-my-marbles.rules is installed or user is in 'input' group.\n\n");
        } else {
            printf("Ensure current user has permission to read /dev/input/event*.\n");
        }
        return 1;
    }

    printf("Available pointer devices:\n");
    for (int i = 0; i < count; i++) {
        printf(" [%d] %s (%s)%s\n",
               i, list[i].name, list[i].path,
               list[i].is_trackman ? " [TrackMan/Marble Detected]" : "");
    }
    printf("\n");

    AppConfig cfg;
    config_load(&cfg);
    if (override_device && override_device[0] != '\0') {
        strncpy(cfg.device_path, override_device, sizeof(cfg.device_path) - 1);
    }

    printf("Configuration:\n");
    printf(" - Device Target: %s (%s)\n", cfg.device_name, cfg.device_path[0] ? cfg.device_path : "Auto-detect");
    printf(" - V-Sensitivity: %d | H-Sensitivity: %d\n", cfg.v_sensitivity, cfg.h_sensitivity);
    printf(" - Reverse Scroll: %s | Smooth Scroll: %s\n", cfg.reverse_scroll ? "YES" : "NO", cfg.smooth_scroll ? "YES" : "NO");
    printf(" - Scroll Button: %s\n", config_button_code_to_name(cfg.scroll_button));
    printf(" - Emulate Click: %s (%s)\n\n",
           cfg.emulate_click ? "YES" : "NO",
           config_button_code_to_name(cfg.emulated_click_button));

    printf("Attempting to open and grab device...\n");
    DeviceContext *ctx = device_open_and_grab(cfg.device_path, cfg.device_name);
    if (!ctx) {
        fprintf(stderr, "Could not open or grab target device.\n");
        fprintf(stderr, "Check permissions on /dev/uinput and /dev/input.\n");
        device_free_scan_list(list, count);
        return 1;
    }

    device_free_scan_list(list, count);

    printf("Successfully grabbed %s (%s)!\n", ctx->current_name, ctx->current_path);
    printf("Virtual device created on /dev/uinput.\n");
    printf("Hold %s and roll the trackball to test scrolling. Press Ctrl+C to exit.\n\n",
           config_button_code_to_name(cfg.scroll_button));

    ScrollEngine engine;
    scroll_engine_init(&engine, &cfg);

    struct input_event ev;
    while (1) {
        int rc = libevdev_next_event(ctx->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
            if (ev.type == EV_KEY && ev.code == cfg.scroll_button) {
                printf("[TEST] Scroll button %s %s\n",
                       config_button_code_to_name(ev.code),
                       ev.value == 1 ? "PRESSED" : "RELEASED");
            } else if (ev.type == EV_REL && engine.button_pressed) {
                if (ev.code == REL_Y) {
                    printf("[TEST] Trackball Y motion: delta=%d | AccumY=%d / %d\n",
                           ev.value, engine.accum_y, engine.effective_sens_y);
                } else if (ev.code == REL_X) {
                    printf("[TEST] Trackball X motion: delta=%d | AccumX=%d / %d\n",
                           ev.value, engine.accum_x, engine.effective_sens_x);
                }
            }

            scroll_engine_process_event(&engine, ctx, &ev);
        } else if (rc == -ENODEV) {
            printf("[TEST] Device disconnected!\n");
            break;
        } else if (rc != -EAGAIN) {
            usleep(2000);
        }
    }

    device_close_and_ungrab(ctx);
    return 0;
}

int main(int argc, char *argv[]) {
    bool test_mode = false;
    bool start_in_tray = false;
    bool force_settings = false;
    const char *custom_device = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            test_mode = true;
        } else if (strcmp(argv[i], "--tray") == 0) {
            start_in_tray = true;
        } else if (strcmp(argv[i], "--settings") == 0) {
            force_settings = true;
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            custom_device = argv[++i];
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("scroll-my-marbles %s\n", APP_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: scroll-my-marbles [OPTIONS]\n\n"
                   "Options:\n"
                   "  --tray        Start minimized in the system tray\n"
                   "  --settings    Open the settings window on startup\n"
                   "  --test        Run interactive diagnostics test mode in terminal\n"
                   "  --device PATH Override device with specific /dev/input/event* path\n"
                   "  --version     Display application version\n"
                   "  --help        Display this help message\n");
            return 0;
        }
    }

    if (test_mode) {
        return run_test_mode(custom_device);
    }

    memset(&g_app_ctx, 0, sizeof(g_app_ctx));
    config_load(&g_app_ctx.config);

    if (custom_device && custom_device[0] != '\0') {
        strncpy(g_app_ctx.config.device_path, custom_device, sizeof(g_app_ctx.config.device_path) - 1);
    }

    g_app_ctx.show_settings_on_start = force_settings || (!start_in_tray);

    /* Start background input worker thread */
    if (!worker_start(&g_app_ctx.worker, &g_app_ctx.config, on_worker_status_changed, &g_app_ctx)) {
        fprintf(stderr, "Warning: failed to start input worker immediately. Will retry in background.\n");
    }

    /* Initialize GTK4 / Libadwaita Application */
    g_app_ctx.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(g_app_ctx.app, "startup", G_CALLBACK(on_app_startup), &g_app_ctx);
    g_signal_connect(g_app_ctx.app, "activate", G_CALLBACK(on_app_activate), &g_app_ctx);
    g_signal_connect(g_app_ctx.app, "shutdown", G_CALLBACK(on_app_shutdown), &g_app_ctx);

    /* Initialize System Tray via DBus StatusNotifierItem */
    TrayCallbacks tray_cbs = {
        .on_settings = on_tray_settings,
        .on_about = on_tray_about,
        .on_quit = on_tray_quit
    };
    g_app_ctx.tray = tray_init(&tray_cbs, &g_app_ctx);

    char dev_name[256] = {0};
    char dev_path[PATH_MAX] = {0};
    bool connected = worker_is_connected(&g_app_ctx.worker);
    worker_get_device_info(&g_app_ctx.worker, dev_name, sizeof(dev_name), dev_path, sizeof(dev_path));
    if (g_app_ctx.tray) {
        tray_set_connected_status(g_app_ctx.tray, connected, dev_name);
    }

    int status = g_application_run(G_APPLICATION(g_app_ctx.app), 0, NULL);
    g_object_unref(g_app_ctx.app);

    return status;
}
