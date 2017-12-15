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

#if !defined(_SELFCAMERA_H_)
#define _SELFCAMERA_H_

#include <app.h>
#include <dlog.h>

#if !defined(PACKAGE)
#define PACKAGE "org.example.selfcamera"
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "selfcamera"

Evas_Object *view_create_win(const char *pkg_name);
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win);

#endif
