/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _EWMH_C
#define _EWMH_C

/**
 * Creates the _NET_SUPPORTING_WM_CHECK window and update the root window
 * property.
 *
 * EWMH: The window is a direct child to the root window, with
 * _NET_WM_NAME set to the name of the window manager.
 */
void ewmh_create_supporting_window();

/**
 * Updates _NET_CLIENT_LIST with a complete list of client window IDs.
 *
 * EWMH: Two properties are used; _NET_CLIENT_LIST in initial mapping order,
 * and _NET_CLIENT_LIST_STACKING in bottom-to-top stacking order.
 */
void ewmh_update_client_list();

/**
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop();

/**
 * Updates _NET_DESKTOP_NAMES with names of all workspaces.
 *
 * EWMH: Simply a list of strings. It doesn't have to match
 * NUMBER_OF_DESKTOPS. Ordinals are used for missing names.
 */
void ewmh_update_desktop_names();

/**
 * Updates _NET_NUMBER_OF_DESKTOPS.
 */
void ewmh_update_number_of_desktops();

/**
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window);

/**
 * Updates _NET_WM_DESKTOP of the window.
 *
 * EWMH: This can be initially set by the client to request a specific
 * workspace, and can be "all." The window manager is required to keep
 * this property updated.
 */
void ewmh_update_window_desktop(Client *client);

/**
 * Updates the workarea for each desktop.
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea();

#endif
