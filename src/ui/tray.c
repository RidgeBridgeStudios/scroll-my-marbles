#include "tray.h"
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sni_introspection_xml =
    "<node>"
    "  <interface name='org.kde.StatusNotifierItem'>"
    "    <property name='Category' type='s' access='read'/>"
    "    <property name='Id' type='s' access='read'/>"
    "    <property name='Title' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='IconName' type='s' access='read'/>"
    "    <property name='Menu' type='o' access='read'/>"
    "    <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "    <method name='ContextMenu'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <method name='Activate'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <method name='SecondaryActivate'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <signal name='NewTitle'/>"
    "    <signal name='NewIcon'/>"
    "    <signal name='NewStatus'>"
    "      <arg name='status' type='s'/>"
    "    </signal>"
    "    <signal name='NewToolTip'/>"
    "  </interface>"
    "</node>";

static const char *dbusmenu_introspection_xml =
    "<node>"
    "  <interface name='com.canonical.dbusmenu'>"
    "    <property name='Version' type='u' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <method name='GetLayout'>"
    "      <arg name='parentId' type='i' direction='in'/>"
    "      <arg name='recursionDepth' type='i' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='revision' type='u' direction='out'/>"
    "      <arg name='layout' type='(ia{sv}av)' direction='out'/>"
    "    </method>"
    "    <method name='GetGroupProperties'>"
    "      <arg name='ids' type='ai' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='properties' type='a(ia{sv})' direction='out'/>"
    "    </method>"
    "    <method name='GetProperty'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='Event'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='eventId' type='s' direction='in'/>"
    "      <arg name='data' type='v' direction='in'/>"
    "      <arg name='timestamp' type='u' direction='in'/>"
    "    </method>"
    "    <method name='AboutToShow'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='needUpdate' type='b' direction='out'/>"
    "    </method>"
    "    <signal name='LayoutUpdated'>"
    "      <arg name='revision' type='u'/>"
    "      <arg name='parent' type='i'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

struct Tray {
    GDBusConnection *bus;
    guint sni_reg_id;
    guint menu_reg_id;
    guint watcher_id;
    char *service_name;
    char *title;
    char *status;
    char *device_name;
    bool connected;
    TrayCallbacks cbs;
    void *user_data;
};

/* StatusNotifierItem Method Call */
static void handle_sni_method_call(GDBusConnection *connection,
                                   const gchar *sender,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *method_name,
                                   GVariant *parameters,
                                   GDBusMethodInvocation *invocation,
                                   gpointer user_data) {
    (void)connection; (void)sender; (void)object_path; (void)interface_name; (void)parameters;
    Tray *t = (Tray *)user_data;

    if (g_strcmp0(method_name, "Activate") == 0 ||
        g_strcmp0(method_name, "ContextMenu") == 0) {
        if (t->cbs.on_settings) {
            t->cbs.on_settings(t->user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

/* StatusNotifierItem Property Get */
static GVariant *handle_sni_get_property(GDBusConnection *connection,
                                         const gchar *sender,
                                         const gchar *object_path,
                                         const gchar *interface_name,
                                         const gchar *property_name,
                                         GError **error,
                                         gpointer user_data) {
    (void)connection; (void)sender; (void)object_path; (void)interface_name; (void)error;
    Tray *t = (Tray *)user_data;

    if (g_strcmp0(property_name, "Category") == 0) {
        return g_variant_new_string("ApplicationStatus");
    } else if (g_strcmp0(property_name, "Id") == 0) {
        return g_variant_new_string("scroll-my-marbles");
    } else if (g_strcmp0(property_name, "Title") == 0) {
        return g_variant_new_string(t->title ? t->title : "Scroll My Marbles");
    } else if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("Active");
    } else if (g_strcmp0(property_name, "IconName") == 0) {
        return g_variant_new_string("scroll-my-marbles");
    } else if (g_strcmp0(property_name, "Menu") == 0) {
        return g_variant_new_object_path("/MenuBar");
    } else if (g_strcmp0(property_name, "ToolTip") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("(sa(iiay)ss)"));
        g_variant_builder_add(&b, "s", "scroll-my-marbles");

        /* Empty icons array */
        g_variant_builder_open(&b, G_VARIANT_TYPE("a(iiay)"));
        g_variant_builder_close(&b);

        g_variant_builder_add(&b, "s", "Scroll My Marbles");

        char tip[256];
        if (t->connected && t->device_name) {
            snprintf(tip, sizeof(tip), "Active: %s", t->device_name);
        } else {
            snprintf(tip, sizeof(tip), "Searching for TrackMan Marble FX...");
        }
        g_variant_builder_add(&b, "s", tip);

        return g_variant_builder_end(&b);
    }
    return NULL;
}

/* DBusMenu Layout Construction */
static GVariant *build_menu_item(int id, const char *label, const char *icon_name, bool enabled) {
    GVariantBuilder dict;
    g_variant_builder_init(&dict, G_VARIANT_TYPE("a{sv}"));

    if (label) {
        g_variant_builder_add(&dict, "{sv}", "label", g_variant_new_string(label));
    }
    if (icon_name) {
        g_variant_builder_add(&dict, "{sv}", "icon-name", g_variant_new_string(icon_name));
    }
    g_variant_builder_add(&dict, "{sv}", "enabled", g_variant_new_boolean(enabled));
    g_variant_builder_add(&dict, "{sv}", "visible", g_variant_new_boolean(TRUE));

    GVariantBuilder item;
    g_variant_builder_init(&item, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add(&item, "i", id);
    g_variant_builder_add_value(&item, g_variant_builder_end(&dict));

    /* Empty child array for leaf items */
    g_variant_builder_open(&item, G_VARIANT_TYPE("av"));
    g_variant_builder_close(&item);

    return g_variant_builder_end(&item);
}

static GVariant *build_menu_layout(Tray *t) {
    (void)t;
    GVariantBuilder root_dict;
    g_variant_builder_init(&root_dict, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&root_dict, "{sv}", "children-display", g_variant_new_string("submenu"));

    GVariantBuilder children;
    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));

    /* Item 1: Settings */
    GVariant *item1 = build_menu_item(1, "Settings...", "preferences-system", true);
    g_variant_builder_add(&children, "v", item1);

    /* Item 2: About */
    GVariant *item2 = build_menu_item(2, "About", "help-about", true);
    g_variant_builder_add(&children, "v", item2);

    /* Item 3: Separator (optional) or Quit */
    GVariant *item3 = build_menu_item(3, "Quit", "application-exit", true);
    g_variant_builder_add(&children, "v", item3);

    GVariantBuilder root;
    g_variant_builder_init(&root, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add(&root, "i", 0);
    g_variant_builder_add_value(&root, g_variant_builder_end(&root_dict));
    g_variant_builder_add_value(&root, g_variant_builder_end(&children));

    return g_variant_builder_end(&root);
}

/* DBusMenu Method Call */
static void handle_dbusmenu_method_call(GDBusConnection *connection,
                                        const gchar *sender,
                                        const gchar *object_path,
                                        const gchar *interface_name,
                                        const gchar *method_name,
                                        GVariant *parameters,
                                        GDBusMethodInvocation *invocation,
                                        gpointer user_data) {
    (void)connection; (void)sender; (void)object_path; (void)interface_name;
    Tray *t = (Tray *)user_data;

    if (g_strcmp0(method_name, "GetLayout") == 0) {
        GVariant *layout = build_menu_layout(t);
        GVariant *ret = g_variant_new("(u@(ia{sv}av))", (guint32)1, layout);
        g_dbus_method_invocation_return_value(invocation, ret);
    } else if (g_strcmp0(method_name, "Event") == 0) {
        gint32 id = 0;
        const gchar *event_id = NULL;
        GVariant *data = NULL;
        guint32 timestamp = 0;
        g_variant_get(parameters, "(isvu)", &id, &event_id, &data, &timestamp);

        if (g_strcmp0(event_id, "clicked") == 0) {
            if (id == 1 && t->cbs.on_settings) {
                t->cbs.on_settings(t->user_data);
            } else if (id == 2 && t->cbs.on_about) {
                t->cbs.on_about(t->user_data);
            } else if (id == 3 && t->cbs.on_quit) {
                t->cbs.on_quit(t->user_data);
            }
        }
        if (data) g_variant_unref(data);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "AboutToShow") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
    } else if (g_strcmp0(method_name, "GetGroupProperties") == 0) {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("a(ia{sv})"));
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ia{sv}))", &b));
    } else {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Method %s is not implemented", method_name);
    }
}

static GVariant *handle_dbusmenu_get_property(GDBusConnection *connection,
                                              const gchar *sender,
                                              const gchar *object_path,
                                              const gchar *interface_name,
                                              const gchar *property_name,
                                              GError **error,
                                              gpointer user_data) {
    (void)connection; (void)sender; (void)object_path; (void)interface_name; (void)error; (void)user_data;
    if (g_strcmp0(property_name, "Version") == 0) {
        return g_variant_new_uint32(3);
    } else if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("notice");
    }
    return NULL;
}

static const GDBusInterfaceVTable sni_vtable = {
    .method_call = handle_sni_method_call,
    .get_property = handle_sni_get_property,
    .set_property = NULL
};

static const GDBusInterfaceVTable dbusmenu_vtable = {
    .method_call = handle_dbusmenu_method_call,
    .get_property = handle_dbusmenu_get_property,
    .set_property = NULL
};

static void register_with_watcher(Tray *t) {
    if (!t->bus) return;

    GVariant *params = g_variant_new("(s)", t->service_name);
    g_dbus_connection_call(t->bus,
                           "org.kde.StatusNotifierWatcher",
                           "/StatusNotifierWatcher",
                           "org.kde.StatusNotifierWatcher",
                           "RegisterStatusNotifierItem",
                           params,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
}

Tray *tray_init(const TrayCallbacks *callbacks, void *user_data) {
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!bus) {
        fprintf(stderr, "Failed to connect to session bus for tray: %s\n", error ? error->message : "unknown");
        g_clear_error(&error);
        return NULL;
    }

    Tray *t = calloc(1, sizeof(Tray));
    t->bus = bus;
    if (callbacks) t->cbs = *callbacks;
    t->user_data = user_data;
    t->title = g_strdup("Scroll My Marbles");
    t->status = g_strdup("Active");

    pid_t pid = getpid();
    t->service_name = g_strdup_printf("org.kde.StatusNotifierItem-%d-1", pid);

    /* Request unique service name */
    g_bus_own_name_on_connection(bus,
                                 t->service_name,
                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                 NULL, NULL, NULL, NULL);

    /* Parse introspection XMLs */
    GDBusNodeInfo *sni_node = g_dbus_node_info_new_for_xml(sni_introspection_xml, NULL);
    GDBusNodeInfo *menu_node = g_dbus_node_info_new_for_xml(dbusmenu_introspection_xml, NULL);

    /* Register /StatusNotifierItem */
    t->sni_reg_id = g_dbus_connection_register_object(bus,
                                                      "/StatusNotifierItem",
                                                      sni_node->interfaces[0],
                                                      &sni_vtable,
                                                      t,
                                                      NULL,
                                                      &error);
    if (error) {
        fprintf(stderr, "Failed to register SNI object: %s\n", error->message);
        g_clear_error(&error);
    }

    /* Register /MenuBar */
    t->menu_reg_id = g_dbus_connection_register_object(bus,
                                                       "/MenuBar",
                                                       menu_node->interfaces[0],
                                                       &dbusmenu_vtable,
                                                       t,
                                                       NULL,
                                                       &error);
    if (error) {
        fprintf(stderr, "Failed to register MenuBar object: %s\n", error->message);
        g_clear_error(&error);
    }

    g_dbus_node_info_unref(sni_node);
    g_dbus_node_info_unref(menu_node);

    /* Register with StatusNotifierWatcher */
    register_with_watcher(t);

    return t;
}

void tray_set_connected_status(Tray *t, bool connected, const char *device_name) {
    if (!t) return;
    t->connected = connected;
    g_free(t->device_name);
    t->device_name = device_name ? g_strdup(device_name) : NULL;

    if (t->bus && t->sni_reg_id > 0) {
        /* Emit NewToolTip signal */
        g_dbus_connection_emit_signal(t->bus,
                                      NULL,
                                      "/StatusNotifierItem",
                                      "org.kde.StatusNotifierItem",
                                      "NewToolTip",
                                      NULL,
                                      NULL);
    }
}

void tray_destroy(Tray *t) {
    if (!t) return;

    if (t->sni_reg_id && t->bus) {
        g_dbus_connection_unregister_object(t->bus, t->sni_reg_id);
    }
    if (t->menu_reg_id && t->bus) {
        g_dbus_connection_unregister_object(t->bus, t->menu_reg_id);
    }
    if (t->bus) {
        g_object_unref(t->bus);
    }

    g_free(t->service_name);
    g_free(t->title);
    g_free(t->status);
    g_free(t->device_name);
    free(t);
}
