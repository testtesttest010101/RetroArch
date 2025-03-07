/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <math.h>

/* Win32/WGL context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <tchar.h>
#include <wchar.h>

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#include <dynamic/dylib.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

#if defined(HAVE_OPENGL) 
#include "../common/gl_common.h"
#elif defined(HAVE_OPENGL_CORE)
#include "../common/gl_core_common.h"
#elif defined(HAVE_OPENGL1)
#include "../common/gl1_common.h"
#endif

#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN)
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif

#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif

#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif

#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001
#endif

#ifndef WGL_CONTEXT_FLAGS_ARB
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#endif

#ifndef WGL_CONTEXT_DEBUG_BIT_ARB
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#endif
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
typedef HGLRC (APIENTRY *wglCreateContextAttribsProc)(HDC, HGLRC, const int*);
static wglCreateContextAttribsProc pcreate_context;
#endif
static BOOL (APIENTRY *p_swap_interval)(int);

static HGLRC win32_hrc;
static HGLRC win32_hw_hrc;
static HDC   win32_hdc;
static bool  win32_use_hw_ctx             = false;
static bool  win32_core_hw_context_enable = false;
static bool  wgl_adaptive_vsync           = false;

#ifdef HAVE_VULKAN
static gfx_ctx_vulkan_data_t win32_vk;
#endif

static unsigned         win32_major       = 0;
static unsigned         win32_minor       = 0;
static int              win32_interval    = 0;
static enum gfx_ctx_api win32_api         = GFX_CTX_NONE;

#ifdef HAVE_DYNAMIC
static dylib_t          dll_handle        = NULL; /* Handle to OpenGL32.dll */
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
static bool wgl_has_extension(const char *extension, const char *extensions)
{
   const char *start      = NULL;
   const char *terminator = NULL;
   const char      *where = strchr(extension, ' ');

   if (where || *extension == '\0')
      return false;

   if (!extensions)
      return false;

   start = extensions;

   for (;;)
   {
      where = strstr(start, extension);
      if (!where)
         break;

      terminator = where + strlen(extension);
      if (where == start || *(where - 1) == ' ')
         if (*terminator == ' ' || *terminator == '\0')
            return true;

      start = terminator;
   }
   return false;
}
#endif

typedef struct gfx_ctx_cgl_data
{
   void *empty;
} gfx_ctx_wgl_data_t;

static gfx_ctx_proc_t gfx_ctx_wgl_get_proc_address(const char *symbol)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         {
            gfx_ctx_proc_t func = (gfx_ctx_proc_t)wglGetProcAddress(symbol);
            if (func)
               return func;
         }
#endif
         break;
      default:
         break;
   }

#ifdef HAVE_DYNAMIC
   return (gfx_ctx_proc_t)GetProcAddress((HINSTANCE)dll_handle, symbol);
#else
   return NULL;
#endif
}

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
static void setup_pixel_format(HDC hdc)
{
   PIXELFORMATDESCRIPTOR pfd = {0};
   pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
   pfd.nVersion     = 1;
   pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   pfd.iPixelType   = PFD_TYPE_RGBA;
   pfd.cColorBits   = 32;
   pfd.cDepthBits   = 0;
   pfd.cStencilBits = 0;
   pfd.iLayerType   = PFD_MAIN_PLANE;

   SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
}

static void create_gl_context(HWND hwnd, bool *quit)
{
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   bool debug                           = hwr->debug_context;
   bool core_context                    = (win32_major * 1000 + win32_minor) >= 3001;
   win32_hdc                            = GetDC(hwnd);

   setup_pixel_format(win32_hdc);

#ifdef GL_DEBUG
   debug = true;
#endif

   if (win32_hrc)
   {
      RARCH_LOG("[WGL]: Using cached GL context.\n");
      video_driver_set_video_cache_context_ack();
   }
   else
   {
      win32_hrc = wglCreateContext(win32_hdc);

      /* We'll create shared context later if not. */
      if (win32_hrc && !core_context && !debug)
      {
         win32_hw_hrc = wglCreateContext(win32_hdc);
         if (win32_hw_hrc)
         {
            if (!wglShareLists(win32_hrc, win32_hw_hrc))
            {
               RARCH_LOG("[WGL]: Failed to share contexts.\n");
               *quit = true;
            }
         }
         else
            *quit = true;
      }
   }

   if (win32_hrc)
   {
      if (wglMakeCurrent(win32_hdc, win32_hrc))
         g_win32_inited = true;
      else
         *quit          = true;
   }
   else
   {
      *quit        = true;
      return;
   }

   if (core_context || debug)
   {
      int attribs[16];
      int *aptr = attribs;

      if (core_context)
      {
         *aptr++ = WGL_CONTEXT_MAJOR_VERSION_ARB;
         *aptr++ = win32_major;
         *aptr++ = WGL_CONTEXT_MINOR_VERSION_ARB;
         *aptr++ = win32_minor;

         /* Technically, we don't have core/compat until 3.2.
          * Version 3.1 is either compat or not depending
          * on GL_ARB_compatibility.
          */
         if ((win32_major * 1000 + win32_minor) >= 3002)
         {
            *aptr++ = WGL_CONTEXT_PROFILE_MASK_ARB;
            *aptr++ = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
         }
      }

      if (debug)
      {
         *aptr++ = WGL_CONTEXT_FLAGS_ARB;
         *aptr++ = WGL_CONTEXT_DEBUG_BIT_ARB;
      }

      *aptr = 0;

      if (!pcreate_context)
         pcreate_context = (wglCreateContextAttribsProc)gfx_ctx_wgl_get_proc_address("wglCreateContextAttribsARB");

      if (pcreate_context)
      {
         HGLRC context = pcreate_context(win32_hdc, NULL, attribs);

         if (context)
         {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(win32_hrc);
            win32_hrc = context;
            if (!wglMakeCurrent(win32_hdc, win32_hrc))
               *quit = true;
         }
         else
            RARCH_ERR("[WGL]: Failed to create core context. Falling back to legacy context.\n");

         if (win32_use_hw_ctx)
         {
            win32_hw_hrc = pcreate_context(win32_hdc, context, attribs);
            if (!win32_hw_hrc)
            {
               RARCH_ERR("[WGL]: Failed to create shared context.\n");
               *quit = true;
            }
         }
      }
      else
         RARCH_ERR("[WGL]: wglCreateContextAttribsARB not supported.\n");
   }

   {

      const char *(WINAPI * wglGetExtensionsStringARB) (HDC) = 0;
      const char *extensions                                 = NULL;

      wglGetExtensionsStringARB = (const char *(WINAPI *) (HDC))
         gfx_ctx_wgl_get_proc_address("wglGetExtensionsStringARB");
      if (wglGetExtensionsStringARB)
         extensions = wglGetExtensionsStringARB(win32_hdc);
      RARCH_LOG("[WGL] extensions: %s\n", extensions);
      if (wgl_has_extension("WGL_EXT_swap_control_tear", extensions))
      {
         RARCH_LOG("[WGL]: Adaptive VSync supported.\n");
         wgl_adaptive_vsync = true;
      }
   }
}
#endif

void create_graphics_context(HWND hwnd, bool *quit)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         create_gl_context(hwnd, quit);
#endif
         break;

      case GFX_CTX_VULKAN_API:
      {
#ifdef HAVE_VULKAN
         RECT rect;
         HINSTANCE instance;
         unsigned width  = 0;
         unsigned height = 0;

         GetClientRect(hwnd, &rect);

         instance = GetModuleHandle(NULL);
         width    = rect.right - rect.left;
         height   = rect.bottom - rect.top;

         if (!vulkan_surface_create(&win32_vk, VULKAN_WSI_WIN32,
                  &instance, &hwnd,
                  width, height, win32_interval))
            *quit = true;

         g_win32_inited = true;
#endif
      }
      break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

void *dinput_wgl;

static void gfx_ctx_wgl_swap_interval(void *data, int interval)
{
   (void)data;

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         win32_interval = interval;
         if (!win32_hrc)
            return;
         if (!p_swap_interval)
            return;

         RARCH_LOG("[WGL]: wglSwapInterval(%i)\n", win32_interval);
         if (!p_swap_interval(win32_interval))
            RARCH_WARN("[WGL]: wglSwapInterval() failed.\n");
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (win32_interval != interval)
         {
            win32_interval = interval;
            if (win32_vk.swapchain)
               win32_vk.need_new_swapchain = true;
         }
#endif
         break;

      case GFX_CTX_NONE:
      default:
         win32_interval = interval;
         break;
   }
}

static void gfx_ctx_wgl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   win32_check_window(quit, resize, width, height);

   switch (win32_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (win32_vk.need_new_swapchain)
            *resize = true;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void gfx_ctx_wgl_swap_buffers(void *data, void *data2)
{
   (void)data;

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
         SwapBuffers(win32_hdc);
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&win32_vk, win32_vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&win32_vk);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_wgl_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   switch (win32_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_create_swapchain(&win32_vk, width, height, win32_interval))
         {
            RARCH_ERR("[Win32/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         if (win32_vk.created_new_swapchain)
            vulkan_acquire_next_image(&win32_vk);
         win32_vk.context.invalid_swapchain = true;
         win32_vk.need_new_swapchain        = false;
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_wgl_update_title(void *data, void *data2)
{
   const settings_t *settings = config_get_ptr();
   video_frame_info_t* video_info = (video_frame_info_t*)data2;
   char title[128];

   title[0] = '\0';

   if (settings->bools.video_memory_show)
   {
#ifndef __WINRT__
      uint64_t mem_bytes_used = frontend_driver_get_used_memory();
      uint64_t mem_bytes_total = frontend_driver_get_total_memory();
      char         mem[128];

      mem[0] = '\0';

      snprintf(
            mem, sizeof(mem), " || MEM: %.2f/%.2fMB", mem_bytes_used / (1024.0f * 1024.0f),
            mem_bytes_total / (1024.0f * 1024.0f));
      strlcat(video_info->fps_text, mem, sizeof(video_info->fps_text));
#endif
   }

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
   {
      const ui_window_t *window = ui_companion_driver_get_window_ptr();

      if (window)
         window->set_title(&main_window, title);
   }
}

static void gfx_ctx_wgl_get_video_size(void *data,
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

static void *gfx_ctx_wgl_init(video_frame_info_t *video_info, void *video_driver)
{
   WNDCLASSEX wndclass     = {0};
   gfx_ctx_wgl_data_t *wgl = (gfx_ctx_wgl_data_t*)calloc(1, sizeof(*wgl));

   if (!wgl)
      return NULL;

   if (g_win32_inited)
      goto error;

#ifdef HAVE_DYNAMIC
   dll_handle = dylib_load("OpenGL32.dll");
#endif

   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = WndProcGL;
   if (!win32_window_init(&wndclass, true, NULL))
      goto error;

   switch (win32_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_context_init(&win32_vk, VULKAN_WSI_WIN32))
            goto error;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return wgl;

error:
   if (wgl)
      free(wgl);
   return NULL;
}

static void gfx_ctx_wgl_destroy(void *data)
{
   HWND            window  = win32_get_window();
   gfx_ctx_wgl_data_t *wgl = (gfx_ctx_wgl_data_t*)data;

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         if (win32_hrc)
         {
            glFinish();
            wglMakeCurrent(NULL, NULL);

            if (!video_driver_is_video_cache_context())
            {
               if (win32_hw_hrc)
                  wglDeleteContext(win32_hw_hrc);
               wglDeleteContext(win32_hrc);
               win32_hrc = NULL;
               win32_hw_hrc = NULL;
            }
         }
#endif
         break;

      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_context_destroy(&win32_vk, win32_vk.vk_surface != VK_NULL_HANDLE);
         if (win32_vk.context.queue_lock)
            slock_free(win32_vk.context.queue_lock);
         memset(&win32_vk, 0, sizeof(win32_vk));
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   if (window && win32_hdc)
   {
      ReleaseDC(window, win32_hdc);
      win32_hdc = NULL;
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

#ifdef HAVE_DYNAMIC
   dylib_close(dll_handle);
#endif

   if (wgl)
      free(wgl);

   wgl_adaptive_vsync           = false;
   win32_core_hw_context_enable = false;
   g_win32_inited               = false;
   win32_major                  = 0;
   win32_minor                  = 0;
   p_swap_interval              = NULL;
}

static bool gfx_ctx_wgl_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifdef HAVE_VULKAN
   win32_vk.fullscreen = fullscreen;
#endif

   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[WGL]: win32_set_video_mode failed.\n");
      goto error;
   }

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
         p_swap_interval = (BOOL (APIENTRY *)(int))gfx_ctx_wgl_get_proc_address("wglSwapIntervalEXT");
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   gfx_ctx_wgl_swap_interval(data, win32_interval);
   return true;

error:
   gfx_ctx_wgl_destroy(data);
   return false;
}

static void gfx_ctx_wgl_input_driver(void *data,
      const char *joypad_name,
      const input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();

#if _WIN32_WINNT >= 0x0501
   /* winraw only available since XP */
   if (string_is_equal(settings->arrays.input_driver, "raw"))
   {
      *input_data = input_winraw.init(joypad_name);
      if (*input_data)
      {
         *input     = &input_winraw;
         dinput_wgl = NULL;
         return;
      }
   }
#endif

#ifdef HAVE_DINPUT
   dinput_wgl  = input_dinput.init(joypad_name);
   *input      = dinput_wgl ? &input_dinput : NULL;
   *input_data = dinput_wgl;
#endif
}

static bool gfx_ctx_wgl_has_focus(void *data)
{
   return win32_has_focus();
}

static bool gfx_ctx_wgl_suppress_screensaver(void *data, bool enable)
{
   return win32_suppress_screensaver(data, enable);
}

static bool gfx_ctx_wgl_has_windowed(void *data)
{
   (void)data;

   return true;
}

static bool gfx_ctx_wgl_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return win32_get_metrics(data, type, value);
}

static enum gfx_ctx_api gfx_ctx_wgl_get_api(void *data)
{
   return win32_api;
}

static bool gfx_ctx_wgl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   win32_major = major;
   win32_minor = minor;
   win32_api   = api;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
   if (api == GFX_CTX_OPENGL_API)
      return true;
#endif
#if defined(HAVE_VULKAN)
   if (api == GFX_CTX_VULKAN_API)
      return true;
#endif

   return false;
}

static void gfx_ctx_wgl_show_mouse(void *data, bool state)
{
   (void)data;
   win32_show_cursor(state);
}

static void gfx_ctx_wgl_bind_hw_render(void *data, bool enable)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         win32_use_hw_ctx = enable;

         if (win32_hdc)
            wglMakeCurrent(win32_hdc, enable ? win32_hw_hrc : win32_hrc);
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }
}

#ifdef HAVE_VULKAN
static void *gfx_ctx_wgl_get_context_data(void *data)
{
   (void)data;
   return &win32_vk.context;
}
#endif

static uint32_t gfx_ctx_wgl_get_flags(void *data)
{
   uint32_t          flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);

   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
         if (wgl_adaptive_vsync)
         {
            BIT32_SET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC);
         }

         if (win32_core_hw_context_enable)
         {
            BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);
         }

         if (string_is_equal(video_driver_get_ident(), "gl1")) { }
         else if (string_is_equal(video_driver_get_ident(), "glcore"))
         {
#ifdef HAVE_SLANG
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
         }
         else
         {
#ifdef HAVE_CG
            if (!win32_core_hw_context_enable)
               BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);
#endif
#ifdef HAVE_GLSL
            BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
         }

         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_SLANG
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return flags;
}

static void gfx_ctx_wgl_set_flags(void *data, uint32_t flags)
{
   switch (win32_api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
         if (BIT32_GET(flags, GFX_CTX_FLAGS_ADAPTIVE_VSYNC))
            wgl_adaptive_vsync = true;

         if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
            win32_core_hw_context_enable = true;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

}

static void gfx_ctx_wgl_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   win32_get_video_output_size(width, height);
}

static void gfx_ctx_wgl_get_video_output_prev(void *data)
{
}

static void gfx_ctx_wgl_get_video_output_next(void *data)
{
}

const gfx_ctx_driver_t gfx_ctx_wgl = {
   gfx_ctx_wgl_init,
   gfx_ctx_wgl_destroy,
   gfx_ctx_wgl_get_api,
   gfx_ctx_wgl_bind_api,
   gfx_ctx_wgl_swap_interval,
   gfx_ctx_wgl_set_video_mode,
   gfx_ctx_wgl_get_video_size,
   win32_get_refresh_rate,
   gfx_ctx_wgl_get_video_output_size,
   gfx_ctx_wgl_get_video_output_prev,
   gfx_ctx_wgl_get_video_output_next,
   gfx_ctx_wgl_get_metrics,
   NULL,
   gfx_ctx_wgl_update_title,
   gfx_ctx_wgl_check_window,
   gfx_ctx_wgl_set_resize,
   gfx_ctx_wgl_has_focus,
   gfx_ctx_wgl_suppress_screensaver,
   gfx_ctx_wgl_has_windowed,
   gfx_ctx_wgl_swap_buffers,
   gfx_ctx_wgl_input_driver,
   gfx_ctx_wgl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_wgl_show_mouse,
   "wgl",
   gfx_ctx_wgl_get_flags,
   gfx_ctx_wgl_set_flags,
   gfx_ctx_wgl_bind_hw_render,
#ifdef HAVE_VULKAN
   gfx_ctx_wgl_get_context_data,
#else
   NULL,
#endif
   NULL
};
