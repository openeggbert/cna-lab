// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// Vendored stb_image_write.h implementation (M205), isolated in its own
// translation unit so it can be compiled without -Wall -Wextra -Werror --
// third-party code doesn't meet this project's own warning bar. See the
// CMakeLists.txt comment next to the `stb_image_write_impl` target.

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
