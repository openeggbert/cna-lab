#include "CopperBoots/CnaSettingsStore.hpp"

#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/StreamReader.hpp"
#include "System/IO/StreamWriter.hpp"

namespace CopperBoots
{
    namespace
    {
        constexpr std::string_view SettingsFileName = "settings.cfg";
        constexpr std::string_view ContainerName = "Settings";

        using Microsoft::Xna::Framework::Storage::StorageContainer;
        using Microsoft::Xna::Framework::Storage::StorageDevice;

        struct OpenStorage
        {
            std::unique_ptr<StorageDevice> Device;
            std::unique_ptr<StorageContainer> Container;
        };

        [[nodiscard]] OpenStorage OpenSettingsContainer()
        {
            StorageDevice::SetAppNameEXT("CopperBoots");
            auto selector = StorageDevice::BeginShowSelector(nullptr, nullptr);
            auto device = StorageDevice::EndShowSelector(selector.get());
            auto opening = device->BeginOpenContainer(
                std::string(ContainerName), nullptr, nullptr);
            auto container = device->EndOpenContainer(opening.get());
            return {std::move(device), std::move(container)};
        }
    }

    SettingsLoadResult CnaSettingsStore::Load()
    {
        OpenStorage storage = OpenSettingsContainer();
        if (!storage.Container->FileExists(std::string(SettingsFileName)))
            return DecodeSettings(std::nullopt);
        auto stream = storage.Container->OpenFile(
            std::string(SettingsFileName), System::IO::FileMode::Open,
            System::IO::FileAccess::Read);
        System::IO::StreamReader reader(stream.get(), true);
        const std::string document = reader.ReadToEnd();
        return DecodeSettings(document);
    }

    void CnaSettingsStore::Save(const GameSettings& settings)
    {
        OpenStorage storage = OpenSettingsContainer();
        auto stream = storage.Container->CreateFile(
            std::string(SettingsFileName));
        System::IO::StreamWriter writer(stream.get(), true);
        writer.Write(EncodeSettings(settings));
        writer.Flush();
    }
}
