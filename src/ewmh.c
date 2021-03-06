/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * ewmh.c: Functions to get/set certain EWMH properties easily.
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "data.h"
#include "table.h"
#include "i3.h"
#include "xcb.h"
#include "util.h"
#include "log.h"

/**
 * Creates the _NET_SUPPORTING_WM_CHECK window and update the root window
 * property.
 *
 * EWMH: The window is a direct child to the root window, with
 * _NET_WM_NAME set to the name of the window manager.
 */
void ewmh_create_supporting_window()
{
        xcb_window_t supporting_window = xcb_generate_id(global_conn);
        uint32_t supporting_values[] = { 0 };

        check_error(global_conn,
                    xcb_create_window_checked(global_conn,
                                              0,
                                              supporting_window,
                                              root,
                                              0, 0, 1, 1,
                                              0,
                                              XCB_WINDOW_CLASS_INPUT_ONLY,
                                              XCB_WINDOW_CLASS_COPY_FROM_PARENT,
                                              0, supporting_values),
                    "Could not create _NET_SUPPORTING_WM_CHECK window");

        check_error(global_conn,
                    xcb_change_property_checked(global_conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTING_WM_CHECK, A_WINDOW, 32, 1, &supporting_window),
                    "Could not set the _NET_SUPPORTING_WM_CHECK property");

        check_error(global_conn,
                    xcb_change_property_checked(global_conn, XCB_PROP_MODE_REPLACE, supporting_window, A_WM_NAME, XCB_ATOM_STRING, 8, strlen("i3"), "i3"),
                    "Could not set the WM_NAME property on the _NET_SUPPORTING_WM_CHECK window");

        /* "i3" is ASCII, but EWMH 1.3 requires us to set the _NET_WM_NAME property. */
        check_error(global_conn,
                    xcb_change_property_checked(global_conn, XCB_PROP_MODE_REPLACE, supporting_window, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3"),
                    "Could not set the _NET_WM_NAME property on the _NET_SUPPORTING_WM_CHECK window");
}

/**
 * Updates _NET_CLIENT_LIST with a complete list of client window IDs.
 *
 * EWMH: Two properties are used; _NET_CLIENT_LIST in initial mapping order,
 * and _NET_CLIENT_LIST_STACKING in bottom-to-top stacking order.
 */
void ewmh_update_client_list()
{
        Workspace *ws;
        Client *client;
        int num_clients = 0;

        TAILQ_FOREACH(ws, workspaces, workspaces) {
                FOR_TABLE(ws) {
                        CIRCLEQ_FOREACH(client, &(ws->table[cols][rows]->clients), clients) {
                                num_clients++;
                        }
                }
        }

        xcb_window_t *windows = smalloc(num_clients * sizeof(*windows));
        int i = 0;

        TAILQ_FOREACH(ws, workspaces, workspaces) {
                FOR_TABLE(ws) {
                        CIRCLEQ_FOREACH(client, &(ws->table[cols][rows]->clients), clients) {
                                windows[i++] = client->child;
                        }
                }
        }

        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_CLIENT_LIST, A_WINDOW, 32, num_clients,
                            windows);

        /* TODO: This should be in stacking order; so first tiles (in any
         * order,) and then floating windows in the correct order. */
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_CLIENT_LIST_STACKING, A_WINDOW, 32, num_clients,
                            windows);

        free(windows);
}

/*
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop() {
        uint32_t current_desktop = c_ws->num;
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_CURRENT_DESKTOP, A_CARDINAL, 32, 1,
                            &current_desktop);
}

/**
 * Updates _NET_DESKTOP_NAMES with names of all workspaces.
 *
 * EWMH: Simply a list of strings. It doesn't have to match
 * NUMBER_OF_DESKTOPS. Ordinals are used for missing names.
 */
void ewmh_update_desktop_names()
{
        Workspace *ws;
        int names_len = 0;

        TAILQ_FOREACH(ws, workspaces, workspaces)
                names_len += strlen(ws->utf8_name) + 1;

        if (names_len == 0)
                return;

        DLOG("Got names in %d bytes\n", names_len);
        char *names = smalloc(names_len);
        int pos = 0;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                strcpy(names + pos, ws->utf8_name);
                pos += strlen(ws->utf8_name) + 1;
        }
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_DESKTOP_NAMES, A_UTF8_STRING, 8,
                            names_len,
                            names);
        free(names);
}

/**
 * Updates _NET_NUMBER_OF_DESKTOPS.
 */
void ewmh_update_number_of_desktops()
{
        uint32_t num_workspaces = 0, num_empty_workspaces = 0;
        Workspace *ws = NULL;

        /* Since the EWMH Pager expects to have CURRENT_DESKTOP within
         * [0, NUMBER_OF_DESKTOPS), we even count workspaces without clients.
         * Trim away workspaces at the end that don't have clients. */
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->output == NULL) {
                        num_empty_workspaces++;
                } else {
                        num_workspaces += 1 + num_empty_workspaces;
                        num_empty_workspaces = 0;
                }
        }

        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_NUMBER_OF_DESKTOPS, A_CARDINAL, 32, 1,
                            &num_workspaces);
}

/*
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window) {
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_ACTIVE_WINDOW, A_WINDOW, 32, 1, &window);
}

/**
 * Updates _NET_WM_DESKTOP of the window.
 *
 * EWMH: This can be initially set by the client to request a specific
 * workspace, and can be "all." The window manager is required to keep
 * this property updated.
 */
void ewmh_update_window_desktop(Client *client)
{
        if (client->workspace == NULL) {
                /* If we don't have a workspace, don't lie about it to
                 * the pagers. */
                xcb_delete_property(global_conn, client->child, A__NET_WM_DESKTOP);
                return;
        }

        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, client->child,
                            A__NET_WM_DESKTOP, A_CARDINAL, 32, 1, &client->workspace->num);
}

/*
 * Updates the workarea for each desktop.
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea() {
        Workspace *ws;
        int num_workspaces = 0, count = 0;
        Rect last_rect = {0, 0, 0, 0};

        /* Get the number of workspaces */
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                /* Check if we need to initialize last_rect. The case that the
                 * first workspace is all-zero may happen when the user
                 * assigned workspace 2 for his first screen, for example. Thus
                 * we need an initialized last_rect in the very first run of
                 * the following loop. */
                if (last_rect.width == 0 && last_rect.height == 0 &&
                    ws->rect.width != 0 && ws->rect.height != 0) {
                        memcpy(&last_rect, &(ws->rect), sizeof(Rect));
                }
                num_workspaces++;
        }

        DLOG("Got %d workspaces\n", num_workspaces);
        uint8_t *workarea = smalloc(sizeof(Rect) * num_workspaces);
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                DLOG("storing %d: %dx%d with %d x %d\n", count, ws->rect.x,
                     ws->rect.y, ws->rect.width, ws->rect.height);
                /* If a workspace is not yet initialized and thus its
                 * dimensions are zero, we will instead put the dimensions
                 * of the last workspace in the list. For example firefox
                 * intersects all workspaces and does not cope so well with
                 * an all-zero workspace. */
                if (ws->rect.width == 0 || ws->rect.height == 0) {
                        DLOG("re-using last_rect (%dx%d, %d, %d)\n",
                             last_rect.x, last_rect.y, last_rect.width,
                             last_rect.height);
                        memcpy(workarea + (sizeof(Rect) * count++), &last_rect, sizeof(Rect));
                        continue;
                }
                memcpy(workarea + (sizeof(Rect) * count++), &(ws->rect), sizeof(Rect));
                memcpy(&last_rect, &(ws->rect), sizeof(Rect));
        }
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            A__NET_WORKAREA, A_CARDINAL, 32,
                            num_workspaces * (sizeof(Rect) / sizeof(uint32_t)),
                            workarea);
        free(workarea);
        xcb_flush(global_conn);
}
