#ifndef GUI_BOTTOMNAV_H
#define GUI_BOTTOMNAV_H

#include "lvgl/lvgl.h"

#include "src/gui/shell/gui.h"

#define BOTTOMNAV_HEIGHT 78

// A shared three-destination bar for the main library surfaces. It only
// changes screens; playback and page-specific controls remain with their
// existing owners.
lv_obj_t *bottomnav_create(lv_obj_t *screen, gui_config_t *cfg, int selected);
void bottomnav_set_visible(lv_obj_t *bar, bool visible);
void bottomnav_init(gui_config_t *cfg);
void bottomnav_search_keyboard(bool visible);

#endif
