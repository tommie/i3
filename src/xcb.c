/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * xcb.c: Helper functions for easier usage of XCB
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include "i3.h"
#include "util.h"
#include "xcb.h"

TAILQ_HEAD(cached_fonts_head, Font) cached_fonts = TAILQ_HEAD_INITIALIZER(cached_fonts);
unsigned int xcb_numlock_mask;

/*
 * Loads a font for usage, getting its height. This function is used very often, so it
 * maintains a cache.
 *
 */
i3Font *load_font(xcb_connection_t *conn, const char *pattern) {
        /* Check if we got the font cached */
        i3Font *font;
        TAILQ_FOREACH(font, &cached_fonts, fonts)
                if (strcmp(font->pattern, pattern) == 0)
                        return font;

        i3Font *new = smalloc(sizeof(i3Font));
        xcb_void_cookie_t font_cookie;
        xcb_list_fonts_with_info_cookie_t info_cookie;

        /* Send all our requests first */
        new->id = xcb_generate_id(conn);
        font_cookie = xcb_open_font_checked(conn, new->id, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

        check_error(conn, font_cookie, "Could not open font");

        /* Get information (height/name) for this font */
        xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(conn, info_cookie, NULL);
        exit_if_null(reply, "Could not load font \"%s\"\n", pattern);

        if (asprintf(&(new->name), "%.*s", xcb_list_fonts_with_info_name_length(reply),
                                           xcb_list_fonts_with_info_name(reply)) == -1)
                die("asprintf() failed\n");
        new->pattern = sstrdup(pattern);
        new->height = reply->font_ascent + reply->font_descent;

        /* Insert into cache */
        TAILQ_INSERT_TAIL(&cached_fonts, new, fonts);

        return new;
}

/*
 * Returns the colorpixel to use for the given hex color (think of HTML).
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for validity.
 * This has to be done by the caller.
 *
 */
uint32_t get_colorpixel(xcb_connection_t *conn, char *hex) {
        char strgroups[3][3] = {{hex[1], hex[2], '\0'},
                                {hex[3], hex[4], '\0'},
                                {hex[5], hex[6], '\0'}};
        uint32_t rgb16[3] = {(strtol(strgroups[0], NULL, 16)),
                             (strtol(strgroups[1], NULL, 16)),
                             (strtol(strgroups[2], NULL, 16))};

        return (rgb16[0] << 16) + (rgb16[1] << 8) + rgb16[2];
}

/*
 * Convenience wrapper around xcb_create_window which takes care of depth, generating an ID and checking
 * for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect dims, uint16_t window_class, int cursor,
                           bool map, uint32_t mask, uint32_t *values) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_window_t result = xcb_generate_id(conn);
        xcb_cursor_t cursor_id = xcb_generate_id(conn);

        /* If the window class is XCB_WINDOW_CLASS_INPUT_ONLY, depth has to be 0 */
        uint16_t depth = (window_class == XCB_WINDOW_CLASS_INPUT_ONLY ? 0 : XCB_COPY_FROM_PARENT);

        /* Use the default cursor (left pointer) */
        if (cursor > -1) {
                i3Font *cursor_font = load_font(conn, "cursor");
                xcb_create_glyph_cursor(conn, cursor_id, cursor_font->id, cursor_font->id,
                                XCB_CURSOR_LEFT_PTR, XCB_CURSOR_LEFT_PTR + 1,
                                0, 0, 0, 65535, 65535, 65535);
        }

        xcb_create_window(conn,
                          depth,
                          result, /* the window id */
                          root, /* parent == root */
                          dims.x, dims.y, dims.width, dims.height, /* dimensions */
                          0, /* border = 0, we draw our own */
                          window_class,
                          XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                          mask,
                          values);

        if (cursor > -1)
                xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, &cursor_id);

        /* Map the window (= make it visible) */
        if (map)
                xcb_map_window(conn, result);

        return result;
}

/*
 * Changes a single value in the graphic context (so one doesn’t have to define an array of values)
 *
 */
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value) {
        xcb_change_gc(conn, gc, mask, &value);
}

/*
 * Draws a line from x,y to to_x,to_y using the given color
 *
 */
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t to_x, uint32_t to_y) {
        xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, colorpixel);
        xcb_point_t points[] = {{x, y}, {to_x, to_y}};
        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2, points);
}

/*
 * Draws a rectangle from x,y with width,height using the given color
 *
 */
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, colorpixel);
        xcb_rectangle_t rect = {x, y, width, height};
        xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);
}

/*
 * Generates a configure_notify event and sends it to the given window
 * Applications need this to think they’ve configured themselves correctly.
 * The truth is, however, that we will manage them.
 *
 */
void fake_configure_notify(xcb_connection_t *conn, Rect r, xcb_window_t window) {
        xcb_configure_notify_event_t generated_event;

        generated_event.event = window;
        generated_event.window = window;
        generated_event.response_type = XCB_CONFIGURE_NOTIFY;

        generated_event.x = r.x;
        generated_event.y = r.y;
        generated_event.width = r.width;
        generated_event.height = r.height;

        generated_event.border_width = 0;
        generated_event.above_sibling = XCB_NONE;
        generated_event.override_redirect = false;

        xcb_send_event(conn, false, window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*)&generated_event);
        xcb_flush(conn);
}

/*
 * Generates a configure_notify_event with absolute coordinates (relative to the X root
 * window, not to the client’s frame) for the given client.
 *
 */
void fake_absolute_configure_notify(xcb_connection_t *conn, Client *client) {
        Rect absolute;

        absolute.x = client->rect.x + client->child_rect.x;
        absolute.y = client->rect.y + client->child_rect.y;
        absolute.width = client->child_rect.width;
        absolute.height = client->child_rect.height;

        fake_configure_notify(conn, absolute, client->child);
}

/*
 * Finds out which modifier mask is the one for numlock, as the user may change this.
 *
 */
void xcb_get_numlock_mask(xcb_connection_t *conn) {
        xcb_key_symbols_t *keysyms;
        xcb_get_modifier_mapping_cookie_t cookie;
        xcb_get_modifier_mapping_reply_t *reply;
        xcb_keycode_t *modmap;
        int mask, i;
        const int masks[8] = { XCB_MOD_MASK_SHIFT,
                               XCB_MOD_MASK_LOCK,
                               XCB_MOD_MASK_CONTROL,
                               XCB_MOD_MASK_1,
                               XCB_MOD_MASK_2,
                               XCB_MOD_MASK_3,
                               XCB_MOD_MASK_4,
                               XCB_MOD_MASK_5 };

        /* Request the modifier map */
        cookie = xcb_get_modifier_mapping_unchecked(conn);

        /* Get the keysymbols */
        keysyms = xcb_key_symbols_alloc(conn);

        if ((reply = xcb_get_modifier_mapping_reply(conn, cookie, NULL)) == NULL) {
                xcb_key_symbols_free(keysyms);
                return;
        }

        modmap = xcb_get_modifier_mapping_keycodes(reply);

        /* Get the keycode for numlock */
#ifdef OLD_XCB_KEYSYMS_API
        xcb_keycode_t numlock = xcb_key_symbols_get_keycode(keysyms, XCB_NUM_LOCK);
#else
        /* For now, we only use the first keysymbol. */
        xcb_keycode_t *numlock_syms = xcb_key_symbols_get_keycode(keysyms, XCB_NUM_LOCK);
        if (numlock_syms == NULL)
                return;
        xcb_keycode_t numlock = *numlock_syms;
        free(numlock_syms);
#endif

        /* Check all modifiers (Mod1-Mod5, Shift, Control, Lock) */
        for (mask = 0; mask < 8; mask++)
                for (i = 0; i < reply->keycodes_per_modifier; i++)
                        if (modmap[(mask * reply->keycodes_per_modifier) + i] == numlock)
                                xcb_numlock_mask = masks[mask];

        xcb_key_symbols_free(keysyms);
        free(reply);
}

/*
 * Raises the given window (typically client->frame) above all other windows
 *
 */
void xcb_raise_window(xcb_connection_t *conn, xcb_window_t window) {
        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
        xcb_configure_window(conn, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

/*
 *
 * Prepares the given Cached_Pixmap for usage (checks whether the size of the
 * object this pixmap is related to (e.g. a window) has changed and re-creates
 * the pixmap if so).
 *
 */
void cached_pixmap_prepare(xcb_connection_t *conn, struct Cached_Pixmap *pixmap) {
        LOG("preparing pixmap\n");

        /* If the Rect did not change, the pixmap does not need to be recreated */
        if (memcmp(&(pixmap->rect), pixmap->referred_rect, sizeof(Rect)) == 0)
                return;

        memcpy(&(pixmap->rect), pixmap->referred_rect, sizeof(Rect));

        if (pixmap->id == 0 || pixmap->gc == 0) {
                LOG("Creating new pixmap...\n");
                pixmap->id = xcb_generate_id(conn);
                pixmap->gc = xcb_generate_id(conn);
        } else {
                LOG("Re-creating this pixmap...\n");
                xcb_free_gc(conn, pixmap->gc);
                xcb_free_pixmap(conn, pixmap->id);
        }

        xcb_create_pixmap(conn, root_depth, pixmap->id,
                          pixmap->referred_drawable, pixmap->rect.width, pixmap->rect.height);

        xcb_create_gc(conn, pixmap->gc, pixmap->id, 0, 0);
}

/*
 * Returns the xcb_charinfo_t for the given character (specified by row and
 * column in the lookup table) if existing, otherwise the minimum bounds.
 *
 */
static xcb_charinfo_t *get_charinfo(int col, int row, xcb_query_font_reply_t *font_info,
                                    xcb_charinfo_t *table, bool dont_fallback) {
        xcb_charinfo_t *result;

        /* Bounds checking */
        if (row < font_info->min_byte1 || row > font_info->max_byte1 ||
            col < font_info->min_char_or_byte2 || col > font_info->max_char_or_byte2)
                return NULL;

        /* If we don’t have a table to lookup the infos per character, return the
         * minimum bounds */
        if (table == NULL)
                return &font_info->min_bounds;

        result = &table[((row - font_info->min_byte1) *
                         (font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1)) +
                        (col - font_info->min_char_or_byte2)];

        /* If the character has an entry in the table, return it */
        if (result->character_width != 0 ||
            (result->right_side_bearing |
             result->left_side_bearing |
             result->ascent |
             result->descent) != 0)
                return result;

        /* Otherwise, get the default character and return its charinfo */
        if (dont_fallback)
                return NULL;

        return get_charinfo((font_info->default_char >> 8),
                            (font_info->default_char & 0xFF),
                            font_info,
                            table,
                            true);
}

/*
 * Calculate the width of the given text (16-bit characters, UCS) with given
 * real length (amount of glyphs) using the given font.
 *
 */
int predict_text_width(xcb_connection_t *conn, const char *font_pattern, char *text, int length) {
        xcb_query_font_reply_t *font_info;
        xcb_charinfo_t *table;
        xcb_generic_error_t *error;
        int i, width = 0;
        i3Font *font = load_font(conn, font_pattern);

        font_info = xcb_query_font_reply(conn, xcb_query_font(conn, font->id), &error);
        if (error != NULL) {
                fprintf(stderr, "ERROR: query font (X error code %d)\n", error->error_code);
                /* We return the rather safe guess of 7 pixels, because a
                 * rendering error is better than a crash. Plus, the user will
                 * see the error on his stderr. */
                return 7;
        }

        /* If no per-char info is available for this font, we use the default */
        if (xcb_query_font_char_infos_length(font_info) == 0) {
                LOG("Falling back on default char_width of %d pixels\n", font_info->max_bounds.character_width);
                return (font_info->max_bounds.character_width * length);
        }

        table = xcb_query_font_char_infos(font_info);

        for (i = 0; i < 2 * length; i += 2) {
                xcb_charinfo_t *info = get_charinfo(text[i+1], text[i], font_info, table, false);
                if (info == NULL)
                        continue;
                width += info->character_width;
        }

        free(font_info);

        return width;
}
