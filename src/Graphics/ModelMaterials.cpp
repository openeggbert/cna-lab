#include "IronGang/Graphics/ModelMaterials.hpp"

#include "../Core/JsonDataFileInternal.hpp"

#include "System/Text/Json/JsonProperty.hpp"

#include <algorithm>

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    namespace
    {
        [[nodiscard]] bool ReadColor(const JsonElement& element, Vector3& out, std::string& error)
        {
            if (element.getValueKindProperty() != JsonValueKind::Array)
            {
                error = "\"baseColor\" must be an array of three numbers";
                return false;
            }
            float components[3] = {0.0F, 0.0F, 0.0F};
            std::size_t count = 0;
            for (const JsonElement& entry : element.EnumerateArray())
            {
                if (count >= 3 || entry.getValueKindProperty() != JsonValueKind::Number)
                {
                    error = "\"baseColor\" must be exactly three numbers";
                    return false;
                }
                const double value = entry.GetDouble();
                if (!(value >= 0.0) || !(value <= 1.0))
                {
                    // Out of range is not a colour anyone meant: it would either clip silently or
                    // make the model brighter than the sun the rest of the scene is lit by.
                    error = "\"baseColor\" components must be within [0, 1]";
                    return false;
                }
                components[count] = static_cast<float>(value);
                ++count;
            }
            if (count != 3)
            {
                error = "\"baseColor\" must be exactly three numbers";
                return false;
            }
            out = Vector3(components[0], components[1], components[2]);
            return true;
        }
    }

    bool ModelMaterialTable::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }
        const JsonElement& root = file.root;

        std::vector<ModelMaterial> materials;
        try
        {
            for (const auto& property : root.EnumerateObject())
            {
                const std::string name = property.getNameProperty();
                if (name != "version" && name != "models")
                {
                    errorMessage = "unknown field \"" + name + "\" in model materials: " + path;
                    return false;
                }
            }

            JsonElement versionElement;
            if (!root.TryGetProperty("version", versionElement) ||
                versionElement.getValueKindProperty() != JsonValueKind::Number)
            {
                errorMessage = "model materials has no numeric \"version\": " + path;
                return false;
            }
            const int version = versionElement.GetInt32();
            if (version != kModelMaterialsFileVersion)
            {
                errorMessage = "model materials version " + std::to_string(version) +
                               " is not supported (expected " + std::to_string(kModelMaterialsFileVersion) +
                               "): " + path;
                return false;
            }

            JsonElement modelsElement;
            if (!root.TryGetProperty("models", modelsElement) ||
                modelsElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "model materials has no \"models\" array: " + path;
                return false;
            }

            for (const JsonElement& entry : modelsElement.EnumerateArray())
            {
                for (const auto& property : entry.EnumerateObject())
                {
                    const std::string name = property.getNameProperty();
                    if (name != "modelId" && name != "baseColor")
                    {
                        errorMessage = "unknown field \"" + name + "\" in a model material: " + path;
                        return false;
                    }
                }

                ModelMaterial material;
                JsonElement idElement;
                if (!entry.TryGetProperty("modelId", idElement) ||
                    idElement.getValueKindProperty() != JsonValueKind::String ||
                    idElement.GetString().empty())
                {
                    errorMessage = "every model material needs a non-empty \"modelId\": " + path;
                    return false;
                }
                material.modelId = idElement.GetString();
                if (std::any_of(materials.begin(), materials.end(),
                                [&material](const ModelMaterial& existing) {
                                    return existing.modelId == material.modelId;
                                }))
                {
                    errorMessage = "duplicate model id \"" + material.modelId + "\": " + path;
                    return false;
                }

                JsonElement colorElement;
                if (!entry.TryGetProperty("baseColor", colorElement))
                {
                    errorMessage = "model material \"" + material.modelId + "\" has no \"baseColor\": " + path;
                    return false;
                }
                std::string colorError;
                if (!ReadColor(colorElement, material.baseColor, colorError))
                {
                    errorMessage = "model material \"" + material.modelId + "\": " + colorError + ": " + path;
                    return false;
                }
                materials.push_back(std::move(material));
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string("failed to read model materials ") + path + ": " + exception.what();
            return false;
        }

        materials_ = std::move(materials);
        return true;
    }

    Vector3 ModelMaterialTable::GetBaseColor(const std::string& modelId) const
    {
        const auto found = std::find_if(materials_.begin(), materials_.end(),
                                        [&modelId](const ModelMaterial& material) {
                                            return material.modelId == modelId;
                                        });
        return found == materials_.end() ? Vector3(1.0F, 1.0F, 1.0F) : found->baseColor;
    }

    bool ModelMaterialTable::Contains(const std::string& modelId) const
    {
        return std::any_of(materials_.begin(), materials_.end(),
                           [&modelId](const ModelMaterial& material) { return material.modelId == modelId; });
    }
}
