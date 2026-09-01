// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once
#include "ValidationResult.hpp"
#include <string>

namespace MeshWorld {

class MC3Validator {
public:
    // Validate an MC3 XML string.
    // chunk_size_m: used for bounds checking (objects must be within [0, chunk_size_m]).
    // Pass 0.0f to skip bounds checking.
    ValidationResult validate(const std::string& xml, float chunk_size_m = 0.0f) const;
};

} // namespace MeshWorld
