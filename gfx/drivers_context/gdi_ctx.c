/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Win32/GDI context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /*_WIN32_WINNT_WIN2K */
#endif

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../dynamic.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

static HDC   win32_gdi_hdc;

static unsigned         win32_gdi_major       = 0;
static unsigned         win32_gdi_minor       = 0;
static int              win32_gdi_interval    = 0;
static enum gfx_ctx_api win32_gdi_api         = GFX_CTX_NONE;

typedef struct gfx_ctx_gdi_data
{
   void *empty;
} gfx_ctx_gdi_data_t;

void *dinput_gdi;

static void setup_gdi_pixel_format(HDC hdc)
{
   PIXELFORMATDESCRIPTOR pfd = {0};
   pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
   pfd.nVersion     = 1;
   pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
   pfd.iPixelType   = PFD_TYPE_RGBA;
   pfd.cColorBits   = 32;
   pfd.cDepthBits   = 0;
   pfd.cStencilBits = 0;
   pfd.iLayerType   = PFD_MAIN_PLANE;

   SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
}

static void gfx_ctx_gdi_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   win32_check_window(quit, resize, width, height);
}

static bool gfx_ctx_gdi_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   switch (win32_gdi_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_gdi_update_window_title(void *data, void *data2)
{
   const settings_t *settings = config_get_ptr();
   video_frame_info_t* video_info = (video_frame_info_t*)data2;
   const ui_window_t *window = ui_companion_driver_get_window_ptr();
   char title[128];

   title[0] = '\0';

   if (settings->bools.video_memory_show)
   {
#ifndef __WINRT__
      int64_t mem_bytes_used = frontend_driver_get_used_memory();
      int64_t mem_bytes_total = frontend_driver_get_total_memory();
      char         mem[128];

      mem[0] = '\0';

      snprintf(
            mem, sizeof(mem), " || MEM: %.2f/%.2fMB", mem_bytes_used / (1024.0f * 1024.0f),
            mem_bytes_total / (1024.0f * 1024.0f));
      strlcat(video_info->fps_text, mem, sizeof(video_info->fps_text));
#endif
   }

   video_driver_get_window_title(title, sizeof(title));

   if (window && title[0])
      window->set_title(&main_window, title);
}

static void gfx_ctx_gdi_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   HWND         window  = win32_get_window();

   (void)data;

   if (!window)
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id           = 0;
      HMONITOR hm_to_use        = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect = current_mon.rcMonitor;
      *width  = mon_rect.right - mon_rect.left;
      *height = mon_rect.bottom - mon_rect.top;
   }
   else
   {
      *width  = g_win32_resize_width;
      *height = g_win32_resize_height;
   }
}

static void *gfx_ctx_gdi_init(
      video_frame_info_t *video_info, void *video_driver)
{
   WNDCLASSEX wndclass     = {0};
   gfx_ctx_gdi_data_t *gdi = (gfx_ctx_gdi_data_t*)calloc(1, sizeof(*gdi));

   if (!gdi)
      return NULL;

   if (g_win32_inited)
      goto error;

   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = WndProcGDI;
   if (!win32_window_init(&wndclass, true, NULL))
      goto error;

   switch (win32_gdi_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return gdi;

error:
   if (gdi)
      free(gdi);
   return NULL;
}

static void gfx_ctx_gdi_destroy(void *data)
{
   gfx_ctx_gdi_data_t *gdi = (gfx_ctx_gdi_data_t*)data;
   HWND     window         = win32_get_window();

   switch (win32_gdi_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (window && win32_gdi_hdc)
   {
      ReleaseDC(window, win32_gdi_hdc);
      win32_gdi_hdc = NULL;
   }

   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

   if (g_win32_restore_desktop)
   {
      win32_monitor_get_info();
      g_win32_restore_desktop     = false;
   }

   if (gdi)
      free(gdi);

   g_win32_inited                   = false;
   win32_gdi_major                  = 0;
   win32_gdi_minor                  = 0;
}

static bool gfx_ctx_gdi_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[GDI]: win32_set_video_mode failed.\n");
      goto error;
   }

   switch (win32_gdi_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;

error:
   gfx_ctx_gdi_destroy(data);
   return false;
}

static void gfx_ctx_gdi_input_driver(void *data,
      const char *joypad_name,
      const input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();

   (void)data;

#if _WIN32_WINNT >= 0x0501
   /* winraw only available since XP */
   if (string_is_equal(settings->arrays.input_driver, "raw"))
   {
      *input_data = input_winraw.init(joypad_name);
      if (*input_data)
      {
         *input = &input_winraw;
         dinput_gdi = NULL;
         return;
      }
   }
#endif

#ifdef HAVE_DINPUT
   dinput_gdi  = input_dinput.init(joypad_name);
   *input      = dinput_gdi ? &input_dinput : NULL;
#else
   dinput_gdi  = NULL;
   *input      = NULL;
#endif
   *input_data = dinput_gdi;
}

static bool gfx_ctx_gdi_has_focus(void *data)
{
   return win32_has_focus();
}

static bool gfx_ctx_gdi_suppress_screensaver(void *data, bool enable)
{
   return win32_suppress_screensaver(data, enable);
}

static bool gfx_ctx_gdi_has_windowed(void *data)
{
   (void)data;

   return true;
}

static bool gfx_ctx_gdi_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return win32_get_metrics(data, type, value);
}

static enum gfx_ctx_api gfx_ctx_gdi_get_api(void *data)
{
   return win32_gdi_api;
}

static bool gfx_ctx_gdi_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   win32_gdi_major = major;
   win32_gdi_minor = minor;
   win32_gdi_api   = api;

   return true;
}

static void gfx_ctx_gdi_show_mouse(void *data, bool state)
{
   (void)data;
   win32_show_cursor(state);
}

static void gfx_ctx_gdi_swap_interval(void *data, int interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_gdi_set_flags(void *data, uint32_t flags)
{
   (void)data;
   (void)flags;
}

static uint32_t gfx_ctx_gdi_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void gfx_ctx_gdi_swap_buffers(void *data, void *data2)
{
   (void)data;

   SwapBuffers(win32_gdi_hdc);
}

void create_gdi_context(HWND hwnd, bool *quit)
{
   (void)quit;
   win32_gdi_hdc = GetDC(hwnd);

   setup_gdi_pixel_format(win32_gdi_hdc);

   g_win32_inited = true;
}

const gfx_ctx_driver_t gfx_ctx_gdi = {
   gfx_ctx_gdi_init,
   gfx_ctx_gdi_destroy,
   gfx_ctx_gdi_get_api,
   gfx_ctx_gdi_bind_api,
   gfx_ctx_gdi_swap_interval,
   gfx_ctx_gdi_set_video_mode,
   gfx_ctx_gdi_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_gdi_get_metrics,
   NULL,
   gfx_ctx_gdi_update_window_title,
   gfx_ctx_gdi_check_window,
   gfx_ctx_gdi_set_resize,
   gfx_ctx_gdi_has_focus,
   gfx_ctx_gdi_suppress_screensaver,
   gfx_ctx_gdi_has_windowed,
   gfx_ctx_gdi_swap_buffers,
   gfx_ctx_gdi_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_gdi_show_mouse,
   "gdi",
   gfx_ctx_gdi_get_flags,
   gfx_ctx_gdi_set_flags,
   NULL,
   NULL,
   NULL
};
