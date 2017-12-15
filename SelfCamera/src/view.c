/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <tizen.h>
#include <app.h>
#include <dlog.h>
#include <efl_extension.h>
#include <Elementary.h>
#include <wav_player.h>
#include <media_content.h>
#include "selfcamera.h"
#include "view.h"
#include "view_defines.h"

#define COUNTER_STR_LEN 3
#define FILE_PREFIX "IMAGE"
#define STR_ERROR "Error"
#define STR_OK "OK"
#define STR_FILE_PROTOCOL "file://"

static struct view_info {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *navi;
	Elm_Object_Item *navi_item;
	Evas_Object *layout;
	Evas_Object *popup;
	Evas_Object *preview_canvas;

	camera_h camera;
	Eina_Bool camera_enabled;
	Ecore_Timer *timer;
	int timer_count;
	int selected_timer_interval;
	char *media_content_folder;
	int selected_mode_btn;
} s_info = {
	.win = NULL,
	.conform = NULL,
	.navi = NULL,
	.navi_item = NULL,
	.layout = NULL,
	.popup = NULL,
	.preview_canvas = NULL,
	.camera = NULL,
	.camera_enabled = false,
	.timer = NULL,
	.timer_count = 0,
	.selected_timer_interval = 1,
	.media_content_folder = NULL,
	.selected_mode_btn = 0,
};


static Evas_Object *_app_navi_add(void);
static Evas_Object *_main_view_add(void);
static void _main_view_stop_timer(void);
static Evas_Object *_view_create_bg(void);

/**
 * @brief Creates essential objects: window, conformant and layout.
 * @param[in] user data pointer
 * @return EINA_TRUE on success, EINA_FALSE on error
 */
Eina_Bool view_create(void *user_data)
{
	elm_config_accel_preference_set("opengl");

	/* Create the window */
	s_info.win = view_create_win(PACKAGE);
	if (s_info.win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a window.");
		return EINA_FALSE;
	}

	/* Create the conformant */
	s_info.conform = view_create_conformant_without_indicator(s_info.win);
	if (s_info.conform == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a conformant");
		return EINA_FALSE;
	}

	/* Add new background to conformant */
	if (!_view_create_bg()) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to create background");
		return EINA_FALSE;
	}

	/* Add naviframe */
	s_info.navi = _app_navi_add();
	if (!s_info.navi) {
		dlog_print(DLOG_ERROR, LOG_TAG, "_app_navi_add() failed");
		return EINA_FALSE;
	}

	/* Add main view to naviframe */
	Evas_Object *view = _main_view_add();
	if (!view)
		dlog_print(DLOG_ERROR, LOG_TAG, "main_view_add() failed");

	/* Show the window after main view is set up */
	evas_object_show(s_info.win);

	return EINA_TRUE;
}

/**
 * @brief Creates a basic window named package.
 * @param[in] pkg_name Name of the window
 * @return Window pointer
 */
Evas_Object *view_create_win(const char *pkg_name)
{
	Evas_Object *win = NULL;

	/*
	 * Window
	 * Create and initialise elm_win.
	 * elm_win is mandatory to manipulate the window.
	 */
	win = elm_win_add(NULL, PACKAGE, ELM_WIN_BASIC);
	elm_win_indicator_mode_set(win, ELM_WIN_INDICATOR_HIDE);
	elm_win_conformant_set(win, EINA_TRUE);

	return win;
}

/**
 * @brief Creates a conformant without indicator for the application.
 * @param[in] win The object to which you want to set this conformant
 * @return Created conformant pointer
 * Conformant is mandatory for base GUI to have proper size
 */
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win)
{
	/*
	 * Conformant
	 * Create and initialise elm_conformant.
	 * elm_conformant is mandatory for base GUI to have proper size
	 * when indicator or virtual keypad is visible.
	 */
	Evas_Object *conform = NULL;

	if (win == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "window is NULL.");
		return NULL;
	}

	conform = elm_conformant_add(win);
	evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(win, conform);

	evas_object_show(conform);

	return conform;
}

/**
 * @brief Called on app_pause app event.
 * Stops camera preview.
 */
void view_pause(void)
{
	camera_state_e cur_state = CAMERA_STATE_NONE;
	int res = camera_get_state(s_info.camera, &cur_state);

	if (res != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera state. Error: %d", res);
		return;
	}
	if (cur_state == CAMERA_STATE_PREVIEW)
		camera_stop_preview(s_info.camera);
}

/**
 * @brief Called on app_resume app event.
 * Starts camera preview.
 */
bool view_resume(void)
{
	camera_state_e cur_state = CAMERA_STATE_NONE;
	int res = camera_get_state(s_info.camera, &cur_state);

	if (res != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera state. Error: %d", res);
		return false;
	}
	if (cur_state != CAMERA_STATE_PREVIEW) {
		res = camera_start_preview(s_info.camera);
		if (res == CAMERA_ERROR_NONE)
			camera_start_focusing(s_info.camera, true);
	}
	return true;
}

/**
 * @brief Destroys window and frees its resources.
 */
void view_destroy(void)
{
	if (s_info.win == NULL)
		return;

	evas_object_del(s_info.win);
}

/**
 * @brief Get resource absolute path.
 * @param[in] file_path Resource path relative to resources directory
 * @return Resource absolute path
 */
static inline const char *_get_resource_path(const char *file_path)
{
	static char res_path[PATH_MAX] = { '\0' };
	static char res_folder_path[PATH_MAX] = { '\0' };
	if (res_folder_path[0] == '\0') {
		char *resource_path_buf = app_get_resource_path();
		strncpy(res_folder_path, resource_path_buf, PATH_MAX - 1);
		free(resource_path_buf);
	}
	snprintf(res_path, PATH_MAX, "%s%s", res_folder_path, file_path);
	return res_path;
}

/**
 * @brief Callback called on H/W Back Key Event.
 * @param[in] data User data
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _app_navi_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	Elm_Object_Item *top = elm_naviframe_top_item_get(s_info.navi);

	if (top == elm_naviframe_bottom_item_get(s_info.navi))
		elm_win_lower(s_info.win);
	else
		elm_naviframe_item_pop(s_info.navi);
}

/**
 * @brief Add and configure naviframe to conform.
 * @return Naviframe object pointer
 */
static Evas_Object *_app_navi_add(void)
{
	Evas_Object *navi = elm_naviframe_add(s_info.conform);
	if (!navi) {
		dlog_print(DLOG_ERROR, LOG_TAG, "elm_naviframe_add() failed");
		return NULL;
	}
	elm_naviframe_prev_btn_auto_pushed_set(navi, EINA_FALSE);

	eext_object_event_callback_add(navi, EEXT_CALLBACK_BACK, _app_navi_back_cb, NULL);
	elm_object_part_content_set(s_info.conform, "elm.swallow.content", navi);
	return navi;
}

/**
 * @brief Callback called on layout object being freed (called after Del).
 * @param[in] data User data
 * @param[in] e Evas
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _main_view_destroy_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	view_pause();
	camera_destroy(s_info.camera);
}

/**
 * @brief Rotate image preview according to display rotate.
 * @param[in] camera The Camera handle
 * @param[in] lens_orientation Rotation from lens
 */
static void _main_view_rotate_image_preview(camera_h camera, int lens_orientation)
{
	int display_rotation_angle = (lens_orientation) % 360;

	camera_rotation_e rotation = CAMERA_ROTATION_NONE;
	switch (display_rotation_angle) {
	case 0:
		break;
	case 90:
		rotation = CAMERA_ROTATION_90;
		break;
	case 180:
		rotation = CAMERA_ROTATION_180;
		break;
	case 270:
		rotation = CAMERA_ROTATION_270;
		break;
	default:
		dlog_print(DLOG_ERROR, LOG_TAG, "Wrong lens_orientation value");
		return;
	}

	camera_set_display_rotation(camera, rotation);
}

/**
 * @brief Set image rotation exif.
 * @param[in] camera The Camera handle
 * @param[in] lens_orientation Rotation from lens
 */
static void _main_view_set_image_rotation_exif(camera_h camera, int lens_orientation)
{
	camera_attr_tag_orientation_e orientation = CAMERA_ATTR_TAG_ORIENTATION_TOP_RIGHT;

	if (camera_attr_enable_tag(camera, true) != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot enable write of EXIF information");
		return;
	}
	switch (lens_orientation) {
	case 0:
		break;
	case 90:
		orientation = CAMERA_ATTR_TAG_ORIENTATION_RIGHT_BOTTOM;
		break;
	case 180:
		orientation = CAMERA_ATTR_TAG_ORIENTATION_BOTTOM_LEFT;
		break;
	case 270:
		orientation = CAMERA_ATTR_TAG_ORIENTATION_LEFT_TOP;
		break;
	default:
		dlog_print(DLOG_ERROR, LOG_TAG, "Wrong lens_orientation value");
		return;
	}

	camera_attr_set_tag_orientation(camera, orientation);
}

/**
 * @brief Initialises camera device.
 * @return EINA_TRUE on success, EINA_FALSE on error
 */
static Eina_Bool _main_view_init_camera(void)
{
	int lens_orientation = 0;
	int result = camera_create(CAMERA_DEVICE_CAMERA1, &s_info.camera);
	if (result != CAMERA_ERROR_NONE || !s_info.preview_canvas)
		return false;

	result = camera_set_display(s_info.camera, CAMERA_DISPLAY_TYPE_EVAS, GET_DISPLAY(s_info.preview_canvas));
	if (result != CAMERA_ERROR_NONE || !s_info.preview_canvas)
		return false;

	camera_set_display_mode(s_info.camera, CAMERA_DISPLAY_MODE_LETTER_BOX);
	camera_set_display_visible(s_info.camera, true);

	if (camera_attr_get_lens_orientation(s_info.camera, &lens_orientation)
			== CAMERA_ERROR_NONE) {
		/* Rotate video preview to compensate different lens
		 * orientations */
		_main_view_rotate_image_preview(s_info.camera, lens_orientation);

		/* Rotate captured image to compensate different lens
		 * orientations */
		_main_view_set_image_rotation_exif(s_info.camera, lens_orientation);

		camera_set_display_flip(s_info.camera, CAMERA_FLIP_HORIZONTAL);
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera lens attribute");
	}

	return view_resume();
}

/**
 * @brief Check if dir->d_name has IMAGE prefix.
 * @param[in] dir "dirent" structure with d_name to check
 * @return strncmp String compare result (0 on equal)
 */
static int _main_view_image_file_filter(const struct dirent *dir)
{
	return strncmp(dir->d_name, FILE_PREFIX, sizeof(FILE_PREFIX) - 1) == 0;
}

/**
 * @brief Gets the last file with IMAGE prefix on given path.
 * @param[out] file_path Full image path output
 * @param[in] size Maximum path string length
 * @return Returned file_path length
 */
static size_t _main_view_get_last_file_path(char *file_path, size_t size)
{
	int n = -1;
	struct dirent **namelist = NULL;
	int ret_size = 0;

	n = scandir(s_info.media_content_folder, &namelist, _main_view_image_file_filter, alphasort);

	if (n > 0) {
		ret_size = snprintf(file_path, size, "%s/%s", s_info.media_content_folder, namelist[n - 1]->d_name);

		/* Need to go through array to clear it */
		int i;
		for (i = 0; i < n; ++i) {
			/* The last file in sorted array will be taken */
			free(namelist[i]);
		}
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG, "No files or failed to make scandir");
	}

	free(namelist);
	return ret_size;
}

/**
 * @brief Smart callback on popup close.
 * @param[in] data User data
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _main_view_popup_close_cb(void *data, Evas_Object *obj, void *event_info)
{
	if (s_info.popup) {
		evas_object_del(s_info.popup);
		s_info.popup = NULL;
	}
}

/**
 * @brief Show warning popup.
 * @param[in] navi Naviframe to create popup
 * @param[in] caption Popup caption
 * @param[in] text Popup message text
 * @param[in] button_text Text on popup confirmation button
 */
static void _main_view_show_warning_popup(Evas_Object *navi,
		const char *caption, const char *text, const char *button_text)
{
	Evas_Object *button = NULL;
	Evas_Object *popup = elm_popup_add(navi);
	if (!popup) {
		dlog_print(DLOG_ERROR, LOG_TAG, "popup is not created");
		return;
	}
	elm_object_part_text_set(popup, "title,text", caption);
	elm_object_text_set(popup, text);
	evas_object_show(popup);

	button = elm_button_add(popup);
	if (!button) {
		dlog_print(DLOG_ERROR, LOG_TAG, "button is not created");
		return;
	}
	elm_object_style_set(button, POPUP_BUTTON_STYLE);
	elm_object_text_set(button, button_text);
	elm_object_part_content_set(popup, POPUP_BUTTON_PART, button);
	evas_object_smart_callback_add(button, "clicked", _main_view_popup_close_cb, NULL);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, _main_view_popup_close_cb, NULL);

	s_info.popup = popup;
}

/**
 * @brief Smart callback on thumbnail click.
 * @param[in] data User data
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _main_view_thumbnail_click_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = 0;
	app_control_h app_control = NULL;
	char file_path[PATH_MAX] = { '\0' };
	char file_path_prepared[PATH_MAX + sizeof(STR_FILE_PROTOCOL)] = { '\0' };

	if (_main_view_get_last_file_path(file_path, sizeof(file_path)) == 0)
		return;

	ret = app_control_create(&app_control);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_control_create() failed.");
		return;
	}

	strncpy(file_path_prepared, STR_FILE_PROTOCOL, PATH_MAX + sizeof(STR_FILE_PROTOCOL));
	strncat(file_path_prepared, file_path, PATH_MAX + sizeof(STR_FILE_PROTOCOL));
	ret = app_control_set_uri(app_control, file_path_prepared);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_control_set_uri() failed.");
		app_control_destroy(app_control);
		return;
	}

	ret = app_control_set_operation(app_control, APP_CONTROL_OPERATION_VIEW);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_control_set_operation() failed.");
		app_control_destroy(app_control);
		return;
	}

	ret = app_control_set_mime(app_control, IMAGE_MIME_TYPE);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_control_set_mime() failed.");
		app_control_destroy(app_control);
		return;
	}

	ret = app_control_send_launch_request(app_control, NULL, NULL);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_control_send_launch_request() failed.");

		if (ret == APP_CONTROL_ERROR_APP_NOT_FOUND) {
			_main_view_show_warning_popup(s_info.navi, STR_ERROR,
					"Image viewing application wasn't found", STR_OK);
		} else {
			_main_view_show_warning_popup(s_info.navi, STR_ERROR,
					"Image viewing application initialisation failed", STR_OK);
		}
	}

	app_control_destroy(app_control);
}

/**
 * @brief Set the thumbnail content to specified file.
 * @param[in] file_path Path to the file to set
 */
static void _main_view_thumbnail_set(const char *file_path)
{
	Evas_Object *img = elm_object_part_content_get(s_info.layout, "thumbnail");
	if (!img) {
		img = elm_image_add(s_info.layout);
		elm_object_part_content_set(s_info.layout, "thumbnail", img);
		evas_object_smart_callback_add(img, "clicked", _main_view_thumbnail_click_cb, NULL);
	}
	elm_image_file_set(img, file_path, NULL);
	elm_object_signal_emit(s_info.layout, "default", "thumbnail_background");
}

/**
 * @brief Load last file thumbnail.
 */
static void _main_view_thumbnail_load(void)
{
	char file_path[PATH_MAX] = { '\0' };
	if (_main_view_get_last_file_path(file_path, sizeof(file_path)))
		_main_view_thumbnail_set(file_path);
	else
		elm_object_signal_emit(s_info.layout, "no_image", "thumbnail_background");
}

/**
 * @brief Update the timer count on timer_text part to current value.
 */
static void _main_view_timer_count_update(void)
{
	char count_string[COUNTER_STR_LEN] = { '\0' };

	if (s_info.timer_count > 0)
		snprintf(count_string, sizeof(count_string), "%d", s_info.timer_count);

	elm_object_part_text_set(s_info.layout, "timer_text", count_string);
	evas_object_show(s_info.layout);
}

/**
 * @brief Callback called on H/W Back Key Event.
 * @param[in] data User data
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _main_view_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	if (s_info.timer)
		_main_view_stop_timer();
}

/**
 * @brief Stop and reset timer_count, update counter and unregister back key
 * callback.
 */
static void _main_view_stop_timer(void)
{
	ecore_timer_del(s_info.timer);
	s_info.timer = NULL;
	s_info.timer_count = 0;
	_main_view_timer_count_update();
	eext_object_event_callback_del(s_info.layout, EEXT_CALLBACK_BACK, _main_view_back_cb);
}

/**
 * @brief Generate image file name with full path and IMAGE prefix from current
 * date and time.
 * @param[out] file_path Output c-string array
 * @param[in] size Maximum file_path length
 * @return Generated file_path length
*/
static size_t _main_view_get_file_path(char *file_path, size_t size)
{
	int chars = 0;
	struct tm localtime = { 0 };
	time_t rawtime = time(NULL);

	if (!file_path) {
		dlog_print(DLOG_ERROR, LOG_TAG, "file_path is NULL");
		return 0;
	}

	if (localtime_r(&rawtime, &localtime) == NULL)
		return 0;

	chars = snprintf(file_path, size, "%s/%s_%04i-%02i-%02i_%02i:%02i:%02i.jpg",
			s_info.media_content_folder, FILE_PREFIX,
			localtime.tm_year + 1900, localtime.tm_mon + 1, localtime.tm_mday,
			localtime.tm_hour, localtime.tm_min, localtime.tm_sec);

	return chars;
}

/**
 * @brief Callback called to get information about image data taken by the
 * camera once per frame while capturing.
 * @param[in] image The image data of the captured picture
 * @param[in] postview The image data of the "postview"
 * @param[in] thumbnail The image data of the thumbnail (it should be NULL
 * if the available thumbnail data does not exist)
 * @param[in] user_data The user data passed from the callback registration
 * function
 */
static void _main_view_capture_cb(camera_image_data_s *image,
		camera_image_data_s *postview, camera_image_data_s *thumbnail,
		void *user_data)
{
	FILE *file = NULL;
	size_t size;
	char filename[PATH_MAX] = { '\0' };

	if (!s_info.camera_enabled) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Camera hasn't been initialised.");
		return;
	}

	view_pause();
	if (image->format == CAMERA_PIXEL_FORMAT_JPEG) {
		size = _main_view_get_file_path(filename, sizeof(filename));
		if (size == 0) {
			dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_get_filename() failed");
			return;
		}

		file = fopen(filename, "w+");
		if (!file) {
			dlog_print(DLOG_ERROR, LOG_TAG, "fopen() failed");
			return;
		}

		size = fwrite(image->data, image->size, 1, file);
		if (size != 1)
			dlog_print(DLOG_ERROR, LOG_TAG, "fwrite() failed");

		fclose(file);
		_main_view_thumbnail_set(filename);
	}
}

/**
 * @brief Callback called when the camera capturing completes.
 * @param[in] user_data The user data passed from the callback registration
 * function
 */
static void _main_view_capture_completed_cb(void *data)
{
	if (!s_info.camera_enabled) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Camera hasn't been initialised.");
		return;
	}
	view_resume();
}

/**
 * @brief Callback called every second until timer_count > 0
 * @param[in] data User data
 * @return ECORE_CALLBACK_RENEW on timer counter > 0 or ECORE_CALLBACK_CANCEL
 * on timer counter == 0
 */
static Eina_Bool _main_view_timer_cb(void *data)
{
	int handle = 0;
	s_info.timer_count = s_info.timer_count - 1;
	if (s_info.timer_count > 0) {
		wav_player_start(_get_resource_path(SOUND_COUNT), SOUND_TYPE_MEDIA, NULL, NULL, &handle);
		_main_view_timer_count_update();
		return ECORE_CALLBACK_RENEW;
	} else {
		_main_view_stop_timer();
		if (s_info.camera_enabled) {
			camera_start_capture(s_info.camera, _main_view_capture_cb,
					_main_view_capture_completed_cb, NULL);
		} else {
			dlog_print(DLOG_ERROR, LOG_TAG, "Camera hasn't been initialised.");
		}
		return ECORE_CALLBACK_CANCEL;
	}
}

/**
 * @brief Start new shutter timer counting
 */
static void _main_view_start_timer(void)
{
	s_info.timer_count = s_info.selected_timer_interval;
	_main_view_timer_count_update();

	if (s_info.timer == NULL) {
		s_info.timer = ecore_timer_add(1.0, _main_view_timer_cb, NULL);
		eext_object_event_callback_add(s_info.layout, EEXT_CALLBACK_BACK, _main_view_back_cb, NULL);
	}
}

/**
 * @brief Callback called on shutter button clicked event.
 * @param[in] data User data pointer
 * @param[in] obj Pointer to the Edje object where the signal comes from
 * @param[in] emission Signal's emission string
 * @param[in] source The signal's source
 */
static void _main_view_shutter_button_cb(void *data, Evas_Object *obj,
		const char *emission, const char *source)
{
	_main_view_start_timer();
}

/**
 * @brief Callback called on timer select.
 * @param[in] interval Pointer used as integer to select timer interval
 * @param[in] obj Pointer to the Edje object where the signal comes from
 * @param[in] emission Signal's emission string
 * @param[in] source The signal's source
 */
static void _main_view_timer_n_cb(void *interval, Evas_Object *obj, const char *emission, const char *source)
{
	s_info.selected_timer_interval = (int)interval;
}

/**
 * @brief Callback called for every available media folder.
 * @param[in] folder The handle to the media folder
 * @param[in] user_data The user data passed from the "foreach" function
 * @return true to continue with the next iteration of the loop, otherwise
 * false to break out of the loop
 * Iterates over a list of folders.
 */
static bool _folder_cb(media_folder_h folder, void *user_data)
{
	char *name = NULL;
	int ret = MEDIA_CONTENT_ERROR_NONE;
	int len = 0;

	ret = media_folder_get_name(folder, &name);
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] media_folder_get_name() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		return false;
	}

	len = strlen(CAMERA_DIRECTORY_NAME);

	if (strncmp(CAMERA_DIRECTORY_NAME, name, len) || len != strlen(name)) {
		free(name);
		return true;
	}

	ret = media_folder_get_path(folder, &s_info.media_content_folder);
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] media_folder_get_path() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		free(name);
		return false;
	}

	free(name);
	return false;
}

/**
 * @brief List all media folders from media content db
 * @return false in case of any problems, otherwise true
 */
static bool _list_media_folders(void)
{
	int ret = MEDIA_CONTENT_ERROR_NONE;;

	ret = media_content_connect();
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] media_content_connect() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		return false;
	}

	ret = media_folder_foreach_folder_from_db(NULL, _folder_cb, NULL);
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "[%s:%d] media_folder_foreach_folder_from_db() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		return false;
	}

	if (media_content_disconnect() != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Error occurred when disconnecting from media db!");
		return false;
	}

	return true;
}

static void _main_view_mode_button_cb(void)
{
	s_info.selected_mode_btn = s_info.selected_mode_btn ^ 1;
}

/**
 * @brief Register needed app/camera callbacks for program to work
 */
static void _main_view_register_cbs(void)
{
	evas_object_smart_callback_add(s_info.layout, EVENT_BACK, _main_view_back_cb, NULL);
	elm_object_signal_callback_add(s_info.layout, EVENT_TIMER_2_SELECTED, "*", _main_view_timer_n_cb, (void *)2);
	elm_object_signal_callback_add(s_info.layout, EVENT_TIMER_5_SELECTED, "*", _main_view_timer_n_cb, (void *)5);
	elm_object_signal_callback_add(s_info.layout, EVENT_TIMER_10_SELECTED, "*", _main_view_timer_n_cb, (void *)10);
	elm_object_signal_callback_add(s_info.layout, EVENT_SHUTTER_CLICKED, "*", _main_view_shutter_button_cb, NULL);

	elm_object_signal_callback_add(s_info.layout, "camera_btn_mode_clicked", "*", _main_view_mode_button_cb, NULL);
}

/*
 * @brief Add a new background to the conform.
 * @return Background object pointer on success, otherwise NULL
 */
static Evas_Object *_view_create_bg(void)
{
	Evas_Object *bg = elm_bg_add(s_info.conform);
	elm_object_part_content_set(s_info.conform, "elm.swallow.bg", bg);
	return bg;
}

/*
 * @brief Adding new view to parent object.
 * @return Main view layout on success, otherwise NULL
 */
static Evas_Object *_main_view_add(void)
{
	_list_media_folders();

	s_info.layout = elm_layout_add(s_info.navi);
	if (!s_info.layout) {
		dlog_print(DLOG_ERROR, LOG_TAG, "elm_layout_add() failed");
		return NULL;
	}
	elm_layout_theme_set(s_info.layout, GRP_MAIN, "application", "default");
	evas_object_event_callback_add(s_info.layout, EVAS_CALLBACK_FREE, _main_view_destroy_cb, NULL);

	elm_layout_file_set(s_info.layout, _get_resource_path(EDJ_MAIN), GRP_MAIN);
	elm_object_signal_emit(s_info.layout, "mouse,clicked,1", "timer_2");

	s_info.preview_canvas = evas_object_image_filled_add(evas_object_evas_get(s_info.layout));

	if (!s_info.preview_canvas) {
		dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_rect_create() failed");
		evas_object_del(s_info.layout);
		return NULL;
	}

	elm_object_part_content_set(s_info.layout, "elm.swallow.content", s_info.preview_canvas);

	s_info.camera_enabled = _main_view_init_camera();
	if (!s_info.camera_enabled) {
		dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_start_preview failed");
		_main_view_show_warning_popup(s_info.navi, STR_ERROR, "Camera initialization failed", STR_OK);
	}

	_main_view_thumbnail_load();
	_main_view_register_cbs();

	s_info.navi_item = elm_naviframe_item_push(s_info.navi, NULL, NULL, NULL, s_info.layout, NULL);
	elm_naviframe_item_title_enabled_set(s_info.navi_item, EINA_FALSE, EINA_FALSE);

	return s_info.layout;
}
