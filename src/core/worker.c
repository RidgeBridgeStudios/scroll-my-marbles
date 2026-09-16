#define _GNU_SOURCE
#include "worker.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

static void *worker_thread_func(void *arg) {
    Worker *w = (Worker *)arg;

    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    int inotify_wd = -1;
    if (inotify_fd >= 0) {
        inotify_wd = inotify_add_watch(inotify_fd, "/dev/input", IN_CREATE | IN_DELETE | IN_ATTRIB);
    }

    while (w->running) {
        pthread_mutex_lock(&w->lock);
        DeviceContext *ctx = w->device_ctx;
        pthread_mutex_unlock(&w->lock);

        /* Setup polling */
        struct pollfd fds[3];
        int nfds = 0;

        /* Index 0: Stop event */
        fds[nfds].fd = w->stop_event_fd;
        fds[nfds].events = POLLIN;
        int stop_idx = nfds++;

        /* Index 1: Inotify for hotplug */
        int inotify_idx = -1;
        if (inotify_fd >= 0) {
            fds[nfds].fd = inotify_fd;
            fds[nfds].events = POLLIN;
            inotify_idx = nfds++;
        }

        /* Index 2: Device input */
        int dev_idx = -1;
        if (ctx && ctx->fd >= 0) {
            fds[nfds].fd = ctx->fd;
            fds[nfds].events = POLLIN;
            dev_idx = nfds++;
        }

        int poll_timeout = (ctx != NULL) ? 1000 : 2000;
        int ret = poll(fds, nfds, poll_timeout);

        if (!w->running) break;
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("worker poll error");
            break;
        }

        /* Check stop event */
        if (fds[stop_idx].revents & POLLIN) {
            uint64_t val;
            read(w->stop_event_fd, &val, sizeof(val));
            if (!w->running) break;
        }

        /* Check inotify hotplug */
        if (inotify_idx >= 0 && (fds[inotify_idx].revents & POLLIN)) {
            char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
            while (read(inotify_fd, buf, sizeof(buf)) > 0) {
                /* Drain inotify buffer */
            }

            pthread_mutex_lock(&w->lock);
            if (!w->device_ctx) {
                /* Attempt reconnect */
                AppConfig cfg_copy;
                config_get_copy(&cfg_copy);
                w->device_ctx = device_open_and_grab(cfg_copy.device_path, cfg_copy.device_name);
                if (w->device_ctx) {
                    w->is_connected = true;
                    strncpy(w->current_dev_name, w->device_ctx->current_name, sizeof(w->current_dev_name) - 1);
                    strncpy(w->current_dev_path, w->device_ctx->current_path, sizeof(w->current_dev_path) - 1);
                    if (w->status_cb) {
                        w->status_cb(true, w->current_dev_name, w->current_dev_path, w->status_user_data);
                    }
                }
            }
            pthread_mutex_unlock(&w->lock);
        }

        /* Check device input */
        if (dev_idx >= 0 && (fds[dev_idx].revents & (POLLIN | POLLERR | POLLHUP))) {
            pthread_mutex_lock(&w->lock);
            ctx = w->device_ctx;

            if (ctx && ctx->dev) {
                struct input_event ev;
                int rc = 0;

                do {
                    rc = libevdev_next_event(ctx->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
                    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                        while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                            scroll_engine_process_event(&w->engine, ctx, &ev);
                            rc = libevdev_next_event(ctx->dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
                        }
                    } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                        scroll_engine_process_event(&w->engine, ctx, &ev);
                    }
                } while (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC);

                if (rc == -ENODEV || (fds[dev_idx].revents & (POLLERR | POLLHUP))) {
                    fprintf(stderr, "Device disconnected: %s\n", ctx->current_name);
                    device_close_and_ungrab(ctx);
                    w->device_ctx = NULL;
                    w->is_connected = false;
                    w->current_dev_name[0] = '\0';
                    w->current_dev_path[0] = '\0';

                    if (w->status_cb) {
                        w->status_cb(false, NULL, NULL, w->status_user_data);
                    }
                }
            }
            pthread_mutex_unlock(&w->lock);
        } else if (!ctx) {
            /* If not connected, periodically retry to detect device */
            pthread_mutex_lock(&w->lock);
            AppConfig cfg_copy;
            config_get_copy(&cfg_copy);
            w->device_ctx = device_open_and_grab(cfg_copy.device_path, cfg_copy.device_name);
            if (w->device_ctx) {
                w->is_connected = true;
                strncpy(w->current_dev_name, w->device_ctx->current_name, sizeof(w->current_dev_name) - 1);
                strncpy(w->current_dev_path, w->device_ctx->current_path, sizeof(w->current_dev_path) - 1);
                if (w->status_cb) {
                    w->status_cb(true, w->current_dev_name, w->current_dev_path, w->status_user_data);
                }
            }
            pthread_mutex_unlock(&w->lock);
        }
    }

    if (inotify_wd >= 0 && inotify_fd >= 0) {
        inotify_rm_watch(inotify_fd, inotify_wd);
    }
    if (inotify_fd >= 0) {
        close(inotify_fd);
    }

    pthread_mutex_lock(&w->lock);
    if (w->device_ctx) {
        device_close_and_ungrab(w->device_ctx);
        w->device_ctx = NULL;
    }
    w->is_connected = false;
    pthread_mutex_unlock(&w->lock);

    return NULL;
}

bool worker_start(Worker *w, const AppConfig *cfg, WorkerStatusCallback cb, void *user_data) {
    if (!w || !cfg) return false;

    memset(w, 0, sizeof(*w));
    pthread_mutex_init(&w->lock, NULL);
    scroll_engine_init(&w->engine, cfg);

    w->status_cb = cb;
    w->status_user_data = user_data;
    w->running = true;

    w->stop_event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (w->stop_event_fd < 0) {
        perror("eventfd creation failed");
        return false;
    }

    /* Initial attempt to grab device */
    w->device_ctx = device_open_and_grab(cfg->device_path, cfg->device_name);
    if (w->device_ctx) {
        w->is_connected = true;
        strncpy(w->current_dev_name, w->device_ctx->current_name, sizeof(w->current_dev_name) - 1);
        strncpy(w->current_dev_path, w->device_ctx->current_path, sizeof(w->current_dev_path) - 1);
        if (cb) cb(true, w->current_dev_name, w->current_dev_path, user_data);
    } else {
        w->is_connected = false;
        if (cb) cb(false, NULL, NULL, user_data);
    }

    if (pthread_create(&w->thread, NULL, worker_thread_func, w) != 0) {
        perror("Failed to spawn worker thread");
        w->running = false;
        if (w->device_ctx) {
            device_close_and_ungrab(w->device_ctx);
            w->device_ctx = NULL;
        }
        close(w->stop_event_fd);
        return false;
    }

    return true;
}

void worker_stop(Worker *w) {
    if (!w || !w->running) return;

    w->running = false;
    uint64_t val = 1;
    /* Wake worker thread to handle shutdown (w->running is false) */
    write(w->stop_event_fd, &val, sizeof(val));

    pthread_join(w->thread, NULL);

    close(w->stop_event_fd);
    w->stop_event_fd = -1;
    pthread_mutex_destroy(&w->lock);
}

void worker_reload_config(Worker *w, const AppConfig *cfg) {
    if (!w || !cfg) return;

    pthread_mutex_lock(&w->lock);
    scroll_engine_update_config(&w->engine, cfg);

    /* Check if target device changed */
    bool device_changed = false;
    if (cfg->device_path[0] != '\0' && strcmp(cfg->device_path, w->current_dev_path) != 0) {
        device_changed = true;
    } else if (cfg->device_name[0] != '\0' && strcmp(cfg->device_name, w->current_dev_name) != 0) {
        device_changed = true;
    }

    if (device_changed) {
        if (w->device_ctx) {
            device_close_and_ungrab(w->device_ctx);
            w->device_ctx = NULL;
            w->is_connected = false;
            w->current_dev_name[0] = '\0';
            w->current_dev_path[0] = '\0';
        }
        /* Wake only: worker must not treat it as shutdown unless w->running is already false */
        uint64_t val = 1;
        write(w->stop_event_fd, &val, sizeof(val));
    }

    pthread_mutex_unlock(&w->lock);
}

bool worker_is_connected(Worker *w) {
    if (!w) return false;
    pthread_mutex_lock(&w->lock);
    bool connected = w->is_connected;
    pthread_mutex_unlock(&w->lock);
    return connected;
}

void worker_trigger_rescan(Worker *w) {
    if (!w || !w->running || w->stop_event_fd < 0) return;
    uint64_t val = 1;
    write(w->stop_event_fd, &val, sizeof(val));
}

void worker_get_device_info(Worker *w, char *out_name, size_t name_size, char *out_path, size_t path_size) {
    if (!w) return;
    pthread_mutex_lock(&w->lock);
    if (out_name && name_size > 0) {
        strncpy(out_name, w->current_dev_name, name_size - 1);
        out_name[name_size - 1] = '\0';
    }
    if (out_path && path_size > 0) {
        strncpy(out_path, w->current_dev_path, path_size - 1);
        out_path[path_size - 1] = '\0';
    }
    pthread_mutex_unlock(&w->lock);
}
