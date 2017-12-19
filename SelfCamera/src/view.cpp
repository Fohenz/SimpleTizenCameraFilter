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

#include "view.h"
#include "selfcamera.h"
#include "view_defines.h"
#include "landmark.h"
#include "imageutils.h"

#define COUNTER_STR_LEN 3
#define FILE_PREFIX "IMAGE"
#define STR_ERROR "Error"
#define STR_OK "OK"
#define STR_FILE_PROTOCOL "file://"
#define MAX_FILTER 14
#define MAX_STICKER 10
#define BUFLEN 256

static struct view_info {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *navi;
	Elm_Object_Item *navi_item;
	Evas_Object *layout;
	Evas_Object *popup;
	Evas_Object *preview_canvas;
	int nose[2];

	camera_h camera;
	Eina_Bool camera_enabled;
	std::vector<dlib::rectangle> faces; /* detected faces */
	char *media_content_folder;
	int selected_mode_btn;
	int height;
	int weight;
	int sticker;
	int filter;
	int cammode;
	int fin;
	int motion;
	int timer;
	Eina_Bool flag_capturing;
	Eina_Bool flag_facerunning;
}s_info =
{	.win = NULL,
	.conform = NULL,
	.navi = NULL,
	.navi_item = NULL,
	.layout = NULL,
	.popup = NULL,
	.preview_canvas = NULL,
	.camera = NULL,
	.camera_enabled = false,
	.media_content_folder = NULL,
	.selected_mode_btn = 0,
	.sticker = 0,
	.filter = 0,
	.fin = 0,
	.cammode = 1, // default front camera
	.motion = 0,
	.timer = 8,
	.flag_capturing = false,
	.flag_facerunning = false,
	.nose[0] = 0,
	.nose[1] = 0,
};

static Evas_Object *_app_navi_add(void);
static Evas_Object *_main_view_add(void);
static Evas_Object *_view_create_bg(void);
dlib::shape_predictor sp; /* shape predictor */
int resolution[2] = { 176, 144 };
static Evas_Object *sticker_btn;
static imageinfo imgarr[STICKER_NUM];

static void* enable_sticker(void* unused) {
	s_info.fin = 1;
	return NULL;
}

static void load_shape_predictor(void* unused1, Ecore_Thread *unused2) {
	/* Load shape predictor */
	const char* resource_path = app_get_resource_path();
	char *file_path = (char *) malloc(sizeof(char) * BUFLEN);

	/* Create a full path to get a shape predictor. */
	snprintf(file_path, BUFLEN, "%s%s", resource_path,
			"shape_predictor_68_face_landmarks.dat");

	dlib::deserialize(file_path) >> sp;
	ecore_main_loop_thread_safe_call_sync(enable_sticker, NULL);
	free(file_path);
}

static void end_func(void *data, Ecore_Thread *thread) {
	return;
}

/**
 * @brief Creates essential objects: window, conformant and layout.
 * @param[in] user data pointer
 * @return EINA_TRUE on success, EINA_FALSE on error
 */
Eina_Bool view_create(void *user_data) {
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

	_image_util_read_stickers(imgarr);

	edje_object_part_object_get(sticker_btn, "face");
	elm_object_disabled_set(sticker_btn, EINA_TRUE);

	/* Show the window after main view is set up */
	evas_object_show(s_info.win);

	ecore_thread_run(load_shape_predictor, end_func, NULL, NULL);

	return EINA_TRUE;
}

/**
 * @brief Creates a basic window named package.
 * @param[in] pkg_name Name of the window
 * @return Window pointer
 */
Evas_Object *view_create_win(const char *pkg_name) {
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
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win) {
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
	evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND,
	EVAS_HINT_EXPAND);
	elm_win_resize_object_add(win, conform);

	evas_object_show(conform);

	return conform;
}

/**
 * @brief Called on app_pause app event.
 * Stops camera preview.
 */
void view_pause(void) {
	camera_state_e cur_state = CAMERA_STATE_NONE;
	int res = camera_get_state(s_info.camera, &cur_state);

	if (res != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera state. Error: %d",
				res);
		return;
	}
	if (cur_state == CAMERA_STATE_PREVIEW) {
		camera_stop_face_detection(s_info.camera);
		camera_stop_preview(s_info.camera);
		s_info.faces.clear();
	}
}

/**
 * @brief Called on app_resume app event.
 * Starts camera preview.
 */
bool view_resume(void) {
	camera_state_e cur_state = CAMERA_STATE_NONE;
	int res = camera_get_state(s_info.camera, &cur_state);

	if (res != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera state. Error: %d",
				res);
		return false;
	}
	if (cur_state != CAMERA_STATE_PREVIEW) {
		/*
		 * * The following actions (stop->start -> stop -> start preview) are required
		 * * to deal with a bug related to the camera brightness changes
		 * * (without applying this workaround, after taking a photo,
		 * * the changes of the camera preview brightness are not visible).
		 */

		int error_code = camera_unset_preview_cb(s_info.camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_unset_preview_cb", error_code);
		}
		error_code = camera_stop_preview(s_info.camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_stop_preview", error_code);
		}
		error_code = camera_start_preview(s_info.camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_preview", error_code);
		}
		error_code = camera_stop_preview(s_info.camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_stop_preview", error_code);
		}

		error_code = camera_start_preview(s_info.camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_preview", error_code);
		}
		camera_start_focusing(s_info.camera, true);

		if (s_info.flag_facerunning == true) {
			camera_start_face_detection(s_info.camera, _camera_face_detected_cb,
					&s_info.faces);
		}

		if (s_info.sticker != 0) {
			camera_set_preview_cb(s_info.camera, _sticker_preview_callback,
					&s_info.faces);
		} else {
			camera_set_preview_cb(s_info.camera, _filter_preview_callback,
					&s_info.faces);
		}
	}
	return true;
}

/**
 * @brief Destroys window and frees its resources.
 */
void view_destroy(void) {
	if (s_info.win == NULL)
		return;

	evas_object_del(s_info.win);
}

/**
 * @brief Get resource absolute path.
 * @param[in] file_path Resource path relative to resources directory
 * @return Resource absolute path
 */
static inline const char *_get_resource_path(const char *file_path) {
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
static void _app_navi_back_cb(void *data, Evas_Object *obj, void *event_info) {
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
static Evas_Object *_app_navi_add(void) {
	Evas_Object *navi = elm_naviframe_add(s_info.conform);
	if (!navi) {
		dlog_print(DLOG_ERROR, LOG_TAG, "elm_naviframe_add() failed");
		return NULL;
	}
	elm_naviframe_prev_btn_auto_pushed_set(navi, EINA_FALSE);

	eext_object_event_callback_add(navi, EEXT_CALLBACK_BACK, _app_navi_back_cb,
	NULL);
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
static void _main_view_destroy_cb(void *data, Evas *e, Evas_Object *obj,
		void *event_info) {
	view_pause();
	camera_destroy(s_info.camera);
}

/**
 * @brief Rotate image preview according to display rotate.
 * @param[in] camera The Camera handle
 * @param[in] lens_orientation Rotation from lens
 */
static void _main_view_rotate_image_preview(camera_h camera,
		int lens_orientation) {
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
static void _main_view_set_image_rotation_exif(camera_h camera,
		int lens_orientation) {
	camera_attr_tag_orientation_e orientation =
			CAMERA_ATTR_TAG_ORIENTATION_TOP_RIGHT;

	if (camera_attr_enable_tag(camera, true) != CAMERA_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"Cannot enable write of EXIF information");
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
static Eina_Bool _main_view_init_camera(void) {
	int lens_orientation = 0;
	int result = camera_create(CAMERA_DEVICE_CAMERA0, &s_info.camera);
	if (result != CAMERA_ERROR_NONE || !s_info.preview_canvas)
		return false;

	result = camera_set_display(s_info.camera, CAMERA_DISPLAY_TYPE_EVAS,
			GET_DISPLAY(s_info.preview_canvas));
	if (result != CAMERA_ERROR_NONE || !s_info.preview_canvas)
		return false;

	camera_set_display_mode(s_info.camera, CAMERA_DISPLAY_MODE_LETTER_BOX);
	camera_set_display_visible(s_info.camera, true);
	/*
	 if (camera_attr_get_lens_orientation(s_info.camera, &lens_orientation)
	 == CAMERA_ERROR_NONE) { */
	/* Rotate video preview to compensate different lens
	 * orientations */
	//_main_view_rotate_image_preview(s_info.camera, lens_orientation);
	/* Rotate captured image to compensate different lens
	 * orientations */
	/*
	 _main_view_set_image_rotation_exif(s_info.camera, lens_orientation);

	 camera_set_display_flip(s_info.camera, CAMERA_FLIP_HORIZONTAL);
	 } else {
	 dlog_print(DLOG_ERROR, LOG_TAG, "Cannot get camera lens attribute");
	 }
	 */
	result = camera_set_preview_resolution(s_info.camera, resolution[0],
			resolution[1]);
	if (CAMERA_ERROR_NONE != result) {
		DLOG_PRINT_ERROR("camera_set_preview_resolution", result);
	}
	return view_resume();
}

/**
 * @brief Check if dir->d_name has IMAGE prefix.
 * @param[in] dir "dirent" structure with d_name to check
 * @return strncmp String compare result (0 on equal)
 */
static int _main_view_image_file_filter(const struct dirent *dir) {
	return strncmp(dir->d_name, FILE_PREFIX, sizeof(FILE_PREFIX) - 1) == 0;

}

/**
 * @brief Gets the last file with IMAGE prefix on given path.
 * @param[out] file_path Full image path output
 * @param[in] size Maximum path string length
 * @return Returned file_path length
 */
static size_t _main_view_get_last_file_path(char *file_path, size_t size) {
	int n = -1;
	struct dirent **namelist = NULL;
	int ret_size = 0;

	n = scandir(s_info.media_content_folder, &namelist,
			_main_view_image_file_filter, alphasort);

	if (n > 0) {
		ret_size = snprintf(file_path, size, "%s/%s",
				s_info.media_content_folder, namelist[n - 1]->d_name);

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
static void _main_view_popup_close_cb(void *data, Evas_Object *obj,
		void *event_info) {
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
		const char *caption, const char *text, const char *button_text) {
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
	evas_object_smart_callback_add(button, "clicked", _main_view_popup_close_cb,
	NULL);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK,
			_main_view_popup_close_cb, NULL);

	s_info.popup = popup;
}

/**
 * @brief Smart callback on thumbnail click.
 * @param[in] data User data
 * @param[in] obj Target object
 * @param[in] event_info Event information
 */
static void _main_view_thumbnail_click_cb(void *data, Evas_Object *obj,
		void *event_info) {
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

	strncpy(file_path_prepared, STR_FILE_PROTOCOL,
	PATH_MAX + sizeof(STR_FILE_PROTOCOL));
	strncat(file_path_prepared, file_path,
	PATH_MAX + sizeof(STR_FILE_PROTOCOL));
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
		dlog_print(DLOG_ERROR, LOG_TAG,
				"app_control_send_launch_request() failed.");

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
static void _main_view_thumbnail_set(const char *file_path) {
	Evas_Object *img = elm_object_part_content_get(s_info.layout, "thumbnail");
	if (!img) {
		img = elm_image_add(s_info.layout);
		elm_object_part_content_set(s_info.layout, "thumbnail", img);
		evas_object_smart_callback_add(img, "clicked",
				_main_view_thumbnail_click_cb, NULL);
	}
	elm_image_file_set(img, file_path, NULL);
	elm_object_signal_emit(s_info.layout, "default", "thumbnail_background");
}

/**
 * @brief Load last file thumbnail.
 */
static void _main_view_thumbnail_load(void) {
	char file_path[PATH_MAX] = { '\0' };
	if (_main_view_get_last_file_path(file_path, sizeof(file_path)))
		_main_view_thumbnail_set(file_path);
	else
		elm_object_signal_emit(s_info.layout, "no_image",
				"thumbnail_background");
}
/**
 * @brief Generate image file name with full path and IMAGE prefix from current
 * date and time.
 * @param[out] file_path Output c-string array
 * @param[in] size Maximum file_path length
 * @return Generated file_path length
 */
static size_t _main_view_get_file_path(char *file_path, size_t size) {
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
			s_info.media_content_folder, FILE_PREFIX, localtime.tm_year + 1900,
			localtime.tm_mon + 1, localtime.tm_mday, localtime.tm_hour,
			localtime.tm_min, localtime.tm_sec);

	return chars;
}

static void _main_view_mycapture_cb(camera_preview_data_s *frame) {
	int error_code;
	size_t size;
	char filename[PATH_MAX] = { '\0' };

	if (!s_info.camera_enabled) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Camera hasn't been initialized.");
		return;
	}

	for (int i = 0; i < frame->data.double_plane.uv_size; i += 2) {
		unsigned char tmp = 0;
		tmp = frame->data.double_plane.uv[i];
		frame->data.double_plane.uv[i] = frame->data.double_plane.uv[i + 1];
		frame->data.double_plane.uv[i + 1] = tmp;
	}

	view_pause();
	if (frame->format == CAMERA_PIXEL_FORMAT_NV12) {
		size = _main_view_get_file_path(filename, sizeof(filename));
		if (size == 0) {
			dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_get_filename() failed");
			return;
		}
		error_code = image_util_encode_jpeg(frame->data.encoded_plane.data,
				frame->width, frame->height, IMAGE_UTIL_COLORSPACE_NV12, 100,
				filename);
		if (error_code != IMAGE_UTIL_ERROR_NONE) {
			dlog_print(DLOG_ERROR, LOG_TAG,
					"image_util_encode_jpeg() failed with %d", error_code);
			return;
		}

		_main_view_thumbnail_set(filename);
	}
	view_resume();
}

/**
 * @brief Callback called on shutter button clicked event.
 * @param[in] data User data pointer
 * @param[in] obj Pointer to the Edje object where the signal comes from
 * @param[in] emission Signal's emission string
 * @param[in] source The signal's source
 */
static void _main_view_shutter_button_cb(void *data, Evas_Object *obj,
		const char *emission, const char *source) {
	if (s_info.camera_enabled) {
		s_info.flag_capturing = true;
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG, "Camera hasn't been initialized.");
	}
}

/**
 * @brief Callback called for every available media folder.
 * @param[in] folder The handle to the media folder
 * @param[in] user_data The user data passed from the "foreach" function
 * @return true to continue with the next iteration of the loop, otherwise
 * false to break out of the loop
 * Iterates over a list of folders.
 */
static bool _folder_cb(media_folder_h folder, void *user_data) {
	char *name = NULL;
	int ret = MEDIA_CONTENT_ERROR_NONE;
	int len = 0;

	ret = media_folder_get_name(folder, &name);
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"[%s:%d] media_folder_get_name() error: %s",
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
		dlog_print(DLOG_ERROR, LOG_TAG,
				"[%s:%d] media_folder_get_path() error: %s",
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
static bool _list_media_folders(void) {
	int ret = MEDIA_CONTENT_ERROR_NONE;
	;

	ret = media_content_connect();
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"[%s:%d] media_content_connect() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		return false;
	}

	ret = media_folder_foreach_folder_from_db(NULL, _folder_cb, NULL);
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"[%s:%d] media_folder_foreach_folder_from_db() error: %s",
				__FILE__, __LINE__, get_error_message(ret));
		return false;
	}

	if (media_content_disconnect() != MEDIA_CONTENT_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"Error occurred when disconnecting from media db!");
		return false;
	}

	return true;
}

static void _camera_face_detected_cb(camera_detected_face_s* faces, int count,
		void* user_data) {
	if (!s_info.faces.empty())
		s_info.faces.clear();

	/* convert camera_detected_face_s faces to std::vector<rectangle> faces */
	for (int i = 0; i < count; i++) {
		dlib::rectangle face;
		face.set_top(faces[i].x);
		face.set_bottom(faces[i].x + faces[i].height);
		face.set_right(resolution[1] - faces[i].y);
		face.set_left(resolution[1] - faces[i].y - faces[i].width);
		s_info.faces.push_back(face);
	}

}

void face_landmark(camera_preview_data_s *frame, int count) {
	dlib::array2d<u_int64_t> img;
	img.set_size(frame->width, frame->height);
	if (frame->data.double_plane.y_size != frame->width * frame->height) {
		return;
	}

	for (int i = 0; i < frame->data.double_plane.y_size; i++) {
		// img[i % frame->width][ i / frame->width] = (frame->data.double_plane.y)[i];
		img[i % frame->width][frame->height - i / frame->width - 1] =
				(frame->data.double_plane.y)[i];
	}

	s_info.timer--;
	if (s_info.timer < 0)
		s_info.timer = 8;
	// Now we will go ask the shape_predictor to tell us the pose of
	// each face we detected.
	for (unsigned long i = 0; i < count; ++i) {
		dlib::full_object_detection shape = sp(img, s_info.faces[i]);

		if (s_info.motion && i == 0) {
			if (s_info.timer == 8) {
				if (s_info.nose[0] != 0 && s_info.nose[1] != 0) {
					int Vx = shape.part(33)(0) - s_info.nose[0];

					if (Vx < 0)
						Vx *= -1;
					int val = s_info.faces[i].width();
					if (Vx > val / 12) {
						s_info.sticker = (s_info.sticker + 2);
						if(s_info.sticker > MAX_STICKER)
						{
							s_info.sticker = 0;
							camera_stop_face_detection(s_info.camera);
							s_info.flag_facerunning = false;
						}
					}
				}
				int H = shape.part(51)(1) - shape.part(57)(1);
				if(H < 0)
					H *= -1;
				if(H > s_info.faces[i].height()/6)
					s_info.flag_capturing = true;
			}
			s_info.nose[0] = shape.part(33)(0);
			s_info.nose[1] = shape.part(33)(1);
		}

		switch (s_info.sticker) {
		case 2:
			draw_nyan(frame, shape, imgarr);
			break;
		case 4:
			draw_rudolph(frame, shape, imgarr);
			break;
		case 6:
			draw_santa(frame, shape, imgarr);
			break;
		case 8:
			draw_landmark(frame, shape);
			break;
		default:
			break;
		}
	}
}

void _sticker_preview_callback(camera_preview_data_s *frame, void *user_data) {
	if (frame->format == CAMERA_PIXEL_FORMAT_NV12
			&& frame->num_of_planes == 2) {

		if (s_info.flag_facerunning) {
			std::vector<dlib::rectangle> buf =
					*((std::vector<dlib::rectangle>*) user_data);
			size_t count = buf.size();

			/* get face landmark */
			if (count > 0) {
				face_landmark(frame, count);
			}
		}

		if (s_info.flag_capturing) {
			s_info.flag_capturing = false;
			_main_view_mycapture_cb(frame);
		}
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"This preview frame format is not supported!");
		//we do nothing, the preview is left intact and displayed without modifications
	}
}

/*
 void filter_1977(camera_preview_data_s *frame) {
 //apply_filter(20, 10, 0);
 //for (int i = 0; i < frame->data.double_plane.uv_size; i++)
 //frame->data.double_plane.uv[i] = 128;
 for (int i = 0; i < frame->data.double_plane.uv_size; i++) {
 int val = frame->data.double_plane.uv[i];
 if (i % 2) { // Cr
 val = val*0.9;
 if (val > 255)
 val = 255;
 if (val < 0)
 val = 0;
 frame->data.double_plane.uv[i] = val;
 } else { //Cb
 val = val*1.1;
 if (val > 255)
 val = 255;
 if (val < 0)
 val = 0;
 frame->data.double_plane.uv[i] = val;
 }
 }
 }*/

void apply_filter(camera_preview_data_s* frame, double Cb, double Cr) {
	camera_attr_set_effect(s_info.camera, CAMERA_ATTR_EFFECT_NONE);
	for (int i = 0; i < frame->data.double_plane.uv_size; i++) {
		int val = frame->data.double_plane.uv[i];
		if (i % 2) { // Cr
			val = val * Cr;
			if (val > 255)
				val = 255;
			if (val < 0)
				val = 0;
			frame->data.double_plane.uv[i] = val;
		} else { //Cb
			val = val * Cb;
			if (val > 255)
				val = 255;
			if (val < 0)
				val = 0;
			frame->data.double_plane.uv[i] = val;
		}
	}

}

void _filter_preview_callback(camera_preview_data_s *frame, void* user_data) {
	if (frame->format == CAMERA_PIXEL_FORMAT_NV12
			&& frame->num_of_planes == 2) {

		switch (s_info.filter) {
		case 4: // red1
			apply_filter(frame, 0.95, 1.05);
			break;
		case 5:
			apply_filter(frame, 0.9, 1.07);
			break;
		case 6:
			apply_filter(frame, 0.85, 1.1);
			break;
		case 7: // blue
			apply_filter(frame, 1.05, 0.95);
			break;
		case 8:
			apply_filter(frame, 1.07, 0.9);
			break;
		case 9:
			apply_filter(frame, 1.1, 0.85);
			break;
		case 10: // green
			apply_filter(frame, 0.97, 0.96);
			break;
		case 11:
			apply_filter(frame, 0.95, 0.95);
			break;
		case 12:
			apply_filter(frame, 0.93, 0.93);
			break;
		}

		if (s_info.flag_capturing) {
			s_info.flag_capturing = false;
			_main_view_mycapture_cb(frame);
		}

	} else {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"This preview frame format is not supported!");
	}
}

static void _main_view_effect_button_cb(void) {
	s_info.filter = (++s_info.filter) % MAX_FILTER;

	camera_state_e state;
	camera_get_state(s_info.camera, &state);
	if (CAMERA_STATE_PREVIEW == state) {
		if (s_info.flag_facerunning == true) {
			camera_stop_face_detection(s_info.camera);
			s_info.flag_facerunning = false;

			if (s_info.sticker != 0) {
				camera_unset_preview_cb(s_info.camera);
				int error_code = camera_set_preview_cb(s_info.camera,
						_filter_preview_callback, &s_info.faces);
				if (CAMERA_ERROR_NONE != error_code) {
					DLOG_PRINT_ERROR("camera_set_preview_cb", error_code);
				}
			}
		}
	}
	switch (s_info.filter) {
	case 0:
		camera_attr_set_effect(s_info.camera, CAMERA_ATTR_EFFECT_NONE);
		break;
	case 1:
		camera_attr_set_effect(s_info.camera, CAMERA_ATTR_EFFECT_MONO);
		break;
	case 2:
		camera_attr_set_effect(s_info.camera, CAMERA_ATTR_EFFECT_NEGATIVE);
		break;
	case 3:
		camera_attr_set_effect(s_info.camera, CAMERA_ATTR_EFFECT_SEPIA);
		break;
	default:
		break;
	}

}

static void _main_view_sticker_button_cb(void) {
	int error_code;

	//sp is not unloaded
	if (s_info.fin != 1) {
		return;
	}

	s_info.sticker = (++s_info.sticker) % MAX_STICKER;

	if (s_info.sticker != 0) {
		camera_unset_preview_cb(s_info.camera);
		int error_code = camera_set_preview_cb(s_info.camera,
				_sticker_preview_callback, &s_info.faces);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_set_preview_cb", error_code);
		}
	}

	if (s_info.sticker == 0 && s_info.flag_facerunning == true) {
		camera_stop_face_detection(s_info.camera);
		s_info.flag_facerunning = false;
	} else if (s_info.flag_facerunning == false) {
		error_code = camera_start_face_detection(s_info.camera,
				_camera_face_detected_cb, &s_info.faces);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_face_detection error", error_code);
			return;
		}
		s_info.flag_facerunning = true;
	}
}

static void _main_view_change_button_cb() {
	s_info.cammode = s_info.cammode ^ 1;
}

static void _main_view_motion_button_cb() {
	if (s_info.filter != 0)
		s_info.motion = 0;
	else
		s_info.motion = s_info.motion ^ 1;
}

/**
 * @brief Register needed app/camera callbacks for program to work
 */
static void _main_view_register_cbs(void) {
	elm_object_signal_callback_add(s_info.layout, EVENT_SHUTTER_CLICKED, "*",
			_main_view_shutter_button_cb, NULL);

	elm_object_signal_callback_add(s_info.layout, "camera_effect_clicked", "*",
			(Edje_Signal_Cb) _main_view_effect_button_cb, NULL);
	elm_object_signal_callback_add(s_info.layout, "camera_sticker_clicked", "*",
			(Edje_Signal_Cb) _main_view_sticker_button_cb, NULL);
	elm_object_signal_callback_add(s_info.layout, "camera_change_clicked", "*",
			(Edje_Signal_Cb) _main_view_change_button_cb, NULL);
	elm_object_signal_callback_add(s_info.layout, "camera_motion_clicked", "*",
			(Edje_Signal_Cb) _main_view_motion_button_cb, NULL);
}

/*
 * @brief Add a new background to the conform.
 * @return Background object pointer on success, otherwise NULL
 */
static Evas_Object *_view_create_bg(void) {
	Evas_Object *bg = elm_bg_add(s_info.conform);
	elm_object_part_content_set(s_info.conform, "elm.swallow.bg", bg);
	return bg;
}

/*
 * @brief Adding new view to parent object.
 * @return Main view layout on success, otherwise NULL
 */
static Evas_Object *_main_view_add(void) {
	_list_media_folders();

	s_info.layout = elm_layout_add(s_info.navi);
	if (!s_info.layout) {
		dlog_print(DLOG_ERROR, LOG_TAG, "elm_layout_add() failed");
		return NULL;
	}
	elm_layout_theme_set(s_info.layout, GRP_MAIN, "application", "default");
	evas_object_event_callback_add(s_info.layout, EVAS_CALLBACK_FREE,
			_main_view_destroy_cb, NULL);

	elm_layout_file_set(s_info.layout, _get_resource_path(EDJ_MAIN), GRP_MAIN);
	//elm_object_signal_emit(s_info.layout, "mouse,clicked,1", "timer_2");

	s_info.preview_canvas = evas_object_image_filled_add(
			evas_object_evas_get(s_info.layout));

	if (!s_info.preview_canvas) {
		dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_rect_create() failed");
		evas_object_del(s_info.layout);
		return NULL;
	}

	elm_object_part_content_set(s_info.layout, "elm.swallow.content",
			s_info.preview_canvas);

	s_info.camera_enabled = _main_view_init_camera();
	if (!s_info.camera_enabled) {
		dlog_print(DLOG_ERROR, LOG_TAG, "_main_view_start_preview failed");
		_main_view_show_warning_popup(s_info.navi, STR_ERROR,
				"Camera initialization failed", STR_OK);
	}

	_main_view_thumbnail_load();
	_main_view_register_cbs();

	s_info.navi_item = elm_naviframe_item_push(s_info.navi, NULL, NULL, NULL,
			s_info.layout, NULL);
	elm_naviframe_item_title_enabled_set(s_info.navi_item, EINA_FALSE,
	EINA_FALSE);

	return s_info.layout;
}
