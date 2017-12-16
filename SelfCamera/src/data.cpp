/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "main.h"
#include "data.h"
#include "landmark.h"
#include "imageutils.h"

typedef struct _camdata {
	camera_h g_camera; /* Camera handle */
	std::vector<dlib::rectangle> faces; /* detected faces */
	dlib::shape_predictor sp; /* shape predictor */

	Evas_Object *cam_display;
	Evas_Object *cam_display_box;
	Evas_Object *display;
	Evas_Object *preview_bt;
	Evas_Object *zoom_bt;
	Evas_Object *brightness_bt;
	Evas_Object *photo_bt;

	Evas_Object *filter_bt;
	Evas_Object *sticker_bt;bool cam_prev;
	int filter;
	int sticker;
	int width;
	int height;
	int count;
} camdata;

static imageinfo imgarr[10];
static camdata cam_data;
//static rgbmat rgb_frame;

static char *camera_directory = NULL;

/**
 * @brief Maps the given camera state to its string representation.
 *
 * @param state  The camera state that should be mapped to its literal
 *               representation
 *
 * @return The string representation of the given camera state
 */
static const char *_camera_state_to_string(camera_state_e state) {

	switch (state) {
	case CAMERA_STATE_NONE:
		return "CAMERA_STATE_NONE";

	case CAMERA_STATE_CREATED:
		return "CAMERA_STATE_CREATED";

	case CAMERA_STATE_PREVIEW:
		return "CAMERA_STATE_PREVIEW";

	case CAMERA_STATE_CAPTURING:
		return "CAMERA_STATE_CAPTURING";

	case CAMERA_STATE_CAPTURED:
		return "CAMERA_STATE_CAPTURED";

	default:
		return "Unknown";
	}
}

/**
 * @brief Gets the ID of the internal storage.
 * @details It assigns the get ID to the variable passed as the user data
 *          to the callback. This callback is called for every storage supported
 *          by the device.
 * @remarks This function matches the storage_device_supported_cb() signature
 *          defined in the storage-expand.h header file.
 *
 * @param storage_id  The unique ID of the detected storage
 * @param type        The type of the detected storage
 * @param state       The current state of the detected storage.
 *                    This argument is not used in this case.
 * @param path        The absolute path to the root directory of the detected
 *                    storage. This argument is not used in this case.
 * @param user_data   The user data passed via void pointer
 *
 * @return @c true to continue iterating over supported storages, @c false to
 *         stop the iteration.
 */
static bool _storage_cb(int storage_id, storage_type_e type,
		storage_state_e state, const char *path, void *user_data) {
	if (STORAGE_TYPE_INTERNAL == type) {
		int *internal_storage_id = (int *) user_data;

		if (NULL != internal_storage_id)
			*internal_storage_id = storage_id;

		/* Internal storage found, stop the iteration. */
		return false;
	} else {
		/* Continue iterating over storages. */
		return true;
	}
}

/**
 * @brief Retrieves all supported camera preview resolutions.
 * @details Called for every preview resolution that is supported by the device.
 * @remarks This function matches the camera_supported_preview_resolution_cb()
 *          signature defined in the camera.h header file.
 *
 * @param width       The preview image width
 * @param height      The preview image height
 * @param user_data   The user data passed from
 *                    the camera_supported_preview_resolution_cb() function
 *
 * @return @c true to continue with the next iteration of the loop,
 *         otherwise @c false to break out of the loop
 */
static bool _preview_resolution_cb(int width, int height, void *user_data) {
	if (NULL != user_data) {
		int *resolution = (int *) user_data;
		resolution[0] = width;
		resolution[1] = height;
		// resolution 176 144
		return false;

		// resolution 480
		if (width > 400)
			return false;
	}

	return true;
}

/**
 * @brief Called to get the information about image data taken by the camera
 *        once per frame while capturing.
 * @details Called when image capturing is finished.
 * @remarks This function matches the camera_capture_completed_cb() signature
 *          defined in the camera.h header file.
 *
 * @param user_data  The user data passed from the callback registration
 *                   function. This argument is not used in this case.
 */

static void _supported_preview_format_cb(camera_pixel_format_e format,
		void *user_data) {
	dlog_print(DLOG_DEBUG, LOG_TAG, "%d", format);
}

static void _camera_completed_cb(void *user_data) {
	/* Start the camera preview again. */
	int error_code = camera_start_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_start_preview", error_code);
		PRINT_MSG("Could not restart the camera preview.");
	}

	/*
	 * The following actions (start -> stop -> start preview) are required
	 * to deal with a bug related to the camera brightness changes
	 * (Without applying this workaround, after taking a photo,
	 * the changes of the camera preview brightness are not visible).
	 */
	error_code = camera_stop_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_stop_preview", error_code);
		PRINT_MSG("Could not stop the camera preview.");
	}

	error_code = camera_start_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_start_preview", error_code);
		PRINT_MSG("Could not restart the camera preview.");
	}

	/*
	 * The following actions (start -> stop -> start preview) are required
	 * to deal with a bug related to the camera brightness changes
	 * (without applying this workaround, after taking a photo,
	 * the changes of the camera preview brightness are not visible).
	 */
	error_code = camera_stop_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_stop_preview", error_code);
		PRINT_MSG("Could not stop the camera preview.");
	}

	error_code = camera_start_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_start_preview", error_code);
		PRINT_MSG("Could not restart the camera preview.");
	}
}

/**
 * @brief Called when the image is saved.
 * @remarks This function matches the Ecore_Cb() signature defined in the
 *          Ecore_Legacy.h header file.
 *
 * @param data  The user data passed via void pointer
 */
static void _image_saved(void *data) {
	PRINT_MSG("Image stored in the %s", (char * ) data);
	free(data);
}

/**
 * @brief Called to get information about image data taken by the camera
 *        once per frame while capturing.
 * @remarks This function matches the camera_capturing_cb() signature defined in
 *          the camera.h header file.
 *
 * @param image      The image data of the captured picture
 * @param postview   The image data of the post view. This argument is not used
 *                   in this case.
 * @param thumbnail  The image data of the thumbnail (it should be @c NULL if
 *                   the available thumbnail data does not exist). This argument
 *                   is not used in this case.
 * @param user_data  The user data passed from the callback registration
 *                   function. This argument is not used in this case.
 */
static void _camera_capturing_cb(camera_image_data_s *image,
		camera_image_data_s *postview, camera_image_data_s *thumbnail,
		void *user_data) {
	if (NULL != image && NULL != image->data) {
		dlog_print(DLOG_DEBUG, LOG_TAG, "Writing image to file.");

		char *file_path = (char *) malloc(sizeof(char) * BUFLEN);

		/* Create a full path to newly created file for storing the taken photo. */
		snprintf(file_path, BUFLEN, "%s/cam%d.jpg", camera_directory,
				(int) time(NULL));

		/* Open the file for writing. */
		FILE *file = fopen(file_path, "w+");

		/* Write the image to a file. */
		fwrite(image->data, 1, image->size, file);

		/* Close the file. */
		fclose(file);

		ecore_job_add(_image_saved, (void *) file_path);
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"An error occurred during taking the photo. The image is NULL.");
	}
}

/**
 * @brief Checks the camera focus state.
 * @details Callback function invoked when the camera focusing state changes.
 * @remarks This function matches the camera_focus_changed_cb() signature
 *          defined in the camera.h header file.
 *
 * @param state      The current state of the auto-focus
 * @param user_data  The user data passed from the callback registration
 *                   function. This argument is not used in this case.
 */
static void _camera_focus_cb(camera_focus_state_e state, void *user_data) {
	if (CAMERA_FOCUS_STATE_FOCUSED == state) {
		/* Take a photo. */
		int error_code = camera_start_capture(cam_data.g_camera,
				_camera_capturing_cb, _camera_completed_cb,
				NULL);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_capture", error_code);
			PRINT_MSG("Could not start taking a photo.");
		}
	}
}

/**
 * @brief Takes a photo.
 * @details Called when the "Take a photo" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void __camera_cb_photo(void *data, Evas_Object *obj, void *event_info) {
	/* Focus the camera on the current view. */
	int error_code = camera_start_focusing(cam_data.g_camera, false);
	if (CAMERA_ERROR_NONE != error_code) {
		if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
			DLOG_PRINT_ERROR("camera_start_focusing", error_code);
			PRINT_MSG("Focusing is not supported on this device."
					" The picture will be taken without focusing.");
		} else {
			dlog_print(DLOG_INFO, LOG_TAG,
					"Focusing is not supported on this device."
							" The picture will be taken without focusing.");
		}

		/*
		 * Take a photo (If the focusing is not supported, then just take a
		 * photo, without focusing).
		 */
		error_code = camera_start_capture(cam_data.g_camera,
				_camera_capturing_cb, _camera_completed_cb,
				NULL);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_capture", error_code);
			PRINT_MSG("Could not start capturing the photo.");
		}
	}
}

/**
 * @brief Sets the camera brightness.
 * @details Called when the "Brightness" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void __camera_cb_bright(void *data, Evas_Object *obj, void *event_info) {
	/*
	 * Get the minimal and maximal supported value for the camera brightness
	 * attribute.
	 */
	int min, max;

	int error_code = camera_attr_get_brightness_range(cam_data.g_camera, &min,
			&max);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_brightness_range", error_code);
		PRINT_MSG("Could not get brightness range.");
	}

	/* Get the current value of the camera brightness attribute. */
	int brightness_level;

	error_code = camera_attr_get_brightness(cam_data.g_camera,
			&brightness_level);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_brightness", error_code);
		PRINT_MSG("Could not get current brightness value.");
	}

	/* Set new value of the camera brightness attribute */
	brightness_level = brightness_level == max ? min : ++brightness_level;
	error_code = camera_attr_set_brightness(cam_data.g_camera,
			brightness_level);
	if (CAMERA_ERROR_NONE != error_code) {
		if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
			DLOG_PRINT_ERROR("camera_attr_set_brightness", error_code);
			PRINT_MSG("Could not set new brightness value.");
		} else
			dlog_print(DLOG_INFO, LOG_TAG,
					"Camera brightness is not supported on this device.");
	} else
		PRINT_MSG("Brightness value set to %d", brightness_level);
}

/**
 * @brief Sets the camera zoom.
 * @details Called when the "Zoom" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void __camera_cb_zoom(void *data, Evas_Object *obj, void *event_info) {
	/* Get the minimal and maximal supported value for the camera zoom attribute. */
	int min, max;

	int error_code = camera_attr_get_zoom_range(cam_data.g_camera, &min, &max);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_zoom_range", error_code);
		PRINT_MSG("Could not get zoom range.");
	}

	/* Get the current value of the camera zoom attribute. */
	int zoom;

	error_code = camera_attr_get_zoom(cam_data.g_camera, &zoom);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_zoom", error_code);
		PRINT_MSG("Could not get current zoom value.");
	}

	/* Set new value of the camera zoom attribute */
	zoom = zoom == max ? min : ++zoom;
	error_code = camera_attr_set_zoom(cam_data.g_camera, zoom);
	if (CAMERA_ERROR_NONE != error_code) {
		if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
			DLOG_PRINT_ERROR("camera_attr_set_zoom", error_code);
			PRINT_MSG("Could not set new zoom value.");
		} else {
			dlog_print(DLOG_INFO, LOG_TAG,
					"Camera zoom is not supported on this device.");
			PRINT_MSG("Camera zoom is not supported on this device.");
		}
	} else
		PRINT_MSG("Zoom value set to %d", zoom);
}

/**
 * @brief Starts the camera preview.
 * @details Called when the "Start preview" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void __camera_cb_preview(void *data, Evas_Object *obj,
		void *event_info) {
	int error_code = CAMERA_ERROR_NONE;

	if (!cam_data.cam_prev) {
		/* Show the camera preview UI element. */
		evas_object_size_hint_weight_set(cam_data.display, EVAS_HINT_EXPAND,
		EVAS_HINT_EXPAND);
		evas_object_size_hint_weight_set(cam_data.cam_display_box,
		EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_show(cam_data.cam_display_box);

		/* Start the camera preview. */
		error_code = camera_start_preview(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_preview", error_code);
			PRINT_MSG("Could not start the camera preview.");
			return;
		}

		PRINT_MSG("Camera preview started.");
		cam_data.cam_prev = true;

		elm_object_text_set(cam_data.preview_bt, "Stop preview");

		/* Enable other camera buttons. */
		elm_object_disabled_set(cam_data.zoom_bt, EINA_FALSE);
		elm_object_disabled_set(cam_data.brightness_bt, EINA_FALSE);
		elm_object_disabled_set(cam_data.photo_bt, EINA_FALSE);
		elm_object_disabled_set(cam_data.filter_bt, EINA_FALSE);
		elm_object_disabled_set(cam_data.sticker_bt, EINA_FALSE);
	} else {
		/* Hide the camera preview UI element. */
		evas_object_size_hint_weight_set(cam_data.display, EVAS_HINT_EXPAND,
				0.0);
		evas_object_size_hint_weight_set(cam_data.cam_display_box,
		EVAS_HINT_EXPAND, 0.0);
		evas_object_hide(cam_data.cam_display_box);

		/* Stop the camera preview. */
		error_code = camera_stop_preview(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_stop_preview", error_code);
			PRINT_MSG("Could not stop the camera preview.");
			return;
		}

		PRINT_MSG("Camera preview stopped.");
		cam_data.cam_prev = false;

		elm_object_text_set(cam_data.preview_bt, "Start preview");

		/* Disable other camera buttons. */
		elm_object_disabled_set(cam_data.zoom_bt, EINA_TRUE);
		elm_object_disabled_set(cam_data.brightness_bt, EINA_TRUE);
		elm_object_disabled_set(cam_data.photo_bt, EINA_TRUE);
	}
}

/**
 * @brief Get the range of filter.
 * @details Called when the "Filter" button is clicked.
 *
 * @param cam_data    The user data.
 * @param min         Minimum number of filter. default is 0
 * @param max  		  Maximum number of filter.
 */
static int camera_attr_get_filter_range(int *min, int *max) {
	*min = 0;
	*max = MAX_FILTER;
	return 0;
}

/**
 * @brief Get the current filter number.
 * @details Called when the "Sticker" button is clicked.
 *
 * @param cam_data    The user data.
 * @param min         Minimum number of filter. default is 0
 * @param max  		  Maximum number of filter.
 */
static int camera_attr_get_filter(int* filter) {
	*filter = cam_data.filter;
	return 0;
}

static int camera_attr_set_filter(int filter) {
	cam_data.filter = filter;
	return 0;
}

void _chroma_mod_sepia(unsigned char* data, uint64_t size) {
	uint64_t i = 0;

	for (i = 0; i < size; i++) {
		int Cval = (int) (data[i]);

		if (i % 2 == 0) //Even byte reflects Cb chroma value
				{
			Cval = 114; //Setting the Cb value to look like sepia
		} else //Odd byte reflects Cr chroma value
		{
			Cval = 144; //Setting the Cr value to look like sepia
		}
		data[i] = (unsigned char) Cval;
	}
}

void _grayscale_mod(unsigned char* data, uint64_t size) {
	uint64_t i = 0;
	for (i = 0; i < size; i++) {
		data[i] = 128;
	}
}

void _invert_mod(unsigned char* data, uint64_t size) {
	uint_fast64_t i = 0; // can't find difference between uint64 vs fast64
	for (i = 0; i < size; i++) {
		data[i] = 255 - data[i];
	}
}

void _nored_mod(unsigned char* data, uint64_t size) {
	uint_fast64_t i = 0; // can't find difference between uint64 vs fast64
	for (i = 0; i < size; i++) {
		if (i % 2 == 1)
			data[i] = 128;
	}
}

void _noblue_mod(unsigned char* data, uint64_t size) {
	uint_fast64_t i = 0; // can't find difference between uint64 vs fast64
	for (i = 0; i < size; i++) {
		if (i % 2 == 0)
			data[i] = 128;
	}
}

void _emboss_mod(unsigned char* data, uint64_t size) {
	uint64_t x = 0;
	uint64_t w = (uint64_t) cam_data.width;
	float mask[9] = { -2.0f, -1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2.0f };

	unsigned char* ndata;
	ndata = (unsigned char*) malloc(sizeof(unsigned char) * size);

	for (x = 0; x < size; x++) {
		float val[9] = { 0, };
		if (x < w + 1)
			val[0] = 0;
		else
			val[0] = data[x - w - 1] * mask[8];

		if (x < w)
			val[1] = 0;
		else
			val[1] = data[x - w] * mask[7];

		if (x < w - 1)
			val[2] = 0;
		else
			val[2] = data[x - w + 1] * mask[6];

		if (x < 1)
			val[3] = 0;
		else
			val[3] = data[x - 1] * mask[5];

		val[4] = data[x] * mask[4];

		if (x + 1 > size)
			val[5] = 0;
		else
			val[5] = data[x + 1] * mask[3];

		if (x + w - 1 > size)
			val[6] = 0;
		else
			val[6] = data[x + w - 1] * mask[2];

		if (x + w > size)
			val[7] = 0;
		else
			val[7] = data[x + w] * mask[1];

		if (x + w + 1 > size)
			val[8] = 0;
		else
			val[8] = data[x + w + 1] * mask[0];

		float sum = 0;
		for (int i = 0; i < 9; i++)
			sum += val[i];

		ndata[x] = (unsigned char) sum;
		//ndata[x] =(unsigned char) (sum/8);
	}
	memcpy(data, ndata, sizeof(unsigned char) * size);
}
void _emboss_mod_uv(unsigned char* data, uint64_t size) {
	uint64_t x = 0;
	uint64_t w = (uint64_t) cam_data.width;
	float mask[9] = { -2.0f, -1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 2.0f };

	unsigned char* ndata;
	ndata = (unsigned char*) malloc(sizeof(unsigned char) * size);

	for (x = 0; x < size; x++) {
		float val[9] = { 0, };
		if (x < w + 2)
			val[0] = 0;
		else
			val[0] = data[x - w - 2] * mask[8];

		if (x < w)
			val[1] = 0;
		else
			val[1] = data[x - w] * mask[7];

		if (x < w - 2)
			val[2] = 0;
		else
			val[2] = data[x - w + 2] * mask[6];

		if (x < 2)
			val[3] = 0;
		else
			val[3] = data[x - 2] * mask[5];

		val[4] = data[x] * mask[4];

		if (x + 2 > size)
			val[5] = 0;
		else
			val[5] = data[x + 2] * mask[3];

		if (x + w - 2 > size)
			val[6] = 0;
		else
			val[6] = data[x + w - 2] * mask[2];

		if (x + w > size)
			val[7] = 0;
		else
			val[7] = data[x + w] * mask[1];

		if (x + w + 2 > size)
			val[8] = 0;
		else
			val[8] = data[x + w + 2] * mask[0];

		float sum = 0;
		for (int i = 0; i < 9; i++)
			sum += val[i];

		ndata[x] = (unsigned char) sum;
		//ndata[x] =(unsigned char) (sum/8);
	}
	memcpy(data, ndata, sizeof(unsigned char) * size);
}

void _gaussian_mod(unsigned char* data, uint64_t size) {
	uint64_t x = 0;
	uint64_t w = (uint64_t) cam_data.width;
//	float xmask[5] = {.0003, .1065, .7866, .1065, .0003};
	float mask[9] = { .0113, .0838, .0113, .0838, .6193, .0838, .0113, .0838,
			.0113 };

	unsigned char* ndata;
	ndata = (unsigned char*) malloc(sizeof(unsigned char) * size);

	for (x = 0; x < size; x++) {
		float val[9] = { 0, };
		if (x < w + 1)
			val[0] = 0;
		else
			val[0] = data[x - w - 1] * mask[8];

		if (x < w)
			val[1] = 0;
		else
			val[1] = data[x - w] * mask[7];

		if (x < w - 1)
			val[2] = 0;
		else
			val[2] = data[x - w + 1] * mask[6];

		if (x < 1)
			val[3] = 0;
		else
			val[3] = data[x - 1] * mask[5];

		val[4] = data[x] * mask[4];

		if (x + 1 > size)
			val[5] = 0;
		else
			val[5] = data[x + 1] * mask[3];

		if (x + w - 1 > size)
			val[6] = 0;
		else
			val[6] = data[x + w - 1] * mask[2];

		if (x + w > size)
			val[7] = 0;
		else
			val[7] = data[x + w] * mask[1];

		if (x + w + 1 > size)
			val[8] = 0;
		else
			val[8] = data[x + w + 1] * mask[0];

		float sum = 0;
		for (int i = 0; i < 8; i++)
			sum += val[i];

		ndata[x] = (unsigned char) sum;
		//ndata[x] =(unsigned char) (sum/8);
	}
	memcpy(data, ndata, sizeof(unsigned char) * size);
}

void _gaussian_mod_uv(unsigned char* data, uint64_t size) {
	uint64_t x = 0;
	uint64_t w = (uint64_t) cam_data.width;
	float mask[9] = { .0113, .0838, .0113, .0838, .6193, .0838, .0113, .0838,
			.0113 };

	unsigned char* ndata = (unsigned char*) malloc(
			sizeof(unsigned char) * size);

	for (x = 0; x < size; x++) {
		float val[9] = { 0, };
		if (x < 8)
			val[0] = 0;
		else
			val[0] = data[x - 8] * mask[8];

		if (x < 6)
			val[1] = 0;
		else
			val[1] = data[x - 6] * mask[7];

		if (x < 4)
			val[2] = 0;
		else
			val[2] = data[x - 4] * mask[6];

		if (x < 2)
			val[3] = 0;
		else
			val[3] = data[x - 2] * mask[5];

		val[4] = data[x] * mask[4];

		if (x + 2 > size)
			val[5] = 0;
		else
			val[5] = data[x + 2] * mask[3];

		if (x + 4 > size)
			val[6] = 0;
		else
			val[6] = data[x + 4] * mask[2];

		if (x + 6 > size)
			val[7] = 0;
		else
			val[7] = data[x + 6] * mask[1];

		if (x + 8 > size)
			val[8] = 0;
		else
			val[8] = data[x + 8] * mask[0];

		float sum = 0;
		for (int i = 0; i < 8; i++)
			sum += val[i];

		ndata[x] = (unsigned char) sum;
	}

	memcpy(data, ndata, sizeof(unsigned char) * size);
}

void _pinky_mod(unsigned char* data, uint64_t size) {
	uint_fast64_t i = 0; // can't find difference between uint64 vs fast64
	for (i = 0; i < size; i++) {
		if (data[i] < 128)
			data[i] *= 1.2;
	}
}

void _pinky_mod_uv(unsigned char* data, uint64_t size) {
	uint_fast64_t i = 0; // can't find difference between uint64 vs fast64
	for (i = 0; i < size; i++) {
		if (data[i] % 2 == 0)
			data[i] *= 1.2;
	}
}

static void __camera_cb_filter(void *data, Evas_Object *obj, void *event_info) {
	/*
	 * Get the minimal and maximal supported value for the camera filter
	 */
	int min, max;

//	_image_util_start_cb(NULL);

	return;

	int error_code = camera_attr_get_filter_range(&min, &max);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_filter_range", error_code);
		PRINT_MSG("Could not get filter range.");
	}

	/* Get the current value of the camera filter attribute. */
	int filter;

	error_code = camera_attr_get_filter(&filter);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_filter", error_code);
		PRINT_MSG("Could not get current filter.");
	}

	/* Set new value of the camera brightness attribute */

	filter = filter == max ? min : ++filter;
	error_code = camera_attr_set_filter(filter);
	if (CAMERA_ERROR_NONE != error_code) {
		if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
			DLOG_PRINT_ERROR("camera_attr_set_filter", error_code);
			PRINT_MSG("Could not set new filter value.");
		} else
			dlog_print(DLOG_INFO, LOG_TAG,
					"Camera filter is not supported on this device.");
	} else
		PRINT_MSG("Filter set to %d", filter);

	/* TODO: Apply filter to preview image */
	//if (camera_set_preview_cb(cam_data.g_camera, _camera_preview_callback, NULL)
	//		== CAMERA_ERROR_NONE) {
	//PRINT_MSG("ready to modify");
	//}
}

static int camera_attr_get_sticker_range(int* min, int* max) {
	*min = 1;
	*max = 5;
	return 0;
}

static int camera_attr_get_sticker(int* sticker) {
	*sticker = cam_data.sticker;
	return 0;
}

static int camera_attr_set_sticker(int sticker) {
	cam_data.sticker = sticker;
	return 0;
}

static void _camera_face_detected_cb(camera_detected_face_s* faces, int count,
		void* user_data) {
	//PRINT_MSG("Face detected: %d", count);
	float time;
	clock_t begin = clock();

	if(!cam_data.faces.empty())
		cam_data.faces.clear();

	/* convert camera_detected_face_s faces to std::vector<rectangle> faces */
	for (int i = 0; i < count; i++) {
		dlib::rectangle face;
		face.set_top(faces[i].x);
		face.set_bottom(faces[i].x + faces[i].height);
		face.set_right(cam_data.height - faces[i].y);
		face.set_left(cam_data.height - faces[i].y - faces[i].width);
		cam_data.faces.push_back(face);

		//PRINT_MSG("Face[%d]_width: %d, Face[%d]_height: %d\n", i, faces[i].width, i, faces[i].height);
		//PRINT_MSG("Face[%d]_x: %d, Face[%d]_y: %d\n", i, faces[i].x, i,	faces[i].y);
	}

	time = (double) (clock() - begin) / CLOCKS_PER_SEC;
	//PRINT_MSG("face format conversion takes %f sec", time);
}

void face_landmark(camera_preview_data_s *frame, int count)
{
	//clock_t begin;
	dlib::array2d<u_int64_t> img;
	img.set_size(frame->width, frame->height);
	if (frame->data.double_plane.y_size != frame->width * frame->height) {
		PRINT_MSG("Error: y_size: %d, width: %d, height: %d",
				frame->data.double_plane.y_size, frame->width, frame->height);
		//return NULL;
	}


	for (int i = 0; i < frame->data.double_plane.y_size; i++) {
		// img[i % frame->width][ i / frame->width] = (frame->data.double_plane.y)[i];
		img[i % frame->width][frame->height - i / frame->width-1] = (frame->data.double_plane.y)[i];
	}

	//float time = (double) (clock() - begin) / CLOCKS_PER_SEC; // TM1: 0.3 sec
	//PRINT_MSG("frame format conversion takes %f sec", time);

	// Now we will go ask the shape_predictor to tell us the pose of
	// each face we detected.
	for (unsigned long i = 0; i < count; ++i) {
		//begin = clock();
		dlib::full_object_detection shape = cam_data.sp(img, cam_data.faces[i]);
		//time = (double) (clock() - begin) / CLOCKS_PER_SEC; // TM1: 0.1 sec
		//PRINT_MSG("Finding landmark takes %f sec", time);

		draw_landmark(frame, shape);
		int x = shape.part(i)(1);
		int y = frame->height - shape.part(i)(0);
		if(imgarr != NULL && imgarr[0].size > 0)
		{
			_image_util_imgcpy(frame, &imgarr[0], x, y);
		}

		 switch(cam_data.sticker) {
		 case 1:
			 sticker_rudolph(frame, shape);
			 break;
		 case 2:
			 sticker_bald(frame, shape);
			 break;
		 case 3:
			 sticker_glasses(frame, shape);
			 break;
		 case 4:
			 sticker_hat(frame, shape);
			 break;
		 case 5:
			 sticker_mustache(frame, shape);
		 break;
		 case 6:
			 sticker_rabbit(frame, shape);
			 break;
		 case 7:
			 sticekr_santa(frame, shape);
			 break;
		 default:
		 break;
		 }
	}
}

void _camera_preview_callback(camera_preview_data_s *frame, void *user_data) {
	if (frame->format == CAMERA_PIXEL_FORMAT_NV12
			&& frame->num_of_planes == 2) {

		std::vector<dlib::rectangle> buf =
				*((std::vector<dlib::rectangle>*) user_data);
		size_t count = buf.size();
		/* get face landmark */
		if (count > 0) {
			//clock_t sTime = clock();
			face_landmark(frame, count);
			//float time = (double) (clock() - sTime) / CLOCKS_PER_SEC; // 0.3 sec in TM1
			//PRINT_MSG("Face landmark takes %f sec", time);

			switch (cam_data.sticker) {
			case 0: //
				break;
			case 1: // mustache
				break;
			case 2: // hat
				break;
			case 3: // ear
				break;
			case 4: // glasses
				break;
			case 5: // hairband
				break;
			default:
				break;
			}

			//time_t eTime = clock();
			//float gap = (float) (eTime - sTime) / (CLOCKS_PER_SEC);
		}
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"This preview frame format is not supported!");
		PRINT_MSG("This preview frame format is not supported!");
		//we do nothing, the preview is left intact and displayed without modifications
	}
}

static void __camera_cb_sticker(void *data, Evas_Object *obj,
		void *event_info) {
	/*
	 * Get the minimal and maximal supported value for the camera sticker
	 */
	int min, max;

	int error_code = camera_attr_get_sticker_range(&min, &max);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_sticker_range", error_code);
		PRINT_MSG("Could not get sticker range.");
	}

	/* Get the current value of the camera sticker attribute. */
	int sticker;

	error_code = camera_attr_get_sticker(&sticker);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_get_sticker", error_code);
		PRINT_MSG("Could not get current sticker.");
	}

	/* Set new value of the camera sticker attribute */
	sticker = sticker == max ? min : ++sticker;
	error_code = camera_attr_set_sticker(sticker);
	if (CAMERA_ERROR_NONE != error_code) {
		if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
			DLOG_PRINT_ERROR("camera_attr_set_sticker", error_code);
			PRINT_MSG("Could not set new sticker value.");
		} else
			dlog_print(DLOG_INFO, LOG_TAG,
					"Camera sticker is not supported on this device.");
	} else
		PRINT_MSG("Sticker set to %d", sticker);

	camera_state_e state;
	error_code = camera_get_state(cam_data.g_camera, &state);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_get_state", error_code);
		return;
	}

	if (CAMERA_STATE_PREVIEW == state) {

		const char* resource_path = app_get_resource_path();
		char *file_path = (char *) malloc(sizeof(char) * BUFLEN);

		/* Create a full path to get a shape predictor. */
		snprintf(file_path, BUFLEN, "%s%s", resource_path,
				"shape_predictor_68_face_landmarks.dat");

		dlib::deserialize(file_path) >> cam_data.sp;
		free(file_path);

		/*
		 * * The following actions (stop->start -> stop -> start preview) are required
		 * * to deal with a bug related to the camera brightness changes
		 * * (without applying this workaround, after taking a photo,
		 * * the changes of the camera preview brightness are not visible).
		 */

		error_code = camera_unset_preview_cb(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_unset_preview_cb", error_code);
		}
		error_code = camera_stop_preview(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_stop_preview", error_code);
			PRINT_MSG("Could not stop the camera preview.");
		}

		error_code = camera_start_preview(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_start_preview", error_code);
			PRINT_MSG("Could not restart the camera preview.");
		}

		error_code = camera_stop_preview(cam_data.g_camera);
		if (CAMERA_ERROR_NONE != error_code) {
			DLOG_PRINT_ERROR("camera_stop_preview", error_code);
			PRINT_MSG("Could not stop the camera preview.");
		}
	}

	error_code = camera_set_preview_cb(cam_data.g_camera,
			_camera_preview_callback, &cam_data.faces);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_set_preview_cb", error_code);
	}

	error_code = camera_start_preview(cam_data.g_camera);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_start_preview", error_code);
		PRINT_MSG("Could not restart the camera preview.");
	}

	error_code = camera_start_face_detection(cam_data.g_camera,
			_camera_face_detected_cb, &cam_data.faces);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_start_face_detection error", error_code);
		return;
	}

}

/**
 * @brief Called when the "Camera" screen is being closed.
 */
void camera_pop_cb() {
	/* Stop camera focusing. */
	camera_cancel_focusing(cam_data.g_camera);

	/* Stop camera preview. */
	camera_stop_preview(cam_data.g_camera);
	cam_data.cam_prev = false;

	/* Unregister camera preview callback. */
	camera_unset_preview_cb(cam_data.g_camera);

	/* Unregister camera focus change callback. */
	camera_unset_focus_changed_cb(cam_data.g_camera);

	/* Unregister camera preview callback. */

	/* Destroy camera handle. */
	camera_destroy(cam_data.g_camera);
	cam_data.g_camera = NULL;

	/* Free the Camera directory path. */
	free(camera_directory);
}

/**
 * @brief Called when the camera preview display is being resized.
 * @details It resizes the camera preview to fit the camera preview display.
 * @remarks This function matches the Evas_Object_Event_Cb() signature defined
 *          in the Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. In this case it is
 *                    used for passing the camera preview object. This argument
 *                    is not used in this case.
 * @param e           The canvas pointer on which the event occurred
 * @param obj         A pointer to the object on which the event occurred. In
 *                    this case it is the camera preview display.
 * @param event_info  In case of the EVAS_CALLBACK_RESIZE event, this parameter
 *                    is NULL. This argument is not used in this case.
 */
void _post_render_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
	Evas_Object **cam_data_image = (Evas_Object **) data;

	/* Get the size of the parent box. */
	int x = 0, y = 0, w = 0, h = 0;
	evas_object_geometry_get(obj, &x, &y, &w, &h);

	/* Set the size of the image object. */
	evas_object_resize(*cam_data_image, w, h);
	evas_object_move(*cam_data_image, 0, y);
}

/**
 * @brief Creates the main view of the application.
 *
 */
void create_buttons_in_main_window(void) {
	/*
	 * Create the window with camera preview and buttons for manipulating the
	 * camera and taking the photo.
	 */
	struct tmp *buf = (struct tmp*) _create_new_cd_display((char*) "Camera",
	NULL);
	cam_data.display = buf->mbox;

	/* Create a box for the camera preview. */
	cam_data.cam_display_box = elm_box_add(cam_data.display);
	elm_box_horizontal_set(cam_data.cam_display_box, EINA_FALSE);
	evas_object_size_hint_align_set(cam_data.cam_display_box, EVAS_HINT_FILL,
	EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(cam_data.cam_display_box, EVAS_HINT_EXPAND,
	EVAS_HINT_EXPAND);
	elm_box_pack_end(cam_data.display, cam_data.cam_display_box);
	evas_object_show(cam_data.cam_display_box);

	Evas *evas = evas_object_evas_get(cam_data.cam_display_box);
	cam_data.cam_display = evas_object_image_add(evas);
	evas_object_event_callback_add(cam_data.cam_display_box,
			EVAS_CALLBACK_RESIZE, _post_render_cb, &(cam_data.cam_display));

	/* Create buttons for the Camera. */
	cam_data.preview_bt = _new_button(buf->tbox, (char *) "Start preview",
			(void *) __camera_cb_preview);
	cam_data.zoom_bt = _new_button(buf->tbox, (char*) "Zoom",
			(void *) __camera_cb_zoom);
	cam_data.brightness_bt = _new_button(buf->tbox, (char*) "Brightness",
			(void *) __camera_cb_bright);
	cam_data.filter_bt = _new_button(buf->bbox, (char *) "Filter",
			(void *) __camera_cb_filter);
	cam_data.sticker_bt = _new_button(buf->bbox, (char *) "Sticker",
			(void *) __camera_cb_sticker);
	cam_data.photo_bt = _new_button(buf->bbox, (char *) "Take a photo",
			(void *) __camera_cb_photo);

	/*
	 * Disable buttons different than "Start preview" when the preview is not
	 * running.
	 */
	elm_object_disabled_set(cam_data.zoom_bt, EINA_TRUE);
	elm_object_disabled_set(cam_data.brightness_bt, EINA_TRUE);
	elm_object_disabled_set(cam_data.photo_bt, EINA_TRUE);
	elm_object_disabled_set(cam_data.filter_bt, EINA_TRUE);
	elm_object_disabled_set(cam_data.sticker_bt, EINA_TRUE);

	/* Create the camera handle for the main camera of the device. */
	// CAMEAR_DEVICE_CAMERA0 : rear
	// CAMEAR_DEVICE_CAMERA1 : front
	int error_code = camera_create(CAMERA_DEVICE_CAMERA0	, &(cam_data.g_camera));
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_create", error_code);
		PRINT_MSG("Could not create a handle to the camera.");
		return;
	}

	/* Check the camera state after creating the handle. */
	camera_state_e state;
	error_code = camera_get_state(cam_data.g_camera, &state);
	if (CAMERA_ERROR_NONE != error_code || CAMERA_STATE_CREATED != state) {
		dlog_print(DLOG_ERROR, LOG_TAG,
				"camera_get_state() failed! Error code = %d, state = %s",
				error_code, _camera_state_to_string(state));
		return;
	}

	/*
	 * Enable EXIF data storing during taking picture. This is required to edit
	 * the orientation of the image.
	 */
	error_code = camera_attr_enable_tag(cam_data.g_camera, true);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_enable_tag", error_code);
		PRINT_MSG("Could not enable the camera tag.");
	}

	/*
	 * Set the camera image orientation. Required (on Kiran device) to save the
	 * image in regular orientation (without any rotation).
	 */
	error_code = camera_attr_set_tag_orientation(cam_data.g_camera,
			CAMERA_ATTR_TAG_ORIENTATION_RIGHT_TOP);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_set_tag_orientation", error_code);
		PRINT_MSG("Could not set the camera image orientation.");
	}

	/* Set the picture quality attribute of the camera to maximum. */
	error_code = camera_attr_set_image_quality(cam_data.g_camera, 100);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_attr_set_image_quality", error_code);
		PRINT_MSG("Could not set the picture quality.");
	}

	/* Set the display for the camera preview. */
	error_code = camera_set_display(cam_data.g_camera, CAMERA_DISPLAY_TYPE_EVAS,
			GET_DISPLAY(cam_data.cam_display));
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_set_display", error_code);
		PRINT_MSG("Could not set the camera display.");
		return;
	}

	/* Set the resolution of the camera preview: */
	int resolution[2];

	/* 1. Find the best resolution that is supported by the device. */
	error_code = camera_foreach_supported_preview_resolution(cam_data.g_camera,
			_preview_resolution_cb, resolution);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_foreach_supported_preview_resolution",
				error_code);
		PRINT_MSG("Could not find the best resolution for the camera preview.");
		return;
	}

	/* 2. Set found supported resolution for the camera preview. */
	//error_code = camera_set_preview_resolution(cam_data.g_camera, resolution[0],
	//resolution[1]);
	error_code = camera_set_preview_resolution(cam_data.g_camera, resolution[0], resolution[1]);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_set_preview_resolution", error_code);
		PRINT_MSG("Could not set the camera preview resolution.");
	} else
		PRINT_MSG("Camera resolution set to: %d %d", resolution[0],
				resolution[1]);
	cam_data.width = resolution[0];
	cam_data.height = resolution[1];

	//camera_foreach_supported_preview_format(cam_data.g_camera, _supported_preview_format_cb, NULL);

	/* Set the capture format for the camera. */
	error_code = camera_set_capture_format(cam_data.g_camera,
			CAMERA_PIXEL_FORMAT_JPEG);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_set_capture_format", error_code);
		PRINT_MSG("Could not set the capturing format.");
	}

	/* Set the focusing callback function. */
	error_code = camera_set_focus_changed_cb(cam_data.g_camera,
			_camera_focus_cb, NULL);
	if (CAMERA_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("camera_set_focus_changed_cb", error_code);
		PRINT_MSG("Could not set a callback for the focus changes.");
	}

	/* Get the path to the Camera directory: */

	/* 1. Get internal storage id. */
	int internal_storage_id = -1;

	error_code = storage_foreach_device_supported(_storage_cb,
			&internal_storage_id);
	if (STORAGE_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("storage_foreach_device_supported", error_code);
		PRINT_MSG("Could not get internal storage id.");
		return;
	}

	/* 2. Get the path to the Camera directory. */
	error_code = storage_get_directory(internal_storage_id,
			STORAGE_DIRECTORY_CAMERA, &camera_directory);
	if (STORAGE_ERROR_NONE != error_code) {
		DLOG_PRINT_ERROR("storage_get_directory", error_code);
		PRINT_MSG("Could not get the path to the Camera directory.");
	}

	_image_util_start_cb(&imgarr[0]);
	PRINT_MSG("Got the imgarr : %d", imgarr[0].size);
}
