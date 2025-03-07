/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - natinusala
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

#ifndef _MENU_WIDGETS_H
#define _MENU_WIDGETS_H

#include <queues/task_queue.h>
#include <queues/message_queue.h>

#include "../../retroarch.h"

#define DEFAULT_BACKDROP               0.75f

#define MSG_QUEUE_PENDING_MAX          32
#define MSG_QUEUE_ONSCREEN_MAX         4

#define MSG_QUEUE_ANIMATION_DURATION      330
#define VOLUME_DURATION                   3000
#define SCREENSHOT_DURATION_IN            66
#define SCREENSHOT_DURATION_OUT           SCREENSHOT_DURATION_IN*10
#define SCREENSHOT_NOTIFICATION_DURATION  6000
#define CHEEVO_NOTIFICATION_DURATION      4000
#define TASK_FINISHED_DURATION            3000
#define HOURGLASS_INTERVAL                5000
#define HOURGLASS_DURATION                1000
#define GENERIC_MESSAGE_DURATION          3000

void menu_widgets_init(bool video_is_threaded);
void menu_widgets_free(void);
bool menu_widgets_ready(void);

bool menu_widgets_msg_queue_push(const char *msg,
      unsigned duration,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category,
      unsigned prio, bool flush);

bool menu_widgets_volume_update_and_show(void);

bool menu_widgets_set_fps_text(char *fps_text);

void menu_widgets_iterate(void);

bool menu_widgets_set_paused(bool is_paused);
bool menu_widgets_set_fast_forward(bool is_fast_forward);
bool menu_widgets_set_rewind(bool is_rewind);

bool menu_widgets_task_msg_queue_push(retro_task_t *task,
      const char *msg,
      unsigned prio, unsigned duration,
      bool flush);

void menu_widgets_screenshot_taken(const char *shotname, const char *filename);

void menu_widgets_start_load_content_animation(const char *content_name, bool remove_extension);
void menu_widgets_cleanup_load_content_animation(void);

void menu_widgets_context_reset(bool is_threaded);

void menu_widgets_context_destroy(void);

bool menu_widgets_push_achievement(const char *title, const char *badge);

/* Warning: not thread safe! */
bool menu_widgets_set_message(char *message);

/* Warning: not thread safe! */
bool menu_widgets_set_libretro_message(const char *message, unsigned duration);

/* All the functions below should be called in
 * the video driver - once they are all added, set
 * enable_menu_widgets to true for that driver */
void menu_widgets_frame(video_frame_info_t *video_info);

#endif
