#if !defined(_LANDMARK_H)
#define _LANDMARK_H

#include <iostream>
#include <dlib/image_processing.h>
#include <dlib/image_transforms.h>
#include <fstream>

std::vector<dlib::full_object_detection> face_landmark(camera_preview_data_s* frame, dlib::shape_predictor* sp, int sticker, std::vector<dlib::rectangle> faces, int count);
void draw_landmark(camera_preview_data_s* frame, const dlib::full_object_detection shape);

#endif
