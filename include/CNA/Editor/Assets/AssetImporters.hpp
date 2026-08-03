// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Assets/AssetImporters.hpp
 * @brief What each importer's settings are, declared the same way a component's are.
 *
 * An importer's settings are a named list of typed, defaulted fields -- which is exactly what
 * `ComponentDescriptor` already describes. Reusing it rather than inventing a parallel schema
 * means the inspector needs no new code to edit them, a plugin's importer is editable on the same
 * terms as a built-in one, and the JSON round-trip is the one `PropertyValue` already has
 * (ANALYSIS.md decision D-05).
 *
 * The registry is separate from the component registry, not shared: an importer id and a component
 * type id are different namespaces, and a project that happened to name a component
 * "CNA.TextureImporter" should not silently become editable as one.
 */

#include <optional>
#include <string>
#include <string_view>

#include "CNA/Editor/Assets/AssetDatabase.hpp"
#include "CNA/Editor/Core/ComponentDescriptor.hpp"

namespace CNA::Editor
{
    /** @brief The importer type ids the editor ships with. */
    namespace ImporterIds
    {
        inline constexpr const char* kTexture = "CNA.TextureImporter";
        inline constexpr const char* kSpriteFont = "CNA.SpriteFontImporter";
        inline constexpr const char* kSoundEffect = "CNA.SoundEffectImporter";
        inline constexpr const char* kSong = "CNA.SongImporter";
        inline constexpr const char* kModel = "CNA.ModelImporter";
    }

    /**
     * @brief Registers the built-in importers' settings schemas into @p registry.
     *
     * Only the importers with settings worth editing are declared. An asset whose importer has no
     * schema is still tracked and still imported -- the inspector simply has nothing to offer for
     * it, which is honest rather than an empty form.
     */
    void registerBuiltinImporters(ComponentRegistry& registry);

    /** @brief An image's dimensions in pixels. */
    struct ImageSize
    {
        int width = 0;
        int height = 0;
    };

    /**
     * @brief Reads an image's dimensions from its header, without decoding it.
     *
     * PNG and BMP only, which is deliberate rather than a stub: both state their size in a fixed
     * header, so a few dozen bytes answer the question. JPEG requires walking its segments and
     * anything else requires a decoder -- neither belongs in the editor's CNA-free layer, and
     * both are better answered by the importer that will eventually load the file for real.
     *
     * @return The size, or std::nullopt when the file cannot be read or its format is not one of
     *         those two. Callers must treat "unknown" as unknown rather than as zero.
     */
    [[nodiscard]] std::optional<ImageSize> readImageSize(const std::string& absolutePath);

    /**
     * @brief Fills in the facts an importer can determine by reading a file.
     *
     * Called after a scan. Only writes where the value would actually change, so a project whose
     * assets have not moved produces no sidecar churn -- a scan that rewrote every sidecar on
     * every open would show up as a repository full of spurious diffs.
     *
     * @return The number of records whose settings changed.
     */
    std::size_t applyImporterFacts(AssetDatabase& assets);
}
