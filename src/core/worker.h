#ifndef WORKER_H
#define WORKER_H

#include "device.h"
#include "scroll_engine.h"
#include "../config/config.h"
#include <stdbool.h>
#include <pthread.h>

typedef void (*WorkerStatusCallback)(bool connected, const char *device_name, const char *device_path, void *user_data);

typedef struct {
    pthread_t thread;
    bool running;
    int stop_event_fd;
    DeviceContext *device_ctx;
    ScrollEngine engine;
    pthread_mutex_t lock;
    WorkerStatusCallback status_cb;
    void *status_user_data;
    char current_dev_name[256];
    char current_dev_path[PATH_MAX];
    bool is_connected;
} Worker;

bool worker_start(Worker *w, const AppConfig *cfg, WorkerStatusCallback cb, void *user_data);
void worker_stop(Worker *w);
void worker_reload_config(Worker *w, const AppConfig *cfg);
bool worker_is_connected(Worker *w);
void worker_trigger_rescan(Worker *w);
void worker_get_device_info(Worker *w, char *out_name, size_t name_size, char *out_path, size_t path_size);

#endif /* WORKER_H */
