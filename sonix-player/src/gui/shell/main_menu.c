#include "main_menu.h"

#include <stdint.h>

#include "src/gui/library/audiobooks.h"
#include "src/gui/shell/gridpage.h"
#include "src/gui/shell/bottomnav.h"
#include "src/gui/shell/gui.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/morepage.h"
#include "src/gui/library/music.h"
#include "src/gui/settings/settings.h"
#include "src/gui/streaming/streaming.h"
#include "src/gui/wireless/wireless.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/gui/fonts/fonts.h"

#include "lvgl/lvgl.h"

lv_obj_t *main_menu_screen;

void main_menu_init(gui_config_t *cfg) {
	// Music and Audiobooks both open with the card out or at a computer, and
	// say so on their own page.
	const grid_entry_t entries[] = {
		{"music", &icon_menu_music, &music_screen, NULL},
		{"streaming", &icon_menu_streaming, &streaming_screen, NULL},
		{"wireless", &icon_menu_wireless, &wireless_screen, NULL},
		{"audiobooks", &icon_menu_audiobooks, &audiobooks_screen, NULL},
		// The door to the More page, which holds the DAC and anything else
		// that does not fit. The main menu has exactly six tiles, and one of
		// them has to be able to keep growing.
		{"more", &icon_menu_more, &morepage_screen, NULL},
		{"settings", &icon_menu_settings, &settings_screen, NULL},
	};

	// Reserve a compact title row and the shared destination bar. This only
	// changes the landing-page geometry; all six original entry points remain.
	lv_obj_t *title = lv_label_create(main_menu_screen);
	lv_label_set_text(title, "Home");
	lv_obj_add_style(title, &theme_style_text, 0);
	lv_obj_set_style_text_font(title, &font_ui_36_bold, 0);
	lv_obj_align(title, LV_ALIGN_TOP_LEFT, cfg->padding, cfg->top_bar_height + cfg->padding + 2);
	gridpage_build_reserved(main_menu_screen, cfg, entries, (int)(sizeof(entries) / sizeof(entries[0])), 2, 3, false,
						62, BOTTOMNAV_HEIGHT);
}
