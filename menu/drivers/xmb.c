/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018 - Alfredo Monclús
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <file/file_path.h>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <gfx/math/matrix_4x4.h>
#include <streams/file_stream.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"
#include "../menu_entries.h"
#include "../menu_input.h"
#include "../menu_thumbnail_path.h"

#include "../../core_info.h"
#include "../../core.h"

#include "../widgets/menu_input_dialog.h"
#include "../widgets/menu_osk.h"
#include "../widgets/menu_filebrowser.h"

#include "../../verbosity.h"
#include "../../dynamic.h"
#include "../../configuration.h"
#include "../../playlist.h"
#include "../../retroarch.h"

#include "../../tasks/tasks_internal.h"

#include "../../cheevos-new/badges.h"
#include "../../content.h"

#define XMB_RIBBON_ROWS 64
#define XMB_RIBBON_COLS 64
#define XMB_RIBBON_VERTICES 2*XMB_RIBBON_COLS*XMB_RIBBON_ROWS-2*XMB_RIBBON_COLS

#ifndef XMB_DELAY
#define XMB_DELAY 166
#endif

#if 0
#define XMB_DEBUG
#endif

/* NOTE: If you change this you HAVE to update
 * xmb_alloc_node() and xmb_copy_node() */
typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   uintptr_t icon;
   uintptr_t content_icon;
   char *fullpath;
} xmb_node_t;

enum
{
   XMB_TEXTURE_MAIN_MENU = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_HISTORY,
   XMB_TEXTURE_FAVORITES,
   XMB_TEXTURE_MUSICS,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   XMB_TEXTURE_MOVIES,
#endif
#ifdef HAVE_NETWORKING
   XMB_TEXTURE_NETPLAY,
   XMB_TEXTURE_ROOM,
   XMB_TEXTURE_ROOM_LAN,
   XMB_TEXTURE_ROOM_RELAY,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_TEXTURE_IMAGES,
#endif
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_CLOSE,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_UNDO,
   XMB_TEXTURE_CORE_INFO,
   XMB_TEXTURE_WIFI,
   XMB_TEXTURE_CORE_OPTIONS,
   XMB_TEXTURE_INPUT_REMAPPING_OPTIONS,
   XMB_TEXTURE_CHEAT_OPTIONS,
   XMB_TEXTURE_DISK_OPTIONS,
   XMB_TEXTURE_SHADER_OPTIONS,
   XMB_TEXTURE_ACHIEVEMENT_LIST,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_RENAME,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
   XMB_TEXTURE_FAVORITE,
   XMB_TEXTURE_ADD_FAVORITE,
   XMB_TEXTURE_MUSIC,
   XMB_TEXTURE_IMAGE,
   XMB_TEXTURE_MOVIE,
   XMB_TEXTURE_CORE,
   XMB_TEXTURE_RDB,
   XMB_TEXTURE_CURSOR,
   XMB_TEXTURE_SWITCH_ON,
   XMB_TEXTURE_SWITCH_OFF,
   XMB_TEXTURE_CLOCK,
   XMB_TEXTURE_BATTERY_FULL,
   XMB_TEXTURE_BATTERY_CHARGING,
   XMB_TEXTURE_BATTERY_80,
   XMB_TEXTURE_BATTERY_60,
   XMB_TEXTURE_BATTERY_40,
   XMB_TEXTURE_BATTERY_20,
   XMB_TEXTURE_POINTER,
   XMB_TEXTURE_ADD,
   XMB_TEXTURE_KEY,
   XMB_TEXTURE_KEY_HOVER,
   XMB_TEXTURE_DIALOG_SLICE,
   XMB_TEXTURE_ACHIEVEMENTS,
   XMB_TEXTURE_AUDIO,
   XMB_TEXTURE_EXIT,
   XMB_TEXTURE_FRAMESKIP,
   XMB_TEXTURE_INFO,
   XMB_TEXTURE_HELP,
   XMB_TEXTURE_NETWORK,
   XMB_TEXTURE_POWER,
   XMB_TEXTURE_SAVING,
   XMB_TEXTURE_UPDATER,
   XMB_TEXTURE_VIDEO,
   XMB_TEXTURE_RECORD,
   XMB_TEXTURE_INPUT_SETTINGS,
   XMB_TEXTURE_MIXER,
   XMB_TEXTURE_LOG,
   XMB_TEXTURE_OSD,
   XMB_TEXTURE_UI,
   XMB_TEXTURE_USER,
   XMB_TEXTURE_PRIVACY,
   XMB_TEXTURE_LATENCY,
   XMB_TEXTURE_DRIVERS,
   XMB_TEXTURE_PLAYLIST,
   XMB_TEXTURE_QUICKMENU,
   XMB_TEXTURE_REWIND,
   XMB_TEXTURE_OVERLAY,
   XMB_TEXTURE_OVERRIDE,
   XMB_TEXTURE_NOTIFICATIONS,
   XMB_TEXTURE_STREAM,
   XMB_TEXTURE_SHUTDOWN,
   XMB_TEXTURE_INPUT_DPAD_U,
   XMB_TEXTURE_INPUT_DPAD_D,
   XMB_TEXTURE_INPUT_DPAD_L,
   XMB_TEXTURE_INPUT_DPAD_R,
   XMB_TEXTURE_INPUT_STCK_U,
   XMB_TEXTURE_INPUT_STCK_D,
   XMB_TEXTURE_INPUT_STCK_L,
   XMB_TEXTURE_INPUT_STCK_R,
   XMB_TEXTURE_INPUT_STCK_P,
   XMB_TEXTURE_INPUT_SELECT,
   XMB_TEXTURE_INPUT_START,
   XMB_TEXTURE_INPUT_BTN_U,
   XMB_TEXTURE_INPUT_BTN_D,
   XMB_TEXTURE_INPUT_BTN_L,
   XMB_TEXTURE_INPUT_BTN_R,
   XMB_TEXTURE_INPUT_LB,
   XMB_TEXTURE_INPUT_RB,
   XMB_TEXTURE_INPUT_LT,
   XMB_TEXTURE_INPUT_RT,
   XMB_TEXTURE_INPUT_ADC,
   XMB_TEXTURE_INPUT_BIND_ALL,
   XMB_TEXTURE_INPUT_MOUSE,
   XMB_TEXTURE_INPUT_LGUN,
   XMB_TEXTURE_INPUT_TURBO,
   XMB_TEXTURE_CHECKMARK,
   XMB_TEXTURE_MENU_ADD,
   XMB_TEXTURE_BRIGHTNESS,
   XMB_TEXTURE_PAUSE,
   XMB_TEXTURE_DEFAULT,
   XMB_TEXTURE_DEFAULT_CONTENT,
   XMB_TEXTURE_MENU_APPLY_TOGGLE,
   XMB_TEXTURE_MENU_APPLY_COG,
   XMB_TEXTURE_LAST
};

enum
{
   XMB_SYSTEM_TAB_MAIN = 0,
   XMB_SYSTEM_TAB_SETTINGS,
   XMB_SYSTEM_TAB_HISTORY,
   XMB_SYSTEM_TAB_FAVORITES,
   XMB_SYSTEM_TAB_MUSIC,
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   XMB_SYSTEM_TAB_VIDEO,
#endif
#ifdef HAVE_IMAGEVIEWER
   XMB_SYSTEM_TAB_IMAGES,
#endif
#ifdef HAVE_NETWORKING
   XMB_SYSTEM_TAB_NETPLAY,
#endif
   XMB_SYSTEM_TAB_ADD,

   /* End of this enum - use the last one to determine num of possible tabs */
   XMB_SYSTEM_TAB_MAX_LENGTH
};

typedef struct xmb_handle
{
   bool mouse_show;
   bool use_ps3_layout;
   bool assets_missing;
   bool is_playlist;
   bool is_db_manager_list;
   bool is_quick_menu;

   uint8_t system_tab_end;
   uint8_t tabs[XMB_SYSTEM_TAB_MAX_LENGTH];

   int depth;
   int old_depth;
   int icon_size;
   int cursor_size;

   size_t categories_selection_ptr;
   size_t categories_selection_ptr_old;
   size_t selection_ptr_old;

   unsigned categories_active_idx;
   unsigned categories_active_idx_old;
   uintptr_t thumbnail;
   uintptr_t left_thumbnail;
   uintptr_t savestate_thumbnail;

   float x;
   float alpha;
   float thumbnail_width;
   float thumbnail_height;
   float left_thumbnail_width;
   float left_thumbnail_height;
   float savestate_thumbnail_width;
   float savestate_thumbnail_height;
   float above_subitem_offset;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;
   float shadow_offset;
   float font_size;
   float font2_size;
   float previous_scale_factor;

   float margins_screen_left;
   float margins_screen_top;
   float margins_setting_left;
   float margins_title_left;
   float margins_title_top;
   float margins_title_bottom;
   float margins_label_left;
   float margins_label_top;
   float icon_spacing_horizontal;
   float icon_spacing_vertical;
   float items_active_alpha;
   float items_active_zoom;
   float items_passive_alpha;
   float items_passive_zoom;
   float margins_dialog;
   float margins_slice;
   float textures_arrow_alpha;
   float categories_x_pos;
   float categories_passive_alpha;
   float categories_passive_zoom;
   float categories_active_zoom;
   float categories_active_alpha;

   char title_name[255];
   char *box_message;
   char *savestate_thumbnail_file_path;
   char *bg_file_path;

   file_list_t *selection_buf_old;
   file_list_t *horizontal_list;

   struct
   {
      menu_texture_item bg;
      menu_texture_item list[XMB_TEXTURE_LAST];
   } textures;

   xmb_node_t main_menu_node;
#ifdef HAVE_IMAGEVIEWER
   xmb_node_t images_tab_node;
#endif
   xmb_node_t music_tab_node;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   xmb_node_t video_tab_node;
#endif
   xmb_node_t settings_tab_node;
   xmb_node_t history_tab_node;
   xmb_node_t favorites_tab_node;
   xmb_node_t add_tab_node;
   xmb_node_t netplay_tab_node;

   font_data_t *font;
   font_data_t *font2;
   video_font_raster_block_t raster_block;
   video_font_raster_block_t raster_block2;

   menu_thumbnail_path_data_t *thumbnail_path_data;
} xmb_handle_t;

float scale_mod[8] = {
   1, 1, 1, 1, 1, 1, 1, 1
};

static float coord_shadow[] = {
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

static float coord_black[] = {
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

static float coord_white[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1
};

static float item_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1
};

float gradient_dark_purple[16] = {
   20/255.0,  13/255.0,  20/255.0, 1.0,
   20/255.0,  13/255.0,  20/255.0, 1.0,
   92/255.0,  44/255.0,  92/255.0, 1.0,
   148/255.0,  90/255.0, 148/255.0, 1.0,
};

float gradient_midnight_blue[16] = {
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
   44/255.0, 62/255.0, 80/255.0, 1.0,
};

float gradient_golden[16] = {
   174/255.0, 123/255.0,  44/255.0, 1.0,
   205/255.0, 174/255.0,  84/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
   58/255.0,   43/255.0,  24/255.0, 1.0,
};

float gradient_legacy_red[16] = {
   171/255.0,  70/255.0,  59/255.0, 1.0,
   171/255.0,  70/255.0,  59/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
   190/255.0,  80/255.0,  69/255.0, 1.0,
};

float gradient_electric_blue[16] = {
   1/255.0,   2/255.0,  67/255.0, 1.0,
   1/255.0,  73/255.0, 183/255.0, 1.0,
   1/255.0,  93/255.0, 194/255.0, 1.0,
   3/255.0, 162/255.0, 254/255.0, 1.0,
};

float gradient_apple_green[16] = {
   102/255.0, 134/255.0,  58/255.0, 1.0,
   122/255.0, 131/255.0,  52/255.0, 1.0,
   82/255.0, 101/255.0,  35/255.0, 1.0,
   63/255.0,  95/255.0,  30/255.0, 1.0,
};

float gradient_undersea[16] = {
   23/255.0,  18/255.0,  41/255.0, 1.0,
   30/255.0,  72/255.0, 114/255.0, 1.0,
   52/255.0,  88/255.0, 110/255.0, 1.0,
   69/255.0, 125/255.0, 140/255.0, 1.0,

};

float gradient_volcanic_red[16] = {
   1.0, 0.0, 0.1, 1.00,
   1.0, 0.1, 0.0, 1.00,
   0.1, 0.0, 0.1, 1.00,
   0.1, 0.0, 0.1, 1.00,
};

float gradient_dark[16] = {
   0.1, 0.1, 0.1, 1.00,
   0.1, 0.1, 0.1, 1.00,
   0.0, 0.0, 0.0, 1.00,
   0.0, 0.0, 0.0, 1.00,
};

float gradient_light[16] = {
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
};

float gradient_morning_blue[16] = {
   221/255.0, 241/255.0, 254/255.0, 1.00,
   135/255.0, 206/255.0, 250/255.0, 1.00,
   1.0, 1.0, 1.0, 1.00,
   170/255.0, 200/255.0, 252/255.0, 1.00,
};

static void xmb_calculate_visible_range(const xmb_handle_t *xmb,
      unsigned height, size_t list_size, unsigned current,
      unsigned *first, unsigned *last);

const char* xmb_theme_ident(void)
{
   settings_t *settings = config_get_ptr();
   switch (settings->uints.menu_xmb_theme)
   {
      case XMB_ICON_THEME_FLATUI:
         return "flatui";
      case XMB_ICON_THEME_RETROACTIVE:
         return "retroactive";
      case XMB_ICON_THEME_RETROSYSTEM:
         return "retrosystem";
      case XMB_ICON_THEME_PIXEL:
         return "pixel";
      case XMB_ICON_THEME_NEOACTIVE:
         return "neoactive";
      case XMB_ICON_THEME_SYSTEMATIC:
         return "systematic";
      case XMB_ICON_THEME_DOTART:
         return "dot-art";
      case XMB_ICON_THEME_CUSTOM:
         return "custom";
      case XMB_ICON_THEME_MONOCHROME_INVERTED:
         return "monochrome";
      case XMB_ICON_THEME_AUTOMATIC:
         return "automatic";
      case XMB_ICON_THEME_AUTOMATIC_INVERTED:
         return "automatic";
      case XMB_ICON_THEME_MONOCHROME:
      default:
         break;
   }
   return "monochrome";
}

/* NOTE: This exists because calloc()ing xmb_node_t is expensive
 * when you can have big lists like MAME and fba playlists */
static xmb_node_t *xmb_alloc_node(void)
{
   xmb_node_t *node = (xmb_node_t*)malloc(sizeof(*node));

   if (!node)
      return NULL;

   node->alpha    = node->label_alpha  = 0;
   node->zoom     = node->x = node->y  = 0;
   node->icon     = node->content_icon = 0;
   node->fullpath = NULL;

   return node;
}

static void xmb_free_node(xmb_node_t *node)
{
   if (!node)
      return;

   if (node->fullpath)
      free(node->fullpath);

   node->fullpath = NULL;

   free(node);
}

/**
 * @brief frees all xmb_node_t in a file_list_t
 *
 * file_list_t asumes userdata holds a simple structure and
 * free()'s it. Can't change this at the time because other
 * code depends on this behavior.
 *
 * @param list
 * @param actiondata whether to free actiondata too
 */
static void xmb_free_list_nodes(file_list_t *list, bool actiondata)
{
   unsigned i, size = (unsigned)file_list_get_size(list);

   for (i = 0; i < size; ++i)
   {
      xmb_free_node((xmb_node_t*)file_list_get_userdata_at_offset(list, i));

      /* file_list_set_userdata() doesn't accept NULL */
      list->list[i].userdata = NULL;

      if (actiondata)
         file_list_free_actiondata(list, i);
   }
}

static xmb_node_t *xmb_copy_node(const xmb_node_t *old_node)
{
   xmb_node_t *new_node = (xmb_node_t*)malloc(sizeof(*new_node));

   if (!new_node)
      return NULL;

   *new_node            = *old_node;
   new_node->fullpath   = old_node->fullpath ? strdup(old_node->fullpath) : NULL;

   return new_node;
}

static float *xmb_gradient_ident(video_frame_info_t *video_info)
{
   switch (video_info->xmb_color_theme)
   {
      case XMB_THEME_DARK_PURPLE:
         return &gradient_dark_purple[0];
      case XMB_THEME_MIDNIGHT_BLUE:
         return &gradient_midnight_blue[0];
      case XMB_THEME_GOLDEN:
         return &gradient_golden[0];
      case XMB_THEME_ELECTRIC_BLUE:
         return &gradient_electric_blue[0];
      case XMB_THEME_APPLE_GREEN:
         return &gradient_apple_green[0];
      case XMB_THEME_UNDERSEA:
         return &gradient_undersea[0];
      case XMB_THEME_VOLCANIC_RED:
         return &gradient_volcanic_red[0];
      case XMB_THEME_DARK:
         return &gradient_dark[0];
      case XMB_THEME_LIGHT:
         return &gradient_light[0];
      case XMB_THEME_MORNING_BLUE:
         return &gradient_morning_blue[0];
      case XMB_THEME_LEGACY_RED:
      default:
         break;
   }

   return &gradient_legacy_red[0];
}

static size_t xmb_list_get_selection(void *data)
{
   xmb_handle_t *xmb      = (xmb_handle_t*)data;

   if (!xmb)
      return 0;

   return xmb->categories_selection_ptr;
}

static size_t xmb_list_get_size(void *data, enum menu_list_type type)
{
   xmb_handle_t *xmb       = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         return menu_entries_get_stack_size(0);
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            return file_list_get_size(xmb->horizontal_list);
         break;
      case MENU_LIST_TABS:
         return xmb->system_tab_end;
   }

   return 0;
}

static void *xmb_list_get_entry(void *data,
      enum menu_list_type type, unsigned i)
{
   size_t list_size        = 0;
   xmb_handle_t *xmb       = (xmb_handle_t*)data;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         {
            file_list_t *menu_stack = menu_entries_get_menu_stack_ptr(0);
            list_size  = menu_entries_get_stack_size(0);
            if (i < list_size)
               return (void*)&menu_stack->list[i];
         }
         break;
      case MENU_LIST_HORIZONTAL:
         if (xmb && xmb->horizontal_list)
            list_size = file_list_get_size(xmb->horizontal_list);
         if (i < list_size)
            return (void*)&xmb->horizontal_list->list[i];
         break;
      default:
         break;
   }

   return NULL;
}

static INLINE float xmb_item_y(const xmb_handle_t *xmb, int i, size_t current)
{
   float iy = xmb->icon_spacing_vertical;

   if (i < (int)current)
      if (xmb->depth > 1)
         iy *= (i - (int)current + xmb->above_subitem_offset);
      else
         iy *= (i - (int)current + xmb->above_item_offset);
   else
      iy    *= (i - (int)current + xmb->under_item_offset);

   if (i == (int)current)
      iy = xmb->icon_spacing_vertical * xmb->active_item_factor;

   return iy;
}

static void xmb_draw_icon(
      video_frame_info_t *video_info,
      int icon_size,
      math_matrix_4x4 *mymat,
      uintptr_t texture,
      float x,
      float y,
      unsigned width,
      unsigned height,
      float alpha,
      float rotation,
      float scale_factor,
      float *color,
      float shadow_offset)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;

   if (
         (x < (-icon_size / 2.0f)) ||
         (x > width)               ||
         (y < (icon_size  / 2.0f)) ||
         (y > height + icon_size)
      )
      return;

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;

   draw.width           = icon_size;
   draw.height          = icon_size;
   draw.rotation        = rotation;
   draw.scale_factor    = scale_factor;
#if defined(VITA) || defined(WIIU)
   draw.width          *= scale_factor;
   draw.height         *= scale_factor;
#endif
   draw.coords          = &coords;
   draw.matrix_data     = mymat;
   draw.texture         = texture;
   draw.prim_type       = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id     = 0;

   if (video_info->xmb_shadows_enable)
   {
      menu_display_set_alpha(coord_shadow, color[3] * 0.35f);

      coords.color      = coord_shadow;
      draw.x            = x + shadow_offset;
      draw.y            = height - y - shadow_offset;

#if defined(VITA) || defined(WIIU)
      if (scale_factor < 1)
      {
         draw.x         = draw.x + (icon_size-draw.width)/2;
         draw.y         = draw.y + (icon_size-draw.width)/2;
      }
#endif
      menu_display_draw(&draw, video_info);
   }

   coords.color         = (const float*)color;
   draw.x               = x;
   draw.y               = height - y;

#if defined(VITA) || defined(WIIU)
   if (scale_factor < 1)
   {
      draw.x            = draw.x + (icon_size-draw.width)/2;
      draw.y            = draw.y + (icon_size-draw.width)/2;
   }
#endif
   menu_display_draw(&draw, video_info);
}

static void xmb_draw_thumbnail(
      video_frame_info_t *video_info,
      xmb_handle_t *xmb, float *color,
      unsigned width, unsigned height,
      float x, float y,
      float w, float h, uintptr_t texture)
{
   menu_display_ctx_rotate_draw_t rotate_draw;
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   math_matrix_4x4 mymat;

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   coords.vertices          = 4;
   coords.vertex            = NULL;
   coords.tex_coord         = NULL;
   coords.lut_tex_coord     = NULL;

   draw.width               = w;
   draw.height              = h;
   draw.coords              = &coords;
   draw.matrix_data         = &mymat;
   draw.texture             = texture;
   draw.prim_type           = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id         = 0;

   if (video_info->xmb_shadows_enable)
   {
      menu_display_set_alpha(coord_shadow, color[3] * 0.35f);

      coords.color          = coord_shadow;
      draw.x                = x + xmb->shadow_offset;
      draw.y                = height - y - xmb->shadow_offset;

      menu_display_draw(&draw, video_info);
   }

   coords.color             = (const float*)color;
   draw.x                   = x;
   draw.y                   = height - y;

   menu_display_set_alpha(color, 1.0f);

   menu_display_draw(&draw, video_info);
}

static void xmb_draw_text(
      video_frame_info_t *video_info,
      xmb_handle_t *xmb,
      const char *str, float x,
      float y, float scale_factor, float alpha,
      enum text_alignment text_align,
      unsigned width, unsigned height, font_data_t* font)
{
   uint32_t color;
   uint8_t a8;
   settings_t *settings;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   a8       = 255 * alpha;

   /* Avoid drawing 100% transparent text */
   if (a8 == 0)
      return;

   settings = config_get_ptr();
   color = FONT_COLOR_RGBA(
         settings->uints.menu_font_color_red,
         settings->uints.menu_font_color_green,
         settings->uints.menu_font_color_blue, a8);

   menu_display_draw_text(font, str, x, y,
         width, height, color, text_align, scale_factor,
         video_info->xmb_shadows_enable,
         xmb->shadow_offset, false);
}

static void xmb_messagebox(void *data, const char *message)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (!xmb || string_is_empty(message))
      return;

   xmb->box_message = strdup(message);
}

static void xmb_render_messagebox_internal(
      video_frame_info_t *video_info,
      xmb_handle_t *xmb, const char *message)
{
   unsigned i, y_position;
   int x, y, longest = 0, longest_width = 0;
   float line_height        = 0;
   unsigned width           = video_info->width;
   unsigned height          = video_info->height;
   struct string_list *list = !string_is_empty(message)
      ? string_split(message, "\n") : NULL;

   if (!list || !xmb || !xmb->font)
   {
      if (list)
         string_list_free(list);
      return;
   }

   if (list->elems == 0)
      goto end;

   line_height      = xmb->font->size * 1.2;

   y_position       = height / 2;
   if (menu_input_dialog_get_display_kb())
      y_position    = height / 4;

   x                = width  / 2;
   y                = y_position - (list->size-1) * line_height / 2;

   /* find the longest line width */
   for (i = 0; i < list->size; i++)
   {
      const char *msg  = list->elems[i].data;
      int len          = (int)utf8len(msg);

      if (len > longest)
      {
         longest       = len;
         longest_width = font_driver_get_message_width(
               xmb->font, msg, (unsigned)strlen(msg), 1);
      }
   }

   menu_display_blend_begin(video_info);

   menu_display_draw_texture_slice(
         video_info,
         x - longest_width/2 - xmb->margins_dialog,
         y + xmb->margins_slice - xmb->margins_dialog,
         256, 256,
         longest_width + xmb->margins_dialog * 2,
         line_height * list->size + xmb->margins_dialog * 2,
         width, height,
         NULL,
         xmb->margins_slice, 1.0,
         xmb->textures.list[XMB_TEXTURE_DIALOG_SLICE]);

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         menu_display_draw_text(xmb->font, msg,
               x - longest_width/2.0,
               y + (i+0.75) * line_height,
               width, height, 0x444444ff, TEXT_ALIGN_LEFT, 1.0f, false, 0, false);
   }

   if (menu_input_dialog_get_display_kb())
      menu_display_draw_keyboard(
            xmb->textures.list[XMB_TEXTURE_KEY_HOVER],
            xmb->font,
            video_info,
            menu_event_get_osk_grid(),
            menu_event_get_osk_ptr(),
            0xffffffff);

end:
   string_list_free(list);
}

static void xmb_update_thumbnail_path(void *data, unsigned i, char pos)
{
   xmb_handle_t *xmb     = (xmb_handle_t*)data;
   const char *core_name = NULL;

   if (!xmb)
      return;

   /* imageviewer content requires special treatment... */
   menu_thumbnail_get_core_name(xmb->thumbnail_path_data, &core_name);
   if (string_is_equal(core_name, "imageviewer"))
   {
      if ((pos == 'R') || (pos == 'L' && !menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT)))
         menu_thumbnail_update_path(xmb->thumbnail_path_data, pos == 'R' ? MENU_THUMBNAIL_RIGHT : MENU_THUMBNAIL_LEFT);
   }
   else
      menu_thumbnail_update_path(xmb->thumbnail_path_data, pos == 'R' ? MENU_THUMBNAIL_RIGHT : MENU_THUMBNAIL_LEFT);
}

static void xmb_update_savestate_thumbnail_path(void *data, unsigned i)
{
   menu_entry_t entry;
   settings_t     *settings = config_get_ptr();
   xmb_handle_t     *xmb    = (xmb_handle_t*)data;

   if (!xmb)
      return;

   menu_entry_init(&entry);
   entry.path_enabled       = false;
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   entry.sublabel_enabled   = false;
   menu_entry_get(&entry, 0, i, NULL, true);

   if (!string_is_empty(xmb->savestate_thumbnail_file_path))
      free(xmb->savestate_thumbnail_file_path);
   xmb->savestate_thumbnail_file_path = NULL;

   if (!string_is_empty(entry.label))
   {
      if (     (settings->bools.savestate_thumbnail_enable)
            && ((string_is_equal(entry.label, "state_slot"))
               || (string_is_equal(entry.label, "loadstate"))
               || (string_is_equal(entry.label, "savestate"))))
      {
         size_t path_size         = 8204 * sizeof(char);
         char             *path   = (char*)malloc(path_size);
         global_t         *global = global_get_ptr();

         path[0] = '\0';

         if (global)
         {
            int state_slot = settings->ints.state_slot;

            if (state_slot > 0)
               snprintf(path, path_size, "%s%d",
                     global->name.savestate, state_slot);
            else if (state_slot < 0)
               fill_pathname_join_delim(path,
                     global->name.savestate, "auto", '.', path_size);
            else
               strlcpy(path, global->name.savestate, path_size);
         }

         strlcat(path, file_path_str(FILE_PATH_PNG_EXTENSION), path_size);

         if (path_is_valid(path))
         {
            if (!string_is_empty(xmb->savestate_thumbnail_file_path))
               free(xmb->savestate_thumbnail_file_path);
            xmb->savestate_thumbnail_file_path = strdup(path);
         }

         free(path);
      }
   }
}

static void xmb_update_thumbnail_image(void *data)
{
   settings_t *settings             = config_get_ptr();
   xmb_handle_t *xmb                = (xmb_handle_t*)data;
   const char *right_thumbnail_path = NULL;
   const char *left_thumbnail_path  = NULL;
   bool supports_rgba               = video_driver_supports_rgba();

   /* Have to wrap `thumbnails_missing` like this to silence
    * brain dead `set but not used` warnings when networking
    * is disabled... */
#ifdef HAVE_NETWORKING
   bool thumbnails_missing          = false;
#endif

   if (!xmb || !settings)
      return;

   if (menu_thumbnail_get_path(xmb->thumbnail_path_data, MENU_THUMBNAIL_RIGHT, &right_thumbnail_path))
   {
      if (path_is_valid(right_thumbnail_path))
         task_push_image_load(right_thumbnail_path,
               supports_rgba, settings->uints.menu_thumbnail_upscale_threshold,
               menu_display_handle_thumbnail_upload, NULL);
      else
      {
         video_driver_texture_unload(&xmb->thumbnail);
#ifdef HAVE_NETWORKING
         thumbnails_missing = true;
#endif
      }
   }
   else
      video_driver_texture_unload(&xmb->thumbnail);

   if (menu_thumbnail_get_path(xmb->thumbnail_path_data, MENU_THUMBNAIL_LEFT, &left_thumbnail_path))
   {
      if (path_is_valid(left_thumbnail_path))
         task_push_image_load(left_thumbnail_path,
               supports_rgba, settings->uints.menu_thumbnail_upscale_threshold,
               menu_display_handle_left_thumbnail_upload, NULL);
      else
      {
         video_driver_texture_unload(&xmb->left_thumbnail);
#ifdef HAVE_NETWORKING
         thumbnails_missing = true;
#endif
      }
   }
   else
      video_driver_texture_unload(&xmb->left_thumbnail);

#ifdef HAVE_NETWORKING
   /* On demand thumbnail downloads */
   if (thumbnails_missing)
   {
      if (settings->bools.network_on_demand_thumbnails)
      {
         const char *system = NULL;

         if (menu_thumbnail_get_system(xmb->thumbnail_path_data, &system))
            task_push_pl_entry_thumbnail_download(system,
                  playlist_get_cached(), menu_navigation_get_selection(),
                  false, true);
      }
   }
#endif
}

static unsigned xmb_get_system_tab(xmb_handle_t *xmb, unsigned i)
{
   if (i <= xmb->system_tab_end)
      return xmb->tabs[i];
   return UINT_MAX;
}

static void xmb_refresh_thumbnail_image(void *data)
{
   xmb_handle_t *xmb                = (xmb_handle_t*)data;

   if (!xmb)
      return;

   /* Only refresh thumbnails if thumbnails are enabled */
   if (  menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT) || 
         menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
   {
      unsigned depth          = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
      unsigned xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);

      /* Only refresh thumbnails if we are viewing a playlist or
       * the quick menu... */

      /* If we are currently viewing a playlist, then it's almost inevitable
       * that we've just gone up a level from the quick menu. In this case,
       * xmb_set_thumbnail_system() will have been called, which resets
       * thumbnail path data. We therefore have to regenerate the thumbnail
       * paths... */
      if (((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && depth == 1) ||
           (xmb_system_tab < XMB_SYSTEM_TAB_SETTINGS && depth == 4)) &&
          xmb->is_playlist)
      {
         if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
            xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'R');

         if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
            xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'L');

         xmb_update_thumbnail_image(xmb);
      }
      else if (xmb->is_quick_menu)
      {
         /* If this is the quick menu (most likely, since this is
          * where the 'download thumbnails' option is located),
          * then thumbnail paths are already valid - just need to
          * update images */
         xmb_update_thumbnail_image(xmb);
      }
   }
}

static void xmb_set_thumbnail_system(void *data, char*s, size_t len)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   menu_thumbnail_set_system(xmb->thumbnail_path_data, s);
}

static void xmb_get_thumbnail_system(void *data, char*s, size_t len)
{
   xmb_handle_t *xmb  = (xmb_handle_t*)data;
   const char *system = NULL;
   if (!xmb)
      return;

   if (menu_thumbnail_get_system(xmb->thumbnail_path_data, &system))
      strlcpy(s, system, len);
}

static void xmb_unload_thumbnail_textures(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   if (xmb->thumbnail)
      video_driver_texture_unload(&xmb->thumbnail);
   if (xmb->left_thumbnail)
      video_driver_texture_unload(&xmb->left_thumbnail);
}

static void xmb_set_thumbnail_content(void *data, const char *s)
{
   size_t selection  = menu_navigation_get_selection();
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   if (xmb->is_playlist)
   {
      /* Playlist content */
      if (string_is_empty(s))
         menu_thumbnail_set_content_playlist(xmb->thumbnail_path_data,
               playlist_get_cached(), selection);
   }
   else if (xmb->is_db_manager_list)
   {
      /* Database list content */
      if (string_is_empty(s))
      {
         menu_entry_t entry;

         menu_entry_init(&entry);
         entry.label_enabled      = false;
         entry.rich_label_enabled = false;
         entry.value_enabled      = false;
         entry.sublabel_enabled   = false;
         menu_entry_get(&entry, 0, selection, NULL, true);

         if (!string_is_empty(entry.path))
            menu_thumbnail_set_content(xmb->thumbnail_path_data, entry.path);
      }
   }
   else if (string_is_equal(s, "imageviewer"))
   {
      /* Filebrowser image updates */
      menu_entry_t entry;
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(selection_buf, selection);

      menu_entry_init(&entry);
      entry.label_enabled      = false;
      entry.rich_label_enabled = false;
      entry.value_enabled      = false;
      entry.sublabel_enabled   = false;
      menu_entry_get(&entry, 0, selection, NULL, true);

      if (node)
         if (  !string_is_empty(entry.path) && 
               !string_is_empty(node->fullpath))
            menu_thumbnail_set_content_image(xmb->thumbnail_path_data, node->fullpath, entry.path);
   }
   else if (!string_is_empty(s))
   {
      /* Annoying leftovers...
       * This is required to ensure that thumbnails are
       * updated correctly when navigating deeply through
       * the sublevels of database manager lists.
       * Showing thumbnails on database entries is a
       * pointless nuisance and a waste of CPU cycles, IMHO... */
      menu_thumbnail_set_content(xmb->thumbnail_path_data, s);
   }
}

static void xmb_update_savestate_thumbnail_image(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   if (!xmb)
      return;

   if (path_is_valid(xmb->savestate_thumbnail_file_path))
      task_push_image_load(xmb->savestate_thumbnail_file_path,
            video_driver_supports_rgba(), 0,
            menu_display_handle_savestate_thumbnail_upload, NULL);
   else
      video_driver_texture_unload(&xmb->savestate_thumbnail);
}

static void xmb_selection_pointer_changed(
      xmb_handle_t *xmb, bool allow_animations)
{
   unsigned i, end, height;
   menu_animation_ctx_tag tag;
   menu_entry_t entry;
   size_t num                 = 0;
   int threshold              = 0;
   menu_list_t     *menu_list = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   if (!xmb)
      return;

   menu_entries_ctl(MENU_ENTRIES_CTL_LIST_GET, &menu_list);

   menu_entry_init(&entry);
   entry.path_enabled       = false;
   entry.label_enabled      = false;
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   entry.sublabel_enabled   = false;
   menu_entry_get(&entry, 0, selection, NULL, true);

   end       = (unsigned)menu_entries_get_size();
   threshold = xmb->icon_size * 10;

   video_driver_get_size(NULL, &height);

   tag       = (uintptr_t)selection_buf;

   menu_animation_kill_by_tag(&tag);
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &num);

   for (i = 0; i < end; i++)
   {
      float iy, real_iy;
      float ia         = xmb->items_passive_alpha;
      float iz         = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(selection_buf, i);

      if (!node)
         continue;

      iy      = xmb_item_y(xmb, i, selection);
      real_iy = iy + xmb->margins_screen_top;

      if (i == selection)
      {
         unsigned     depth      = (unsigned)xmb_list_get_size(xmb, MENU_LIST_PLAIN);
         unsigned xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);
         unsigned entry_type     = menu_entry_get_type_new(&entry);

         ia                      = xmb->items_active_alpha;
         iz                      = xmb->items_active_zoom;
         if (
               menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT) || 
               menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT)
            )
         {
            bool update_thumbnails = false;

            /* Playlist updates */
            if (((xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && depth == 1) ||
                 (xmb_system_tab < XMB_SYSTEM_TAB_SETTINGS && depth == 4)) &&
                xmb->is_playlist)
            {
               xmb_set_thumbnail_content(xmb, NULL);
               update_thumbnails = true;
            }
            /* Database list updates
             * (pointless nuisance...) */
            else if (depth == 4 && xmb->is_db_manager_list)
            {
               xmb_set_thumbnail_content(xmb, NULL);
               update_thumbnails = true;
            }
            /* Filebrowser image updates */
            else if (
                  entry_type == FILE_TYPE_IMAGEVIEWER || 
                  entry_type == FILE_TYPE_IMAGE)
            {
               xmb_set_thumbnail_content(xmb, "imageviewer");
               update_thumbnails = true;
            }

            if (update_thumbnails)
            {
               if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
                  xmb_update_thumbnail_path(xmb, i /* will be ignored */, 'R');

               if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
                  xmb_update_thumbnail_path(xmb, i /* will be ignored */, 'L');

               xmb_update_thumbnail_image(xmb);
            }
         }

         xmb_update_savestate_thumbnail_path(xmb, i);
         xmb_update_savestate_thumbnail_image(xmb);
      }

      if (     (!allow_animations)
            || (real_iy < -threshold
               || real_iy > height+threshold))
      {
         node->alpha = node->label_alpha = ia;
         node->y = iy;
         node->zoom = iz;
      }
      else
      {
         settings_t *settings = config_get_ptr();

         /* Move up/down animation */
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.tag          = tag;
         anim_entry.cb           = NULL;

         switch (settings->uints.menu_xmb_animation_move_up_down)
         {
            case 0:
               anim_entry.duration     = XMB_DELAY;
               anim_entry.easing_enum  = EASING_OUT_QUAD;
               break;
            case 1:
               anim_entry.duration     = XMB_DELAY * 4;
               anim_entry.easing_enum  = EASING_OUT_EXPO;
               break;
         }

         menu_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = iz;
         anim_entry.subject      = &node->zoom;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = iy;
         anim_entry.subject      = &node->y;

         menu_animation_push(&anim_entry);
      }
   }
}

static void xmb_list_open_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height = 0;
   int        threshold = xmb->icon_size * 10;
   size_t           end = 0;

   end = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia = 0;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = xmb->items_active_alpha;
      if (dir == -1)
         ia = 0;

      real_y = node->y + xmb->margins_screen_top;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = ia;
         node->label_alpha = 0;
         node->x = xmb->icon_size * dir * -2;
      }
      else
      {
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = XMB_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = xmb->icon_size * dir * -2;
         anim_entry.subject      = &node->x;

         menu_animation_push(&anim_entry);
      }
   }
}

static void xmb_list_open_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   unsigned xmb_system_tab = 0;
   size_t skip             = 0;
   int        threshold    = xmb->icon_size * 10;
   size_t           end    = file_list_get_size(list);

   video_driver_get_size(NULL, &height);

   for (i = 0; i < end; i++)
   {
      float ia;
      float real_y;
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (dir == 1)
      {
         node->alpha       = 0;
         node->label_alpha = 0;
      }
      else if (dir == -1)
      {
         if (i != current)
            node->alpha    = 0;
         node->label_alpha = 0;
      }

      node->x        = xmb->icon_size * dir * 2;
      node->y        = xmb_item_y(xmb, i, current);
      node->zoom     = xmb->categories_passive_zoom;

      real_y         = node->y + xmb->margins_screen_top;

      if (i == current)
      {
         node->zoom  = xmb->categories_active_zoom;
         ia          = xmb->items_active_alpha;
      }
      else
         ia          = xmb->items_passive_alpha;

      if (real_y < -threshold || real_y > height+threshold)
      {
         node->alpha = node->label_alpha = ia;
         node->x     = 0;
      }
      else
      {
         menu_animation_ctx_entry_t anim_entry;

         anim_entry.duration     = XMB_DELAY;
         anim_entry.target_value = ia;
         anim_entry.subject      = &node->alpha;
         anim_entry.easing_enum  = EASING_OUT_QUAD;
         anim_entry.tag          = (uintptr_t)list;
         anim_entry.cb           = NULL;

         menu_animation_push(&anim_entry);

         anim_entry.subject      = &node->label_alpha;

         menu_animation_push(&anim_entry);

         anim_entry.target_value = 0;
         anim_entry.subject      = &node->x;

         menu_animation_push(&anim_entry);
      }
   }

   xmb->old_depth = xmb->depth;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &skip);

   xmb_system_tab = xmb_get_system_tab(xmb,
         (unsigned)xmb->categories_selection_ptr);

   if (xmb_system_tab <= XMB_SYSTEM_TAB_SETTINGS)
   {
      if (  menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT) || 
            menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
      {
         /* This code is horrible, full of hacks...
          * This hack ensures that thumbnails are not cleared
          * when selecting an entry from a collection via
          * 'load content'... */
         if (xmb->depth != 5)
            xmb_unload_thumbnail_textures(xmb);

         if (xmb->is_playlist || xmb->is_db_manager_list)
         {
            xmb_set_thumbnail_content(xmb, NULL);

            if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
               xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'R');

            if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
               xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'L');

            xmb_update_thumbnail_image(xmb);
         }
      }
   }
}

static xmb_node_t *xmb_node_allocate_userdata(
      xmb_handle_t *xmb, unsigned i)
{
   xmb_node_t *tmp  = NULL;
   xmb_node_t *node = xmb_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node->alpha = xmb->categories_passive_alpha;
   node->zoom  = xmb->categories_passive_zoom;

   if ((i + xmb->system_tab_end) == xmb->categories_active_idx)
   {
      node->alpha = xmb->categories_active_alpha;
      node->zoom  = xmb->categories_active_zoom;
   }

   tmp = (xmb_node_t*)file_list_get_userdata_at_offset(
         xmb->horizontal_list, i);
   xmb_free_node(tmp);

   file_list_set_userdata(xmb->horizontal_list, i, node);

   return node;
}

static xmb_node_t* xmb_get_userdata_from_horizontal_list(
      xmb_handle_t *xmb, unsigned i)
{
   return (xmb_node_t*)
      file_list_get_userdata_at_offset(xmb->horizontal_list, i);
}

static void xmb_push_animations(xmb_node_t *node,
      uintptr_t tag, float ia, float ix)
{
   menu_animation_ctx_entry_t anim_entry;

   anim_entry.duration     = XMB_DELAY;
   anim_entry.target_value = ia;
   anim_entry.subject      = &node->alpha;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   anim_entry.tag          = tag;
   anim_entry.cb           = NULL;

   menu_animation_push(&anim_entry);

   anim_entry.subject      = &node->label_alpha;

   menu_animation_push(&anim_entry);

   anim_entry.target_value = ix;
   anim_entry.subject      = &node->x;

   menu_animation_push(&anim_entry);
}

static void xmb_list_switch_old(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, height;
   size_t end          = file_list_get_size(list);
   float ix            = -xmb->icon_spacing_horizontal * dir;
   float ia            = 0;
   unsigned first      = 0;
   unsigned last       = (unsigned)(end > 0 ? end - 1 : 0);

   video_driver_get_size(NULL, &height);
   xmb_calculate_visible_range(xmb, height, end,
         (unsigned)current, &first, &last);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i >= first && i <= last)
         xmb_push_animations(node, (uintptr_t)list, ia, ix);
      else
      {
         node->alpha = node->label_alpha = ia;
         node->x     = ix;
      }
   }
}

static void xmb_list_switch_new(xmb_handle_t *xmb,
      file_list_t *list, int dir, size_t current)
{
   unsigned i, first, last, height;
   size_t end           = 0;
   settings_t *settings = config_get_ptr();

   if (settings->bools.menu_dynamic_wallpaper_enable)
   {
      size_t path_size = PATH_MAX_LENGTH * sizeof(char);
      char       *path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      char       *tmp  = string_replace_substring(xmb->title_name, "/", " ");

      path[0]          = '\0';

      if (tmp)
      {
         fill_pathname_join_noext(
               path,
               settings->paths.directory_dynamic_wallpapers,
               tmp,
               path_size);
         free(tmp);
      }

      strlcat(path,
            file_path_str(FILE_PATH_PNG_EXTENSION),
            path_size);

      if (!path_is_valid(path))
         fill_pathname_application_special(path, path_size,
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

      if (!string_is_equal(path, xmb->bg_file_path))
      {
         if (path_is_valid(path))
         {
            task_push_image_load(path,
                  video_driver_supports_rgba(), 0,
                  menu_display_handle_wallpaper_upload, NULL);
            if (!string_is_empty(xmb->bg_file_path))
               free(xmb->bg_file_path);
            xmb->bg_file_path = strdup(path);
         }
      }

      free(path);
   }

   end = file_list_get_size(list);

   first = 0;
   last  = (unsigned)(end > 0 ? end - 1 : 0);

   video_driver_get_size(NULL, &height);
   xmb_calculate_visible_range(xmb, height, end, (unsigned)current, &first, &last);

   for (i = 0; i < end; i++)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, i);
      float ia         = xmb->items_passive_alpha;

      if (!node)
         continue;

      node->x           = xmb->icon_spacing_horizontal * dir;
      node->alpha       = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = xmb->items_active_alpha;

      if (i >= first && i <= last)
         xmb_push_animations(node, (uintptr_t)list, ia, 0);
      else
      {
         node->x     = 0;
         node->alpha = node->label_alpha = ia;
      }
   }
}

static void xmb_set_title(xmb_handle_t *xmb)
{
   if (xmb->categories_selection_ptr <= xmb->system_tab_end)
      menu_entries_get_title(xmb->title_name, sizeof(xmb->title_name));
   else
   {
      const char *path = NULL;
      menu_entries_get_at_offset(
            xmb->horizontal_list,
            xmb->categories_selection_ptr - (xmb->system_tab_end + 1),
            &path, NULL, NULL, NULL, NULL);

      if (!path)
         return;

      fill_pathname_base_noext(
            xmb->title_name, path, sizeof(xmb->title_name));
   }
}

static xmb_node_t* xmb_get_node(xmb_handle_t *xmb, unsigned i)
{
   switch (xmb_get_system_tab(xmb, i))
   {
      case XMB_SYSTEM_TAB_SETTINGS:
         return &xmb->settings_tab_node;
#ifdef HAVE_IMAGEVIEWER
      case XMB_SYSTEM_TAB_IMAGES:
         return &xmb->images_tab_node;
#endif
      case XMB_SYSTEM_TAB_MUSIC:
         return &xmb->music_tab_node;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case XMB_SYSTEM_TAB_VIDEO:
         return &xmb->video_tab_node;
#endif
      case XMB_SYSTEM_TAB_HISTORY:
         return &xmb->history_tab_node;
      case XMB_SYSTEM_TAB_FAVORITES:
         return &xmb->favorites_tab_node;
#ifdef HAVE_NETWORKING
      case XMB_SYSTEM_TAB_NETPLAY:
         return &xmb->netplay_tab_node;
#endif
      case XMB_SYSTEM_TAB_ADD:
         return &xmb->add_tab_node;
      default:
         if (i > xmb->system_tab_end)
            return xmb_get_userdata_from_horizontal_list(
                  xmb, i - (xmb->system_tab_end + 1));
   }

   return &xmb->main_menu_node;
}

static void xmb_list_switch_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t entry;
      settings_t *settings        = config_get_ptr();
      float ia                    = xmb->categories_passive_alpha;
      float iz                    = xmb->categories_passive_zoom;
      xmb_node_t *node            = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories_active_idx)
      {
         ia = xmb->categories_active_alpha;
         iz = xmb->categories_active_zoom;
      }

      /* Horizontal icon animation */

      entry.target_value = ia;
      entry.subject      = &node->alpha;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      entry.tag          = -1;
      entry.cb           = NULL;

      switch (settings->uints.menu_xmb_animation_horizontal_highlight)
      {
         case 0:
            entry.duration     = XMB_DELAY;
            entry.easing_enum  = EASING_OUT_QUAD;
            break;
         case 1:
            entry.duration     = XMB_DELAY + (XMB_DELAY / 2);
            entry.easing_enum  = EASING_IN_SINE;
            break;
         case 2:
            entry.duration     = XMB_DELAY * 2;
            entry.easing_enum  = EASING_OUT_BOUNCE;
            break;
      }

      menu_animation_push(&entry);

      entry.target_value = iz;
      entry.subject      = &node->zoom;

      menu_animation_push(&entry);
   }
}

static void xmb_list_switch(xmb_handle_t *xmb)
{
   menu_animation_ctx_entry_t anim_entry;
   int dir                    = -1;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   settings_t *settings = config_get_ptr();

   if (xmb->categories_selection_ptr > xmb->categories_selection_ptr_old)
      dir = 1;

   xmb->categories_active_idx += dir;

   xmb_list_switch_horizontal_list(xmb);

   anim_entry.duration     = XMB_DELAY;
   anim_entry.target_value = xmb->icon_spacing_horizontal
      * -(float)xmb->categories_selection_ptr;
   anim_entry.subject      = &xmb->categories_x_pos;
   anim_entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   anim_entry.tag          = -1;
   anim_entry.cb           = NULL;

   if (anim_entry.subject)
      menu_animation_push(&anim_entry);

   dir = -1;
   if (xmb->categories_selection_ptr > xmb->categories_selection_ptr_old)
      dir = 1;

   xmb_list_switch_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);

   /* Check if we are to have horizontal animations. */
   if (settings->bools.menu_horizontal_animation)
      xmb_list_switch_new(xmb, selection_buf, dir, selection);
   xmb->categories_active_idx_old = (unsigned)xmb->categories_selection_ptr;

   if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT) || menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
   {
      xmb_unload_thumbnail_textures(xmb);

      if (xmb->is_playlist)
      {
         xmb_set_thumbnail_content(xmb, NULL);

         if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
            xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'R');

         if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
            xmb_update_thumbnail_path(xmb, 0 /* will be ignored */, 'L');

         xmb_update_thumbnail_image(xmb);
      }
   }
}

static void xmb_list_open_horizontal_list(xmb_handle_t *xmb)
{
   unsigned j;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (j = 0; j <= list_size; j++)
   {
      menu_animation_ctx_entry_t anim_entry;
      float ia          = 0;
      xmb_node_t *node  = xmb_get_node(xmb, j);

      if (!node)
         continue;

      if (j == xmb->categories_active_idx)
         ia = xmb->categories_active_alpha;
      else if (xmb->depth <= 1)
         ia = xmb->categories_passive_alpha;

      anim_entry.duration     = XMB_DELAY;
      anim_entry.target_value = ia;
      anim_entry.subject      = &node->alpha;
      anim_entry.easing_enum  = EASING_OUT_QUAD;
      /* TODO/FIXME - integer conversion resulted in change of sign */
      anim_entry.tag          = -1;
      anim_entry.cb           = NULL;

      if (anim_entry.subject)
         menu_animation_push(&anim_entry);
   }
}

static void xmb_context_destroy_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   for (i = 0; i < list_size; i++)
   {
      const char *path = NULL;
      xmb_node_t *node = xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
         continue;

      file_list_get_at_offset(xmb->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path || !strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      video_driver_texture_unload(&node->icon);
      video_driver_texture_unload(&node->content_icon);
   }
}

static void xmb_init_horizontal_list(xmb_handle_t *xmb)
{
   menu_displaylist_info_t info;
   settings_t *settings         = config_get_ptr();

   menu_displaylist_info_init(&info);

   info.list                    = xmb->horizontal_list;
   info.path                    = strdup(
         settings->paths.directory_playlist);
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB));
   info.exts                    = strdup(
         file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_PLAYLISTS_TAB;

   if (settings->bools.menu_content_show_playlists && !string_is_empty(info.path))
   {
      if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
      {
         size_t i;
         for (i = 0; i < xmb->horizontal_list->size; i++)
            xmb_node_allocate_userdata(xmb, (unsigned)i);
         menu_displaylist_process(&info);
      }
   }

   menu_displaylist_info_free(&info);
}

static void xmb_toggle_horizontal_list(xmb_handle_t *xmb)
{
   unsigned i;
   size_t list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   for (i = 0; i <= list_size; i++)
   {
      xmb_node_t *node = xmb_get_node(xmb, i);

      if (!node)
         continue;

      node->alpha = 0;
      node->zoom  = xmb->categories_passive_zoom;

      if (i == xmb->categories_active_idx)
      {
         node->alpha = xmb->categories_active_alpha;
         node->zoom  = xmb->categories_active_zoom;
      }
      else if (xmb->depth <= 1)
         node->alpha = xmb->categories_passive_alpha;
   }
}

static void xmb_context_reset_horizontal_list(
      xmb_handle_t *xmb)
{
   unsigned i;
   int depth; /* keep this integer */
   size_t list_size                =
      xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL);

   xmb->categories_x_pos           =
      xmb->icon_spacing_horizontal *
      -(float)xmb->categories_selection_ptr;

   depth                           = (xmb->depth > 1) ? 2 : 1;
   xmb->x                          = xmb->icon_size * -(depth*2-2);

   for (i = 0; i < list_size; i++)
   {
      const char *path                          = NULL;
      xmb_node_t *node                          =
         xmb_get_userdata_from_horizontal_list(xmb, i);

      if (!node)
      {
         node = xmb_node_allocate_userdata(xmb, i);
         if (!node)
            continue;
      }

      file_list_get_at_offset(xmb->horizontal_list, i,
            &path, NULL, NULL, NULL);

      if (!path)
         continue;

      if (!strstr(path, file_path_str(FILE_PATH_LPL_EXTENSION)))
         continue;

      {
         struct texture_image ti;
         char *sysname             = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *iconpath            = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *texturepath         = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));
         char *content_texturepath = (char*)
            malloc(PATH_MAX_LENGTH * sizeof(char));

         iconpath[0]    = sysname[0] =
            texturepath[0] = content_texturepath[0] = '\0';

         fill_pathname_base_noext(sysname, path,
               PATH_MAX_LENGTH * sizeof(char));

         fill_pathname_application_special(iconpath,
               PATH_MAX_LENGTH * sizeof(char),
               APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

         fill_pathname_join_concat(texturepath, iconpath, sysname,
               file_path_str(FILE_PATH_PNG_EXTENSION),
               PATH_MAX_LENGTH * sizeof(char));

         /* If the playlist icon doesn't exist return default */

         if (!path_is_valid(texturepath))
               fill_pathname_join_concat(texturepath, iconpath, "default",
               file_path_str(FILE_PATH_PNG_EXTENSION),
               PATH_MAX_LENGTH * sizeof(char));

         ti.width         = 0;
         ti.height        = 0;
         ti.pixels        = NULL;
         ti.supports_rgba = video_driver_supports_rgba();

         if (image_texture_load(&ti, texturepath))
         {
            if (ti.pixels)
            {
               video_driver_texture_unload(&node->icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->icon);
            }

            image_texture_free(&ti);
         }

         fill_pathname_join_delim(sysname, sysname,
               file_path_str(FILE_PATH_CONTENT_BASENAME), '-',
               PATH_MAX_LENGTH * sizeof(char));
         strlcat(content_texturepath, iconpath, PATH_MAX_LENGTH * sizeof(char));
         strlcat(content_texturepath, sysname, PATH_MAX_LENGTH * sizeof(char));

         /* If the content icon doesn't exist return default-content */

         if (!path_is_valid(content_texturepath))
         {
            strlcat(iconpath, "default", PATH_MAX_LENGTH * sizeof(char));
            fill_pathname_join_delim(content_texturepath, iconpath,
                  file_path_str(FILE_PATH_CONTENT_BASENAME), '-',
                  PATH_MAX_LENGTH * sizeof(char));
         }

         if (image_texture_load(&ti, content_texturepath))
         {
            if (ti.pixels)
            {
               video_driver_texture_unload(&node->content_icon);
               video_driver_texture_load(&ti,
                     TEXTURE_FILTER_MIPMAP_LINEAR, &node->content_icon);
            }

            image_texture_free(&ti);
         }

         free(sysname);
         free(iconpath);
         free(texturepath);
         free(content_texturepath);
      }
   }

   xmb_toggle_horizontal_list(xmb);
}

static void xmb_refresh_horizontal_list(xmb_handle_t *xmb)
{
   xmb_context_destroy_horizontal_list(xmb);
   if (xmb->horizontal_list)
   {
      xmb_free_list_nodes(xmb->horizontal_list, false);
      file_list_free(xmb->horizontal_list);
   }
   xmb->horizontal_list = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);

   xmb->horizontal_list         = (file_list_t*)
      calloc(1, sizeof(file_list_t));

   if (xmb->horizontal_list)
      xmb_init_horizontal_list(xmb);

   xmb_context_reset_horizontal_list(xmb);
}

static int xmb_environ(enum menu_environ_cb type, void *data, void *userdata)
{
   xmb_handle_t *xmb        = (xmb_handle_t*)userdata;

   switch (type)
   {
      case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
         if (!xmb)
            return -1;
         xmb->mouse_show = true;
         break;
      case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
         if (!xmb)
            return -1;
         xmb->mouse_show = false;
         break;
      case MENU_ENVIRON_RESET_HORIZONTAL_LIST:
         if (!xmb)
            return -1;

         xmb_refresh_horizontal_list(xmb);
         break;
      default:
         return -1;
   }

   return 0;
}

static void xmb_list_open(xmb_handle_t *xmb)
{
   menu_animation_ctx_entry_t entry;

   settings_t *settings       = config_get_ptr();
   int                    dir = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   xmb->depth = (int)xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;
   else
      return; /* If menu hasn't changed, do nothing */

   xmb_list_open_horizontal_list(xmb);

   xmb_list_open_old(xmb, xmb->selection_buf_old,
         dir, xmb->selection_ptr_old);
   xmb_list_open_new(xmb, selection_buf,
         dir, selection);

   /* Main Menu opening animation */

   entry.target_value = xmb->icon_size * -(xmb->depth*2-2);
   entry.subject      = &xmb->x;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   switch (settings->uints.menu_xmb_animation_opening_main_menu)
   {
      case 0:
         entry.easing_enum  = EASING_OUT_QUAD;
         entry.duration     = XMB_DELAY;
         break;
      case 1:
         entry.easing_enum  = EASING_OUT_CIRC;
         entry.duration     = XMB_DELAY * 2;
         break;
      case 2:
         entry.easing_enum  = EASING_OUT_EXPO;
         entry.duration     = XMB_DELAY * 3;
         break;
      case 3:
         entry.easing_enum  = EASING_OUT_BOUNCE;
         entry.duration     = XMB_DELAY * 4;
         break;
   }

   switch (xmb->depth)
   {
      case 1:
      case 2:
         menu_animation_push(&entry);

         entry.target_value = xmb->depth - 1;
         entry.subject      = &xmb->textures_arrow_alpha;

         menu_animation_push(&entry);
         break;
   }

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(void *data,
      const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;
   unsigned xmb_system_tab;

   if (!xmb)
      return;

   /* Determine whether this is a playlist */
   xmb_system_tab = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);
   xmb->is_playlist = (xmb_system_tab == XMB_SYSTEM_TAB_FAVORITES) ||
                      (xmb_system_tab == XMB_SYSTEM_TAB_HISTORY) ||
#ifdef HAVE_IMAGEVIEWER
                      (xmb_system_tab == XMB_SYSTEM_TAB_IMAGES) ||
#endif
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST)) ||
                      string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST));
   xmb->is_playlist = xmb->is_playlist && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL));

   /* Determine whether this is a database manager list */
   xmb->is_db_manager_list = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST));

   /* Determine whether this is the quick menu */
   xmb->is_quick_menu = string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS));

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_PREVENT_POPULATE, NULL))
   {
      xmb_selection_pointer_changed(xmb, false);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      return;
   }

   xmb_set_title(xmb);

   if (xmb->categories_selection_ptr != xmb->categories_active_idx_old)
      xmb_list_switch(xmb);
   else
      xmb_list_open(xmb);
}

static uintptr_t xmb_icon_get_id(xmb_handle_t *xmb,
      xmb_node_t *core_node, xmb_node_t *node,
      enum msg_hash_enums enum_idx, unsigned type, bool active, bool checked)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CORE_OPTIONS:
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
      case MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST:
         return xmb->textures.list[XMB_TEXTURE_ADD_FAVORITE];
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
      case MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION:
         return xmb->textures.list[XMB_TEXTURE_UNDO];
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_DISK_OPTIONS:
      case MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS:
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
      case MENU_ENUM_LABEL_DISK_INDEX:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENT_LIST];
      case MENU_ENUM_LABEL_SAVE_STATE:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_ENUM_LABEL_LOAD_STATE:
      case MENU_ENUM_LABEL_CONFIGURATIONS:
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_ENUM_LABEL_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_RESTART_CONTENT:
      case MENU_ENUM_LABEL_REBOOT:
      case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
      case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
      case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_RENAME_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_RENAME];
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
      case MENU_ENUM_LABEL_FAVORITES: /* "Start Directory" */
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR:
         return xmb->textures.list[XMB_TEXTURE_RDB];

      /* Menu collection submenus */
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return xmb->textures.list[XMB_TEXTURE_ZIP];
      case MENU_ENUM_LABEL_GOTO_FAVORITES:
         return xmb->textures.list[XMB_TEXTURE_FAVORITE];
      case MENU_ENUM_LABEL_GOTO_IMAGES:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_GOTO_VIDEO:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      case MENU_ENUM_LABEL_GOTO_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC];

      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
         return xmb->textures.list[XMB_TEXTURE_QUICKMENU];
      case MENU_ENUM_LABEL_START_CORE:
      case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_ENUM_LABEL_CORE_LIST:
      case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
      case MENU_ENUM_LABEL_CORE_SETTINGS:
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
      case MENU_ENUM_LABEL_SCAN_FILE:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
      case MENU_ENUM_LABEL_UPDATER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_UPDATER];
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
         return xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST:
      case MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
      case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
#ifdef HAVE_VIDEO_LAYOUT
      case MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS:
#endif
         return xmb->textures.list[XMB_TEXTURE_OVERLAY];
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
      case MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS:
      case MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_ENUM_LABEL_INFORMATION:
      case MENU_ENUM_LABEL_INFORMATION_LIST:
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
         return xmb->textures.list[XMB_TEXTURE_INFO];
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         return xmb->textures.list[XMB_TEXTURE_CURSOR];
      case MENU_ENUM_LABEL_HELP_LIST:
      case MENU_ENUM_LABEL_HELP_CONTROLS:
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
      case MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO:
         return xmb->textures.list[XMB_TEXTURE_HELP];
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         return xmb->textures.list[XMB_TEXTURE_EXIT];
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_DRIVERS];
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_VIDEO];
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_AUDIO];
      case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_MIXER];
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
      case MENU_ENUM_LABEL_START_NET_RETROPAD:
         return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];
      case MENU_ENUM_LABEL_LATENCY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_LATENCY];
      case MENU_ENUM_LABEL_SAVING_SETTINGS:
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         return xmb->textures.list[XMB_TEXTURE_SAVING];
      case MENU_ENUM_LABEL_LOGGING_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_LOG];
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
      case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_FRAMESKIP];
      case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
      case MENU_ENUM_LABEL_RECORDING_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_RECORD];
      case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
         return xmb->textures.list[XMB_TEXTURE_STREAM];
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
      case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
      case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME:
      case MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR:
      case MENU_ENUM_LABEL_CORE_DELETE:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_OSD];
      case MENU_ENUM_LABEL_SHOW_WIMP:
      case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_UI];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
      case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
         return xmb->textures.list[XMB_TEXTURE_POWER];
#endif
      case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_POWER];
      case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENTS];
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_PLAYLIST];
      case MENU_ENUM_LABEL_USER_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_USER];
      case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_PRIVACY];
      case MENU_ENUM_LABEL_REWIND_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_REWIND];
      case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_OVERRIDE];
      case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
         return xmb->textures.list[XMB_TEXTURE_NOTIFICATIONS];
#ifdef HAVE_NETWORKING
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
         return xmb->textures.list[XMB_TEXTURE_ROOM];
      case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
      case MENU_ENUM_LABEL_NETWORK_SETTINGS:
      case MENU_ENUM_LABEL_WIFI_SETTINGS:
      case MENU_ENUM_LABEL_NETWORK_INFO_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_NETWORK];
#endif
      case MENU_ENUM_LABEL_SHUTDOWN:
         return xmb->textures.list[XMB_TEXTURE_SHUTDOWN];
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return xmb->textures.list[XMB_TEXTURE_CHECKMARK];
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
      case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
         return xmb->textures.list[XMB_TEXTURE_MENU_ADD];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
         return xmb->textures.list[XMB_TEXTURE_MENU_APPLY_TOGGLE];
      case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return xmb->textures.list[XMB_TEXTURE_MENU_APPLY_COG];
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      default:
         break;
   }

   switch(type)
   {
      case FILE_TYPE_DIRECTORY:
         return xmb->textures.list[XMB_TEXTURE_FOLDER];
      case FILE_TYPE_PLAIN:
      case FILE_TYPE_IN_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case FILE_TYPE_RPL_ENTRY:
         if (core_node)
            return core_node->content_icon;

         switch (xmb_get_system_tab(xmb,
                  (unsigned)xmb->categories_selection_ptr))
         {
            case XMB_SYSTEM_TAB_FAVORITES:
               return xmb->textures.list[XMB_TEXTURE_FAVORITE];
            case XMB_SYSTEM_TAB_MUSIC:
               return xmb->textures.list[XMB_TEXTURE_MUSIC];
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               return xmb->textures.list[XMB_TEXTURE_IMAGE];
#endif
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            case XMB_SYSTEM_TAB_VIDEO:
               return xmb->textures.list[XMB_TEXTURE_MOVIE];
#endif
            default:
               break;
         }
         return xmb->textures.list[XMB_TEXTURE_FILE];
      case FILE_TYPE_SHADER:
      case FILE_TYPE_SHADER_PRESET:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case FILE_TYPE_CARCHIVE:
         return xmb->textures.list[XMB_TEXTURE_ZIP];
      case FILE_TYPE_MUSIC:
         return xmb->textures.list[XMB_TEXTURE_MUSIC];
      case FILE_TYPE_IMAGE:
      case FILE_TYPE_IMAGEVIEWER:
         return xmb->textures.list[XMB_TEXTURE_IMAGE];
      case FILE_TYPE_MOVIE:
         return xmb->textures.list[XMB_TEXTURE_MOVIE];
      case FILE_TYPE_CORE:
      case FILE_TYPE_DIRECT_LOAD:
         return xmb->textures.list[XMB_TEXTURE_CORE];
      case FILE_TYPE_RDB:
         return xmb->textures.list[XMB_TEXTURE_RDB];
      case FILE_TYPE_CURSOR:
         return xmb->textures.list[XMB_TEXTURE_CURSOR];
      case FILE_TYPE_PLAYLIST_ENTRY:
      case MENU_SETTING_ACTION_RUN:
         return xmb->textures.list[XMB_TEXTURE_RUN];
      case MENU_SETTING_ACTION_RESUME_ACHIEVEMENTS:
         return xmb->textures.list[XMB_TEXTURE_RESUME];
      case MENU_SETTING_ACTION_CLOSE:
      case MENU_SETTING_ACTION_DELETE_ENTRY:
         return xmb->textures.list[XMB_TEXTURE_CLOSE];
      case MENU_SETTING_ACTION_SAVESTATE:
         return xmb->textures.list[XMB_TEXTURE_SAVESTATE];
      case MENU_SETTING_ACTION_LOADSTATE:
         return xmb->textures.list[XMB_TEXTURE_LOADSTATE];
      case FILE_TYPE_RDB_ENTRY:
      case MENU_SETTING_ACTION_CORE_INFORMATION:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_SETTING_ACTION_CORE_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CORE_OPTIONS];
      case MENU_SETTING_ACTION_CORE_INPUT_REMAPPING_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_INPUT_REMAPPING_OPTIONS];
      case MENU_SETTING_ACTION_CORE_CHEAT_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_CHEAT_OPTIONS];
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_DISK_OPTIONS];
      case MENU_SETTING_ACTION_CORE_SHADER_OPTIONS:
         return xmb->textures.list[XMB_TEXTURE_SHADER_OPTIONS];
      case MENU_SETTING_ACTION_SCREENSHOT:
         return xmb->textures.list[XMB_TEXTURE_SCREENSHOT];
      case MENU_SETTING_ACTION_RESET:
         return xmb->textures.list[XMB_TEXTURE_RELOAD];
      case MENU_SETTING_ACTION_PAUSE_ACHIEVEMENTS:
         return xmb->textures.list[XMB_TEXTURE_PAUSE];
#ifdef HAVE_LAKKA_SWITCH
      case MENU_SET_SWITCH_BRIGHTNESS:
         return xmb->textures.list[XMB_TEXTURE_BRIGHTNESS];
#endif
      case MENU_SETTING_GROUP:
         return xmb->textures.list[XMB_TEXTURE_SETTING];
      case MENU_INFO_MESSAGE:
         return xmb->textures.list[XMB_TEXTURE_CORE_INFO];
      case MENU_WIFI:
         return xmb->textures.list[XMB_TEXTURE_WIFI];
#ifdef HAVE_NETWORKING
      case MENU_ROOM:
         return xmb->textures.list[XMB_TEXTURE_ROOM];
      case MENU_ROOM_LAN:
         return xmb->textures.list[XMB_TEXTURE_ROOM_LAN];
      case MENU_ROOM_RELAY:
         return xmb->textures.list[XMB_TEXTURE_ROOM_RELAY];
#endif
   }

#ifdef HAVE_CHEEVOS
   if (
         (type >= MENU_SETTINGS_CHEEVOS_START) &&
         (type < MENU_SETTINGS_NETPLAY_ROOMS_START)
      )
   {
      int new_id = type - MENU_SETTINGS_CHEEVOS_START;
      if (get_badge_texture(new_id) != 0)
         return get_badge_texture(new_id);
      /* Should be replaced with placeholder badge icon. */
      return xmb->textures.list[XMB_TEXTURE_ACHIEVEMENTS];
   }
#endif

   if (
         (type >= MENU_SETTINGS_INPUT_BEGIN) &&
         (type <= MENU_SETTINGS_INPUT_DESC_END)
      )
      {
         unsigned input_id;
         if (type < MENU_SETTINGS_INPUT_DESC_BEGIN)
         /* Input User # Binds only */
         {
            input_id = MENU_SETTINGS_INPUT_BEGIN;
            if ( type == input_id + 1)
               return xmb->textures.list[XMB_TEXTURE_INPUT_ADC];
            if ( type == input_id + 2)
               return xmb->textures.list[XMB_TEXTURE_INPUT_SETTINGS];
            if ( type == input_id + 3)
               return xmb->textures.list[XMB_TEXTURE_INPUT_BIND_ALL];
            if ( type == input_id + 4)
               return xmb->textures.list[XMB_TEXTURE_RELOAD];
            if ( type == input_id + 5)
               return xmb->textures.list[XMB_TEXTURE_SAVING];
            if ( type == input_id + 6)
               return xmb->textures.list[XMB_TEXTURE_INPUT_MOUSE];
            if ((type > (input_id + 30)) && (type < (input_id + 42)))
               return xmb->textures.list[XMB_TEXTURE_INPUT_LGUN];
            if ( type == input_id + 42)
               return xmb->textures.list[XMB_TEXTURE_INPUT_TURBO];
            /* align to use the same code of Quickmenu controls */
            input_id = input_id + 7;
         }
         else
         {
            /* Quickmenu controls repeats the same icons for all users */
            input_id = MENU_SETTINGS_INPUT_DESC_BEGIN;
            while (type > (input_id + 23))
            {
               input_id = (input_id + 24) ;
            }
         }
         /* This is utilized for both Input # Binds and Quickmenu controls */
         if ( type == input_id )
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_D];
         if ( type == (input_id + 1))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_L];
         if ( type == (input_id + 2))
            return xmb->textures.list[XMB_TEXTURE_INPUT_SELECT];
         if ( type == (input_id + 3))
            return xmb->textures.list[XMB_TEXTURE_INPUT_START];
         if ( type == (input_id + 4))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_U];
         if ( type == (input_id + 5))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_D];
         if ( type == (input_id + 6))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_L];
         if ( type == (input_id + 7))
            return xmb->textures.list[XMB_TEXTURE_INPUT_DPAD_R];
         if ( type == (input_id + 8))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_R];
         if ( type == (input_id + 9))
            return xmb->textures.list[XMB_TEXTURE_INPUT_BTN_U];
         if ( type == (input_id + 10))
            return xmb->textures.list[XMB_TEXTURE_INPUT_LB];
         if ( type == (input_id + 11))
            return xmb->textures.list[XMB_TEXTURE_INPUT_RB];
         if ( type == (input_id + 12))
            return xmb->textures.list[XMB_TEXTURE_INPUT_LT];
         if ( type == (input_id + 13))
            return xmb->textures.list[XMB_TEXTURE_INPUT_RT];
         if ( type == (input_id + 14))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_P];
         if ( type == (input_id + 15))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_P];
         if ( type == (input_id + 16))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_R];
         if ( type == (input_id + 17))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_L];
         if ( type == (input_id + 18))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_D];
         if ( type == (input_id + 19))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_U];
         if ( type == (input_id + 20))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_R];
         if ( type == (input_id + 21))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_L];
         if ( type == (input_id + 22))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_D];
         if ( type == (input_id + 23))
            return xmb->textures.list[XMB_TEXTURE_INPUT_STCK_U];
      }

   if (checked)
      return xmb->textures.list[XMB_TEXTURE_CHECKMARK];

   if (type == MENU_SETTING_ACTION)
      return xmb->textures.list[XMB_TEXTURE_SETTING];

   return xmb->textures.list[XMB_TEXTURE_SUBSETTING];

}

static void xmb_calculate_visible_range(const xmb_handle_t *xmb,
      unsigned height, size_t list_size, unsigned current,
      unsigned *first, unsigned *last)
{
   unsigned j;
   float    base_y = xmb->margins_screen_top;

   *first = 0;
   *last  = (unsigned)(list_size ? list_size - 1 : 0);

   if (current)
   {
      for (j = current; j-- > 0; )
      {
         float bottom = xmb_item_y(xmb, j, current)
            + base_y + xmb->icon_size;

         if (bottom < 0)
            break;

         *first = j;
      }
   }

   for (j = current+1; j < list_size; j++)
   {
      float top = xmb_item_y(xmb, j, current) + base_y;

      if (top > height)
         break;

      *last = j;
   }
}

static int xmb_draw_item(
      video_frame_info_t *video_info,
      menu_entry_t *entry,
      math_matrix_4x4 *mymat,
      xmb_handle_t *xmb,
      xmb_node_t *core_node,
      file_list_t *list,
      float *color,
      size_t i,
      size_t current,
      unsigned width,
      unsigned height
      )
{
   float icon_x, icon_y, label_offset;
   menu_animation_ctx_ticker_t ticker;
   char tmp[255];
   const char *ticker_str            = NULL;
   unsigned entry_type               = 0;
   const float half_size             = xmb->icon_size / 2.0f;
   uintptr_t texture_switch          = 0;
   bool do_draw_text                 = false;
   unsigned ticker_limit             = 35 * scale_mod[0];
   xmb_node_t *   node               = (xmb_node_t*)
      file_list_get_userdata_at_offset(list, i);
   settings_t *settings              = config_get_ptr();

   /* Initial ticker configuration */
   ticker.type_enum = (enum menu_animation_ticker_type)settings->uints.menu_ticker_type;
   ticker.spacer = NULL;

   if (!node)
      return 0;

   tmp[0] = '\0';

   icon_y = xmb->margins_screen_top + node->y + half_size;

   if (icon_y < half_size)
      return 0;

   if (icon_y > height + xmb->icon_size)
      return -1;

   icon_x = node->x + xmb->margins_screen_left +
      xmb->icon_spacing_horizontal - half_size;

   if (icon_x < -half_size || icon_x > width)
      return 0;

   entry_type = menu_entry_get_type_new(entry);

   if (entry_type == FILE_TYPE_CONTENTLIST_ENTRY)
   {
      char entry_path[PATH_MAX_LENGTH] = {0};
      strlcpy(entry_path, entry->path, sizeof(entry_path));

      fill_short_pathname_representation(entry_path, entry_path,
            sizeof(entry_path));

      if (!string_is_empty(entry_path))
         strlcpy(entry->path, entry_path, sizeof(entry->path));
   }

   if (string_is_equal(entry->value,
            msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
         (string_is_equal(entry->value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))))
   {
      if (xmb->textures.list[XMB_TEXTURE_SWITCH_OFF])
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_OFF];
      else
         do_draw_text = true;
   }
   else if (string_is_equal(entry->value,
            msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
         (string_is_equal(entry->value,
                          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))))
   {
      if (xmb->textures.list[XMB_TEXTURE_SWITCH_ON])
         texture_switch = xmb->textures.list[XMB_TEXTURE_SWITCH_ON];
      else
         do_draw_text = true;
   }
   else
   {
      if (!string_is_empty(entry->value))
      {
         if (
               string_is_equal(entry->value, "...")     ||
               string_is_equal(entry->value, "(PRESET)")  ||
               string_is_equal(entry->value, "(SHADER)")  ||
               string_is_equal(entry->value, "(COMP)")  ||
               string_is_equal(entry->value, "(CORE)")  ||
               string_is_equal(entry->value, "(MOVIE)") ||
               string_is_equal(entry->value, "(MUSIC)") ||
               string_is_equal(entry->value, "(DIR)")   ||
               string_is_equal(entry->value, "(RDB)")   ||
               string_is_equal(entry->value, "(CURSOR)")||
               string_is_equal(entry->value, "(CFILE)") ||
               string_is_equal(entry->value, "(FILE)")  ||
               string_is_equal(entry->value, "(IMAGE)")
            )
         {
         }
         else
            do_draw_text = true;
      }
      else
         do_draw_text = true;

   }

   if (string_is_empty(entry->value))
   {
      if (xmb->savestate_thumbnail ||
            !xmb->use_ps3_layout ||
            (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT)
             && xmb->thumbnail) ||
            (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT)
             && xmb->left_thumbnail
             && settings->bools.menu_xmb_vertical_thumbnails)
         )
      {
         ticker_limit = 40 * scale_mod[1];

         /* Can increase text length if thumbnail is downscaled */
         if (settings->uints.menu_xmb_thumbnail_scale_factor < 100)
            ticker_limit +=
                  (unsigned)((1.0f - ((float)settings->uints.menu_xmb_thumbnail_scale_factor / 100.0f)) *
                             15.0f * scale_mod[1]);
      }
      else
         ticker_limit = 70 * scale_mod[2];
   }

   menu_entry_get_rich_label(entry, &ticker_str);

   ticker.s        = tmp;
   ticker.len      = ticker_limit;
   ticker.idx      = menu_animation_get_ticker_idx();
   ticker.str      = ticker_str;
   ticker.selected = (i == current);

   if (ticker.str)
      menu_animation_ticker(&ticker);

   label_offset = xmb->margins_label_top;

   if (settings->bools.menu_show_sublabels)
   {
      if (i == current && width > 320 && height > 240
            && !string_is_empty(entry->sublabel))
      {
         char entry_sublabel[512] = {0};

         label_offset      = - xmb->margins_label_top;

         word_wrap(entry_sublabel, entry->sublabel, 50 * scale_mod[3], true, 0);

         xmb_draw_text(video_info, xmb, entry_sublabel,
               node->x + xmb->margins_screen_left +
               xmb->icon_spacing_horizontal + xmb->margins_label_left,
               xmb->margins_screen_top + node->y + xmb->margins_label_top*3.5,
               1, node->label_alpha, TEXT_ALIGN_LEFT,
               width, height, xmb->font2);
      }
   }

   xmb_draw_text(video_info, xmb, tmp,
         node->x + xmb->margins_screen_left +
         xmb->icon_spacing_horizontal + xmb->margins_label_left,
         xmb->margins_screen_top + node->y + label_offset,
         1, node->label_alpha, TEXT_ALIGN_LEFT,
         width, height, xmb->font);

   tmp[0]          = '\0';

   ticker.s        = tmp;
   ticker.len      = 35 * scale_mod[7];
   ticker.idx      = menu_animation_get_ticker_idx();
   ticker.selected = (i == current);

   if (!string_is_empty(entry->value))
   {
      const char *entry_value = NULL;
      menu_entry_get_value(entry, &entry_value);
      ticker.str   = entry_value;

      menu_animation_ticker(&ticker);
   }

   if (do_draw_text)
      xmb_draw_text(video_info, xmb, tmp,
            node->x +
            + xmb->margins_screen_left
            + xmb->icon_spacing_horizontal
            + xmb->margins_label_left
            + xmb->margins_setting_left,
            xmb->margins_screen_top + node->y + xmb->margins_label_top,
            1,
            node->label_alpha,
            TEXT_ALIGN_LEFT,
            width, height, xmb->font);

   menu_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

   if (
         (!xmb->assets_missing) &&
         (color[3] != 0) &&
         (
            (entry->checked) ||
            !((entry_type >= MENU_SETTING_DROPDOWN_ITEM) && (entry_type <= MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL))
         )
      )
   {
      math_matrix_4x4 mymat_tmp;
      menu_display_ctx_rotate_draw_t rotate_draw;
      uintptr_t texture        = xmb_icon_get_id(xmb, core_node, node,
            entry->enum_idx, entry_type, (i == current), entry->checked);
      float x                  = icon_x;
      float y                  = icon_y;
      float rotation           = 0;
      float scale_factor       = node->zoom;

      rotate_draw.matrix       = &mymat_tmp;
      rotate_draw.rotation     = rotation;
      rotate_draw.scale_x      = scale_factor;
      rotate_draw.scale_y      = scale_factor;
      rotate_draw.scale_z      = 1;
      rotate_draw.scale_enable = true;

      menu_display_rotate_z(&rotate_draw, video_info);

      xmb_draw_icon(video_info,
         xmb->icon_size,
         &mymat_tmp,
         texture,
         x,
         y,
         width,
         height,
         1.0,
         rotation,
         scale_factor,
         &color[0],
         xmb->shadow_offset);
   }

   menu_display_set_alpha(color, MIN(node->alpha, xmb->alpha));

   if (texture_switch != 0 && color[3] != 0 && !xmb->assets_missing)
      xmb_draw_icon(video_info,
            xmb->icon_size,
            mymat,
            texture_switch,
            node->x + xmb->margins_screen_left
            + xmb->icon_spacing_horizontal
            + xmb->icon_size / 2.0 + xmb->margins_setting_left,
            xmb->margins_screen_top + node->y + xmb->icon_size / 2.0,
            width, height,
            node->alpha,
            0,
            1,
            &color[0],
            xmb->shadow_offset);

   return 0;
}

static void xmb_draw_items(
      video_frame_info_t *video_info,
      xmb_handle_t *xmb,
      file_list_t *list,
      size_t current, size_t cat_selection_ptr, float *color,
      unsigned width, unsigned height)
{
   size_t i;
   unsigned first, last;
   math_matrix_4x4 mymat;
   menu_display_ctx_rotate_draw_t rotate_draw;
   xmb_node_t *core_node       = NULL;
   size_t end                  = 0;

   if (!list || !list->size || !xmb)
      return;

   if (cat_selection_ptr > xmb->system_tab_end)
      core_node = xmb_get_userdata_from_horizontal_list(
            xmb, (unsigned)(cat_selection_ptr - (xmb->system_tab_end + 1)));

   end                      = file_list_get_size(list);

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (list == xmb->selection_buf_old)
   {
      xmb_node_t *node = (xmb_node_t*)
         file_list_get_userdata_at_offset(list, current);

      if (node && (uint8_t)(255 * node->alpha) == 0)
         return;

      i = 0;
   }

   first = (unsigned)i;
   last  = (unsigned)(end - 1);

   xmb_calculate_visible_range(xmb, height, end, (unsigned)current, &first, &last);

   menu_display_blend_begin(video_info);

   for (i = first; i <= last; i++)
   {
      int ret;
      menu_entry_t entry;
      menu_entry_init(&entry);
      entry.label_enabled      = false;
      entry.sublabel_enabled   = (i == current);
      menu_entry_get(&entry, 0, i, list, true);
      ret = xmb_draw_item(video_info,
            &entry,
            &mymat,
            xmb, core_node,
            list, color,
            i, current,
            width, height);
      if (ret == -1)
         break;
   }

   menu_display_blend_end(video_info);
}

static void xmb_context_reset_internal(xmb_handle_t *xmb,
      bool is_threaded, bool reinit_textures);

static void xmb_render(void *data, bool is_idle)
{
   size_t i;
   settings_t   *settings   = config_get_ptr();
   xmb_handle_t *xmb        = (xmb_handle_t*)data;
   unsigned      end        = (unsigned)menu_entries_get_size();
   bool mouse_enable        = settings->bools.menu_mouse_enable;
   bool pointer_enable      = settings->bools.menu_pointer_enable;

   unsigned width, height;
   float scale_factor;

    if (!xmb)
      return;

   video_driver_get_size(&width, &height);

   scale_factor = (settings->uints.menu_xmb_scale_factor * (float)width) / (1920.0 * 100);

   if (scale_factor >= 0.1f && scale_factor != xmb->previous_scale_factor)
      xmb_context_reset_internal(xmb, video_driver_is_threaded(),
            false);

   xmb->previous_scale_factor = scale_factor;

   if (pointer_enable || mouse_enable)
   {
      unsigned height;
      size_t selection  = menu_navigation_get_selection();
      int16_t pointer_y = menu_input_pointer_state(MENU_POINTER_Y_AXIS);
      int16_t mouse_y   = menu_input_mouse_state(MENU_MOUSE_Y_AXIS)
         + (xmb->cursor_size/2);
      unsigned first = 0, last = end;

      video_driver_get_size(NULL, &height);

      if (height)
         xmb_calculate_visible_range(xmb, height,
               end, (unsigned)selection, &first, &last);

      for (i = first; i <= last; i++)
      {
         float item_y1     = xmb->margins_screen_top
            + xmb_item_y(xmb, (int)i, selection);
         float item_y2     = item_y1 + xmb->icon_size;

         if (pointer_enable)
         {
            if (pointer_y > item_y1 && pointer_y < item_y2)
               menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &i);
         }

         if (mouse_enable)
         {
            if (mouse_y > item_y1 && mouse_y < item_y2)
               menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &i);
         }
      }
   }

   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   if (i >= end)
   {
      i = 0;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &i);
   }

   menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);
}

static bool xmb_shader_pipeline_active(video_frame_info_t *video_info)
{
   if (string_is_not_equal(menu_driver_ident(), "xmb"))
      return false;
   if (video_info->menu_shader_pipeline == XMB_SHADER_PIPELINE_WALLPAPER)
      return false;
   return true;
}

static void xmb_draw_bg(
      xmb_handle_t *xmb,
      video_frame_info_t *video_info,
      unsigned width,
      unsigned height,
      float alpha,
      uintptr_t texture_id,
      float *coord_black,
      float *coord_white)
{
   menu_display_ctx_draw_t draw;

   bool running              = video_info->libretro_running;

   draw.x                    = 0;
   draw.y                    = 0;
   draw.texture              = texture_id;
   draw.width                = width;
   draw.height               = height;
   draw.color                = &coord_black[0];
   draw.vertex               = NULL;
   draw.tex_coord            = NULL;
   draw.vertex_count         = 4;
   draw.prim_type            = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id          = 0;
   draw.pipeline.active      = xmb_shader_pipeline_active(video_info);

   menu_display_blend_begin(video_info);
   menu_display_set_viewport(video_info->width, video_info->height);

#ifdef HAVE_SHADERPIPELINE
   if (video_info->menu_shader_pipeline > XMB_SHADER_PIPELINE_WALLPAPER
         &&
         (video_info->xmb_color_theme != XMB_THEME_WALLPAPER))
   {
      draw.color = xmb_gradient_ident(video_info);

      if (running)
         menu_display_set_alpha(draw.color, coord_black[3]);
      else
         menu_display_set_alpha(draw.color, coord_white[3]);

      menu_display_draw_gradient(&draw, video_info);

      draw.pipeline.id = VIDEO_SHADER_MENU_2;

      switch (video_info->menu_shader_pipeline)
      {
         case XMB_SHADER_PIPELINE_RIBBON:
            draw.pipeline.id  = VIDEO_SHADER_MENU;
            break;
         case XMB_SHADER_PIPELINE_SIMPLE_SNOW:
            draw.pipeline.id  = VIDEO_SHADER_MENU_3;
            break;
         case XMB_SHADER_PIPELINE_SNOW:
            draw.pipeline.id  = VIDEO_SHADER_MENU_4;
            break;
         case XMB_SHADER_PIPELINE_BOKEH:
            draw.pipeline.id  = VIDEO_SHADER_MENU_5;
            break;
         case XMB_SHADER_PIPELINE_SNOWFLAKE:
            draw.pipeline.id  = VIDEO_SHADER_MENU_6;
            break;
         default:
            break;
      }

      menu_display_draw_pipeline(&draw, video_info);
   }
   else
#endif
   {
      uintptr_t texture           = draw.texture;

      if (video_info->xmb_color_theme != XMB_THEME_WALLPAPER)
         draw.color = xmb_gradient_ident(video_info);

      if (running)
         menu_display_set_alpha(draw.color, coord_black[3]);
      else
         menu_display_set_alpha(draw.color, coord_white[3]);

      if (video_info->xmb_color_theme != XMB_THEME_WALLPAPER)
         menu_display_draw_gradient(&draw, video_info);

      {
         float override_opacity = video_info->menu_wallpaper_opacity;
         bool add_opacity       = false;

         draw.texture           = texture;
         menu_display_set_alpha(draw.color, coord_white[3]);

         if (draw.texture)
            draw.color = &coord_white[0];

         if (running || video_info->xmb_color_theme == XMB_THEME_WALLPAPER)
            add_opacity = true;

         menu_display_draw_bg(&draw, video_info, add_opacity, override_opacity);
      }
   }

   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void xmb_draw_dark_layer(
      xmb_handle_t *xmb,
      video_frame_info_t *video_info,
      unsigned width,
      unsigned height)
{
   menu_display_ctx_draw_t draw;
   struct video_coords coords;
   float black[16] = {
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
      0, 0, 0, 1,
   };

   menu_display_set_alpha(black, MIN(xmb->alpha, 0.75));

   coords.vertices      = 4;
   coords.vertex        = NULL;
   coords.tex_coord     = NULL;
   coords.lut_tex_coord = NULL;
   coords.color         = &black[0];

   draw.x           = 0;
   draw.y           = 0;
   draw.width       = width;
   draw.height      = height;
   draw.coords      = &coords;
   draw.matrix_data = NULL;
   draw.texture     = menu_display_white_texture;
   draw.prim_type   = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
   draw.pipeline.id = 0;

   menu_display_blend_begin(video_info);
   menu_display_draw(&draw, video_info);
   menu_display_blend_end(video_info);
}

static void xmb_frame(void *data, video_frame_info_t *video_info)
{
   math_matrix_4x4 mymat;
   unsigned i;
   menu_display_ctx_rotate_draw_t rotate_draw;
   char msg[1024];
   char title_msg[255];
   char title_truncated[255];
   size_t selection                        = 0;
   size_t percent_width                    = 0;
   const int min_thumb_size                = 50;
   bool render_background                  = false;
   file_list_t *selection_buf              = NULL;
   unsigned width                          = video_info->width;
   unsigned height                         = video_info->height;
   const float under_thumb_margin          = 0.96;
   float scale_factor                      = 0.0f;
   float thumbnail_scale_factor            = 0.0f;
   float pseudo_font_length                = 0.0f;
   xmb_handle_t *xmb                       = (xmb_handle_t*)data;
   settings_t *settings                    = config_get_ptr();
   unsigned xmb_system_tab                 = xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr);
   bool hide_thumbnails                    = false;

   if (!xmb)
      return;

   scale_factor                            = (settings->uints.menu_xmb_scale_factor * (float)width) / (1920.0 * 100);
   thumbnail_scale_factor                  = ((float)settings->uints.menu_xmb_thumbnail_scale_factor / 100.0f);
   pseudo_font_length                      = xmb->icon_spacing_horizontal * 4 - xmb->icon_size / 4;

   msg[0]             = '\0';
   title_msg[0]       = '\0';
   title_truncated[0] = '\0';

   font_driver_bind_block(xmb->font,  &xmb->raster_block);
   font_driver_bind_block(xmb->font2, &xmb->raster_block2);

   xmb->raster_block.carr.coords.vertices  = 0;
   xmb->raster_block2.carr.coords.vertices = 0;

   menu_display_set_alpha(coord_black, MIN(
            (float)video_info->xmb_alpha_factor/100, xmb->alpha));
   menu_display_set_alpha(coord_white, xmb->alpha);

   xmb_draw_bg(
         xmb,
         video_info,
         width,
         height,
         xmb->alpha,
         xmb->textures.bg,
         coord_black,
         coord_white);

   selection = menu_navigation_get_selection();

   strlcpy(title_truncated,
         xmb->title_name, sizeof(title_truncated));

   if (selection > 1)
   {
      /* skip 25 utf8 multi-byte chars */
      char *end = title_truncated;

      for(i = 0; i < 25 && *end; i++)
      {
         end++;
         while((*end & 0xC0) == 0x80)
            end++;
      }

      *end = '\0';
   }

   /* Title text */
   xmb_draw_text(video_info, xmb,
         title_truncated, xmb->margins_title_left,
         xmb->margins_title_top,
         1, 1, TEXT_ALIGN_LEFT,
         width, height, xmb->font);

   if (settings->bools.menu_core_enable)
   {
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      xmb_draw_text(video_info, xmb, title_msg, xmb->margins_title_left,
            height - xmb->margins_title_bottom, 1, 1, TEXT_ALIGN_LEFT,
            width, height, xmb->font);
   }

   rotate_draw.matrix       = &mymat;
   rotate_draw.rotation     = 0;
   rotate_draw.scale_x      = 1;
   rotate_draw.scale_y      = 1;
   rotate_draw.scale_z      = 1;
   rotate_draw.scale_enable = true;

   menu_display_rotate_z(&rotate_draw, video_info);
   menu_display_blend_begin(video_info);

   /* Save State thumbnail, right side */
   if (xmb->savestate_thumbnail)
   {
      float thumb_width     = 0.0f;
      float thumb_height    = 0.0f;
      float thumb_max_width = (float)width - (xmb->icon_size / 6)
         - (xmb->margins_screen_left * scale_mod[5]) -
         xmb->icon_spacing_horizontal - pseudo_font_length;
      float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

      /* Limit thumbnail width */
      if (xmb->savestate_thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
      {
         thumb_width  = (xmb->savestate_thumbnail_width * scale_mod[4]) *
            (thumb_max_width_scaled / (xmb->savestate_thumbnail_width * scale_mod[4]));
         thumb_height = (xmb->savestate_thumbnail_height * scale_mod[4]) *
            (thumb_max_width_scaled / (xmb->savestate_thumbnail_width * scale_mod[4]));
      }
      else
      {
         thumb_width  = xmb->savestate_thumbnail_width * scale_mod[4];
         thumb_height = xmb->savestate_thumbnail_height * scale_mod[4];
      }

      /* Limit thumbnail height to screen height + margin. */
      if (xmb->margins_screen_top + xmb->icon_size + thumb_height >=
            ((float)height * under_thumb_margin))
      {
         thumb_width = thumb_width *
            ((((float)height * under_thumb_margin) -
              xmb->margins_screen_top - xmb->icon_size) /
             thumb_height);
         thumb_height = thumb_height *
            ((((float)height * under_thumb_margin) -
              xmb->margins_screen_top - xmb->icon_size) /
             thumb_height);
      }

      xmb_draw_thumbnail(video_info,
            xmb, &coord_white[0], width, height,
            (float)width - (xmb->icon_size / 6) - thumb_max_width +
            ((thumb_max_width - thumb_width) / 2),
            xmb->margins_screen_top + xmb->icon_size + thumb_height,
            thumb_width, thumb_height,
            xmb->savestate_thumbnail);
   }

   /* This is used for hiding thumbnails when going into sub-levels in the
    * Quick Menu as well as when selecting "Information" for a playlist entry.
    * NOTE: This is currently a pretty crude check, simply going by menu depth
    * and not specifically identifying which menu we're actually in. */
   hide_thumbnails = xmb_system_tab > XMB_SYSTEM_TAB_SETTINGS && xmb->depth > 2;

   /* Right thumbnail big size */
   if (!hide_thumbnails && !xmb->savestate_thumbnail && xmb->use_ps3_layout &&
         (!settings->bools.menu_xmb_vertical_thumbnails ||
         (settings->bools.menu_xmb_vertical_thumbnails && !xmb->left_thumbnail)))
   {
      /* Do not draw the right thumbnail if there is no space available */

      if (((xmb->margins_screen_top +
                  xmb->icon_size + min_thumb_size) <= height) &&
            ((xmb->margins_screen_left * scale_mod[5] +
              xmb->icon_spacing_horizontal +
              pseudo_font_length + min_thumb_size) <= width))
      {
         if (xmb->thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
         {
            /* Limit thumbnail width */

            float thumb_width     = 0.0f;
            float thumb_height    = 0.0f;
            float thumb_max_width = (float)width - (xmb->icon_size / 6)
               - (xmb->margins_screen_left * scale_mod[5]) -
               xmb->icon_spacing_horizontal - pseudo_font_length;
            float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

#ifdef XMB_DEBUG
            RARCH_LOG("[XMB thumbnail] width: %.2f, height: %.2f\n",
                  xmb->thumbnail_width, xmb->thumbnail_height);
            RARCH_LOG("[XMB thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

            if (xmb->thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
            {
               thumb_width  = (xmb->thumbnail_width * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->thumbnail_width * scale_mod[4]));
               thumb_height = (xmb->thumbnail_height * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->thumbnail_width * scale_mod[4]));
            }
            else
            {
               thumb_width  = xmb->thumbnail_width * scale_mod[4];
               thumb_height = xmb->thumbnail_height * scale_mod[4];
            }

            /* Limit thumbnail height to screen height + margin. */

            if (xmb->margins_screen_top + xmb->icon_size + thumb_height >=
                  ((float)height * under_thumb_margin))
            {
               thumb_width = thumb_width *
                  ((((float)height * under_thumb_margin) -
                    xmb->margins_screen_top - xmb->icon_size) /
                   thumb_height);
               thumb_height = thumb_height *
                  ((((float)height * under_thumb_margin) -
                    xmb->margins_screen_top - xmb->icon_size) /
                   thumb_height);
            }

            xmb_draw_thumbnail(video_info,
                  xmb, &coord_white[0], width, height,
                  (float)width - (xmb->icon_size / 6) - thumb_max_width +
                  ((thumb_max_width - thumb_width) / 2),
                  xmb->margins_screen_top + xmb->icon_size + thumb_height,
                  thumb_width, thumb_height,
                  xmb->thumbnail);
         }
      }
   }

   /* Left thumbnail in the left margin */
   /* Do not draw the left thumbnail if there is no space available */
   if (!hide_thumbnails && xmb->use_ps3_layout &&
         !settings->bools.menu_xmb_vertical_thumbnails &&
         (xmb->margins_screen_top + xmb->icon_size *
         (!(xmb->depth == 1)? 2.1 : 1) + min_thumb_size)
         <= (float)height)
   {
      /* Left Thumbnail in the left margin */

      if (xmb->left_thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
      {
         /* Limit left thumbnail width */

         float left_thumb_width     = 0.0f;
         float left_thumb_height    = 0.0f;
         float thumb_max_width      = xmb->icon_size * 3.4;
         float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;
         float y_offset_factor      = !(xmb->depth == 1) ? 2.1 : 1;
         float thumb_max_height     = ((float)height - (96.0 * scale_factor)) -
                                      xmb->margins_screen_top -
                                      (xmb->icon_size * y_offset_factor);

#ifdef XMB_DEBUG
         RARCH_LOG("[XMB left thumbnail] width: %.2f, height: %.2f\n",
               xmb->left_thumbnail_width, xmb->left_thumbnail_height);
         RARCH_LOG("[XMB left thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

         if (xmb->left_thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
         {
            left_thumb_width  = (xmb->left_thumbnail_width * scale_mod[4]) *
               (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
            left_thumb_height = (xmb->left_thumbnail_height * scale_mod[4]) *
               (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
         }
         else
         {
            left_thumb_width  = xmb->left_thumbnail_width * scale_mod[4];
            left_thumb_height = xmb->left_thumbnail_height * scale_mod[4];
         }

         /* Limit left thumbnail height to screen height + margin. */
         if (left_thumb_height >= thumb_max_height)
         {
            left_thumb_width = left_thumb_width *
               (thumb_max_height / left_thumb_height);

            left_thumb_height = thumb_max_height;
         }

         xmb_draw_thumbnail(video_info,
               xmb, &coord_white[0], width, height,
               (xmb->icon_size / 6) +
               ((thumb_max_width - left_thumb_width) / 2),
               xmb->margins_screen_top + (xmb->icon_size * y_offset_factor) +
               left_thumb_height + (!(xmb->depth == 1) ?
               (1.0f - thumbnail_scale_factor) * 0.5f * (thumb_max_height - left_thumb_height) : 0),
               left_thumb_width, left_thumb_height,
               xmb->left_thumbnail);
      }
   }

   /* No Right Thumbnail, draw only the left one big size */
   if (!hide_thumbnails && !xmb->savestate_thumbnail && xmb->use_ps3_layout &&
      settings->bools.menu_xmb_vertical_thumbnails && !xmb->thumbnail)
   {
      /* Do not draw the left thumbnail if there is no space available */

      if (((xmb->margins_screen_top +
                  xmb->icon_size + min_thumb_size) <= height) &&
            ((xmb->margins_screen_left * scale_mod[5] +
              xmb->icon_spacing_horizontal +
              pseudo_font_length + min_thumb_size) <= width))
      {
         if (xmb->left_thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
         {
            /* Limit left thumbnail width */

            float left_thumb_width     = 0.0f;
            float left_thumb_height    = 0.0f;
            float thumb_max_width = (float)width - (xmb->icon_size / 6)
               - (xmb->margins_screen_left * scale_mod[5]) -
               xmb->icon_spacing_horizontal - pseudo_font_length;
            float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

#ifdef XMB_DEBUG
            RARCH_LOG("[XMB thumbnail] width: %.2f, height: %.2f\n",
                  xmb->thumbnail_width, xmb->thumbnail_height);
            RARCH_LOG("[XMB thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

            if (xmb->left_thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
            {
               left_thumb_width  = (xmb->left_thumbnail_width * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
               left_thumb_height = (xmb->left_thumbnail_height * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
            }
            else
            {
               left_thumb_width  = xmb->left_thumbnail_width * scale_mod[4];
               left_thumb_height = xmb->left_thumbnail_height * scale_mod[4];
            }

            /* Limit left thumbnail height to screen height + margin. */

            if (xmb->margins_screen_top + xmb->icon_size + left_thumb_height >=
                  ((float)height * under_thumb_margin))
            {
               left_thumb_width = left_thumb_width *
                  ((((float)height * under_thumb_margin) -
                    xmb->margins_screen_top - xmb->icon_size) /
                   left_thumb_height);
               left_thumb_height = left_thumb_height *
                  ((((float)height * under_thumb_margin) -
                    xmb->margins_screen_top - xmb->icon_size) /
                   left_thumb_height);
            }

            xmb_draw_thumbnail(video_info,
                  xmb, &coord_white[0], width, height,
                  (float)width - (xmb->icon_size / 6) - thumb_max_width +
                  ((thumb_max_width - left_thumb_width) / 2),
                  xmb->margins_screen_top + xmb->icon_size + left_thumb_height,
                  left_thumb_width, left_thumb_height,
                  xmb->left_thumbnail);
         }
      }
   }

   /* PSP Layout Only - Left thumbnail in the left margin */
   /* Do not draw the left thumbnail if there is no space available */
   if (!hide_thumbnails && !xmb->use_ps3_layout &&
         (xmb->margins_screen_top + xmb->icon_size * 1.5)
         <= (float)height)
   {
      /* Left Thumbnail in the left margin */

      if (xmb->left_thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
      {
         /* Limit left thumbnail width */

         float left_thumb_width     = 0.0f;
         float left_thumb_height    = 0.0f;
         float thumb_max_width = xmb->icon_size * 2.4;
         float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

#ifdef XMB_DEBUG
         RARCH_LOG("[XMB left thumbnail] width: %.2f, height: %.2f\n",
               xmb->left_thumbnail_width, xmb->left_thumbnail_height);
         RARCH_LOG("[XMB left thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

         if (xmb->left_thumbnail_width > thumb_max_width_scaled)
         {
            left_thumb_width  = xmb->left_thumbnail_width *
               (thumb_max_width_scaled / xmb->left_thumbnail_width);
            left_thumb_height = xmb->left_thumbnail_height *
               (thumb_max_width_scaled / xmb->left_thumbnail_width);
         }
         else
         {
            left_thumb_width  = xmb->left_thumbnail_width;
            left_thumb_height = xmb->left_thumbnail_height;
         }

         /* Limit left thumbnail height to screen height + margin. */
         if (xmb->margins_screen_top + xmb->icon_size +
               left_thumb_height >= ((float)height - (xmb->icon_size * 0.5)))
         {
            left_thumb_width = left_thumb_width *
               (((float)height - (xmb->icon_size * 0.5) -
               xmb->margins_screen_top - xmb->icon_size) /
               left_thumb_height);

            left_thumb_height = left_thumb_height *
               (((float)height - (xmb->icon_size * 0.5) -
               xmb->margins_screen_top - xmb->icon_size) /
               left_thumb_height);
         }

         xmb_draw_thumbnail(video_info,
               xmb, &coord_white[0], width, height,
               ((thumb_max_width - left_thumb_width) / 2),
               xmb->margins_screen_top + (xmb->icon_size * (!(xmb->depth == 1)? 2.2 : 0.75)) +
               left_thumb_height,
               left_thumb_width, left_thumb_height,
               xmb->left_thumbnail);
      }
   }

   /* Clock image */
   menu_display_set_alpha(item_color, MIN(xmb->alpha, 1.00f));

   if (video_info->battery_level_enable)
   {
      menu_display_ctx_powerstate_t powerstate;
      char msg[12];

      msg[0] = '\0';

      powerstate.s   = msg;
      powerstate.len = sizeof(msg);

      menu_display_powerstate(&powerstate);

      if (powerstate.battery_enabled)
      {
         size_t x_pos      = xmb->icon_size / 6;
         size_t x_pos_icon = xmb->margins_title_left;

         if (coord_white[3] != 0 &&  !xmb->assets_missing)
            xmb_draw_icon(video_info,
                  xmb->icon_size,
                  &mymat,
                  xmb->textures.list[
                     powerstate.charging? XMB_TEXTURE_BATTERY_CHARGING   :
                     (powerstate.percent > 80)? XMB_TEXTURE_BATTERY_FULL :
                     (powerstate.percent > 60)? XMB_TEXTURE_BATTERY_80   :
                     (powerstate.percent > 40)? XMB_TEXTURE_BATTERY_60   :
                     (powerstate.percent > 20)? XMB_TEXTURE_BATTERY_40   :
                     XMB_TEXTURE_BATTERY_20
                  ],
                  width - (xmb->icon_size / 2) - x_pos_icon,
                  xmb->icon_size,
                  width,
                  height,
                  1,
                  0,
                  1,
                  &item_color[0],
                  xmb->shadow_offset);

         percent_width = (unsigned)
            font_driver_get_message_width(
                  xmb->font, msg, (unsigned)strlen(msg), 1);

         xmb_draw_text(video_info, xmb, msg,
               width - xmb->margins_title_left - x_pos,
               xmb->margins_title_top, 1, 1, TEXT_ALIGN_RIGHT,
               width, height, xmb->font);
      }
   }

   if (video_info->timedate_enable)
   {
      menu_display_ctx_datetime_t datetime;
      char timedate[255];
      int x_pos = 0;

      if (coord_white[3] != 0 && !xmb->assets_missing)
      {
         int x_pos = 0;

         if (percent_width)
            x_pos = percent_width + (xmb->icon_size / 2.5);

         xmb_draw_icon(video_info,
               xmb->icon_size,
               &mymat,
               xmb->textures.list[XMB_TEXTURE_CLOCK],
               width - xmb->icon_size - x_pos,
               xmb->icon_size,
               width,
               height,
               1,
               0,
               1,
               &item_color[0],
               xmb->shadow_offset);
      }

      timedate[0]        = '\0';

      datetime.s         = timedate;
      datetime.len       = sizeof(timedate);
      datetime.time_mode = settings->uints.menu_timedate_style;

      menu_display_timedate(&datetime);

      if (percent_width)
         x_pos = percent_width + (xmb->icon_size / 2.5);

      xmb_draw_text(video_info, xmb, timedate,
            width - xmb->margins_title_left - xmb->icon_size / 4 - x_pos,
            xmb->margins_title_top, 1, 1, TEXT_ALIGN_RIGHT,
            width, height, xmb->font);
   }

   /* Arrow image */
   menu_display_set_alpha(item_color,
         MIN(xmb->textures_arrow_alpha, xmb->alpha));

   if (coord_white[3] != 0 && !xmb->assets_missing)
      xmb_draw_icon(video_info,
            xmb->icon_size,
            &mymat,
            xmb->textures.list[XMB_TEXTURE_ARROW],
            xmb->x + xmb->margins_screen_left +
            xmb->icon_spacing_horizontal -
            xmb->icon_size / 2.0 + xmb->icon_size,
            xmb->margins_screen_top +
            xmb->icon_size / 2.0 + xmb->icon_spacing_vertical
            * xmb->active_item_factor,
            width,
            height,
            xmb->textures_arrow_alpha,
            0,
            1,
            &item_color[0],
            xmb->shadow_offset);

   menu_display_blend_begin(video_info);

   /* Horizontal tab icons */
   if (!xmb->assets_missing)
   {
      for (i = 0; i <= xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
            + xmb->system_tab_end; i++)
      {
         xmb_node_t *node = xmb_get_node(xmb, i);

         if (!node)
            continue;

         menu_display_set_alpha(item_color, MIN(node->alpha, xmb->alpha));

         if (item_color[3] != 0)
         {
            menu_display_ctx_rotate_draw_t rotate_draw;
            math_matrix_4x4 mymat;
            uintptr_t texture        = node->icon;
            float x                  = xmb->x + xmb->categories_x_pos +
               xmb->margins_screen_left +
               xmb->icon_spacing_horizontal
               * (i + 1) - xmb->icon_size / 2.0;
            float y                  = xmb->margins_screen_top
               + xmb->icon_size / 2.0;
            float rotation           = 0;
            float scale_factor       = node->zoom;

            rotate_draw.matrix       = &mymat;
            rotate_draw.rotation     = rotation;
            rotate_draw.scale_x      = scale_factor;
            rotate_draw.scale_y      = scale_factor;
            rotate_draw.scale_z      = 1;
            rotate_draw.scale_enable = true;

            menu_display_rotate_z(&rotate_draw, video_info);

            xmb_draw_icon(video_info,
                  xmb->icon_size,
                  &mymat,
                  texture,
                  x,
                  y,
                  width,
                  height,
                  1.0,
                  rotation,
                  scale_factor,
                  &item_color[0],
                  xmb->shadow_offset);
         }
      }
   }

   /* Right side 2 thumbnails on top of each other */
   /* here to be displayed above the horizontal icons */
   if (!hide_thumbnails && !xmb->savestate_thumbnail && xmb->use_ps3_layout &&
      xmb->left_thumbnail && xmb->thumbnail &&
      settings->bools.menu_xmb_vertical_thumbnails)
   {
      /* Do not draw the right thumbnail if there is no space available */
      if (((xmb->margins_screen_top +
                  xmb->icon_size + min_thumb_size) <= height) &&
            ((xmb->margins_screen_left * scale_mod[5] +
              xmb->icon_spacing_horizontal +
              pseudo_font_length + min_thumb_size) <= width))
      {
         if (xmb->thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
         {
            /* Limit right thumbnail width */

            float thumb_width     = 0.0f;
            float thumb_height    = 0.0f;
            float thumb_max_width = (float)width - (xmb->icon_size / 6) -
               (xmb->margins_screen_left * scale_mod[5]) -
               xmb->icon_spacing_horizontal - pseudo_font_length;
            float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

#ifdef XMB_DEBUG
            RARCH_LOG("[XMB thumbnail] width: %.2f, height: %.2f\n",
                  xmb->thumbnail_width, xmb->thumbnail_height);
            RARCH_LOG("[XMB thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

            if (xmb->thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
            {
               thumb_width  = (xmb->thumbnail_width * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->thumbnail_width * scale_mod[4]));
               thumb_height = (xmb->thumbnail_height * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->thumbnail_width * scale_mod[4]));
            }
            else
            {
               thumb_width  = xmb->thumbnail_width * scale_mod[4];
               thumb_height = xmb->thumbnail_height * scale_mod[4];
            }

            /* Limit right thumbnail height to usable area. */

            if (thumb_height >=
                  ((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2)
            {
               thumb_width = thumb_width *
                  ((((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) /
                   thumb_height);
               thumb_height = thumb_height *
                  ((((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) /
                   thumb_height);
            }

            xmb_draw_thumbnail(video_info,
                  xmb, &coord_white[0], width, height,
                  (float)width - (xmb->icon_size / 6) - thumb_max_width +
                  ((thumb_max_width - thumb_width) / 2),
                  xmb->icon_size + ((((float)height / 2 -
                           (xmb->icon_size + (xmb->icon_size/12))) - thumb_height) / 2) +
                  thumb_height,
                  thumb_width, thumb_height,
                  xmb->thumbnail);
         }
      }

      /* Do not draw the left thumbnail if there is no space available */

      if (((xmb->margins_screen_top +
                  xmb->icon_size + min_thumb_size) <= height) &&
            ((xmb->margins_screen_left * scale_mod[5] +
              xmb->icon_spacing_horizontal +
              pseudo_font_length + min_thumb_size) <= width))
      {
         if (xmb->left_thumbnail && menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
         {
            /* Limit left thumbnail width */

            float left_thumb_width  = 0.0f;
            float left_thumb_height = 0.0f;
            float thumb_max_width = (float)width - (xmb->icon_size / 6) -
               (xmb->margins_screen_left * scale_mod[5]) -
               xmb->icon_spacing_horizontal - pseudo_font_length;
            float thumb_max_width_scaled = thumb_max_width * thumbnail_scale_factor;

#ifdef XMB_DEBUG
            RARCH_LOG("[XMB left thumbnail] width: %.2f, height: %.2f\n",
                  xmb->left_thumbnail_width, xmb->left_thumbnail_height);
            RARCH_LOG("[XMB left thumbnail] w: %.2f, h: %.2f\n", width, height);
#endif

            if (xmb->left_thumbnail_width * scale_mod[4] > thumb_max_width_scaled)
            {
               left_thumb_width  = (xmb->left_thumbnail_width * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
               left_thumb_height = (xmb->left_thumbnail_height * scale_mod[4]) *
                  (thumb_max_width_scaled / (xmb->left_thumbnail_width * scale_mod[4]));
            }
            else
            {
               left_thumb_width  = xmb->left_thumbnail_width * scale_mod[4];
               left_thumb_height = xmb->left_thumbnail_height * scale_mod[4];
            }

            /* Limit left thumbnail height to usable area. */

            if (left_thumb_height >=
                  ((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2)
            {
               left_thumb_width = left_thumb_width *
                  ((((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) /
                   left_thumb_height);
               left_thumb_height = left_thumb_height *
                  ((((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) /
                   left_thumb_height);
            }

            xmb_draw_thumbnail(video_info,
                  xmb, &coord_white[0], width, height,
                  (float)width - (xmb->icon_size / 6) - thumb_max_width +
                  ((thumb_max_width - left_thumb_width) / 2),
                  xmb->icon_size +
                  (((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) +
                  (((((float)height - ((xmb->icon_size / 6) * 2) - xmb->icon_size) / 2) -
                    left_thumb_height) / 2) + left_thumb_height,
                  left_thumb_width, left_thumb_height,
                  xmb->left_thumbnail);
         }
      }
   }

   menu_display_blend_end(video_info);

   /* Vertical icons */
   xmb_draw_items(
         video_info,
         xmb,
         xmb->selection_buf_old,
         xmb->selection_ptr_old,
         (xmb_list_get_size(xmb, MENU_LIST_PLAIN) > 1)
         ? xmb->categories_selection_ptr :
         xmb->categories_selection_ptr_old,
         &item_color[0],
         width,
         height);

   selection_buf = menu_entries_get_selection_buf_ptr(0);

   xmb_draw_items(
         video_info,
         xmb,
         selection_buf,
         selection,
         xmb->categories_selection_ptr,
         &item_color[0],
         width,
         height);

   font_driver_flush(video_info->width, video_info->height, xmb->font,
         video_info);
   font_driver_bind_block(xmb->font, NULL);

   font_driver_flush(video_info->width, video_info->height, xmb->font2,
         video_info);
   font_driver_bind_block(xmb->font2, NULL);

   if (menu_input_dialog_get_display_kb())
   {
      const char *str   = menu_input_dialog_get_buffer();
      const char *label = menu_input_dialog_get_label_buffer();

      snprintf(msg, sizeof(msg), "%s\n%s", label, str);
      render_background = true;
   }

   if (!string_is_empty(xmb->box_message))
   {
      strlcpy(msg, xmb->box_message,
            sizeof(msg));
      free(xmb->box_message);
      xmb->box_message  = NULL;
      render_background = true;
   }

   if (render_background)
   {
      xmb_draw_dark_layer(xmb, video_info, width, height);
      xmb_render_messagebox_internal(
            video_info, xmb, msg);
   }

   /* Cursor image */
   if (xmb->mouse_show)
   {
      menu_display_set_alpha(coord_white, MIN(xmb->alpha, 1.00f));
      menu_display_draw_cursor(
            video_info,
            &coord_white[0],
            xmb->cursor_size,
            xmb->textures.list[XMB_TEXTURE_POINTER],
            menu_input_mouse_state(MENU_MOUSE_X_AXIS),
            menu_input_mouse_state(MENU_MOUSE_Y_AXIS),
            width,
            height);
   }

   menu_display_unset_viewport(video_info->width, video_info->height);
}

static void xmb_layout_ps3(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();

   float scale_factor            =
      (settings->uints.menu_xmb_scale_factor * width) / (1920.0 * 100);

   xmb->above_subitem_offset     =   1.5;
   xmb->above_item_offset        =  -1.0;
   xmb->active_item_factor       =   3.0;
   xmb->under_item_offset        =   5.0;

   xmb->categories_active_zoom   = 1.0;
   xmb->categories_passive_zoom  = 0.5;
   xmb->items_active_zoom        = 1.0;
   xmb->items_passive_zoom       = 0.5;

   xmb->categories_active_alpha  = 1.0;
   xmb->categories_passive_alpha = 0.85;
   xmb->items_active_alpha       = 1.0;
   xmb->items_passive_alpha      = 0.85;

   xmb->shadow_offset            = 2.0;

   new_font_size                 = 32.0  * scale_factor;
   xmb->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;

   xmb->thumbnail_width          = 1024.0 * scale_factor;
   xmb->left_thumbnail_width     = 1024.0 * scale_factor;
   xmb->savestate_thumbnail_width= 460.0 * scale_factor;
   xmb->cursor_size              = 64.0 * scale_factor;

   xmb->icon_spacing_horizontal  = 200.0 * scale_factor;
   xmb->icon_spacing_vertical    = 64.0 * scale_factor;

   xmb->margins_screen_top       = (256+32) * scale_factor;
   xmb->margins_screen_left      = 336.0 * scale_factor;

   xmb->margins_title_left       = 60 * scale_factor;
   xmb->margins_title_top        = 60 * scale_factor + new_font_size / 3;
   xmb->margins_title_bottom     = 60 * scale_factor - new_font_size / 3;

   xmb->margins_label_left       = 85.0 * scale_factor;
   xmb->margins_label_top        = new_font_size / 3.0;

   xmb->margins_setting_left     = 600.0 * scale_factor * scale_mod[6];
   xmb->margins_dialog           = 48 * scale_factor;

   xmb->margins_slice            = 16;

   xmb->icon_size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;

   menu_display_set_header_height(new_header_height);
}

static void xmb_layout_psp(xmb_handle_t *xmb, int width)
{
   unsigned new_font_size, new_header_height;
   settings_t *settings          = config_get_ptr();
   float scale_factor            =
      ((settings->uints.menu_xmb_scale_factor * width) / (1920.0 * 100)) * 1.5;
#ifdef _3DS
   scale_factor                  =
      settings->uints.menu_xmb_scale_factor / 400.0;
#endif

   xmb->above_subitem_offset     =  1.5;
   xmb->above_item_offset        = -1.0;
   xmb->active_item_factor       =  2.0;
   xmb->under_item_offset        =  3.0;

   xmb->categories_active_zoom   = 1.0;
   xmb->categories_passive_zoom  = 1.0;
   xmb->items_active_zoom        = 1.0;
   xmb->items_passive_zoom       = 1.0;

   xmb->categories_active_alpha  = 1.0;
   xmb->categories_passive_alpha = 0.85;
   xmb->items_active_alpha       = 1.0;
   xmb->items_passive_alpha      = 0.85;

   xmb->shadow_offset            = 1.0;

   new_font_size                 = 32.0  * scale_factor;
   xmb->font2_size               = 24.0  * scale_factor;
   new_header_height             = 128.0 * scale_factor;
   xmb->margins_screen_top       = (256+32) * scale_factor;

   xmb->thumbnail_width          = 460.0 * scale_factor;
   xmb->left_thumbnail_width     = 400.0 * scale_factor;
   xmb->savestate_thumbnail_width= 460.0 * scale_factor;
   xmb->cursor_size              = 64.0;

   xmb->icon_spacing_horizontal  = 250.0 * scale_factor;
   xmb->icon_spacing_vertical    = 108.0 * scale_factor;

   xmb->margins_screen_left      = 136.0 * scale_factor;
   xmb->margins_title_left       = 60 * scale_factor;
   xmb->margins_title_top        = 60 * scale_factor + new_font_size / 3;
   xmb->margins_title_bottom     = 60 * scale_factor - new_font_size / 3;
   xmb->margins_label_left       = 85.0 * scale_factor;
   xmb->margins_label_top        = new_font_size / 3.0;
   xmb->margins_setting_left     = 600.0 * scale_factor;
   xmb->margins_dialog           = 48 * scale_factor;
   xmb->margins_slice            = 16;
   xmb->icon_size                = 128.0 * scale_factor;
   xmb->font_size                = new_font_size;

   menu_display_set_header_height(new_header_height);
}

static void xmb_layout(xmb_handle_t *xmb)
{
   unsigned width, height, i, current, end;
   settings_t *settings       = config_get_ptr();
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();

   video_driver_get_size(&width, &height);

   switch (settings->uints.menu_xmb_layout)
   {
      /* Automatic */
      case 0:
         {
            xmb->use_ps3_layout        = false;
            xmb->use_ps3_layout        = width > 320 && height > 240;

            /* Mimic the layout of the PSP instead of the PS3 on tiny screens */
            if (xmb->use_ps3_layout)
               xmb_layout_ps3(xmb, width);
            else
               xmb_layout_psp(xmb, width);
         }
         break;
         /* PS3 */
      case 1:
         xmb->use_ps3_layout        = true;
         xmb_layout_ps3(xmb, width);
         break;
         /* PSP */
      case 2:
         xmb->use_ps3_layout        = false;
         xmb_layout_psp(xmb, width);
         break;
   }

#ifdef XMB_DEBUG
   RARCH_LOG("[XMB] margin screen left: %.2f\n",  xmb->margins_screen_left);
   RARCH_LOG("[XMB] margin screen top:  %.2f\n",  xmb->margins_screen_top);
   RARCH_LOG("[XMB] margin title left:  %.2f\n",  xmb->margins_title_left);
   RARCH_LOG("[XMB] margin title top:   %.2f\n",  xmb->margins_title_top);
   RARCH_LOG("[XMB] margin title bott:  %.2f\n",  xmb->margins_title_bottom);
   RARCH_LOG("[XMB] margin label left:  %.2f\n",  xmb->margins_label_left);
   RARCH_LOG("[XMB] margin label top:   %.2f\n",  xmb->margins_label_top);
   RARCH_LOG("[XMB] margin sett left:   %.2f\n",  xmb->margins_setting_left);
   RARCH_LOG("[XMB] icon spacing hor:   %.2f\n",  xmb->icon_spacing_horizontal);
   RARCH_LOG("[XMB] icon spacing ver:   %.2f\n",  xmb->icon_spacing_vertical);
   RARCH_LOG("[XMB] icon size:          %.2f\n",  xmb->icon_size);
#endif

   current = (unsigned)selection;
   end     = (unsigned)menu_entries_get_size();

   for (i = 0; i < end; i++)
   {
      float ia         = xmb->items_passive_alpha;
      float iz         = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            selection_buf, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia             = xmb->items_active_alpha;
         iz             = xmb->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = ia;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
   }

   if (xmb->depth <= 1)
      return;

   current = (unsigned)xmb->selection_ptr_old;
   end     = (unsigned)file_list_get_size(xmb->selection_buf_old);

   for (i = 0; i < end; i++)
   {
      float         ia = 0;
      float         iz = xmb->items_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            xmb->selection_buf_old, i);

      if (!node)
         continue;

      if (i == current)
      {
         ia             = xmb->items_active_alpha;
         iz             = xmb->items_active_alpha;
      }

      node->alpha       = ia;
      node->label_alpha = 0;
      node->zoom        = iz;
      node->y           = xmb_item_y(xmb, i, current);
      node->x           = xmb->icon_size * 1 * -2;
   }
}

static void xmb_ribbon_set_vertex(float *ribbon_verts,
      unsigned idx, unsigned row, unsigned col)
{
   ribbon_verts[idx++] = ((float)col) / (XMB_RIBBON_COLS-1) * 2.0f - 1.0f;
   ribbon_verts[idx++] = ((float)row) / (XMB_RIBBON_ROWS-1) * 2.0f - 1.0f;
}

static void xmb_init_ribbon(xmb_handle_t * xmb)
{
   video_coords_t coords;
   unsigned r, c, col;
   unsigned i                = 0;
   video_coord_array_t *ca   = menu_display_get_coords_array();
   unsigned   vertices_total = XMB_RIBBON_VERTICES;
   float *dummy              = (float*)calloc(4 * vertices_total, sizeof(float));
   float *ribbon_verts       = (float*)calloc(2 * vertices_total, sizeof(float));

   /* Set up vertices */
   for (r = 0; r < XMB_RIBBON_ROWS - 1; r++)
   {
      for (c = 0; c < XMB_RIBBON_COLS; c++)
      {
         col = r % 2 ? XMB_RIBBON_COLS - c - 1 : c;
         xmb_ribbon_set_vertex(ribbon_verts, i,     r,     col);
         xmb_ribbon_set_vertex(ribbon_verts, i + 2, r + 1, col);
         i  += 4;
      }
   }

   coords.color         = dummy;
   coords.vertex        = ribbon_verts;
   coords.tex_coord     = dummy;
   coords.lut_tex_coord = dummy;
   coords.vertices      = vertices_total;

   video_coord_array_append(ca, &coords, coords.vertices);

   free(dummy);
   free(ribbon_verts);
}

static void *xmb_init(void **userdata, bool video_is_threaded)
{
   unsigned width, height;
   int i;
   xmb_handle_t *xmb          = NULL;
   settings_t *settings       = config_get_ptr();
   menu_handle_t *menu        = (menu_handle_t*)calloc(1, sizeof(*menu));
   float scale_value          = settings->uints.menu_xmb_scale_factor;

   /* scaling multiplier formulas made from these values:     */
   /* xmb_scale 50 = {2.5, 2.5,   2, 1.7, 2.5,   4, 2.4, 2.5} */
   /* xmb_scale 75 = {  2, 1.6, 1.6, 1.4, 1.5, 2.3, 1.9, 1.3} */

   if (scale_value < 100)
   {
      /* text length & word wrap (base 35 apply to file browser, 1st column) */
      scale_mod[0] = -0.03 * scale_value + 4.083;
      /* playlist text length when thumbnail is ON (small, base 40) */
      scale_mod[1] = -0.03 * scale_value + 3.95;
      /* playlist text length when thumbnail is OFF (large, base 70) */
      scale_mod[2] = -0.02 * scale_value + 3.033;
      /* sub-label length & word wrap */
      scale_mod[3] = -0.014 * scale_value + 2.416;
      /* thumbnail size & vertical margin from top */
      scale_mod[4] = -0.03 * scale_value + 3.916;
      /* thumbnail horizontal left margin (horizontal positioning) */
      scale_mod[5] = -0.06 * scale_value + 6.933;
      /* margin before 2nd column start (shaders parameters, cheats...) */
      scale_mod[6] = -0.028 * scale_value + 3.866;
      /* text length & word wrap (base 35 apply to 2nd column in cheats, shaders, etc) */
      scale_mod[7] = 134.179 * pow(scale_value, -1.0778);

      for (i = 0; i < 8; i++)
         if (scale_mod[i] < 1)
            scale_mod[i] = 1;
   }

   if (!menu)
      return NULL;

   if (!menu_display_init_first_driver(video_is_threaded))
   {
      free(menu);
      return NULL;
   }

   video_driver_get_size(&width, &height);

   xmb = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!xmb)
   {
      free(menu);
      return NULL;
   }

   *userdata = xmb;

   xmb->selection_buf_old     = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (!xmb->selection_buf_old)
      goto error;

   xmb->categories_active_idx         = 0;
   xmb->categories_active_idx_old     = 0;
   xmb->x                             = 0;
   xmb->categories_x_pos              = 0;
   xmb->textures_arrow_alpha          = 0;
   xmb->depth                         = 1;
   xmb->old_depth                     = 1;
   xmb->alpha                         = 0;

   xmb->system_tab_end                = 0;
   xmb->tabs[xmb->system_tab_end]     = XMB_SYSTEM_TAB_MAIN;
   if (settings->bools.menu_content_show_settings && !settings->bools.kiosk_mode_enable)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_SETTINGS;
   if (settings->bools.menu_content_show_favorites)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_FAVORITES;
   if (settings->bools.menu_content_show_history)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_HISTORY;
#ifdef HAVE_IMAGEVIEWER
   if (settings->bools.menu_content_show_images)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_IMAGES;
#endif
   if (settings->bools.menu_content_show_music)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_MUSIC;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   if (settings->bools.menu_content_show_video)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_VIDEO;
#endif
#ifdef HAVE_NETWORKING
   if (settings->bools.menu_content_show_netplay)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_NETPLAY;
#endif
#ifdef HAVE_LIBRETRODB
   if (settings->bools.menu_content_show_add && !settings->bools.kiosk_mode_enable)
      xmb->tabs[++xmb->system_tab_end] = XMB_SYSTEM_TAB_ADD;
#endif

   menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   /* TODO/FIXME - we don't use framebuffer at all
    * for XMB, we should refactor this dependency
    * away. */

   menu_display_set_width(width);
   menu_display_set_height(height);

   menu_display_allocate_white_texture();

   xmb->horizontal_list         = (file_list_t*)calloc(1, sizeof(file_list_t));

   if (xmb->horizontal_list)
      xmb_init_horizontal_list(xmb);

   xmb_init_ribbon(xmb);

   xmb->thumbnail_path_data = menu_thumbnail_path_init();
   if (!xmb->thumbnail_path_data)
      goto error;

   return menu;

error:
   free(menu);

   if (xmb->selection_buf_old)
      free(xmb->selection_buf_old);
   xmb->selection_buf_old = NULL;
   if (xmb->horizontal_list)
   {
      xmb_free_list_nodes(xmb->horizontal_list, false);
      file_list_free(xmb->horizontal_list);
   }
   xmb->horizontal_list = NULL;
   return NULL;
}

static void xmb_free(void *data)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (xmb)
   {
      if (xmb->selection_buf_old)
      {
         xmb_free_list_nodes(xmb->selection_buf_old, false);
         file_list_free(xmb->selection_buf_old);
      }

      if (xmb->horizontal_list)
      {
         xmb_free_list_nodes(xmb->horizontal_list, false);
         file_list_free(xmb->horizontal_list);
      }

      xmb->selection_buf_old = NULL;
      xmb->horizontal_list   = NULL;

      video_coord_array_free(&xmb->raster_block.carr);
      video_coord_array_free(&xmb->raster_block2.carr);

      if (!string_is_empty(xmb->box_message))
         free(xmb->box_message);
      if (!string_is_empty(xmb->savestate_thumbnail_file_path))
         free(xmb->savestate_thumbnail_file_path);
      if (!string_is_empty(xmb->bg_file_path))
         free(xmb->bg_file_path);

      if (xmb->thumbnail_path_data)
         free(xmb->thumbnail_path_data);
   }

   font_driver_bind_block(NULL, NULL);
}

static void xmb_context_bg_destroy(xmb_handle_t *xmb)
{
   if (!xmb)
      return;
   video_driver_texture_unload(&xmb->textures.bg);
   video_driver_texture_unload(&menu_display_white_texture);
}

static bool xmb_load_image(void *userdata, void *data, enum menu_image_type type)
{
   xmb_handle_t *xmb = (xmb_handle_t*)userdata;

   if (!xmb)
      return false;

   if (!data)
   {
      /* If this happens, the image we attempted to load
       * was corrupt/incorrectly formatted. If this was a
       * thumbnail image, have unload any existing thumbnails
       * (otherwise entry with 'corrupt' thumbnail will show
       * thumbnail from last selected 'good' entry) */
      if (type == MENU_IMAGE_THUMBNAIL)
         video_driver_texture_unload(&xmb->thumbnail);
      else if (type == MENU_IMAGE_LEFT_THUMBNAIL)
         video_driver_texture_unload(&xmb->left_thumbnail);

      return false;
   }

   switch (type)
   {
      case MENU_IMAGE_NONE:
         break;
      case MENU_IMAGE_WALLPAPER:
         xmb_context_bg_destroy(xmb);
         video_driver_texture_unload(&xmb->textures.bg);
         video_driver_texture_load(data,
               TEXTURE_FILTER_MIPMAP_LINEAR,
               &xmb->textures.bg);
         menu_display_allocate_white_texture();
         break;
      case MENU_IMAGE_THUMBNAIL:
         {
            struct texture_image *img  = (struct texture_image*)data;
            xmb->thumbnail_height      = xmb->thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&xmb->thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &xmb->thumbnail);
         }
         break;
      case MENU_IMAGE_LEFT_THUMBNAIL:
         {
            struct texture_image *img  = (struct texture_image*)data;
            xmb->left_thumbnail_height      = xmb->left_thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&xmb->left_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &xmb->left_thumbnail);
         }
         break;
      case MENU_IMAGE_SAVESTATE_THUMBNAIL:
         {
            struct texture_image *img       = (struct texture_image*)data;
            xmb->savestate_thumbnail_height = xmb->savestate_thumbnail_width
               * (float)img->height / (float)img->width;
            video_driver_texture_unload(&xmb->savestate_thumbnail);
            video_driver_texture_load(data,
                  TEXTURE_FILTER_MIPMAP_LINEAR, &xmb->savestate_thumbnail);
         }
         break;
   }

   return true;
}

static const char *xmb_texture_path(unsigned id)
{
   switch (id)
   {
      case XMB_TEXTURE_MAIN_MENU:
#if defined(HAVE_LAKKA)
         return "lakka.png";
#else
         return "retroarch.png";
#endif
      case XMB_TEXTURE_SETTINGS:
         return "settings.png";
      case XMB_TEXTURE_HISTORY:
         return "history.png";
      case XMB_TEXTURE_FAVORITES:
         return "favorites.png";
      case XMB_TEXTURE_ADD_FAVORITE:
         return "add-favorite.png";
      case XMB_TEXTURE_MUSICS:
         return "musics.png";
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case XMB_TEXTURE_MOVIES:
         return "movies.png";
#endif
#ifdef HAVE_IMAGEVIEWER
      case XMB_TEXTURE_IMAGES:
         return "images.png";
#endif
      case XMB_TEXTURE_SETTING:
         return "setting.png";
      case XMB_TEXTURE_SUBSETTING:
         return "subsetting.png";
      case XMB_TEXTURE_ARROW:
         return "arrow.png";
      case XMB_TEXTURE_RUN:
         return "run.png";
      case XMB_TEXTURE_CLOSE:
         return "close.png";
      case XMB_TEXTURE_RESUME:
         return "resume.png";
      case XMB_TEXTURE_CLOCK:
         return "clock.png";
      case XMB_TEXTURE_BATTERY_FULL:
         return "battery-full.png";
      case XMB_TEXTURE_BATTERY_CHARGING:
         return "battery-charging.png";
      case XMB_TEXTURE_BATTERY_80:
         return "battery-80.png";
      case XMB_TEXTURE_BATTERY_60:
         return "battery-60.png";
      case XMB_TEXTURE_BATTERY_40:
         return "battery-40.png";
      case XMB_TEXTURE_BATTERY_20:
         return "battery-20.png";
      case XMB_TEXTURE_POINTER:
         return "pointer.png";
      case XMB_TEXTURE_SAVESTATE:
         return "savestate.png";
      case XMB_TEXTURE_LOADSTATE:
         return "loadstate.png";
      case XMB_TEXTURE_UNDO:
         return "undo.png";
      case XMB_TEXTURE_CORE_INFO:
         return "core-infos.png";
      case XMB_TEXTURE_WIFI:
         return "wifi.png";
      case XMB_TEXTURE_CORE_OPTIONS:
         return "core-options.png";
      case XMB_TEXTURE_INPUT_REMAPPING_OPTIONS:
         return "core-input-remapping-options.png";
      case XMB_TEXTURE_CHEAT_OPTIONS:
         return "core-cheat-options.png";
      case XMB_TEXTURE_DISK_OPTIONS:
         return "core-disk-options.png";
      case XMB_TEXTURE_SHADER_OPTIONS:
         return "core-shader-options.png";
      case XMB_TEXTURE_ACHIEVEMENT_LIST:
         return "achievement-list.png";
      case XMB_TEXTURE_SCREENSHOT:
         return "screenshot.png";
      case XMB_TEXTURE_RELOAD:
         return "reload.png";
      case XMB_TEXTURE_RENAME:
         return "rename.png";
      case XMB_TEXTURE_FILE:
         return "file.png";
      case XMB_TEXTURE_FOLDER:
         return "folder.png";
      case XMB_TEXTURE_ZIP:
         return "zip.png";
      case XMB_TEXTURE_MUSIC:
         return "music.png";
      case XMB_TEXTURE_FAVORITE:
         return "favorites-content.png";
      case XMB_TEXTURE_IMAGE:
         return "image.png";
      case XMB_TEXTURE_MOVIE:
         return "movie.png";
      case XMB_TEXTURE_CORE:
         return "core.png";
      case XMB_TEXTURE_RDB:
         return "database.png";
      case XMB_TEXTURE_CURSOR:
         return "cursor.png";
      case XMB_TEXTURE_SWITCH_ON:
         return "on.png";
      case XMB_TEXTURE_SWITCH_OFF:
         return "off.png";
      case XMB_TEXTURE_ADD:
         return "add.png";
#ifdef HAVE_NETWORKING
      case XMB_TEXTURE_NETPLAY:
         return "netplay.png";
      case XMB_TEXTURE_ROOM:
         return "menu_room.png";
      case XMB_TEXTURE_ROOM_LAN:
         return "menu_room_lan.png";
      case XMB_TEXTURE_ROOM_RELAY:
         return "menu_room_relay.png";
#endif
      case XMB_TEXTURE_KEY:
         return "key.png";
      case XMB_TEXTURE_KEY_HOVER:
         return "key-hover.png";
      case XMB_TEXTURE_DIALOG_SLICE:
         return "dialog-slice.png";
      case XMB_TEXTURE_ACHIEVEMENTS:
         return "menu_achievements.png";
      case XMB_TEXTURE_AUDIO:
         return "menu_audio.png";
      case XMB_TEXTURE_DRIVERS:
         return "menu_drivers.png";
      case XMB_TEXTURE_EXIT:
         return "menu_exit.png";
      case XMB_TEXTURE_FRAMESKIP:
         return "menu_frameskip.png";
      case XMB_TEXTURE_HELP:
         return "menu_help.png";
      case XMB_TEXTURE_INFO:
         return "menu_info.png";
      case XMB_TEXTURE_INPUT_SETTINGS:
         return "Libretro - Pad.png";
      case XMB_TEXTURE_LATENCY:
         return "menu_latency.png";
      case XMB_TEXTURE_NETWORK:
         return "menu_network.png";
      case XMB_TEXTURE_POWER:
         return "menu_power.png";
      case XMB_TEXTURE_RECORD:
         return "menu_record.png";
      case XMB_TEXTURE_SAVING:
         return "menu_saving.png";
      case XMB_TEXTURE_UPDATER:
         return "menu_updater.png";
      case XMB_TEXTURE_VIDEO:
         return "menu_video.png";
      case XMB_TEXTURE_MIXER:
         return "menu_mixer.png";
      case XMB_TEXTURE_LOG:
         return "menu_log.png";
      case XMB_TEXTURE_OSD:
         return "menu_osd.png";
      case XMB_TEXTURE_UI:
         return "menu_ui.png";
      case XMB_TEXTURE_USER:
         return "menu_user.png";
      case XMB_TEXTURE_PRIVACY:
         return "menu_privacy.png";
      case XMB_TEXTURE_PLAYLIST:
         return "menu_playlist.png";
      case XMB_TEXTURE_QUICKMENU:
         return "menu_quickmenu.png";
      case XMB_TEXTURE_REWIND:
         return "menu_rewind.png";
      case XMB_TEXTURE_OVERLAY:
         return "menu_overlay.png";
      case XMB_TEXTURE_OVERRIDE:
         return "menu_override.png";
      case XMB_TEXTURE_NOTIFICATIONS:
         return "menu_notifications.png";
      case XMB_TEXTURE_STREAM:
         return "menu_stream.png";
      case XMB_TEXTURE_SHUTDOWN:
         return "menu_shutdown.png";
      case XMB_TEXTURE_INPUT_DPAD_U:
         return "input_DPAD-U.png";
      case XMB_TEXTURE_INPUT_DPAD_D:
         return "input_DPAD-D.png";
      case XMB_TEXTURE_INPUT_DPAD_L:
         return "input_DPAD-L.png";
      case XMB_TEXTURE_INPUT_DPAD_R:
         return "input_DPAD-R.png";
      case XMB_TEXTURE_INPUT_STCK_U:
         return "input_STCK-U.png";
      case XMB_TEXTURE_INPUT_STCK_D:
         return "input_STCK-D.png";
      case XMB_TEXTURE_INPUT_STCK_L:
         return "input_STCK-L.png";
      case XMB_TEXTURE_INPUT_STCK_R:
         return "input_STCK-R.png";
      case XMB_TEXTURE_INPUT_STCK_P:
         return "input_STCK-P.png";
      case XMB_TEXTURE_INPUT_BTN_U:
         return "input_BTN-U.png";
      case XMB_TEXTURE_INPUT_BTN_D:
         return "input_BTN-D.png";
      case XMB_TEXTURE_INPUT_BTN_L:
         return "input_BTN-L.png";
      case XMB_TEXTURE_INPUT_BTN_R:
         return "input_BTN-R.png";
      case XMB_TEXTURE_INPUT_LB:
         return "input_LB.png";
      case XMB_TEXTURE_INPUT_RB:
         return "input_RB.png";
      case XMB_TEXTURE_INPUT_LT:
         return "input_LT.png";
      case XMB_TEXTURE_INPUT_RT:
         return "input_RT.png";
      case XMB_TEXTURE_INPUT_SELECT:
         return "input_SELECT.png";
      case XMB_TEXTURE_INPUT_START:
         return "input_START.png";
      case XMB_TEXTURE_INPUT_ADC:
         return "input_ADC.png";
      case XMB_TEXTURE_INPUT_BIND_ALL:
         return "input_BIND_ALL.png";
      case XMB_TEXTURE_INPUT_MOUSE:
         return "input_MOUSE.png";
      case XMB_TEXTURE_INPUT_LGUN:
         return "input_LGUN.png";
      case XMB_TEXTURE_INPUT_TURBO:
         return "input_TURBO.png";
      case XMB_TEXTURE_CHECKMARK:
         return "menu_check.png";
      case XMB_TEXTURE_MENU_ADD:
         return "menu_add.png";
      case XMB_TEXTURE_BRIGHTNESS:
         return "menu_brightness.png";
      case XMB_TEXTURE_PAUSE:
         return "menu_pause.png";
      case XMB_TEXTURE_DEFAULT:
         return "default.png";
      case XMB_TEXTURE_DEFAULT_CONTENT:
         return "default-content.png";
      case XMB_TEXTURE_MENU_APPLY_TOGGLE:
         return "menu_apply_toggle.png";
      case XMB_TEXTURE_MENU_APPLY_COG:
         return "menu_apply_cog.png";
   }
   return NULL;
}

static void xmb_context_reset_textures(
      xmb_handle_t *xmb, const char *iconpath)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   xmb->assets_missing = false;

   menu_display_allocate_white_texture();

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
   {
      if (!menu_display_reset_textures_list(xmb_texture_path(i), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
      {
         RARCH_WARN("[XMB] Asset missing: %s%s\n", iconpath, xmb_texture_path(i));
         /* New extra battery icons could be missing */
         if (i == XMB_TEXTURE_BATTERY_80 || i == XMB_TEXTURE_BATTERY_60 || i == XMB_TEXTURE_BATTERY_40 || i == XMB_TEXTURE_BATTERY_20)
         {
            if (  /* If there are no extra battery icons revert to the old behaviour */
                  !menu_display_reset_textures_list(xmb_texture_path(XMB_TEXTURE_BATTERY_FULL), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL)
                  && !(settings->uints.menu_xmb_theme == XMB_ICON_THEME_CUSTOM)
               )
               goto error;
            else continue;
         }
         /* If the icon is missing return the subsetting (because some themes are incomplete) */
         if (!(i == XMB_TEXTURE_DIALOG_SLICE || i == XMB_TEXTURE_KEY_HOVER || i == XMB_TEXTURE_KEY))
         {
            /* OSD Warning only if subsetting icon is missing */
            if (
                  !menu_display_reset_textures_list(xmb_texture_path(XMB_TEXTURE_SUBSETTING), iconpath, &xmb->textures.list[i], TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL)
                  && !(settings->uints.menu_xmb_theme == XMB_ICON_THEME_CUSTOM)
               )
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_MISSING_ASSETS), 1, 256, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               /* Do not draw icons if subsetting is missing */
               goto error;
            }
            /* Do not draw icons if this ones are is missing */
            switch (i)
            {
               case XMB_TEXTURE_POINTER:
               case XMB_TEXTURE_ARROW:
               case XMB_TEXTURE_CLOCK:
               case XMB_TEXTURE_BATTERY_CHARGING:
               case XMB_TEXTURE_BATTERY_FULL:
               case XMB_TEXTURE_DEFAULT:
               case XMB_TEXTURE_DEFAULT_CONTENT:
                  goto error;
            }
         }
      }
   }

   xmb->main_menu_node.icon     = xmb->textures.list[XMB_TEXTURE_MAIN_MENU];
   xmb->main_menu_node.alpha    = xmb->categories_active_alpha;
   xmb->main_menu_node.zoom     = xmb->categories_active_zoom;

   xmb->settings_tab_node.icon  = xmb->textures.list[XMB_TEXTURE_SETTINGS];
   xmb->settings_tab_node.alpha = xmb->categories_active_alpha;
   xmb->settings_tab_node.zoom  = xmb->categories_active_zoom;

   xmb->history_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_HISTORY];
   xmb->history_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->history_tab_node.zoom   = xmb->categories_active_zoom;

   xmb->favorites_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_FAVORITES];
   xmb->favorites_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->favorites_tab_node.zoom   = xmb->categories_active_zoom;

   xmb->music_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MUSICS];
   xmb->music_tab_node.alpha    = xmb->categories_active_alpha;
   xmb->music_tab_node.zoom     = xmb->categories_active_zoom;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   xmb->video_tab_node.icon     = xmb->textures.list[XMB_TEXTURE_MOVIES];
   xmb->video_tab_node.alpha    = xmb->categories_active_alpha;
   xmb->video_tab_node.zoom     = xmb->categories_active_zoom;
#endif

#ifdef HAVE_IMAGEVIEWER
   xmb->images_tab_node.icon    = xmb->textures.list[XMB_TEXTURE_IMAGES];
   xmb->images_tab_node.alpha   = xmb->categories_active_alpha;
   xmb->images_tab_node.zoom    = xmb->categories_active_zoom;
#endif

   xmb->add_tab_node.icon       = xmb->textures.list[XMB_TEXTURE_ADD];
   xmb->add_tab_node.alpha      = xmb->categories_active_alpha;
   xmb->add_tab_node.zoom       = xmb->categories_active_zoom;

#ifdef HAVE_NETWORKING
   xmb->netplay_tab_node.icon   = xmb->textures.list[XMB_TEXTURE_NETPLAY];
   xmb->netplay_tab_node.alpha  = xmb->categories_active_alpha;
   xmb->netplay_tab_node.zoom   = xmb->categories_active_zoom;
#endif

   /* Recolor */
   if (
         (settings->uints.menu_xmb_theme == XMB_ICON_THEME_MONOCHROME_INVERTED) ||
         (settings->uints.menu_xmb_theme == XMB_ICON_THEME_AUTOMATIC_INVERTED)
      )
      memcpy(item_color, coord_black, sizeof(item_color));
   else
   {
      if (
            (settings->uints.menu_xmb_theme == XMB_ICON_THEME_MONOCHROME) ||
            (settings->uints.menu_xmb_theme == XMB_ICON_THEME_AUTOMATIC)
         )
      {
         for (i=0;i<16;i++)
            {
               if ((i==3) || (i==7) || (i==11) || (i==15))
                  {
                     item_color[i] = 1;
                     continue;
                  }
               item_color[i] = 0.95;
            }
      }
      else
         memcpy(item_color, coord_white, sizeof(item_color));
   }

return;

error:
   xmb->assets_missing = true;
   RARCH_WARN("[XMB] Critical asset missing, no icons will be drawn\n");
   return;
}

static void xmb_context_reset_background(const char *iconpath)
{
   char *path                  = NULL;
   settings_t *settings        = config_get_ptr();
   const char *path_menu_wp    = settings->paths.path_menu_wallpaper;

   if (!string_is_empty(path_menu_wp))
      path = strdup(path_menu_wp);
   else if (!string_is_empty(iconpath))
   {
      path    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      path[0] = '\0';

      fill_pathname_join(path, iconpath, "bg.png",
            PATH_MAX_LENGTH * sizeof(char));
   }

   if (path_is_valid(path))
      task_push_image_load(path,
            video_driver_supports_rgba(), 0,
            menu_display_handle_wallpaper_upload, NULL);

#ifdef ORBIS
   /* To avoid weird behaviour on orbis with remote host */
   RARCH_LOG("[XMB] after task\n");
   sleep(5);
#endif
   if (path)
      free(path);
}

static void xmb_context_reset_internal(xmb_handle_t *xmb,
      bool is_threaded, bool reinit_textures)
{
   char bg_file_path[PATH_MAX_LENGTH] = {0};
   char *iconpath    = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   iconpath[0]       = '\0';

   fill_pathname_application_special(bg_file_path,
         sizeof(bg_file_path), APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG);

   if (!string_is_empty(bg_file_path))
   {
      if (!string_is_empty(xmb->bg_file_path))
         free(xmb->bg_file_path);
      xmb->bg_file_path = strdup(bg_file_path);
   }

   fill_pathname_application_special(iconpath,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS);

   xmb_layout(xmb);
   if (xmb->font)
   {
      menu_display_font_free(xmb->font);
      xmb->font = NULL;
   }
   if (xmb->font2)
   {
      menu_display_font_free(xmb->font2);
      xmb->font2 = NULL;
   }
   xmb->font = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
         xmb->font_size,
         is_threaded);
   xmb->font2 = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
         xmb->font2_size,
         is_threaded);

   if (reinit_textures)
   {
      xmb_context_reset_textures(xmb, iconpath);
      xmb_context_reset_background(iconpath);
   }

   xmb_context_reset_horizontal_list(xmb);

   if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT) || menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
   {
      if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_RIGHT))
         xmb_update_thumbnail_path(xmb, 0, 'R');

      if (menu_thumbnail_is_enabled(MENU_THUMBNAIL_LEFT))
         xmb_update_thumbnail_path(xmb, 0, 'L');

      xmb_update_thumbnail_image(xmb);
   }
   xmb_update_savestate_thumbnail_image(xmb);

   free(iconpath);
}

static void xmb_context_reset(void *data, bool is_threaded)
{
   xmb_handle_t *xmb = (xmb_handle_t*)data;

   if (xmb)
      xmb_context_reset_internal(xmb, is_threaded, true);
}

static void xmb_navigation_clear(void *data, bool pending_push)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   if (!pending_push)
      xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_pointer_changed(void *data)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_set(void *data, bool scroll)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_navigation_alphabet(void *data, size_t *unused)
{
   xmb_handle_t  *xmb  = (xmb_handle_t*)data;
   xmb_selection_pointer_changed(xmb, true);
}

static void xmb_list_insert(void *userdata,
      file_list_t *list,
      const char *path,
      const char *fullpath,
      const char *unused,
      size_t list_size,
      unsigned entry_type)
{
   int current            = 0;
   int i                  = (int)list_size;
   xmb_node_t *node       = NULL;
   xmb_handle_t *xmb      = (xmb_handle_t*)userdata;
   size_t selection       = menu_navigation_get_selection();

   if (!xmb || !list)
      return;

   node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

   if (!node)
      node = xmb_alloc_node();

   if (!node)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   current           = (int)selection;

   if (!string_is_empty(fullpath))
   {
      if (node->fullpath)
         free(node->fullpath);

      node->fullpath = strdup(fullpath);
   }

   node->alpha       = xmb->items_passive_alpha;
   node->zoom        = xmb->items_passive_zoom;
   node->label_alpha = node->alpha;
   node->y           = xmb_item_y(xmb, i, current);
   node->x           = 0;

   if (i == current)
   {
      node->alpha       = xmb->items_active_alpha;
      node->label_alpha = xmb->items_active_alpha;
      node->zoom        = xmb->items_active_alpha;
   }

   file_list_set_userdata(list, i, node);
}

static void xmb_list_clear(file_list_t *list)
{
   menu_animation_ctx_tag tag = (uintptr_t)list;

   menu_animation_kill_by_tag(&tag);

   xmb_free_list_nodes(list, false);
}

static void xmb_list_free(file_list_t *list, size_t a, size_t b)
{
   xmb_list_clear(list);
}

static void xmb_list_deep_copy(const file_list_t *src, file_list_t *dst,
      size_t first, size_t last)
{
   size_t i, j = 0;
   menu_animation_ctx_tag tag = (uintptr_t)dst;

   menu_animation_kill_by_tag(&tag);

   xmb_free_list_nodes(dst, true);

   file_list_clear(dst);
   file_list_reserve(dst, (last + 1) - first);

   for (i = first; i <= last; ++i)
   {
      struct item_file *d = &dst->list[j];
      struct item_file *s = &src->list[i];

      void *src_udata = s->userdata;
      void *src_adata = s->actiondata;

      *d       = *s;
      d->alt   = string_is_empty(d->alt)   ? NULL : strdup(d->alt);
      d->path  = string_is_empty(d->path)  ? NULL : strdup(d->path);
      d->label = string_is_empty(d->label) ? NULL : strdup(d->label);

      if (src_udata)
         file_list_set_userdata(dst, j, (void*)xmb_copy_node((const xmb_node_t*)src_udata));

      if (src_adata)
      {
         void *data = malloc(sizeof(menu_file_list_cbs_t));
         memcpy(data, src_adata, sizeof(menu_file_list_cbs_t));
         file_list_set_actiondata(dst, j, data);
      }

      ++j;
   }

   dst->size = j;
}

static void xmb_list_cache(void *data, enum menu_list_type type, unsigned action)
{
   size_t stack_size, list_size;
   xmb_handle_t      *xmb     = (xmb_handle_t*)data;
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   settings_t *settings       = config_get_ptr();

   if (!xmb)
      return;

   /* Check whether to enable the horizontal animation. */
   if (settings->bools.menu_horizontal_animation)
   {
      unsigned first = 0, last = 0;
      unsigned height = 0;
      video_driver_get_size(NULL, &height);

      /* FIXME: this shouldn't be happening at all */
      if (selection >= selection_buf->size)
         selection = selection_buf->size ? selection_buf->size - 1 : 0;

      xmb->selection_ptr_old = selection;

      xmb_calculate_visible_range(xmb, height, selection_buf->size,
            (unsigned)xmb->selection_ptr_old, &first, &last);

      xmb_list_deep_copy(selection_buf, xmb->selection_buf_old, first, last);

      xmb->selection_ptr_old -= first;
      last                   -= first;
      first                   = 0;
   }
   else
      xmb->selection_ptr_old = 0;

   list_size = xmb_list_get_size(xmb, MENU_LIST_HORIZONTAL)
      + xmb->system_tab_end;

   switch (type)
   {
      case MENU_LIST_PLAIN:
         break;
      case MENU_LIST_HORIZONTAL:
         xmb->categories_selection_ptr_old = xmb->categories_selection_ptr;

         switch (action)
         {
            case MENU_ACTION_LEFT:
               if (xmb->categories_selection_ptr == 0)
               {
                  xmb->categories_selection_ptr = list_size;
                  xmb->categories_active_idx    = (unsigned)(list_size - 1);
               }
               else
                  xmb->categories_selection_ptr--;
               break;
            default:
               if (xmb->categories_selection_ptr == list_size)
               {
                  xmb->categories_selection_ptr = 0;
                  xmb->categories_active_idx    = 1;
               }
               else
                  xmb->categories_selection_ptr++;
               break;
         }

         stack_size = menu_stack->size;

         if (menu_stack->list[stack_size - 1].label)
            free(menu_stack->list[stack_size - 1].label);
         menu_stack->list[stack_size - 1].label = NULL;

         switch (xmb_get_system_tab(xmb, (unsigned)xmb->categories_selection_ptr))
         {
            case XMB_SYSTEM_TAB_MAIN:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS;
               break;
            case XMB_SYSTEM_TAB_SETTINGS:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTINGS_TAB;
               break;
#ifdef HAVE_IMAGEVIEWER
            case XMB_SYSTEM_TAB_IMAGES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_IMAGES_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_MUSIC:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_MUSIC_TAB;
               break;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            case XMB_SYSTEM_TAB_VIDEO:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_VIDEO_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_HISTORY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_HISTORY_TAB;
               break;
            case XMB_SYSTEM_TAB_FAVORITES:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_FAVORITES_TAB;
               break;
#ifdef HAVE_NETWORKING
            case XMB_SYSTEM_TAB_NETPLAY:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_NETPLAY_TAB;
               break;
#endif
            case XMB_SYSTEM_TAB_ADD:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB));
               menu_stack->list[stack_size - 1].type =
                  MENU_ADD_TAB;
               break;
            default:
               menu_stack->list[stack_size - 1].label =
                  strdup(msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU));
               menu_stack->list[stack_size - 1].type =
                  MENU_SETTING_HORIZONTAL_MENU;
               break;
         }
         break;
      default:
         break;
   }
}

static void xmb_context_destroy(void *data)
{
   unsigned i;
   xmb_handle_t *xmb   = (xmb_handle_t*)data;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      video_driver_texture_unload(&xmb->textures.list[i]);

   video_driver_texture_unload(&xmb->thumbnail);
   video_driver_texture_unload(&xmb->left_thumbnail);
   video_driver_texture_unload(&xmb->savestate_thumbnail);

   xmb_context_destroy_horizontal_list(xmb);
   xmb_context_bg_destroy(xmb);

   menu_display_font_free(xmb->font);
   menu_display_font_free(xmb->font2);

   xmb->font = NULL;
   xmb->font2 = NULL;
}

static void xmb_toggle(void *userdata, bool menu_on)
{
   menu_animation_ctx_entry_t entry;
   bool tmp             = false;
   xmb_handle_t *xmb    = (xmb_handle_t*)userdata;

   if (!xmb)
      return;

   xmb->depth         = (int)xmb_list_get_size(xmb, MENU_LIST_PLAIN);

   if (!menu_on)
   {
      xmb->alpha = 0;
      return;
   }

   entry.duration     = XMB_DELAY * 2;
   entry.target_value = 1.0f;
   entry.subject      = &xmb->alpha;
   entry.easing_enum  = EASING_OUT_QUAD;
   /* TODO/FIXME - integer conversion resulted in change of sign */
   entry.tag          = -1;
   entry.cb           = NULL;

   menu_animation_push(&entry);

   tmp = !menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL);

   if (tmp)
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);

   xmb_toggle_horizontal_list(xmb);
}

static int xmb_deferred_push_content_actions(menu_displaylist_info_t *info)
{
   if (!menu_displaylist_ctl(
            DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS, info))
      return -1;
   menu_displaylist_process(info);
   menu_displaylist_info_free(info);
   return 0;
}

static int xmb_list_bind_init_compare_label(menu_file_list_cbs_t *cbs)
{
   if (cbs && cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_CONTENT_ACTIONS:
            cbs->action_deferred_push = xmb_deferred_push_content_actions;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int xmb_list_bind_init(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (xmb_list_bind_init_compare_label(cbs) == 0)
      return 0;

   return -1;
}

static int xmb_list_push(void *data, void *userdata,
      menu_displaylist_info_t *info, unsigned type)
{
   menu_displaylist_ctx_parse_entry_t entry;
   int ret                = -1;
   core_info_list_t *list = NULL;
   menu_handle_t *menu    = (menu_handle_t*)data;
   const struct retro_subsystem_info* subsystem;

   switch (type)
   {
      case DISPLAYLIST_LOAD_CONTENT_LIST:
         {
            settings_t *settings = config_get_ptr();

            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES),
                  msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES),
                  MENU_ENUM_LABEL_FAVORITES,
                  MENU_SETTING_ACTION, 0, 0);

            core_info_get_list(&list);
            if (core_info_list_num_info_files(list))
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST),
                     MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,
                     MENU_SETTING_ACTION, 0, 0);
            }

#ifdef HAVE_LIBRETRODB
            menu_entries_append_enum(info->list,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB),
                  msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB),
                  MENU_ENUM_LABEL_PLAYLISTS_TAB,
                  MENU_SETTING_ACTION, 0, 0);
#endif

            if (frontend_driver_parse_drive_list(info->list, true) != 0)
               menu_entries_append_enum(info->list, "/",
                     msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                     MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
                     MENU_SETTING_ACTION, 0, 0);

            if (!settings->bools.kiosk_mode_enable)
            {
               menu_entries_append_enum(info->list,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
                     MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,
                     MENU_SETTING_ACTION, 0, 0);
            }

            info->need_push    = true;
            info->need_refresh = true;
            ret = 0;
         }
         break;
      case DISPLAYLIST_MAIN_MENU:
         {
            settings_t   *settings      = config_get_ptr();
            rarch_system_info_t *system = runloop_get_system_info();
            menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, info->list);

            entry.data            = menu;
            entry.info            = info;
            entry.parse_type      = PARSE_ACTION;
            entry.add_empty_entry = false;

            if (!string_is_empty(system->info.library_name) &&
                  !string_is_equal(system->info.library_name,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (system->load_no_content)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_START_CORE;
               menu_displaylist_setting(&entry);
            }

#ifndef HAVE_DYNAMIC
            if (frontend_driver_has_fork())
#endif
            {
               if (settings->bools.menu_show_load_core)
               {
                  entry.enum_idx   = MENU_ENUM_LABEL_CORE_LIST;
                  menu_displaylist_setting(&entry);
               }
            }

            if (settings->bools.menu_show_load_content)
            {

               entry.enum_idx      = MENU_ENUM_LABEL_LOAD_CONTENT_LIST;
               menu_displaylist_setting(&entry);
               /* Core fully loaded, use the subsystem data */
               if (system->subsystem.data)
                     subsystem = system->subsystem.data;
               /* Core not loaded completely, use the data we peeked on load core */
               else
                  subsystem = subsystem_data;

               menu_subsystem_populate(subsystem, info);
            }

            entry.enum_idx      = MENU_ENUM_LABEL_ADD_CONTENT_LIST;
            menu_displaylist_setting(&entry);
#ifdef HAVE_QT
            if (settings->bools.desktop_menu_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHOW_WIMP;
               menu_displaylist_setting(&entry);
            }
#endif
#if defined(HAVE_NETWORKING)
            if (settings->bools.menu_show_online_updater && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_ONLINE_UPDATER;
               menu_displaylist_setting(&entry);
            }
#endif
            if (!settings->bools.menu_content_show_settings && !string_is_empty(settings->paths.menu_content_show_settings_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.kiosk_mode_enable && !string_is_empty(settings->paths.kiosk_mode_password))
            {
               entry.enum_idx      = MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_information)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_INFORMATION_LIST;
               menu_displaylist_setting(&entry);
            }

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_CPU_PROFILE;
            menu_displaylist_setting(&entry);
#endif

#ifdef HAVE_LAKKA_SWITCH
            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_GPU_PROFILE;
            menu_displaylist_setting(&entry);

            entry.enum_idx      = MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL;
            menu_displaylist_setting(&entry);
#endif

            if (settings->bools.menu_show_configurations && !settings->bools.kiosk_mode_enable)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_CONFIGURATIONS_LIST;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_help)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_HELP_LIST;
               menu_displaylist_setting(&entry);
            }

#if !defined(IOS)
            if (settings->bools.menu_show_restart_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_RESTART_RETROARCH;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_quit_retroarch)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_QUIT_RETROARCH;
               menu_displaylist_setting(&entry);
            }
#endif
            if (settings->bools.menu_show_reboot)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_REBOOT;
               menu_displaylist_setting(&entry);
            }

            if (settings->bools.menu_show_shutdown)
            {
               entry.enum_idx      = MENU_ENUM_LABEL_SHUTDOWN;
               menu_displaylist_setting(&entry);
            }

            info->need_push    = true;
            ret = 0;
         }
         break;
   }
   return ret;
}

static bool xmb_menu_init_list(void *data)
{
   menu_displaylist_info_t info;

   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   file_list_t *selection_buf   = menu_entries_get_selection_buf_ptr(0);

   menu_displaylist_info_init(&info);

   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU));
   info.exts                    =
      strdup(file_path_str(FILE_PATH_LPL_EXTENSION_NO_DOT));
   info.type_default            = FILE_TYPE_PLAIN;
   info.enum_idx                = MENU_ENUM_LABEL_MAIN_MENU;

   menu_entries_append_enum(menu_stack, info.path,
         info.label,
         MENU_ENUM_LABEL_MAIN_MENU,
         info.type, info.flags, 0);

   info.list  = selection_buf;

   if (!menu_displaylist_ctl(DISPLAYLIST_MAIN_MENU, &info))
      goto error;

   info.need_push = true;

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);
   return true;

error:
   menu_displaylist_info_free(&info);
   return false;
}

static int xmb_pointer_tap(void *userdata,
      unsigned x, unsigned y, unsigned ptr,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   unsigned header_height = menu_display_get_header_height();

   if (y < header_height)
   {
      size_t selection = menu_navigation_get_selection();
      return (unsigned)menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
   }
   else if (ptr <= (menu_entries_get_size() - 1))
   {
      size_t selection         = menu_navigation_get_selection();
      if (ptr == selection && cbs && cbs->action_select)
         return (unsigned)menu_entry_action(entry, (unsigned)selection, MENU_ACTION_SELECT);

      menu_navigation_set_selection(ptr);
      menu_driver_navigation_set(false);
   }

   return 0;
}

#ifdef HAVE_MENU_WIDGETS
static bool xmb_get_load_content_animation_data(void *userdata, menu_texture_item *icon, char **playlist_name)
{
   xmb_handle_t *xmb = (xmb_handle_t*) userdata;

   if (xmb->categories_selection_ptr > xmb->system_tab_end)
   {
      xmb_node_t *node = (xmb_node_t*) file_list_get_userdata_at_offset(xmb->horizontal_list, xmb->categories_selection_ptr - xmb->system_tab_end-1);

      *icon            = node->icon;
      *playlist_name   = xmb->title_name;
   }
   else
   {
      *icon            = xmb->textures.list[XMB_TEXTURE_QUICKMENU];
      *playlist_name   = "RetroArch";
   }

   return true;
}
#endif

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_messagebox,
   generic_menu_iterate,
   xmb_render,
   xmb_frame,
   xmb_init,
   xmb_free,
   xmb_context_reset,
   xmb_context_destroy,
   xmb_populate_entries,
   xmb_toggle,
   xmb_navigation_clear,
   NULL, /*xmb_navigation_pointer_changed,*/ /* Note: navigation_set() is called each time navigation_increment/decrement() */
   NULL, /*xmb_navigation_pointer_changed,*/ /* is called, so linking these just duplicates work... */
   xmb_navigation_set,
   xmb_navigation_pointer_changed,
   xmb_navigation_alphabet,
   xmb_navigation_alphabet,
   xmb_menu_init_list,
   xmb_list_insert,
   NULL,
   xmb_list_free,
   xmb_list_clear,
   xmb_list_cache,
   xmb_list_push,
   xmb_list_get_selection,
   xmb_list_get_size,
   xmb_list_get_entry,
   NULL,
   xmb_list_bind_init,
   xmb_load_image,
   "xmb",
   xmb_environ,
   xmb_pointer_tap,
   xmb_update_thumbnail_path,
   xmb_update_thumbnail_image,
   xmb_refresh_thumbnail_image,
   xmb_set_thumbnail_system,
   xmb_get_thumbnail_system,
   xmb_set_thumbnail_content,
   menu_display_osk_ptr_at_pos,
   xmb_update_savestate_thumbnail_path,
   xmb_update_savestate_thumbnail_image,
   NULL, /* pointer_down */
   NULL, /* pointer_up   */
#ifdef HAVE_MENU_WIDGETS
   xmb_get_load_content_animation_data
#else
   NULL
#endif
};
