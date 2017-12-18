#include "imageutils.h"
#include <image_util.h>
#include <storage.h>

#define BUFLEN 256

static char sample_file_path[BUFLEN];
static const char *image_util_stickers_filename[STICKER_NUM] = {
"deer_left_big.jpg","deer_right_big.jpg","deer_nose_big.jpg",
"deer_left_small.jpg", "deer_right_small.jpg", "deer_nose_small.jpg",
"hat0.jpg","hat1.jpg","hat2.jpg",
"beard_rot.jpg", "deer_nose_s.jpg",
"glasses_rot.jpg", "santa_rot.jpg",
"deer_left_mid.jpg", "deer_right_mid.jpg",
"cat_left.jpg", "cat_right.jpg"
};

void _image_util_yuvcpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q)
{
	int sh = imginfo->height;
	int sw = imginfo->width;
	int sy_size = sh*sw;

	int fh = frame->height;
	int fw = frame->width;

	p -= sw/2;
	q -= sh/2;
	if(p%2 != 0) p++;
	if(q%2 != 0) q++;

	int pp = 0;
	int qq = 0;
	int pt = 0;
	int ptpt = 0;
	int pti = 0;
	int ptipti = 0;
	int ii = 0;
	int jj = 0;

	unsigned char y,u,v;

	for(int j=0;j<sh;j++){
		for(int i=0;i<sw;i++){
			pp = p+i;
			qq = q+j;
			if(pp < 0 || qq < 0) continue;
			pt = pp + qq*fw;
			pti = i + j*sw;

			pp /= 2;
			qq /= 2;
			ptpt = (pp + qq*fw/2)*2;
			ii = i / 2;
			jj = j / 2;
			ptipti = (ii + jj*sw/2)*2 + sy_size;
			y = imginfo->data[pti];
			if(ptipti < imginfo->size){
				u = imginfo->data[ptipti];
				v = imginfo->data[ptipti+1];
			} else
			{
				continue;
			}

			if(y > 230 && (u > 124 || u < 132) && (v > 124 || v < 132))
			{
				continue;
			} else
			{
				if(pt < frame->data.double_plane.y_size && ptpt < frame->data.double_plane.uv_size)
				{
					frame->data.double_plane.y[pt] = imginfo->data[pti];
					frame->data.double_plane.uv[ptpt] = v;
					frame->data.double_plane.uv[ptpt+1] = u;
				}
			}
		}
	}
}

void _image_util_imgcpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q)
{
	int sh = imginfo->height;
	int sw = imginfo->width;
	int sy_size = sh*sw;

	int fh = frame->height;
	int fw = frame->width;

	p -= sw/2;
	q -= sh/2;

	int pt = p + q*fw;
	int pti = 0;

#ifdef ALPHA
	// copy Y plane
	for(int i=0;i<sw;i++)
	{
		if(pt > frame->data.double_plane.y_size || pti > sy_size)
			break;
		memcpy(frame->data.double_plane.y + pt, imginfo->data + pti, sizeof(unsigned char)*sh);
		pt += fw;
		pti += sh;
	}

	// copy UV plane
	p /=2;
	q /=2;
	pt = (p + q*fw/2)*2;
	pti = sy_size;

	for(int i=0;i<sw/2;i++)
	{
		if(pt > frame->data.double_plane.uv_size || pti > imginfo->size)
			break;
		memcpy(frame->data.double_plane.uv + pt, imginfo->data + pti, sizeof(unsigned char)*sh);
		pt += fw;
		pti += sh;
	}
#else
	for(int i=0;i<sw;i++)
	{
		//if(pt >= frame->data.double_plane.y_size || pti >= sy_size)
		//	break;
		for(int j=0;j<sh;j++)
		{
			if(pt+j < frame->data.double_plane.y_size && pti+j < sy_size)
				if(imginfo->data[pti+j] <= 220)
					frame->data.double_plane.y[pt+j] = imginfo->data[pti+j];
		}
		pt += fw;
		pti += sh;
	}


	// copy UV plane
	p /=2;
	q /=2;
	pt = (p + q*fw/2)*2+1;
	pti = sy_size;

	for(int i=0;i<sw/2;i++)
	{
		//if(pt >= frame->data.double_plane.uv_size || pti >= imginfo->size)
		//	break;
		for(int j=0;j<sh;j++)
		{
			if(pt+j < frame->data.double_plane.uv_size && pti+j < imginfo->size)
			{
				unsigned char tmp = imginfo->data[pti+j];
				if(tmp != 128)
					frame->data.double_plane.uv[pt+j] = tmp;
				if(i == sw/4)
					tmp =0;
			}
		}
		pt += fw;
		pti += sh;
	}
#endif
}

void _image_util_read_stickers(imageinfo* imgarr)
{
	for(int i=0;i<STICKER_NUM;i++)
	{
		_image_util_read_jpeg(&imgarr[i], i);
	}
}

void _image_util_read_jpeg(imageinfo* imginfo, int idx)
{
    /* Decode the given JPEG file to the img_source buffer. */
    unsigned char *img_source = NULL;
    int width, height;
    unsigned int size_decode;

    char *resource_path = app_get_resource_path();
    snprintf(sample_file_path, BUFLEN, "%s%s", resource_path, image_util_stickers_filename[idx]);

    int error_code = image_util_decode_jpeg(sample_file_path, IMAGE_UTIL_COLORSPACE_NV12, &img_source, &width, &height, &size_decode);
    if (error_code != IMAGE_UTIL_ERROR_NONE) {
        //DLOG_PRINT_ERROR("image_util_decode_jpeg", error_code);
        imginfo->error = error_code;
        return;
    }

    imginfo->error = error_code;
    imginfo->size = size_decode;
    imginfo->width = width;
    imginfo->height = height;
	imginfo->data = (unsigned char*)malloc(sizeof(unsigned char)*imginfo->size);
	memcpy(imginfo->data, img_source, sizeof(unsigned char)*imginfo->size);

    //DLOG_PRINT_DEBUG_MSG("Decoded image width: %d height: %d size: %d", width, height, size_decode);

    /* no need to transform RGB->NV12, just decode into NV12 */
}

const char *_map_colorspace(image_util_colorspace_e color_space)
{
    switch (color_space) {
    case IMAGE_UTIL_COLORSPACE_YV12:
        return "IMAGE_UTIL_COLORSPACE_YV12";

    case IMAGE_UTIL_COLORSPACE_YUV422:
        return "IMAGE_UTIL_COLORSPACE_YUV422";

    case IMAGE_UTIL_COLORSPACE_I420:
        return "IMAGE_UTIL_COLORSPACE_I420";

    case IMAGE_UTIL_COLORSPACE_NV12:
        return "IMAGE_UTIL_COLORSPACE_NV12";

    case IMAGE_UTIL_COLORSPACE_UYVY:
        return "IMAGE_UTIL_COLORSPACE_UYVY";

    case IMAGE_UTIL_COLORSPACE_YUYV:
        return "IMAGE_UTIL_COLORSPACE_YUYV";

    case IMAGE_UTIL_COLORSPACE_RGB565:
        return "IMAGE_UTIL_COLORSPACE_RGB565";

    case IMAGE_UTIL_COLORSPACE_RGB888:
        return "IMAGE_UTIL_COLORSPACE_RGB888";

    case IMAGE_UTIL_COLORSPACE_ARGB8888:
        return "IMAGE_UTIL_COLORSPACE_ARGB8888";

    case IMAGE_UTIL_COLORSPACE_BGRA8888:
        return "IMAGE_UTIL_COLORSPACE_BGRA8888";

    case IMAGE_UTIL_COLORSPACE_RGBA8888:
        return "IMAGE_UTIL_COLORSPACE_RGBA8888";

    case IMAGE_UTIL_COLORSPACE_BGRX8888:
        return "IMAGE_UTIL_COLORSPACE_BGRX8888";

    case IMAGE_UTIL_COLORSPACE_NV21:
        return "IMAGE_UTIL_COLORSPACE_NV21";

    case IMAGE_UTIL_COLORSPACE_NV16:
        return "IMAGE_UTIL_COLORSPACE_NV16";

    case IMAGE_UTIL_COLORSPACE_NV61:
        return "IMAGE_UTIL_COLORSPACE_NV61";
    }
}

void _image_util_santacpy(camera_preview_data_s* frame, imageinfo* imginfo, int p, int q)
{
	int sh = imginfo->height;
	int sw = imginfo->width;
	int sy_size = sh*sw;

	int fh = frame->height;
	int fw = frame->width;

	p -= sw/2;
	q -= sh/2;
	if(p%2 != 0) p++;
	if(q%2 != 0) q++;

	int pp = 0;
	int qq = 0;
	int pt = 0;
	int ptpt = 0;
	int pti = 0;
	int ptipti = 0;
	int ii = 0;
	int jj = 0;

	unsigned char y,u,v;

	for(int j=0;j<sh;j++){
		for(int i=0;i<sw;i++){
			pp = p+i;
			qq = q+j;
			if(pp < 0 || qq < 0) continue;
			pt = pp + qq*fw;
			pti = i + j*sw;

			pp /= 2;
			qq /= 2;
			ptpt = (pp + qq*fw/2)*2;
			ii = i / 2;
			jj = j / 2;
			ptipti = (ii + jj*sw/2)*2 + sy_size;
			y = imginfo->data[pti];
			if(ptipti < imginfo->size){
				u = imginfo->data[ptipti];
				v = imginfo->data[ptipti+1];
			} else
			{
				continue;
			}

			if(y < 30 && (u > 124 || u < 132) && (v > 124 || v < 132))
			{
				continue;
			} else
			{
				if(pt < frame->data.double_plane.y_size && ptpt < frame->data.double_plane.uv_size)
				{
					frame->data.double_plane.y[pt] = imginfo->data[pti];
					frame->data.double_plane.uv[ptpt] = v;
					frame->data.double_plane.uv[ptpt+1] = u;
				}
			}
		}
	}
}
