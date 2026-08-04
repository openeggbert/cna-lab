// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Assets/AssetImporters.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace CNA::Editor
{
    /** @brief Helpers shared by the sprite-font reader and the facts pass. */
    namespace Detail
    {
        /** @brief Returns the text between `<tag>` and `</tag>`, or an empty string. */
        std::string readTag(const std::string& text, std::string_view tag)
        {
            const std::string open = "<" + std::string{tag} + ">";
            const std::string close = "</" + std::string{tag} + ">";

            const std::size_t start = text.find(open);
            if (start == std::string::npos) { return {}; }

            const std::size_t from = start + open.size();
            const std::size_t end = text.find(close, from);
            if (end == std::string::npos) { return {}; }

            std::string value = text.substr(from, end - from);
            const std::size_t first = value.find_first_not_of(" \t\r\n");
            const std::size_t last = value.find_last_not_of(" \t\r\n");
            if (first == std::string::npos) { return {}; }
            return value.substr(first, last - first + 1);
        }

        /** @brief Parses a number, answering zero rather than throwing on anything else. */
        float toFloat(const std::string& text)
        {
            try { return std::stof(text); }
            catch (const std::exception&) { return 0.0f; }
        }

        /**
         * @brief Turns a `CharacterRegion` bound into a character code.
         *
         * XNA writes these either as the character itself or as an XML entity, and a file written
         * by hand may use either -- so both are read. The space at the start of the usual region
         * is exactly the case that makes the entity form common.
         */
        int toCharacterCode(const std::string& text)
        {
            if (text.empty()) { return 0; }

            if (text.size() > 3 && text.rfind("&#", 0) == 0 && text.back() == ';')
            {
                const bool hexadecimal = text[2] == 'x' || text[2] == 'X';
                try
                {
                    return std::stoi(text.substr(hexadecimal ? 3 : 2, text.size() - (hexadecimal ? 4 : 3)),
                                     nullptr, hexadecimal ? 16 : 10);
                }
                catch (const std::exception&) { return 0; }
            }
            return static_cast<unsigned char>(text.front());
        }

        /**
         * @brief Writes what a `.spritefont` says about itself into its sidecar.
         *
         * @return True when something changed, so that opening a project twice produces no diff.
         */
        bool applySpriteFontFacts(AssetDatabase& assets, const AssetRecord& record);
    }

    namespace
    {
        PropertyDescriptor makeProperty(std::string name,
                                        std::string displayName,
                                        PropertyType type,
                                        PropertyValue defaultValue,
                                        std::string tooltip = {})
        {
            PropertyDescriptor property;
            property.name = std::move(name);
            property.displayName = std::move(displayName);
            property.type = type;
            property.defaultValue = std::move(defaultValue);
            property.tooltip = std::move(tooltip);
            return property;
        }

        ComponentDescriptor makeTextureImporter()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = ImporterIds::kTexture;
            descriptor.displayName = "Texture Importer";
            descriptor.category = "Import";

            PropertyDescriptor wrap = makeProperty("wrapMode", "Wrap Mode", PropertyType::Enum,
                                                   PropertyValue{PropertyValue::EnumValue{"Clamp"}},
                                                   "How coordinates outside 0..1 are sampled. "
                                                   "Matches XNA's TextureAddressMode.");
            wrap.enumOptions = {"Clamp", "Wrap", "Mirror"};

            PropertyDescriptor filter = makeProperty("filterMode", "Filter Mode", PropertyType::Enum,
                                                     PropertyValue{PropertyValue::EnumValue{"Linear"}},
                                                     "Point keeps pixel art crisp; Linear smooths. "
                                                     "Matches XNA's SamplerState presets.");
            filter.enumOptions = {"Point", "Linear", "Anisotropic"};

            descriptor.properties = {
                std::move(wrap),
                std::move(filter),
                makeProperty("generateMipmaps", "Generate Mipmaps", PropertyType::Boolean,
                             PropertyValue{false},
                             "Costs a third more memory and is wrong for most 2D art, which is "
                             "drawn at its native size."),
                makeProperty("premultiplyAlpha", "Premultiply Alpha", PropertyType::Boolean,
                             PropertyValue{true},
                             "XNA's SpriteBatch defaults to premultiplied blending, so this is on "
                             "by default; turning it off means also passing NonPremultiplied when "
                             "drawing, or the edges of every sprite will halo."),

                // Read-only, because it is a fact about the file rather than a choice about it.
                // Shown because "why is this sprite blurry" is usually answered by its size.
                [] {
                    PropertyDescriptor size = makeProperty("pixelSize", "Pixel Size",
                                                           PropertyType::Vector2,
                                                           PropertyValue{EditorVector2{}},
                                                           "Read from the file's header. Zero means the format is one the editor cannot measure yet.");
                    size.readOnly = true;
                    return size;
                }(),
            };

            descriptor.unique = true;
            return descriptor;
        }

        ComponentDescriptor makeSpriteFontImporter()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = ImporterIds::kSpriteFont;
            descriptor.displayName = "Sprite Font Importer";
            descriptor.category = "Import";

            // Every field here is read-only, and that is the design rather than an omission. A
            // `.spritefont` is the content pipeline's own input: it already declares the font, the
            // size, the spacing and the character range, and an editable copy in the sidecar would
            // be a second answer to a question the build asks the file. So the editor reports what
            // the file says instead -- which is the part a person actually wants, since "what font
            // is this and does it cover the characters I need" is otherwise an XML file away.
            const auto fact = [](std::string name, std::string display, PropertyType type,
                                 PropertyValue defaultValue, std::string tooltip) {
                PropertyDescriptor property = makeProperty(std::move(name), std::move(display), type,
                                                           std::move(defaultValue), std::move(tooltip));
                property.readOnly = true;
                return property;
            };

            descriptor.properties = {
                fact("fontName", "Font", PropertyType::String, PropertyValue{std::string{}},
                     "The typeface the .spritefont asks the content pipeline to rasterise."),
                fact("pointSize", "Size", PropertyType::Float, PropertyValue{0.0f}, "In points."),
                fact("spacing", "Spacing", PropertyType::Float, PropertyValue{0.0f},
                     "Extra horizontal space between glyphs, in pixels."),
                fact("useKerning", "Kerning", PropertyType::Boolean, PropertyValue{true},
                     "Whether the pipeline applies the font's own kerning pairs."),
                fact("characterRange", "Characters", PropertyType::String, PropertyValue{std::string{}},
                     "The first CharacterRegion the file declares, as a code range. Answers "
                     "\"why does this text render as boxes\" without opening the XML."),
            };
            return descriptor;
        }

        ComponentDescriptor makeSoundEffectImporter()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = ImporterIds::kSoundEffect;
            descriptor.displayName = "Sound Effect Importer";
            descriptor.category = "Import";

            PropertyDescriptor volume = makeProperty("importVolume", "Import Volume",
                                                     PropertyType::Float, PropertyValue{1.0f},
                                                     "Applied once at import rather than at every "
                                                     "play, so a clip that is simply too loud is "
                                                     "fixed in one place.");
            volume.minimum = 0.0;
            volume.maximum = 1.0;

            descriptor.properties = {
                std::move(volume),
                makeProperty("loadIntoMemory", "Load Into Memory", PropertyType::Boolean,
                             PropertyValue{true},
                             "Off streams the clip from disk instead, which is right for music-"
                             "length audio and wrong for a footstep."),
            };
            return descriptor;
        }

        ComponentDescriptor makeModelImporter()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = ImporterIds::kModel;
            descriptor.displayName = "Model Importer";
            descriptor.category = "Import";

            PropertyDescriptor scale = makeProperty("scaleFactor", "Scale Factor",
                                                    PropertyType::Float, PropertyValue{1.0f},
                                                    "Applied at import, so a model authored in "
                                                    "centimetres does not need scaling on every "
                                                    "entity that uses it.");
            scale.minimum = 0.0001;
            scale.maximum = 10000.0;

            descriptor.properties = {
                std::move(scale),
                makeProperty("importMaterials", "Import Materials", PropertyType::Boolean,
                             PropertyValue{true}),
                makeProperty("importAnimations", "Import Animations", PropertyType::Boolean,
                             PropertyValue{true}),
            };
            return descriptor;
        }
    }

    std::optional<ImageSize> readImageSize(const std::string& absolutePath)
    {
        std::ifstream stream{absolutePath, std::ios::binary};
        if (!stream) { return std::nullopt; }

        std::array<char, 32> header{};
        stream.read(header.data(), static_cast<std::streamsize>(header.size()));
        const std::size_t read = static_cast<std::size_t>(stream.gcount());

        const auto byteAt = [&](std::size_t index) {
            return static_cast<std::uint32_t>(static_cast<unsigned char>(header[index]));
        };

        // PNG: an 8-byte signature, then an IHDR chunk whose first two fields are the width and
        // the height, big-endian. Fixed offsets, so no parsing is needed.
        static const std::array<unsigned char, 8> kPngSignature{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (read >= 24)
        {
            bool isPng = true;
            for (std::size_t index = 0; index < kPngSignature.size(); ++index)
            {
                if (byteAt(index) != kPngSignature[index]) { isPng = false; break; }
            }

            if (isPng)
            {
                const auto bigEndian = [&](std::size_t at) {
                    return static_cast<int>((byteAt(at) << 24) | (byteAt(at + 1) << 16)
                                            | (byteAt(at + 2) << 8) | byteAt(at + 3));
                };
                return ImageSize{bigEndian(16), bigEndian(20)};
            }
        }

        // BMP: "BM", then a DIB header whose width and height are little-endian at fixed offsets.
        // The height is signed and negative for a top-down image, so its magnitude is what counts.
        if (read >= 26 && byteAt(0) == 'B' && byteAt(1) == 'M')
        {
            const auto littleEndian = [&](std::size_t at) {
                return static_cast<std::int32_t>(byteAt(at) | (byteAt(at + 1) << 8)
                                                 | (byteAt(at + 2) << 16) | (byteAt(at + 3) << 24));
            };
            const std::int32_t height = littleEndian(22);
            return ImageSize{static_cast<int>(littleEndian(18)),
                             static_cast<int>(height < 0 ? -height : height)};
        }

        return std::nullopt;
    }

    bool Detail::applySpriteFontFacts(AssetDatabase& assets, const AssetRecord& record)
    {
        const std::optional<SpriteFontDescription> description =
            readSpriteFontDescription(assets.resolvePath(record.sourcePath));
        if (!description) { return false; }

        JsonValue facts = JsonValue::makeObject();
        facts.set("fontName", JsonValue{description->fontName});
        facts.set("pointSize", JsonValue{static_cast<double>(description->pointSize)});
        facts.set("spacing", JsonValue{static_cast<double>(description->spacing)});
        facts.set("useKerning", JsonValue{description->useKerning});
        facts.set("characterRange", JsonValue{std::to_string(description->firstCharacter) + "-"
                                              + std::to_string(description->lastCharacter)});

        // Compared before writing, so that opening a project twice produces no diff -- the same
        // rule the texture facts follow, and the one that keeps `--headless` safe to run against a
        // repository you want left alone.
        bool unchanged = true;
        for (const auto& [name, value] : facts.getMembers())
        {
            if (Json::write(record.importerSettings[name], false) != Json::write(value, false))
            {
                unchanged = false;
                break;
            }
        }
        if (unchanged) { return false; }

        AssetRecord* mutableRecord = assets.findMutable(record.id);
        if (mutableRecord == nullptr) { return false; }

        if (mutableRecord->importerSettings.isNull())
        {
            mutableRecord->importerSettings = JsonValue::makeObject();
        }
        for (const auto& [name, value] : facts.getMembers())
        {
            mutableRecord->importerSettings.set(name, value);
        }
        assets.writeSidecar(record.id);
        return true;
    }

    std::optional<SpriteFontDescription> readSpriteFontDescription(const std::string& path)
    {
        std::ifstream stream{path, std::ios::binary};
        if (!stream) { return std::nullopt; }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const std::string text = buffer.str();

        // The one structural check. Without it any XML file with a <Size> element would be read as
        // a sprite font, and the inspector would report confident nonsense about it. Matched on the
        // suffix because XNA writes `Graphics:FontDescription` while hand-written and
        // MonoGame-flavoured files often say `SpriteFontDescription`; both are the same asset.
        if (text.find("<Asset") == std::string::npos
            || text.find("FontDescription") == std::string::npos)
        {
            return std::nullopt;
        }

        SpriteFontDescription description;
        description.fontName = Detail::readTag(text, "FontName");

        const std::string size = Detail::readTag(text, "Size");
        if (!size.empty()) { description.pointSize = Detail::toFloat(size); }

        const std::string spacing = Detail::readTag(text, "Spacing");
        if (!spacing.empty()) { description.spacing = Detail::toFloat(spacing); }

        const std::string kerning = Detail::readTag(text, "UseKerning");
        if (!kerning.empty()) { description.useKerning = kerning != "false" && kerning != "False"; }

        description.firstCharacter = Detail::toCharacterCode(Detail::readTag(text, "Start"));
        description.lastCharacter = Detail::toCharacterCode(Detail::readTag(text, "End"));
        return description;
    }

    std::size_t applyImporterFacts(AssetDatabase& assets)
    {
        std::size_t changed = 0;

        for (const AssetRecord* record : assets.getAll())
        {
            if (record->type == AssetType::SpriteFont)
            {
                changed += Detail::applySpriteFontFacts(assets, *record) ? 1 : 0;
                continue;
            }
            if (record->type != AssetType::Texture2D) { continue; }

            const std::optional<ImageSize> size = readImageSize(assets.resolvePath(record->sourcePath));
            if (!size) { continue; }

            const EditorVector2 measured{static_cast<float>(size->width),
                                         static_cast<float>(size->height)};

            const JsonValue& stored = record->importerSettings["pixelSize"];
            if (!stored.isNull()
                && PropertyValue::fromJson(stored, PropertyType::Vector2).get<EditorVector2>() == measured)
            {
                continue;
            }

            AssetRecord* mutableRecord = assets.findMutable(record->id);
            if (mutableRecord == nullptr) { continue; }

            if (mutableRecord->importerSettings.isNull())
            {
                mutableRecord->importerSettings = JsonValue::makeObject();
            }
            mutableRecord->importerSettings.set("pixelSize", PropertyValue{measured}.toJson());
            assets.writeSidecar(record->id);
            ++changed;
        }

        return changed;
    }

    void registerBuiltinImporters(ComponentRegistry& registry)
    {
        registry.registerComponent(makeTextureImporter());
        registry.registerComponent(makeSpriteFontImporter());
        registry.registerComponent(makeSoundEffectImporter());
        registry.registerComponent(makeModelImporter());
    }
}
