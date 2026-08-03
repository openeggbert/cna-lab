// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Core/ComponentDescriptor.hpp"

#include <algorithm>

namespace CNA::Editor
{
    const PropertyDescriptor* ComponentDescriptor::findProperty(std::string_view name) const
    {
        const auto found = std::find_if(properties.begin(), properties.end(),
                                        [&](const PropertyDescriptor& property) { return property.name == name; });
        return found == properties.end() ? nullptr : &*found;
    }

    bool ComponentRegistry::registerComponent(ComponentDescriptor descriptor)
    {
        if (descriptor.typeId.empty()) { return false; }
        const std::string key = descriptor.typeId;
        descriptors_[key] = std::move(descriptor);
        return true;
    }

    const ComponentDescriptor* ComponentRegistry::find(std::string_view typeId) const
    {
        const auto found = descriptors_.find(std::string{typeId});
        return found == descriptors_.end() ? nullptr : &found->second;
    }

    std::vector<std::string> ComponentRegistry::getTypeIds() const
    {
        std::vector<std::string> ids;
        ids.reserve(descriptors_.size());
        for (const auto& [typeId, descriptor] : descriptors_) { ids.push_back(typeId); }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    bool ComponentRegistry::unregisterComponent(std::string_view typeId)
    {
        return descriptors_.erase(std::string{typeId}) > 0;
    }
}
