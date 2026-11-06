#include "bottomnav.h"

#include <stdint.h>

#include "src/gui/library/music.h"
#include "src/gui/library/search.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/nowplaying/coverflow.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/main_menu.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "lvgl/lvgl.h"

typedef struct {
	const char *label;
	const lv_image_dsc_t *icon;
	lv_obj_t *screen;
} destination_t;

static lv_obj_t *search_bar;

static void destination_cb(lv_event_t *e) {
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}
	if (player_sheet_drag_active() || switcher_back_drag_active() || coverflow_drag_active()) {
		return;
	}
	lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
	if (!target || target == lv_screen_active()) {
		return;
	}
	// Bottom navigation is a top-level jump. Clearing the page stack makes
	// Back from a destination return to Home, while each feature keeps its own
	// internal history when opened from that destination.
	screen_history_reset();
	switch_screen_no_history(target);
}

lv_obj_t *bottomnav_create(lv_obj_t *screen, gui_config_t *cfg, int selected) {
	if (!screen || !cfg) {
		return NULL;
	}
	const destination_t destinations[] = {
		{"Home", &icon_music2, main_menu_screen},
		{"Search", &icon_search, search_screen},
		{"Your Library", &icon_list_music, music_screen},
	};

	lv_obj_t *bar = lv_obj_create(screen);
	lv_obj_remove_style_all(bar);
	lv_obj_set_size(bar, cfg->screen_width, BOTTOMNAV_HEIGHT);
	lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_obj_add_style(bar, &theme_style_panel, 0);
	lv_obj_set_style_border_width(bar, 0, 0);
	lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
	lv_obj_set_style_border_color(bar, theme()->surface_pressed, 0);
	lv_obj_set_style_border_width(bar, 1, 0);
	lv_obj_set_style_pad_hor(bar, 8, 0);
	lv_obj_set_style_pad_ver(bar, 5, 0);
	lv_obj_set_style_pad_column(bar, 4, 0);
	lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_scrollable(bar, false);

	for (int i = 0; i < 3; i++) {
		lv_obj_t *button = lv_btn_create(bar);
		lv_obj_set_size(button, (cfg->screen_width - 28) / 3, BOTTOMNAV_HEIGHT - 12);
		lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
		lv_obj_add_style(button, &theme_style_card_pressed, LV_STATE_PRESSED);
		lv_obj_set_style_border_width(button, 0, 0);
		lv_obj_set_style_shadow_width(button, 0, 0);
		lv_obj_set_style_radius(button, 18, 0);
		lv_obj_set_style_pad_all(button, 2, 0);
		lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_gap(button, 1, 0);
		lv_obj_add_event_cb(button, destination_cb, LV_EVENT_CLICKED, destinations[i].screen);

		lv_obj_t *icon_bg = lv_obj_create(button);
		lv_obj_remove_style_all(icon_bg);
		lv_obj_set_size(icon_bg, 54, 32);
		lv_obj_set_style_radius(icon_bg, LV_RADIUS_CIRCLE, 0);
		lv_obj_add_style(icon_bg, &theme_style_switch_checked, 0);
		lv_obj_set_style_bg_opa(icon_bg, i == selected ? LV_OPA_30 : LV_OPA_TRANSP, 0);
		lv_obj_set_clickable(icon_bg, false);
		lv_obj_set_scrollable(icon_bg, false);

		lv_obj_t *icon = lv_image_create(icon_bg);
		lv_image_set_src(icon, destinations[i].icon);
		lv_obj_add_style(icon, &theme_style_icon, 0);
		lv_obj_center(icon);

		lv_obj_t *label = lv_label_create(button);
		lv_label_set_text(label, destinations[i].label);
		lv_obj_add_style(label, i == selected ? &theme_style_text : &theme_style_text_dim, 0);
		lv_obj_set_style_text_font(label, &font_ui_18, 0);
		lv_obj_set_width(label, lv_pct(100));
		lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_set_clickable(label, false);
	}

	return bar;
}

void bottomnav_set_visible(lv_obj_t *bar, bool visible) {
	if (bar) {
		lv_obj_set_hidden(bar, !visible);
	}
}

void bottomnav_init(gui_config_t *cfg) {
	if (!cfg) {
		return;
	}
	/* Lumia Start owns its navigation and uses the whole canvas. */
	search_bar = bottomnav_create(search_screen, cfg, 1);
	bottomnav_create(music_screen, cfg, 2);
	bottomnav_set_visible(search_bar, false);
}

void bottomnav_search_keyboard(bool visible) { bottomnav_set_visible(search_bar, !visible); }
