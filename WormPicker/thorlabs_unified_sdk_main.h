/*
* Copyright 2017 by Thorlabs, Inc.  All rights reserved.  Unauthorized use (including,
* without limitation, distribution and copying) is strictly prohibited.  All use requires, and is
* subject to, explicit written authorization and nondisclosure agreements with Thorlabs, Inc.
*/

#pragma once

//------------------------------------------------------------------------------
// Exported function prototype typedefs.
//------------------------------------------------------------------------------

typedef char* (*TL_MODULE_GET_VERSION)(); /* tl_module_get_version */
typedef int (*TL_OPEN_SDK)(); /* tl_open_sdk */
typedef int (*TL_CLOSE_SDK)(); /* tl_close_sdk */
typedef int (*TL_GET_DEVICE_IDS) (char*, int); /* tl_get_device_ids */
typedef int (*TL_OPEN_DEVICE) (char*, char*, void**); /* tl_open_device */
typedef int (*TL_CLOSE_DEVICE) (void*); /* tl_close_device */
typedef int (*TL_GET_DATA) (void*, const char*, void*, int, int); /* tl_get_data */
typedef int (*TL_SET_DATA) (void*, const char*, void*, int); /* tl_set_data */
typedef int (*TL_GET_DEVICE_MODULE_IDS) (char*, int); /* tl_get_device_module_ids */
typedef int (*TL_DEVICE_MODULE_GET_DATA) (const char*, const char*, void*, int); /* tl_device_module_get_data */
typedef int (*TL_DEVICE_MODULE_SET_DATA) (const char*, const char*, void*, int); /* tl_device_module_set_data */
