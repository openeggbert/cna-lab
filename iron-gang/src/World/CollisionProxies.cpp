#include "IronGang/World/CollisionProxies.hpp"

#include "../Core/JsonDataFileInternal.hpp"
#include "../Core/JsonReadHelpers.hpp"

namespace IronGang
{
    using System::Text::Json::JsonElement;
    using System::Text::Json::JsonValueKind;

    bool CollisionProxySet::LoadFromFile(const std::string& path, std::string& errorMessage)
    {
        JsonDataFile file;
        if (!LoadJsonDataFile(path, file, errorMessage))
        {
            return false;
        }
        const JsonElement& root = file.root;

        std::string id;
        std::vector<CollisionProxy> proxies;
        try
        {
            if (!JsonRead::OnlyFields(root, {"id", "version", "proxies"}, "collision proxies", path,
                                      errorMessage))
            {
                return false;
            }
            double version = 0.0;
            if (!JsonRead::NumberField(root, "version", version) ||
                static_cast<int>(version) != kCollisionProxyFileVersion)
            {
                errorMessage = "collision proxy file version must be " +
                               std::to_string(kCollisionProxyFileVersion) + ": " + path;
                return false;
            }
            if (!JsonRead::StringField(root, "id", id))
            {
                errorMessage = "collision proxy file has no non-empty \"id\": " + path;
                return false;
            }

            JsonElement proxiesElement;
            if (!root.TryGetProperty("proxies", proxiesElement) ||
                proxiesElement.getValueKindProperty() != JsonValueKind::Array)
            {
                errorMessage = "collision proxy file has no \"proxies\" array: " + path;
                return false;
            }
            for (const JsonElement& entry : proxiesElement.EnumerateArray())
            {
                if (!JsonRead::OnlyFields(entry, {"name", "center", "halfExtents"}, "a collision proxy",
                                          path, errorMessage))
                {
                    return false;
                }
                CollisionProxy proxy;
                JsonElement centerElement;
                JsonElement extentsElement;
                if (!JsonRead::StringField(entry, "name", proxy.name) ||
                    !entry.TryGetProperty("center", centerElement) ||
                    !JsonRead::Vector3Field(centerElement, proxy.bounds.center) ||
                    !entry.TryGetProperty("halfExtents", extentsElement) ||
                    !JsonRead::Vector3Field(extentsElement, proxy.bounds.halfExtents))
                {
                    errorMessage = "every collision proxy needs a \"name\" and three-number "
                                   "\"center\" and \"halfExtents\": " + path;
                    return false;
                }
                if (!(proxy.bounds.halfExtents.X > 0.0F) || !(proxy.bounds.halfExtents.Y > 0.0F) ||
                    !(proxy.bounds.halfExtents.Z > 0.0F))
                {
                    // A zero-thickness collider is one objects tunnel straight through, which is
                    // worse than no collider at all because it looks like there is one.
                    errorMessage = "collision proxy \"" + proxy.name +
                                   "\" has a non-positive half-extent: " + path;
                    return false;
                }
                proxies.push_back(std::move(proxy));
            }
        }
        catch (const std::exception& exception)
        {
            errorMessage = std::string("failed to read collision proxies ") + path + ": " +
                           exception.what();
            return false;
        }

        id_ = std::move(id);
        proxies_ = std::move(proxies);
        return true;
    }
}
