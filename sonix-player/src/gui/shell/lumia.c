#include "lumia.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/main_menu.h"
#include "src/gui/shell/morepage.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/library/music.h"
#include "src/gui/library/search.h"
#include "src/gui/library/audiobooks.h"
#include "src/gui/library/libraryscan.h"
#include "src/gui/library/medialist.h"
#include "src/gui/settings/settings.h"
#include "src/gui/settings/musicsettings.h"
#include "src/gui/settings/appearance.h"
#include "src/gui/settings/fwupdate.h"
#include "src/gui/bluetooth/btsettings.h"
#include "src/gui/streaming/radiopage.h"
#include "src/gui/streaming/streaming.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/nowplaying/coverloader.h"
#include "src/system/playback/device_state.h"
#include "src/system/playback/sleeptimer.h"
#include "src/system/core/config.h"

/* Independent presentation screens use the existing playback and navigation
 * services. No mock music, borrowed cover pointers, or hardware calls here. */
#define CYAN 0x00AEEF
#define SLOT 108
#define ROWS 4
static lv_obj_t *library_page, *playing_page, *settings_page;
static lv_obj_t *home_title, *home_artist, *play_title, *play_artist, *time_label, *total_label;
static lv_obj_t *home_play_icon, *play_icon, *seek_bar, *list_box, *page_label;
static lv_obj_t *art[9], *fallback[9], *sleep_value;
static cover_image_t covers[9];
static lv_obj_t *tabs[3];
static library_index_t *index_handle;
static int kind, offset, count, filled, screen_w, screen_h, top, playing_cover_side;
static char names[ROWS][512], paths[ROWS][512], track_path[512];
static bool seeking;
static lv_obj_t *seen_screen;

static lv_obj_t *text(lv_obj_t *parent, const char *s, int x, int y, int w, lv_font_t *font, uint32_t color) {
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, s);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    lv_obj_set_clickable(o, false);
    return o;
}
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(o, false);
    return o;
}
static lv_obj_t *glyph(lv_obj_t *parent, const lv_image_dsc_t *src, int cx, int cy, int size) {
    lv_obj_t *o = lv_image_create(parent);
    lv_image_set_src(o, src);
    /* Scale the complete transparent glyph, never crop a decorated PNG. */
    lv_image_set_scale(o, (uint32_t)size * 256 / src->header.w);
    lv_obj_set_pos(o, cx - (int)src->header.w / 2, cy - (int)src->header.h / 2);
    lv_obj_set_style_image_recolor(o, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_clickable(o, false);
    return o;
}
static void clicked(lv_event_t *e);
static lv_obj_t *button(lv_obj_t *p, int x, int y, int w, int h, const char *label,
                        const lv_image_dsc_t *icon, int action, uint32_t color) {
    lv_obj_t *o = box(p, x, y, w, h, color);
    lv_obj_set_clickable(o, true);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x087EAA), LV_STATE_PRESSED);
    lv_obj_add_event_cb(o, clicked, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    if (icon) glyph(o, icon, w / 2, h / 2 - (label ? 12 : 0), h > 150 ? 80 : 34);
    if (label) text(o, label, 8, h - 32, w - 16, w < 120 ? &font_ui_16 : &font_ui_22, 0xffffff);
    return o;
}
static void header(lv_obj_t *page, const char *title) {
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(page, false);
    text(page, "SONIX", 20, top, screen_w - 40, &font_ui_20, 0xffffff);
    text(page, title, 18, top + 25, screen_w - 36, &font_ui_48, 0xffffff);
}
static void footer(lv_obj_t *p, int action) {
    lv_obj_t *b = button(p, 20, screen_h - 62, 48, 48, NULL, &icon_chevron_left, 0, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, lv_color_white(), 0);
    button(p, screen_w - 72, screen_h - 62, 52, 48, NULL, &icon_ellipsis_vertical, action, 0);
}
static void clear_art(int n) {
    if (art[n]) lv_image_set_src(art[n], &icon_album_big);
    coverloader_release(SLOT + n);
    cover_free(&covers[n]);
    if (art[n]) lv_obj_set_hidden(art[n], true);
    if (fallback[n]) lv_obj_set_hidden(fallback[n], false);
}
static void artwork(lv_obj_t *parent, int n, int x, int y, int side) {
    lv_obj_t *b = box(parent, x, y, side, side, 0x102A39);
    fallback[n] = glyph(b, &icon_album_big, side / 2, side / 2, side / 2);
    art[n] = lv_image_create(b);
    lv_obj_set_hidden(art[n], true);
    lv_obj_set_clickable(b, false);
    lv_obj_set_clickable(art[n], false);
}
static void refresh_library(void);
static void seek_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) seeking = true;
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        device_state_seek(lv_slider_get_value(seek_bar));
        seeking = false;
    }
}
static void clicked(lv_event_t *e) {
    int a = (int)(intptr_t)lv_event_get_user_data(e);
    if (switcher_back_drag_active() || player_sheet_drag_active()) return;
    switch (a) {
    case 0: screen_history_reset(); switch_screen_no_history(main_menu_screen); break;
    case 1: switch_screen(library_page); break;
    case 2: switch_screen(playing_page); break;
    case 3: device_state_toggle_play_pause(); break;
    case 4: switch_screen(btsettings_screen); break;
    case 5: switch_screen(radiopage_screen); break;
    case 6: search_open(); break;
    case 7: switch_screen(settings_page); break;
    case 8: switch_screen(audiobooks_screen); break;
    case 9: switch_screen(morepage_screen); break;
    case 10: player_screen_prev(); break;
    case 11: player_screen_next(); break;
    case 12: player_sheet_open(true); break;
    case 13: switch_screen(settings_screen); break;
    case 14: switch_screen(musicsettings_screen); break;
    case 15: switch_screen(appearance_screen); break;
    case 16: fwupdate_show(); break;
    case 17: {
        int m = sleeptimer_enabled(SLEEPTIMER_MUSIC) ? sleeptimer_minutes(SLEEPTIMER_MUSIC) : 0;
        m = m == 0 ? 15 : m == 15 ? 30 : m == 30 ? 60 : 0;
        sleeptimer_set_minutes(SLEEPTIMER_MUSIC, m);
        sleeptimer_set_enabled(SLEEPTIMER_MUSIC, m != 0);
        break;
    }
    case 18: libraryscan_choose_folders(); break;
    case 19: switch_screen(music_screen); break;
    case 20: case 21: case 22: kind = a - 20; offset = 0; refresh_library(); break;
    case 23: if (offset >= ROWS) offset -= ROWS; refresh_library(); break;
    case 24: if (offset + ROWS < count) offset += ROWS; refresh_library(); break;
    case 25: switch_screen(streaming_screen); break;
    default:
        if (a >= 100 && a < 100 + filled) {
            int n = a - 100;
            if (kind == 0) medialist_open("album", LIBRARY_LIST_TRACKS, LIBRARY_FILTER_ALBUM, names[n]);
            else if (kind == 1) medialist_open(names[n], LIBRARY_LIST_ALBUMS, LIBRARY_FILTER_ARTIST, names[n]);
            else { player_play_file(paths[n]); switch_screen(playing_page); }
        }
    }
}
static bool list_row(const char *name, const char *path, const char *artist, void *unused) {
    (void)unused;
    if (filled >= ROWS) return false;
    int n = filled++;
    snprintf(names[n], sizeof(names[n]), "%s", name ? name : "");
    snprintf(paths[n], sizeof(paths[n]), "%s", path ? path : "");
    char title[512];
    if (kind == 0) library_album_title(names[n], title, sizeof(title));
    else snprintf(title, sizeof(title), "%s", names[n]);
    if (kind == 0) {
        int side = (screen_w - 64) / 2;
        int max_side = (screen_h - top - 256 - 116) / 2;
        if (side > max_side) side = max_side;
        int x = (screen_w - 48 - 2 * side - 16) / 2 + (n % 2) * (side + 16);
        int y = (n / 2) * (side + 62);
        lv_obj_t *b = button(list_box, x, y, side, side + 54, NULL, NULL, 100 + n, 0);
        artwork(b, n, 0, 0, side);
        text(b, title, 0, side + 4, side, &font_ui_20, 0xffffff);
        text(b, artist ? artist : "", 0, side + 29, side, &font_ui_16, 0xaaaaaa);
        if (path && *path) coverloader_request(SLOT + n, path, side);
    } else {
        lv_obj_t *b = button(list_box, 0, n * 96, screen_w - 40, 90, NULL, NULL, 100 + n, 0);
        text(b, title, 0, 12, screen_w - 44, &font_ui_28, 0xffffff);
        text(b, artist ? artist : "", 0, 51, screen_w - 44, &font_ui_20, CYAN);
    }
    return true;
}
static void refresh_library(void) {
    for (int n = 0; n < ROWS; n++) { clear_art(n); art[n] = fallback[n] = NULL; }
    lv_obj_clean(list_box);
    if (index_handle) library_index_close(index_handle);
    static const library_list_t types[] = {LIBRARY_LIST_ALBUMS, LIBRARY_LIST_ARTISTS, LIBRARY_LIST_TRACKS};
    index_handle = library_index_open(types[kind], LIBRARY_FILTER_NONE, NULL, LIBRARY_ORDER_DEFAULT, false);
    count = index_handle ? library_index_count(index_handle) : 0;
    if (offset >= count) offset = 0;
    filled = 0;
    if (index_handle) library_index_window(index_handle, offset, ROWS, list_row, NULL);
    if (!filled) {
        text(list_box, "Your music starts here", 0, 40, screen_w - 48, &font_ui_28, 0xffffff);
        button(list_box, 0, 104, screen_w - 48, 64, "scan music", NULL, 18, CYAN);
    }
    for (int n = 0; n < 3; n++) lv_obj_set_style_border_width(tabs[n], n == kind ? 3 : 0, 0);
    lv_label_set_text_fmt(page_label, "%d / %d", count ? offset / ROWS + 1 : 0, (count + ROWS - 1) / ROWS);
}
static bool home_album(const char *name, const char *path, const char *artist, void *user) {
    (void)name; (void)artist;
    int *n = user;
    if (*n >= 3) return false;
    if (path && *path) coverloader_request(SLOT + 6 + *n, path, 96);
    (*n)++;
    return true;
}
static void refresh_home_albums(void) {
    for (int n = 6; n < 9; n++) clear_art(n);
    library_index_t *ix = library_index_open(LIBRARY_LIST_ALBUMS, LIBRARY_FILTER_NONE, NULL, LIBRARY_ORDER_DEFAULT, false);
    if (ix) { int n = 0; library_index_window(ix, 0, 3, home_album, &n); library_index_close(ix); }
}
static void tick(lv_timer_t *timer) {
    (void)timer;
    lv_obj_t *active = lv_screen_active();
    if (active != main_menu_screen && active != library_page && active != playing_page && active != settings_page) {
        seen_screen = active;
        return;
    }
    if (active == library_page && (seen_screen != active || (index_handle && library_index_stale(index_handle)))) refresh_library();
    if (active == main_menu_screen && seen_screen != active) refresh_home_albums();
    seen_screen = active;
    device_state_t s;
    device_state_get(&s);
    const char *title = s.metadata.title[0] ? s.metadata.title : s.current_file[0] ? strrchr(s.current_file, '/') : NULL;
    if (title && title[0] == '/') title++;
    if (!title || !*title) title = "Choose some music";
    lv_label_set_text(home_title, title);
    lv_label_set_text(play_title, title);
    lv_label_set_text(home_artist, s.metadata.artist);
    lv_label_set_text(play_artist, s.metadata.artist);
    bool playing = s.status == AUDIO_STATUS_PLAYING;
    lv_image_set_src(home_play_icon, playing ? &icon_pause : &icon_play);
    lv_image_set_src(play_icon, playing ? &icon_pause : &icon_play);
    if (!seeking) {
        lv_slider_set_range(seek_bar, 0, s.progress_total_secs > 0 ? (int)s.progress_total_secs : 1);
        lv_slider_set_value(seek_bar, (int)s.progress_current_secs, LV_ANIM_OFF);
    }
    int p = (int)s.progress_current_secs, t = (int)s.progress_total_secs;
    lv_label_set_text_fmt(time_label, "%d:%02d", p / 60, p % 60);
    lv_label_set_text_fmt(total_label, "%d:%02d", t / 60, t % 60);
    int minutes = sleeptimer_enabled(SLEEPTIMER_MUSIC) ? sleeptimer_minutes(SLEEPTIMER_MUSIC) : 0;
    if (minutes) lv_label_set_text_fmt(sleep_value, "%d minutes · tap to change", minutes);
    else lv_label_set_text(sleep_value, "Off · tap for 15 / 30 / 60 min");
    const char *path = s.live ? s.live_cover : s.current_file;
    if (strcmp(track_path, path)) {
        snprintf(track_path, sizeof(track_path), "%s", path);
        for (int n = 4; n < 6; n++) {
            clear_art(n);
            if (*path) coverloader_request(SLOT + n, path, n == 4 ? 112 : playing_cover_side);
        }
    }
    for (int n = 0; n < 9; n++) {
        cover_image_t got = {0}; bool finished = false;
        if (art[n] && coverloader_take(SLOT + n, &got, &finished)) {
            lv_image_set_src(art[n], &icon_album_big);
            cover_free(&covers[n]); covers[n] = got;
            lv_image_set_src(art[n], &covers[n].dsc);
            lv_obj_set_hidden(art[n], false); lv_obj_set_hidden(fallback[n], true);
        }
    }
}
static lv_obj_t *setting_row(const char *name, const char *value, int y, int action) {
    lv_obj_t *b = button(settings_page, 20, y, screen_w - 40, 84, NULL, NULL, action, 0);
    text(b, name, 0, 0, screen_w - 48, &font_ui_28, 0xffffff);
    return text(b, value, 0, 40, screen_w - 48, &font_ui_20, CYAN);
}
void lumia_init(gui_config_t *cfg) {
    screen_w = cfg->screen_width; screen_h = cfg->screen_height; top = cfg->top_bar_height + 12;
    library_page = lv_obj_create(NULL); playing_page = lv_obj_create(NULL); settings_page = lv_obj_create(NULL);
    header(main_menu_screen, "start"); header(library_page, "music");
    header(playing_page, "now playing"); header(settings_page, "settings");
    int y = top + 106, w = screen_w - 32;
    lv_obj_t *now = button(main_menu_screen, 16, y, w, 136, NULL, NULL, 2, CYAN);
    artwork(now, 4, 10, 12, 112);
    home_title = text(now, "Choose some music", 134, 18, w - 190, &font_ui_24, 0xffffff);
    home_artist = text(now, "", 134, 55, w - 145, &font_ui_20, 0xffffff);
    text(now, "now playing", 134, 99, w - 145, &font_ui_18, 0xffffff);
    lv_obj_t *pause = button(now, w - 52, 10, 48, 50, NULL, NULL, 3, CYAN);
    home_play_icon = glyph(pause, &icon_play, 24, 25, 32);
    y += 144;
    int half = (w - 8) / 2, small = (half - 8) / 2;
    button(main_menu_screen, 16, y, half, 196, "music", &icon_music_note, 1, CYAN);
    button(main_menu_screen, 24 + half, y, small, 94, "bluetooth", &icon_bluetooth, 4, CYAN);
    button(main_menu_screen, 32 + half + small, y, small, 94, "radio", &icon_radio_player, 5, CYAN);
    button(main_menu_screen, 24 + half, y + 102, small, 94, "search", &icon_search, 6, CYAN);
    button(main_menu_screen, 32 + half + small, y + 102, small, 94, "settings", &icon_music_settings, 7, CYAN);
    y += 204;
    lv_obj_t *albums = button(main_menu_screen, 16, y, w, 132, "albums", NULL, 1, 0x102A39);
    for (int n = 0; n < 3; n++) artwork(albums, 6 + n, 8 + n * (w - 16) / 3, 6, 96);
    y += 140;
    button(main_menu_screen, 16, y, half, 62, "audiobooks", NULL, 8, CYAN);
    button(main_menu_screen, 24 + half, y, half, 62, "more", NULL, 9, CYAN);
    y += 68;
    button(main_menu_screen, 16, y, w, 48, "streaming services", NULL, 25, 0);
    /* The layout remains usable on a 480x720 host as well as the R1 panel. */
    lv_obj_set_scrollable(main_menu_screen, true); lv_obj_set_scroll_dir(main_menu_screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(main_menu_screen, LV_SCROLLBAR_MODE_AUTO);
    const char *labels[] = {"albums", "artists", "songs"};
    for (int n = 0; n < 3; n++) {
        tabs[n] = button(library_page, 20 + n * (screen_w - 40) / 3, top + 96, (screen_w - 40) / 3, 52, labels[n], NULL, 20 + n, 0);
        lv_obj_set_style_border_side(tabs[n], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(tabs[n], lv_color_hex(CYAN), 0);
    }
    list_box = box(library_page, 24, top + 160, screen_w - 48, screen_h - top - 256, 0);
    lv_obj_set_scrollable(list_box, true); lv_obj_set_scroll_dir(list_box, LV_DIR_VER);
    button(library_page, screen_w / 2 - 95, screen_h - 62, 48, 48, NULL, &icon_chevron_left, 23, 0);
    page_label = text(library_page, "", screen_w / 2 - 35, screen_h - 50, 80, &font_ui_18, 0xffffff);
    button(library_page, screen_w / 2 + 45, screen_h - 62, 48, 48, NULL, &icon_chevron_right, 24, 0);
    footer(library_page, 19);
    int side = screen_w - 144;
    if (side > screen_h - top - 390) side = screen_h - top - 390;
    playing_cover_side = side;
    artwork(playing_page, 5, (screen_w - side) / 2, top + 104, side);
    int py = top + 114 + side;
    play_title = text(playing_page, "Choose some music", 20, py, screen_w - 40, &font_ui_32, 0xffffff);
    play_artist = text(playing_page, "", 20, py + 44, screen_w - 40, &font_ui_24, CYAN);
    seek_bar = lv_slider_create(playing_page);
    lv_obj_set_pos(seek_bar, 24, py + 90); lv_obj_set_size(seek_bar, screen_w - 48, 8);
    lv_obj_set_style_bg_color(seek_bar, lv_color_hex(CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(seek_bar, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_add_event_cb(seek_bar, seek_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(seek_bar, seek_event, LV_EVENT_RELEASED, NULL);
    time_label = text(playing_page, "", 20, py + 106, 120, &font_ui_18, 0xffffff);
    total_label = text(playing_page, "", screen_w - 140, py + 106, 120, &font_ui_18, 0xffffff);
    lv_obj_set_style_text_align(total_label, LV_TEXT_ALIGN_RIGHT, 0);
    for (int n = 0; n < 3; n++) {
        lv_obj_t *b = button(playing_page, screen_w / 2 - 132 + n * 96, py + 142, 72, 72, NULL, NULL, n == 0 ? 10 : n == 1 ? 3 : 11, 0);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, lv_color_white(), 0);
        lv_obj_t *g = glyph(b, n == 0 ? &icon_skip_back : n == 1 ? &icon_play : &icon_skip_forward, 36, 36, 32);
        if (n == 1) play_icon = g;
    }
    footer(playing_page, 12);
    /* Short panels can scroll the controls instead of clipping them. */
    if (py + 214 > screen_h - 68) lv_obj_set_scrollable(playing_page, true);
    text(settings_page, "system", 20, top + 96, 200, &font_ui_28, CYAN);
    button(settings_page, 230, top + 90, 150, 50, "audio", NULL, 14, 0);
    int sy = top + 154;
    setting_row("Bluetooth", "Devices and pairing", sy, 4);
    setting_row("Display & system", "Brightness, power and controls", sy + 84, 13);
    sleep_value = setting_row("Sleep timer", "Off", sy + 168, 17);
    setting_row("Appearance", "Theme and text size", sy + 252, 15);
    setting_row("Firmware update", "Check for updates", sy + 336, 16);
    footer(settings_page, 13);
    switcher_attach_back_gesture(library_page); switcher_attach_back_gesture(playing_page); switcher_attach_back_gesture(settings_page);
    lv_timer_create(tick, 250, NULL);
}

bool lumia_owns_navigation(lv_obj_t *screen) {
    return screen && (screen == library_page || screen == playing_page || screen == settings_page);
}
