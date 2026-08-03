// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/BuiltinComponents.hpp
 * @brief The component types every CNA-native project starts with.
 *
 * These are descriptors, not C++ classes -- see ComponentDescriptor.hpp for why. The set is
 * deliberately small and 2D-first, matching plan.md Phase 1: Transform, SpriteRenderer, Camera,
 * AudioSource, plus the 3D types that only Phase 3 will make meaningful. Adding a built-in is a
 * one-function change here and requires no other code anywhere in the editor.
 */

#include "CNA/Editor/Core/ComponentDescriptor.hpp"

namespace CNA::Editor
{
    /** @brief Stable type ids for the built-in components. */
    namespace BuiltinComponentIds
    {
        inline constexpr const char* kTransform = "CNA.Transform";
        inline constexpr const char* kSpriteRenderer = "CNA.SpriteRenderer";
        inline constexpr const char* kCamera = "CNA.Camera";
        inline constexpr const char* kAudioSource = "CNA.AudioSource";
        inline constexpr const char* kModelRenderer = "CNA.ModelRenderer";
        inline constexpr const char* kLight = "CNA.Light";
    }

    /**
     * @brief Registers every built-in component descriptor into @p registry.
     *
     * Safe to call more than once: registration replaces by type id, so a second call simply
     * restores the built-in schema over anything a plugin overrode.
     */
    void registerBuiltinComponents(ComponentRegistry& registry);
}
