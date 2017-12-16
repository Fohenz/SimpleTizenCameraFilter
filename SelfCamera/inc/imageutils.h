/*
 * imageutils.h
 *
 *  Created on: Dec 10, 2017
 *      Author: tazo
 */

#ifndef IMAGEUTILS_H_
#define IMAGEUTILS_H_

#include <image_util.h>
#include "view.h"

#define STICKER_NUM 17

typedef struct _imageinfo{
	unsigned char* data;
	int size;
	int width;
	int height;
	int error;
}imageinfo;

void _image_util_yuvcpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q);

void _image_util_imgcpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q);

void _image_util_read_stickers(imageinfo* imgarr);

void _image_util_read_jpeg(imageinfo* imginfo, int id);

const char *_map_colorspace(image_util_colorspace_e color_space);

void _image_util_santacpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q);

#endif /* IMAGEUTILS_H_ */
