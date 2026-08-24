#include "WolfGame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string_view>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioChannels.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"

namespace WolfCna
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Audio;
    using namespace Microsoft::Xna::Framework::Graphics;
    using namespace Microsoft::Xna::Framework::Input;

    namespace
    {
        constexpr int PanelSize = 64;
        constexpr int PanelCount = World::MaterialPanelCount;
        constexpr int AtlasWidth = PanelSize * PanelCount;
        constexpr int AtlasHeight = PanelSize;
        constexpr std::array<std::string_view, 4> CampaignLevelFiles = {
            "assets/levels/starter.level",
            "assets/levels/sector-02.level",
            "assets/levels/sector-03.level",
            "assets/levels/sector-04.level"
        };
        constexpr std::array<std::string_view, 4> CampaignLevelNames = {
            "SECTOR 1 STORAGE",
            "SECTOR 2 FOUNDRY",
            "SECTOR 3 LABS",
            "SECTOR 4 ARCHIVE"
        };
        constexpr std::string_view ProgressFile = "wolf-cna-progress.dat";
        constexpr float KnifeAttackVisualSeconds = 0.11f;
        constexpr float SidearmAttackVisualSeconds = 0.13f;
        constexpr float RepeaterAttackVisualSeconds = 0.14f;
        constexpr float HeavyAttackVisualSeconds = 0.17f;

        int Noise(int x, int y)
        {
            std::uint32_t n = static_cast<std::uint32_t>(x * 374761393u + y * 668265263u);
            n = (n ^ (n >> 13u)) * 1274126177u;
            return static_cast<int>((n ^ (n >> 16u)) & 15u);
        }

        std::uint8_t ByteClamp(int value)
        {
            return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
        }

        std::vector<SharpRuntime::bytecs> MakeTone(float frequency, int sampleCount)
        {
            constexpr float sampleRate = 22050.0f;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const float phase = 2.0f * MathHelper::Pi * frequency * sampleIndex / sampleRate;
                const float envelope = 1.0f - static_cast<float>(sampleIndex) / sampleCount;
                const auto sample = static_cast<std::int16_t>(std::sin(phase) * envelope * 14000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }

            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeGunshot()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr int sampleCount = 4200;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));
            std::uint32_t noiseState = 0x4f1bbcdcu;

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                noiseState = noiseState * 1664525u + 1013904223u;
                const float noise = static_cast<float>(static_cast<int>((noiseState >> 16u) & 0xffffu) - 32768) / 32768.0f;
                const float progress = static_cast<float>(sampleIndex) / sampleCount;
                const float envelope = (1.0f - progress) * (1.0f - progress);
                const float body = std::sin(2.0f * MathHelper::Pi * 92.0f * sampleIndex / sampleRate);
                const auto sample = static_cast<std::int16_t>(
                    std::clamp((noise * 0.78f + body * 0.22f) * envelope, -1.0f, 1.0f) * 28000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }
            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeDoorMovement()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr int sampleCount = 9000;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const float time = static_cast<float>(sampleIndex) / sampleRate;
                const float progress = static_cast<float>(sampleIndex) / sampleCount;
                const float motor = std::sin(2.0f * MathHelper::Pi * (72.0f + 24.0f * progress) * time);
                const float teeth = std::sin(2.0f * MathHelper::Pi * 310.0f * time) > 0.0f ? 1.0f : -1.0f;
                const float envelope = std::sin(MathHelper::Pi * progress);
                const auto sample = static_cast<std::int16_t>((motor * 0.72f + teeth * 0.18f) * envelope * 22000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }
            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeAmmoPickup()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr int sampleCount = 5200;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const float progress = static_cast<float>(sampleIndex) / sampleCount;
                const float frequency = progress < 0.5f ? 440.0f : 660.0f;
                const float phase = 2.0f * MathHelper::Pi * frequency * sampleIndex / sampleRate;
                const float envelope = 1.0f - progress;
                const auto sample = static_cast<std::int16_t>(std::sin(phase) * envelope * 23000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }
            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeProjectileImpact()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr int sampleCount = 2400;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));
            std::uint32_t noiseState = 0xa13759bdu;

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                noiseState = noiseState * 1664525u + 1013904223u;
                const float noise = static_cast<float>(
                    static_cast<int>((noiseState >> 16u) & 0xffffu) - 32768) / 32768.0f;
                const float progress = static_cast<float>(sampleIndex) / sampleCount;
                const float envelope = (1.0f - progress) * (1.0f - progress);
                const float spark = std::sin(
                    2.0f * MathHelper::Pi * (920.0f - progress * 540.0f) *
                    static_cast<float>(sampleIndex) / sampleRate);
                const auto sample = static_cast<std::int16_t>(
                    std::clamp((noise * 0.58f + spark * 0.42f) * envelope, -1.0f, 1.0f) *
                    19000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }
            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeSectorCompletionFanfare()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr float durationSeconds = 1.9f;
            constexpr int sampleCount = static_cast<int>(sampleRate * durationSeconds);
            constexpr std::array<float, 4> melodyFrequencies = {
                293.66f,
                369.99f,
                440.0f,
                587.33f};
            constexpr float noteSeconds = 0.27f;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const float time = static_cast<float>(sampleIndex) / sampleRate;
                float signal = 0.0f;

                for (std::size_t note = 0; note < melodyFrequencies.size(); ++note)
                {
                    const float noteStart = static_cast<float>(note) * 0.25f;
                    const float noteTime = time - noteStart;
                    if (noteTime < 0.0f || noteTime >= noteSeconds)
                        continue;

                    const float attack = std::min(1.0f, noteTime / 0.025f);
                    const float release = std::min(1.0f, (noteSeconds - noteTime) / 0.08f);
                    const float envelope = attack * release;
                    const float frequency = melodyFrequencies[note];
                    const float bright = std::sin(2.0f * MathHelper::Pi * frequency * noteTime);
                    const float brass = std::sin(2.0f * MathHelper::Pi * frequency * 2.0f * noteTime);
                    signal += (bright * 0.72f + brass * 0.18f) * envelope;
                }

                const float chordTime = time - 0.98f;
                if (chordTime >= 0.0f)
                {
                    const float chordProgress = chordTime / (durationSeconds - 0.98f);
                    const float envelope = std::max(0.0f, 1.0f - chordProgress);
                    constexpr std::array<float, 3> chord = {293.66f, 369.99f, 440.0f};
                    for (const float frequency : chord)
                        signal += std::sin(2.0f * MathHelper::Pi * frequency * chordTime) * envelope * 0.28f;
                }

                const auto sample = static_cast<std::int16_t>(
                    std::clamp(signal, -1.0f, 1.0f) * 21000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }
            return pcm;
        }

        std::vector<SharpRuntime::bytecs> MakeAmbientLoop()
        {
            constexpr float sampleRate = 22050.0f;
            constexpr int sampleCount = 44100;
            std::vector<SharpRuntime::bytecs> pcm;
            pcm.reserve(static_cast<std::size_t>(sampleCount * 2));

            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const float time = static_cast<float>(sampleIndex) / sampleRate;
                const float drone = std::sin(2.0f * MathHelper::Pi * 55.0f * time) * 0.18f;
                const float pulse = std::sin(2.0f * MathHelper::Pi * 82.5f * time) * 0.10f;
                const float signal = std::sin(2.0f * MathHelper::Pi * 220.0f * time) *
                    (0.025f + 0.02f * std::sin(2.0f * MathHelper::Pi * 0.5f * time));
                const auto sample = static_cast<std::int16_t>((drone + pulse + signal) * 9000.0f);
                pcm.push_back(static_cast<SharpRuntime::bytecs>(sample & 0xff));
                pcm.push_back(static_cast<SharpRuntime::bytecs>((sample >> 8) & 0xff));
            }

            return pcm;
        }

        std::array<std::string_view, 7> Glyph(char c)
        {
            switch (c)
            {
            case 'A': return {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
            case 'B': return {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
            case 'C': return {"01111", "10000", "10000", "10000", "10000", "10000", "01111"};
            case 'D': return {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
            case 'E': return {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
            case 'F': return {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
            case 'G': return {"01111", "10000", "10000", "10111", "10001", "10001", "01110"};
            case 'H': return {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
            case 'I': return {"11111", "00100", "00100", "00100", "00100", "00100", "11111"};
            case 'J': return {"00111", "00010", "00010", "00010", "10010", "10010", "01100"};
            case 'K': return {"10001", "10010", "10100", "11000", "10100", "10010", "10001"};
            case 'L': return {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
            case 'M': return {"10001", "11011", "10101", "10101", "10001", "10001", "10001"};
            case 'N': return {"10001", "11001", "10101", "10011", "10001", "10001", "10001"};
            case 'O': return {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
            case 'P': return {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
            case 'Q': return {"01110", "10001", "10001", "10001", "10101", "10010", "01101"};
            case 'R': return {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
            case 'S': return {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
            case 'T': return {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
            case 'U': return {"10001", "10001", "10001", "10001", "10001", "10001", "01110"};
            case 'V': return {"10001", "10001", "10001", "10001", "10001", "01010", "00100"};
            case 'W': return {"10001", "10001", "10001", "10101", "10101", "10101", "01010"};
            case 'X': return {"10001", "10001", "01010", "00100", "01010", "10001", "10001"};
            case 'Y': return {"10001", "10001", "01010", "00100", "00100", "00100", "00100"};
            case 'Z': return {"11111", "00001", "00010", "00100", "01000", "10000", "11111"};
            case '0': return {"01110", "10001", "10011", "10101", "11001", "10001", "01110"};
            case '1': return {"00100", "01100", "00100", "00100", "00100", "00100", "01110"};
            case '2': return {"01110", "10001", "00001", "00010", "00100", "01000", "11111"};
            case '3': return {"11110", "00001", "00001", "01110", "00001", "00001", "11110"};
            case '4': return {"00010", "00110", "01010", "10010", "11111", "00010", "00010"};
            case '5': return {"11111", "10000", "10000", "11110", "00001", "00001", "11110"};
            case '6': return {"01110", "10000", "10000", "11110", "10001", "10001", "01110"};
            case '7': return {"11111", "00001", "00010", "00100", "01000", "01000", "01000"};
            case '8': return {"01110", "10001", "10001", "01110", "10001", "10001", "01110"};
            case '9': return {"01110", "10001", "10001", "01111", "00001", "00001", "01110"};
            case '%': return {"11001", "11010", "00100", "01000", "10110", "00110", "00000"};
            case '/': return {"00001", "00010", "00010", "00100", "01000", "01000", "10000"};
            case '>': return {"10000", "01000", "00100", "00010", "00100", "01000", "10000"};
            default: return {"00000", "00000", "00000", "00000", "00000", "00000", "00000"};
            }
        }

        void DrawHudText(
            SpriteBatch& batch,
            Texture2D& pixel,
            int x,
            int y,
            std::string_view text,
            Color color,
            int scale = 2)
        {
            for (char character : text)
            {
                const auto glyph = Glyph(character);
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < 5; ++column)
                        if (glyph[row][column] == '1')
                            batch.Draw(pixel, Rectangle(x + column * scale, y + row * scale, scale, scale), color);
                x += 6 * scale;
            }
        }

        int HudTextWidth(std::string_view text, int scale = 2)
        {
            return text.empty() ? 0 : static_cast<int>(text.size()) * 6 * scale - scale;
        }
    }

    WolfGame::WolfGame()
        : playerPosition_(world_.PlayerStart())
    {
        static_cast<void>(exploration_.Visit(playerPosition_.X, playerPosition_.Z));
        getContentProperty().setRootDirectoryProperty("Content");

        graphics_ = std::make_unique<GraphicsDeviceManager>(this);
        graphics_->setIsFullScreenProperty(false);
        graphics_->ApplyChanges();
        const CampaignProfile profile = CampaignProgress::Load(
            std::string(ProgressFile),
            static_cast<int>(CampaignLevelFiles.size()));
        highestUnlockedLevel_ = profile.highestUnlocked;
        soundVolumeStep_ = profile.soundVolume;
        fieldOfViewDegrees_ = profile.fieldOfView;
        difficulty_ = static_cast<Difficulty>(profile.difficulty);
    }

    const std::string& WolfGame::GetTypeName() const
    {
        static const std::string name = "WolfCna.WolfGame";
        return name;
    }

    void WolfGame::Initialize()
    {
        auto& device = getGraphicsDeviceProperty();
        device.setDepthStencilStateProperty(DepthStencilState::Default);
        device.setBlendStateProperty(BlendState::Opaque);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.getSamplerStatesProperty()[0] = SamplerState::PointClamp;

        Game::Initialize();
    }

    void WolfGame::LoadContent()
    {
        auto& device = getGraphicsDeviceProperty();

        effect_ = std::make_unique<BasicEffect>(device);
        world_.Upload(device);
        CreateProceduralAtlas();
        guardSprite_ = std::make_unique<Texture2D>("assets/sprites/security-guard.png", device);
        houndSprite_ = std::make_unique<Texture2D>("assets/sprites/security-hound.png", device);
        rapidTrooperSprite_ = std::make_unique<Texture2D>("assets/sprites/rapid-trooper.png", device);
        heavyUnitSprite_ = std::make_unique<Texture2D>("assets/sprites/heavy-unit.png", device);
        guardAttackSprite_ = std::make_unique<Texture2D>("assets/sprites/security-guard-attack.png", device);
        houndAttackSprite_ = std::make_unique<Texture2D>("assets/sprites/security-hound-attack.png", device);
        rapidTrooperAttackSprite_ = std::make_unique<Texture2D>("assets/sprites/rapid-trooper-attack.png", device);
        heavyUnitAttackSprite_ = std::make_unique<Texture2D>("assets/sprites/heavy-unit-attack.png", device);
        guardPainSprite_ = std::make_unique<Texture2D>("assets/sprites/security-guard-pain.png", device);
        houndPainSprite_ = std::make_unique<Texture2D>("assets/sprites/security-hound-pain.png", device);
        rapidTrooperPainSprite_ = std::make_unique<Texture2D>("assets/sprites/rapid-trooper-pain.png", device);
        heavyUnitPainSprite_ = std::make_unique<Texture2D>("assets/sprites/heavy-unit-pain.png", device);
        defeatedGuardSprite_ = std::make_unique<Texture2D>("assets/sprites/security-guard-defeated.png", device);
        defeatedHoundSprite_ = std::make_unique<Texture2D>("assets/sprites/security-hound-defeated.png", device);
        defeatedRapidTrooperSprite_ = std::make_unique<Texture2D>("assets/sprites/rapid-trooper-defeated.png", device);
        defeatedHeavyUnitSprite_ = std::make_unique<Texture2D>("assets/sprites/heavy-unit-defeated.png", device);
        ammoPickupSprite_ = std::make_unique<Texture2D>("assets/pickups/ammo-box.png", device);
        healthPickupSprite_ = std::make_unique<Texture2D>("assets/pickups/health-kit.png", device);
        goldBarsSprite_ = std::make_unique<Texture2D>("assets/pickups/gold-bars.png", device);
        goldenGobletSprite_ = std::make_unique<Texture2D>("assets/pickups/golden-goblet.png", device);
        peaceMedallionSprite_ = std::make_unique<Texture2D>("assets/pickups/peace-medallion.png", device);
        accessCardSprite_ = std::make_unique<Texture2D>("assets/props/access-card.png", device);
        repeaterPickupSprite_ = std::make_unique<Texture2D>("assets/props/repeater-pickup.png", device);
        heavyWeaponPickupSprite_ = std::make_unique<Texture2D>("assets/props/heavy-automatic-pickup.png", device);
        terminalSprite_ = std::make_unique<Texture2D>("assets/props/terminal.png", device);
        relaySprite_ = std::make_unique<Texture2D>("assets/props/power-relay.png", device);
        exitSprite_ = std::make_unique<Texture2D>("assets/props/sector-exit.png", device);
        enemyProjectileSprite_ = std::make_unique<Texture2D>("assets/props/enemy-projectile.png", device);
        storagePlantSprite_ = std::make_unique<Texture2D>("assets/decorations/storage-plant.png", device);
        foundryPlantSprite_ = std::make_unique<Texture2D>("assets/decorations/foundry-plant.png", device);
        labsPlantSprite_ = std::make_unique<Texture2D>("assets/decorations/labs-plant.png", device);
        archivePlantSprite_ = std::make_unique<Texture2D>("assets/decorations/archive-plant.png", device);
        titleBackground_ = std::make_unique<Texture2D>("assets/title/title-background.png", device);
        CreateProceduralBloodDecal();
        CreateProceduralDecorationTextures();
        CreateProceduralEnemyImpactTexture();
        CreateHudResources();
        SoundEffect::setMasterVolumeProperty(static_cast<float>(soundVolumeStep_) / 4.0f);
        CreateSoundEffects();

        Game::LoadContent();
    }

    void WolfGame::CreateProceduralAtlas()
    {
        auto& device = getGraphicsDeviceProperty();

        atlas_ = std::make_unique<Texture2D>(device, AtlasWidth, AtlasHeight);
        std::vector<Color> pixels(
            static_cast<std::size_t>(AtlasWidth * AtlasHeight),
            Color(0, 0, 0, 255));
        int floorR = 58;
        int floorG = 63;
        int floorB = 65;
        int ceilingR = 135;
        int ceilingG = 137;
        int ceilingB = 129;
        int doorR = 49;
        int doorG = 86;
        int doorB = 128;
        int securityR = 132;
        int securityG = 46;
        int securityB = 50;
        if (levelIndex_ == 1)
        {
            floorR = 45; floorG = 71; floorB = 58;
            ceilingR = 102; ceilingG = 126; ceilingB = 104;
            doorR = 44; doorG = 110; doorB = 97;
            securityR = 161; securityG = 96; securityB = 33;
        }
        else if (levelIndex_ == 2)
        {
            floorR = 53; floorG = 61; floorB = 82;
            ceilingR = 137; ceilingG = 151; ceilingB = 174;
            doorR = 67; doorG = 91; doorB = 151;
            securityR = 145; securityG = 47; securityB = 104;
        }
        else if (levelIndex_ == 3)
        {
            floorR = 62; floorG = 43; floorB = 68;
            ceilingR = 148; ceilingG = 123; ceilingB = 139;
            doorR = 118; doorG = 76; doorB = 54;
            securityR = 164; securityG = 53; securityB = 76;
        }

        for (int y = 0; y < PanelSize; ++y)
        {
            for (int x = 0; x < PanelSize; ++x)
            {
                const int noise = Noise(x, y);

                // Panel 4: dark steel floor tiles.
                {
                    const int ax = x + PanelSize * 4;
                    const bool seam = (x % 8) == 0 || (y % 8) == 0;
                    const int checker = ((x / 8) + (y / 8)) & 1;

                    Color c(0, 0, 0, 255);
                    if (seam)
                    {
                        c = Color(35, 39, 42, 255);
                    }
                    else
                    {
                        const int base = floorR + (checker ? 10 : 0);
                        c = Color(
                            ByteClamp(base + noise / 3),
                            ByteClamp(floorG + (checker ? 10 : 0) + noise / 3),
                            ByteClamp(floorB + (checker ? 10 : 0) + noise / 3),
                            ByteClamp(255));
                    }

                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = c;
                }

                // Panel 5: pale concrete ceiling with panel seams.
                {
                    const int ax = x + PanelSize * 5;
                    const bool seam = (x % 16) == 0 || (y % 16) == 0;

                    Color c(0, 0, 0, 255);
                    if (seam)
                    {
                        c = Color(ceilingR - 61, ceilingG - 61, ceilingB - 61, 255);
                    }
                    else
                    {
                        c = Color(
                            ByteClamp(ceilingR + noise),
                            ByteClamp(ceilingG + noise),
                            ByteClamp(ceilingB + noise),
                            ByteClamp(255));
                    }

                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = c;
                }

                // Panel 6: blue bunker door.
                {
                    const int ax = x + PanelSize * 6;
                    const bool seam = x < 3 || x > PanelSize - 4 || (y % 8) == 0;
                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = seam
                        ? Color(doorR / 2, doorG / 2, doorB / 2, 255)
                        : Color(ByteClamp(doorR + noise), ByteClamp(doorG + noise), ByteClamp(doorB + noise), ByteClamp(255));
                }

                // Panel 7: red security door.
                {
                    const int ax = x + PanelSize * 7;
                    const bool seam = x < 3 || x > PanelSize - 4 || (y % 8) == 0;
                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = seam
                        ? Color(securityR / 2, securityG / 2, securityB / 2, 255)
                        : Color(ByteClamp(securityR + noise), ByteClamp(securityG + noise), ByteClamp(securityB + noise), ByteClamp(255));
                }
            }
        }

        const auto copyMaterialPanel = [&](const std::string& path, int panel)
        {
            Texture2D source(path, device);
            const int sourceWidth = source.getWidthProperty();
            const int sourceHeight = source.getHeightProperty();
            std::vector<Color> sourcePixels(
                static_cast<std::size_t>(sourceWidth * sourceHeight),
                Color(0, 0, 0, 0));
            source.GetData(sourcePixels.data(), static_cast<int>(sourcePixels.size()));

            for (int y = 0; y < PanelSize; ++y)
            {
                const int sourceY = std::min(
                    sourceHeight - 1,
                    ((y * 2 + 1) * sourceHeight) / (PanelSize * 2));
                for (int x = 0; x < PanelSize; ++x)
                {
                    const int sourceX = std::min(
                        sourceWidth - 1,
                        ((x * 2 + 1) * sourceWidth) / (PanelSize * 2));
                    pixels[static_cast<std::size_t>(
                        y * AtlasWidth + panel * PanelSize + x)] =
                        sourcePixels[static_cast<std::size_t>(sourceY * sourceWidth + sourceX)];
                }
            }
        };

        copyMaterialPanel("assets/materials/wall-stone.png", 0);
        copyMaterialPanel("assets/materials/wall-brick.png", 1);
        copyMaterialPanel("assets/materials/wall-steel.png", 2);
        copyMaterialPanel("assets/materials/wall-lab.png", 3);
        copyMaterialPanel("assets/materials/table-wood.png", 8);

        atlas_->SetData(pixels.data(), static_cast<int>(pixels.size()));
    }

    void WolfGame::CreateProceduralBloodDecal()
    {
        constexpr int size = 64;
        auto& device = getGraphicsDeviceProperty();
        bloodDecal_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> pixels(
            static_cast<std::size_t>(size * size),
            Color(0, 0, 0, 0));

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float nx = (static_cast<float>(x) + 0.5f - size * 0.5f) / (size * 0.5f);
                const float ny = (static_cast<float>(y) + 0.5f - size * 0.5f) / (size * 0.5f);
                const float angle = std::atan2(ny, nx);
                const float radius = std::sqrt(nx * nx + ny * ny);
                const float irregularEdge =
                    0.67f + std::sin(angle * 5.0f) * 0.075f + std::sin(angle * 9.0f + 0.8f) * 0.045f;
                const bool mainPool = radius < irregularEdge;

                const auto inDrop = [nx, ny](float centerX, float centerY, float radiusValue)
                {
                    const float dx = nx - centerX;
                    const float dy = ny - centerY;
                    return dx * dx + dy * dy < radiusValue * radiusValue;
                };
                const bool droplets =
                    inDrop(-0.77f, 0.18f, 0.09f) ||
                    inDrop(0.72f, -0.34f, 0.075f) ||
                    inDrop(0.42f, 0.76f, 0.055f);
                if (!mainPool && !droplets)
                    continue;

                const int variation = Noise(x + 41, y + 73) / 2;
                pixels[static_cast<std::size_t>(y * size + x)] = Color(
                    112 + variation,
                    8 + variation / 3,
                    14 + variation / 2,
                    mainPool ? 218 : 196);
            }
        }

        bloodDecal_->SetData(pixels.data(), static_cast<int>(pixels.size()));
    }

    void WolfGame::CreateProceduralDecorationTextures()
    {
        constexpr int size = 64;
        auto& device = getGraphicsDeviceProperty();

        paintingTexture_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> paintingPixels(
            static_cast<std::size_t>(size * size),
            Color(0, 0, 0, 255));
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const bool outerFrame = x < 5 || x >= size - 5 || y < 5 || y >= size - 5;
                const bool innerFrame = x < 8 || x >= size - 8 || y < 8 || y >= size - 8;
                Color color(0, 0, 0, 255);
                if (outerFrame)
                    color = Color(48 + Noise(x, y) / 2, 24, 13, 255);
                else if (innerFrame)
                    color = Color(164, 111 + Noise(x, y), 42, 255);
                else
                {
                    const int localY = y - 8;
                    const int hillLine = 33 + static_cast<int>(5.0f * std::sin(static_cast<float>(x) * 0.18f));
                    const int sunX = x - 46;
                    const int sunY = y - 20;
                    if (sunX * sunX + sunY * sunY < 45)
                        color = Color(246, 191, 54, 255);
                    else if (y < hillLine)
                        color = Color(42 + localY / 2, 101 + localY, 151 + localY, 255);
                    else if (y < 48)
                        color = Color(41 + Noise(x, y), 102 + Noise(x + 9, y), 61, 255);
                    else
                        color = Color(24, 61 + Noise(x, y), 49, 255);
                }
                paintingPixels[static_cast<std::size_t>(y * size + x)] = color;
            }
        }
        paintingTexture_->SetData(paintingPixels.data(), static_cast<int>(paintingPixels.size()));

        peaceBannerTexture_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> bannerPixels(
            static_cast<std::size_t>(size * size),
            Color(0, 0, 0, 255));
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const int weave = ((x + y) & 3) == 0 ? 8 : 0;
                Color color(18, 72 + weave, 91 + weave, 255);
                if (x < 3 || x >= size - 3 || y < 3 || y >= size - 3)
                    color = Color(10, 36, 51, 255);

                const float dx = static_cast<float>(x) - 31.5f;
                const float dy = static_cast<float>(y) - 29.5f;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const bool ring = std::abs(distance - 20.0f) < 2.0f;
                const bool stem = std::abs(dx) < 2.0f && dy >= -1.0f && dy <= 20.0f;
                const bool branches = dy >= 0.0f && dy <= 15.0f && std::abs(std::abs(dx) - dy) < 2.2f;
                if (ring || stem || branches)
                    color = Color(226, 225, 198, 255);

                bannerPixels[static_cast<std::size_t>(y * size + x)] = color;
            }
        }
        peaceBannerTexture_->SetData(bannerPixels.data(), static_cast<int>(bannerPixels.size()));

        ceilingLampTexture_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> lampPixels(static_cast<std::size_t>(size * size), Color(0, 0, 0, 0));
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float dx = static_cast<float>(x) - 31.5f;
                const float dy = static_cast<float>(y) - 31.5f;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance > 28.0f)
                    continue;

                Color color = distance > 23.0f
                    ? Color(44, 48, 52, 255)
                    : distance > 18.0f
                        ? Color(119, 123, 116, 255)
                        : Color(255, 224 + Noise(x, y), 145 + Noise(x + 4, y), 245);
                if ((x % 8 == 0 || y % 8 == 0) && distance < 20.0f)
                    color = Color(194, 169, 106, 255);
                lampPixels[static_cast<std::size_t>(y * size + x)] = color;
            }
        }
        ceilingLampTexture_->SetData(lampPixels.data(), static_cast<int>(lampPixels.size()));

        lampLightTexture_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> lightPixels(static_cast<std::size_t>(size * size), Color(0, 0, 0, 0));
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float nx = (static_cast<float>(x) + 0.5f - size * 0.5f) / (size * 0.5f);
                const float ny = (static_cast<float>(y) + 0.5f - size * 0.5f) / (size * 0.5f);
                const float radius = std::sqrt(nx * nx + ny * ny);
                if (radius >= 1.0f)
                    continue;

                const float falloff = 1.0f - radius;
                const float softened = falloff * falloff * (3.0f - 2.0f * falloff);
                const int alpha = static_cast<int>(18.0f + softened * 78.0f);
                lightPixels[static_cast<std::size_t>(y * size + x)] = Color(
                    255,
                    224 + Noise(x + 17, y + 31) / 3,
                    142 + Noise(x + 43, y + 7) / 3,
                    alpha);
            }
        }
        lampLightTexture_->SetData(lightPixels.data(), static_cast<int>(lightPixels.size()));
    }

    void WolfGame::CreateHudResources()
    {
        auto& device = getGraphicsDeviceProperty();

        hudSpriteBatch_ = std::make_unique<SpriteBatch>(device);
        hudPixel_ = std::make_unique<Texture2D>(device, 1, 1);
        const Color pixel(255, 255, 255, 255);
        hudPixel_->SetData(&pixel, 1);

        knifeView_ = std::make_unique<Texture2D>("assets/weapons/knife.png", device);
        sidearmView_ = std::make_unique<Texture2D>("assets/weapons/sidearm.png", device);
        repeaterView_ = std::make_unique<Texture2D>("assets/weapons/repeater.png", device);
        heavyWeaponView_ = std::make_unique<Texture2D>("assets/weapons/heavy-automatic.png", device);
        knifeAttackView_ = std::make_unique<Texture2D>("assets/weapons/knife-attack.png", device);
        sidearmAttackView_ = std::make_unique<Texture2D>("assets/weapons/sidearm-attack.png", device);
        repeaterAttackView_ = std::make_unique<Texture2D>("assets/weapons/repeater-attack.png", device);
        heavyWeaponAttackView_ = std::make_unique<Texture2D>("assets/weapons/heavy-automatic-attack.png", device);
    }

    void WolfGame::CreateProceduralEnemyImpactTexture()
    {
        auto& device = getGraphicsDeviceProperty();
        constexpr int size = 64;
        enemyImpactSprite_ = std::make_unique<Texture2D>(device, size, size);
        std::vector<Color> pixels(static_cast<std::size_t>(size * size), Color(0, 0, 0, 0));

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const float dx = static_cast<float>(x) - 31.5f;
                const float dy = static_cast<float>(y) - 31.5f;
                const float radius = std::sqrt(dx * dx + dy * dy);
                const bool core = radius < 7.0f;
                const bool ring = std::abs(radius - 16.0f) < 2.6f;
                const bool rays = radius < 28.0f &&
                    (std::abs(dx) < 1.8f || std::abs(dy) < 1.8f ||
                     std::abs(std::abs(dx) - std::abs(dy)) < 1.8f);
                if (!core && !ring && !rays)
                    continue;

                const int alpha = core ? 245 : ring ? 210 : std::max(28, 190 - static_cast<int>(radius * 5.0f));
                pixels[static_cast<std::size_t>(y * size + x)] = core
                    ? Color(255, 249, 205, alpha)
                    : ring ? Color(112, 225, 255, alpha) : Color(255, 161, 72, alpha);
            }
        }
        enemyImpactSprite_->SetData(pixels.data(), static_cast<int>(pixels.size()));
    }

    void WolfGame::CreateSoundEffects()
    {
        shotSound_ = std::make_unique<SoundEffect>(
            MakeGunshot(),
            22050,
            AudioChannels::Mono);
        knifeSound_ = std::make_unique<SoundEffect>(
            MakeTone(185.0f, 2400),
            22050,
            AudioChannels::Mono);
        pickupSound_ = std::make_unique<SoundEffect>(
            MakeTone(660.0f, 2400),
            22050,
            AudioChannels::Mono);
        ammoPickupSound_ = std::make_unique<SoundEffect>(
            MakeAmmoPickup(),
            22050,
            AudioChannels::Mono);
        doorSound_ = std::make_unique<SoundEffect>(
            MakeDoorMovement(),
            22050,
            AudioChannels::Mono);
        lockedSound_ = std::make_unique<SoundEffect>(
            MakeTone(90.0f, 1800),
            22050,
            AudioChannels::Mono);
        hurtSound_ = std::make_unique<SoundEffect>(
            MakeTone(120.0f, 2200),
            22050,
            AudioChannels::Mono);
        enemyDefeatedSound_ = std::make_unique<SoundEffect>(
            MakeTone(105.0f, 3200),
            22050,
            AudioChannels::Mono);
        terminalSound_ = std::make_unique<SoundEffect>(
            MakeTone(520.0f, 4400),
            22050,
            AudioChannels::Mono);
        guardShotSound_ = std::make_unique<SoundEffect>(
            MakeTone(270.0f, 1700),
            22050,
            AudioChannels::Mono);
        enemyImpactSound_ = std::make_unique<SoundEffect>(
            MakeProjectileImpact(),
            22050,
            AudioChannels::Mono);
        secretSound_ = std::make_unique<SoundEffect>(
            MakeTone(790.0f, 4200),
            22050,
            AudioChannels::Mono);
        guardAlertSound_ = std::make_unique<SoundEffect>(
            MakeTone(360.0f, 3000),
            22050,
            AudioChannels::Mono);
        houndAlertSound_ = std::make_unique<SoundEffect>(
            MakeTone(175.0f, 2500),
            22050,
            AudioChannels::Mono);
        houndAttackSound_ = std::make_unique<SoundEffect>(
            MakeTone(105.0f, 1500),
            22050,
            AudioChannels::Mono);
        extraLifeSound_ = std::make_unique<SoundEffect>(
            MakeTone(880.0f, 5200),
            22050,
            AudioChannels::Mono);
        exitSound_ = std::make_unique<SoundEffect>(
            MakeTone(440.0f, 7000),
            22050,
            AudioChannels::Mono);
        completionFanfareSound_ = std::make_unique<SoundEffect>(
            MakeSectorCompletionFanfare(),
            22050,
            AudioChannels::Mono);
        ambientSound_ = std::make_unique<SoundEffect>(
            MakeAmbientLoop(),
            22050,
            AudioChannels::Mono);
        ambientInstance_ = std::make_unique<SoundEffectInstance>(ambientSound_->CreateInstance());
        ambientInstance_->setIsLoopedProperty(true);
        ambientInstance_->setVolumeProperty(0.12f);
        ambientInstance_->Play();
    }

    void WolfGame::DrawHud()
    {
        if (!hudSpriteBatch_ || !hudPixel_ || !sidearmView_ || !knifeView_ ||
            !repeaterView_ || !heavyWeaponView_ || !knifeAttackView_ ||
            !sidearmAttackView_ || !repeaterAttackView_ || !heavyWeaponAttackView_)
            return;

        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const int centerX = viewport.getXProperty() + viewport.getWidthProperty() / 2;
        const int centerY = viewport.getYProperty() + viewport.getHeightProperty() / 2;
        // Generated PNGs contain straight-alpha color data, so the matching blend
        // state is required to prevent RGB in transparent pixels from bleeding
        // into a white rectangle around muzzle flashes.
        hudSpriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::NonPremultiplied);
        const int panelHeight = 84;
        const int panelY = viewport.getYProperty() + viewport.getHeightProperty() - panelHeight;
        if (playerImpactFlashSeconds_ > 0.0f)
        {
            const int alpha = static_cast<int>(std::lround(
                std::clamp(playerImpactFlashSeconds_ / 0.18f, 0.0f, 1.0f) * 72.0f));
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(
                    viewport.getXProperty(),
                    viewport.getYProperty(),
                    viewport.getWidthProperty(),
                    viewport.getHeightProperty() - panelHeight),
                Color(255, 70, 32, alpha));
        }
        const int viewSize = std::clamp(viewport.getHeightProperty() / 3, 144, 236);
        Texture2D* idleTexture = weapon_ == Weapon::Knife
            ? knifeView_.get()
            : weapon_ == Weapon::Sidearm
                ? sidearmView_.get()
                : weapon_ == Weapon::Repeater ? repeaterView_.get() : heavyWeaponView_.get();
        Texture2D* attackTexture = weapon_ == Weapon::Knife
            ? knifeAttackView_.get()
            : weapon_ == Weapon::Sidearm
                ? sidearmAttackView_.get()
                : weapon_ == Weapon::Repeater ? repeaterAttackView_.get() : heavyWeaponAttackView_.get();
        Texture2D* viewTexture = weaponFlashSeconds_ > 0.0f ? attackTexture : idleTexture;
        int weaponSize = viewSize;
        int weaponX = centerX - weaponSize / 2;
        int weaponY = panelY - weaponSize + 18;
        if (weaponFlashSeconds_ > 0.0f)
        {
            const float actionDuration = weapon_ == Weapon::Knife
                ? KnifeAttackVisualSeconds
                : weapon_ == Weapon::Sidearm
                    ? SidearmAttackVisualSeconds
                    : weapon_ == Weapon::Repeater
                        ? RepeaterAttackVisualSeconds
                        : HeavyAttackVisualSeconds;
            const float remaining = std::clamp(weaponFlashSeconds_ / actionDuration, 0.0f, 1.0f);
            if (weapon_ == Weapon::Knife)
            {
                const float lungePhase = std::sin((1.0f - remaining) * MathHelper::Pi);
                const int lunge = static_cast<int>(std::lround(lungePhase * viewSize * 0.12f));
                weaponSize += lunge;
                weaponX = centerX - weaponSize / 2 - lunge / 3;
                weaponY = panelY - weaponSize + 18 - lunge / 2;
            }
            else
            {
                const int recoil = static_cast<int>(std::lround(remaining * viewSize * 0.065f));
                weaponY += recoil;
            }
        }
        const Rectangle weaponRectangle(weaponX, weaponY, weaponSize, weaponSize);
        if (weaponFlashSeconds_ > 0.0f && weapon_ != Weapon::Knife)
        {
            hudSpriteBatch_->Draw(
                *idleTexture,
                weaponRectangle,
                Color(255, 255, 255, 255));
            hudSpriteBatch_->Draw(
                *attackTexture,
                weaponRectangle,
                Color(255, 255, 255, 176));
        }
        else
        {
            hudSpriteBatch_->Draw(
                *viewTexture,
                weaponRectangle,
                Color(255, 255, 255, 255));
        }
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(viewport.getXProperty(), panelY, viewport.getWidthProperty(), panelHeight), Color(31, 62, 137, 255));
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(viewport.getXProperty(), panelY, viewport.getWidthProperty(), 3), Color(14, 25, 70, 255));
        const Color labelColor(188, 213, 255, 255);
        const Color valueColor(255, 233, 136, 255);
        const auto drawReadout = [&](int slot, std::string_view label, const std::string& value)
        {
            const int center = viewport.getXProperty() + viewport.getWidthProperty() * (slot * 2 + 1) / 12;
            DrawHudText(*hudSpriteBatch_, *hudPixel_, center - HudTextWidth(label) / 2, panelY + 17, label, labelColor);
            DrawHudText(*hudSpriteBatch_, *hudPixel_, center - HudTextWidth(value) / 2, panelY + 47, value, valueColor);
        };
        drawReadout(0, "LEVEL", std::to_string(levelIndex_ + 1));
        drawReadout(1, "SCORE", std::to_string(score_));
        drawReadout(2, "LIVES", std::to_string(lives_));
        drawReadout(3, "HEALTH", std::to_string(health_) + "%");
        drawReadout(4, "AMMO", std::to_string(ammo_));
        const int weaponCenter = viewport.getXProperty() + viewport.getWidthProperty() * 11 / 12;
        hudSpriteBatch_->Draw(
            *idleTexture,
            Rectangle(weaponCenter - 30, panelY + 12, 60, 60),
            Color(255, 255, 255, 255));
        if (cheatMessageSeconds_ > 0.0f)
        {
            constexpr std::string_view message = "LOADOUT READY";
            const int messageWidth = HudTextWidth(message);
            const int messageX = centerX - messageWidth / 2;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(messageX - 12, centerY - 26, messageWidth + 24, 31),
                Color(17, 59, 116, 255));
            DrawHudText(*hudSpriteBatch_, *hudPixel_, messageX, centerY - 18, message, Color(255, 233, 136, 255));
        }
        else if (objectiveMessageSeconds_ > 0.0f && !objectiveMessage_.empty())
        {
            const int messageWidth = HudTextWidth(objectiveMessage_);
            const int messageX = centerX - messageWidth / 2;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(messageX - 12, centerY - 26, messageWidth + 24, 31),
                Color(17, 59, 116, 255));
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                messageX,
                centerY - 18,
                objectiveMessage_,
                Color(125, 244, 186, 255));
        }
        if (completed_)
        {
            const World::CompletionStats stats = world_.GetCompletionStats();
            const std::array<std::string, 4> rows = {
                "KILLS " + std::to_string(stats.defeatedEnemies) + "/" + std::to_string(stats.totalEnemies),
                "TREASURE " + std::to_string(stats.collectedGold) + "/" + std::to_string(stats.totalGold),
                "SECRETS " + std::to_string(stats.foundSecrets) + "/" + std::to_string(stats.totalSecrets),
                "TIME " + std::to_string(static_cast<int>(levelElapsedSeconds_)) + "S"};
            const std::string_view prompt = levelIndex_ + 1 < static_cast<int>(CampaignLevelFiles.size())
                ? "SPACE NEXT"
                : "SPACE TITLE";
            const int cardTop = centerY - 94;
            constexpr int cardWidth = 260;
            constexpr int cardHeight = 180;
            const int cardLeft = centerX - cardWidth / 2;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(cardLeft, cardTop, cardWidth, cardHeight),
                Color(17, 59, 116, 255));
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(cardLeft, cardTop, cardWidth, 3),
                Color(184, 238, 255, 255));
            const std::string_view message = "LEVEL COMPLETE";
            const int messageWidth = HudTextWidth(message);
            const int messageX = centerX - messageWidth / 2;
            DrawHudText(*hudSpriteBatch_, *hudPixel_, messageX, cardTop + 16, message, Color(184, 238, 255, 255));
            for (int index = 0; index < static_cast<int>(rows.size()); ++index)
                DrawHudText(
                    *hudSpriteBatch_,
                    *hudPixel_,
                    centerX - HudTextWidth(rows[static_cast<std::size_t>(index)]) / 2,
                    cardTop + 48 + index * 22,
                    rows[static_cast<std::size_t>(index)],
                    Color(255, 233, 136, 255));
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                centerX - HudTextWidth(prompt) / 2,
                cardTop + 146,
                prompt,
                Color(255, 233, 136, 255));
        }
        else if (screen_ == Screen::Paused)
        {
            constexpr int cardWidth = 300;
            constexpr int cardHeight = 174;
            const int cardLeft = centerX - cardWidth / 2;
            const int cardTop = centerY - cardHeight / 2;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(cardLeft, cardTop, cardWidth, cardHeight),
                Color(17, 59, 116, 255));
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(cardLeft, cardTop, cardWidth, 3),
                Color(184, 238, 255, 255));
            constexpr std::string_view title = "PAUSED";
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                centerX - HudTextWidth(title) / 2,
                cardTop + 14,
                title,
                Color(184, 238, 255, 255));
            const std::array<std::string, 4> options{
                "RESUME",
                "SOUND " + std::to_string(soundVolumeStep_ * 25) + "%",
                "VIEW " + std::to_string(fieldOfViewDegrees_) + " DEG",
                "QUIT TO TITLE"};
            for (int index = 0; index < static_cast<int>(options.size()); ++index)
            {
                const int y = cardTop + 48 + index * 25;
                const Color color = pauseMenuSelection_ == index
                    ? Color(255, 233, 136, 255)
                    : Color(202, 223, 255, 255);
                if (pauseMenuSelection_ == index)
                {
                    DrawHudText(
                        *hudSpriteBatch_,
                        *hudPixel_,
                        cardLeft + 20,
                        y,
                        ">",
                        color);
                }
                DrawHudText(
                    *hudSpriteBatch_,
                    *hudPixel_,
                    centerX - HudTextWidth(options[static_cast<std::size_t>(index)]) / 2,
                    y,
                    options[static_cast<std::size_t>(index)],
                    color);
            }
            constexpr std::string_view prompt = "P OR ESC RESUME";
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                centerX - HudTextWidth(prompt, 1) / 2,
                cardTop + 153,
                prompt,
                Color(184, 238, 255, 255),
                1);
        }
        else if (screen_ == Screen::GameOver)
        {
            constexpr std::string_view message = "GAME OVER";
            const int messageWidth = HudTextWidth(message);
            const int messageX = centerX - messageWidth / 2;
            const int messageY = centerY - 34;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(messageX - 15, messageY - 11, messageWidth + 30, 57),
                Color(17, 59, 116, 255));
            DrawHudText(*hudSpriteBatch_, *hudPixel_, messageX, messageY, message, Color(184, 238, 255, 255));
            constexpr std::string_view prompt = "SPACE TITLE";
            DrawHudText(*hudSpriteBatch_, *hudPixel_, centerX - HudTextWidth(prompt) / 2, messageY + 21, prompt, Color(255, 233, 136, 255));
        }
        hudSpriteBatch_->End();
    }

    void WolfGame::DrawAutomap()
    {
        if (!hudSpriteBatch_ || !hudPixel_ || exploration_.Width() <= 0 || exploration_.Height() <= 0)
            return;

        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const int width = viewport.getWidthProperty();
        const int height = viewport.getHeightProperty();
        const int cellSize = std::max(
            2,
            std::min(
                std::max(2, (width - 64) / exploration_.Width()),
                std::max(2, (height - 96) / exploration_.Height())));
        const int mapWidth = exploration_.Width() * cellSize;
        const int mapHeight = exploration_.Height() * cellSize;
        const int mapLeft = viewport.getXProperty() + (width - mapWidth) / 2;
        const int mapTop = viewport.getYProperty() + (height - mapHeight) / 2 + 10;
        const int edge = std::max(1, cellSize / 5);
        const auto& rows = level_.Rows();
        const Color playerColor(255, 231, 116, 255);
        const Color doorColor(75, 137, 193, 255);
        const Color lockedDoorColor(174, 57, 65, 255);
        const Color secretColor(194, 144, 50, 255);
        const Color goalColor(55, 225, 220, 255);

        hudSpriteBatch_->Begin();
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(viewport.getXProperty(), viewport.getYProperty(), width, height),
            Color(2, 6, 16, 246));

        constexpr std::string_view title = "EXPLORED MAP";
        DrawHudText(
            *hudSpriteBatch_,
            *hudPixel_,
            viewport.getXProperty() + width / 2 - HudTextWidth(title) / 2,
            viewport.getYProperty() + 14,
            title,
            Color(184, 238, 255, 255));

        const World::ObjectiveStatus objective = world_.GetObjectiveStatus();
        const std::string powerStatus =
            "POWER " + std::to_string(objective.activatedRelays) + "/" + std::to_string(objective.totalRelays);
        const std::string terminalStatus =
            "TERMINAL " + std::to_string(objective.activatedTerminals) + "/" + std::to_string(objective.totalTerminals);
        constexpr int objectiveGap = 18;
        const int objectiveWidth = HudTextWidth(powerStatus) + objectiveGap + HudTextWidth(terminalStatus);
        const int objectiveLeft = viewport.getXProperty() + (width - objectiveWidth) / 2;
        DrawHudText(
            *hudSpriteBatch_,
            *hudPixel_,
            objectiveLeft,
            viewport.getYProperty() + 32,
            powerStatus,
            objective.activatedRelays == objective.totalRelays
                ? Color(88, 230, 132, 255)
                : Color(199, 94, 231, 255));
        DrawHudText(
            *hudSpriteBatch_,
            *hudPixel_,
            objectiveLeft + HudTextWidth(powerStatus) + objectiveGap,
            viewport.getYProperty() + 32,
            terminalStatus,
            objective.activatedTerminals == objective.totalTerminals
                ? Color(88, 230, 132, 255)
                : Color(242, 164, 61, 255));

        for (int z = 0; z < exploration_.Height(); ++z)
        {
            for (int x = 0; x < exploration_.Width(); ++x)
            {
                if (!exploration_.IsVisited(x, z))
                    continue;

                const char symbol = rows[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
                Color floorColor(29, 58, 91, 255);
                if (symbol == 'D')
                    floorColor = doorColor;
                else if (symbol == 'Q')
                    floorColor = lockedDoorColor;
                else if (symbol == 'S')
                    floorColor = secretColor;
                else if (symbol == 'E' || symbol == 'M')
                    floorColor = Color(36, 173, 177, 255);

                const int left = mapLeft + x * cellSize;
                const int top = mapTop + z * cellSize;
                hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, cellSize, cellSize), floorColor);

                const auto isWall = [&rows](int wallX, int wallZ)
                {
                    return wallZ < 0 || wallX < 0 ||
                        wallZ >= static_cast<int>(rows.size()) ||
                        wallX >= static_cast<int>(rows[static_cast<std::size_t>(wallZ)].size()) ||
                        rows[static_cast<std::size_t>(wallZ)][static_cast<std::size_t>(wallX)] == '#';
                };
                const Color wallColor(119, 148, 180, 255);
                if (isWall(x, z - 1))
                    hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, cellSize, edge), wallColor);
                if (isWall(x, z + 1))
                    hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top + cellSize - edge, cellSize, edge), wallColor);
                if (isWall(x - 1, z))
                    hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, edge, cellSize), wallColor);
                if (isWall(x + 1, z))
                    hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left + cellSize - edge, top, edge, cellSize), wallColor);
            }
        }

        if (exploration_.GoalX() >= 0 && exploration_.GoalZ() >= 0)
        {
            const int goalCenterX = mapLeft + exploration_.GoalX() * cellSize + cellSize / 2;
            const int goalCenterY = mapTop + exploration_.GoalZ() * cellSize + cellSize / 2;
            const int goalSize = std::max(7, cellSize + 2);
            const int goalThickness = std::max(2, cellSize / 4);
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(goalCenterX - goalSize / 2, goalCenterY - goalThickness / 2, goalSize, goalThickness),
                goalColor);
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(goalCenterX - goalThickness / 2, goalCenterY - goalSize / 2, goalThickness, goalSize),
                goalColor);

            constexpr std::string_view goalLabel = "GOAL";
            constexpr int goalLabelScale = 1;
            const int goalLabelWidth = HudTextWidth(goalLabel, goalLabelScale);
            int goalLabelX = goalCenterX + goalSize / 2 + 4;
            if (goalLabelX + goalLabelWidth + 4 > viewport.getXProperty() + width)
                goalLabelX = goalCenterX - goalSize / 2 - goalLabelWidth - 4;
            goalLabelX = std::clamp(
                goalLabelX,
                viewport.getXProperty() + 3,
                viewport.getXProperty() + width - goalLabelWidth - 3);
            const int goalLabelY = std::clamp(
                goalCenterY - 3,
                viewport.getYProperty() + 35,
                viewport.getYProperty() + height - 32);
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(goalLabelX - 2, goalLabelY - 2, goalLabelWidth + 4, 11),
                Color(2, 6, 16, 230));
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                goalLabelX,
                goalLabelY,
                goalLabel,
                goalColor,
                goalLabelScale);
        }

        const int playerX = static_cast<int>(std::floor(playerPosition_.X));
        const int playerZ = static_cast<int>(std::floor(playerPosition_.Z));
        const int playerCenterX = mapLeft + playerX * cellSize + cellSize / 2;
        const int playerCenterY = mapTop + playerZ * cellSize + cellSize / 2;
        const int markerSize = std::max(3, cellSize / 2);
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(playerCenterX - markerSize / 2, playerCenterY - markerSize / 2, markerSize, markerSize),
            playerColor);
        const int directionLength = std::max(5, cellSize);
        for (int step = 1; step <= directionLength; ++step)
        {
            const int directionX = playerCenterX + static_cast<int>(std::lround(std::sin(yaw_) * step));
            const int directionY = playerCenterY - static_cast<int>(std::lround(std::cos(yaw_) * step));
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(directionX - 1, directionY - 1, 3, 3),
                playerColor);
        }

        constexpr int legendWidth = 74;
        constexpr int legendHeight = 65;
        int legendLeft = viewport.getXProperty() + 8;
        if (legendLeft + legendWidth > mapLeft - 6)
            legendLeft = mapLeft + mapWidth + 6;
        legendLeft = std::clamp(
            legendLeft,
            viewport.getXProperty() + 3,
            viewport.getXProperty() + width - legendWidth - 3);
        const int legendTop = std::clamp(
            mapTop,
            viewport.getYProperty() + 52,
            viewport.getYProperty() + height - legendHeight - 28);
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(legendLeft, legendTop, legendWidth, legendHeight),
            Color(7, 18, 38, 235));
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(legendLeft, legendTop, legendWidth, 1),
            Color(119, 148, 180, 255));
        const auto drawLegendItem = [&](int row, std::string_view label, Color color)
        {
            const int itemTop = legendTop + 5 + row * 12;
            hudSpriteBatch_->Draw(*hudPixel_, Rectangle(legendLeft + 5, itemTop + 1, 7, 7), color);
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                legendLeft + 17,
                itemTop + 1,
                label,
                Color(211, 226, 246, 255),
                1);
        };
        drawLegendItem(0, "PLAYER", playerColor);
        drawLegendItem(1, "DOOR", doorColor);
        drawLegendItem(2, "LOCK", lockedDoorColor);
        drawLegendItem(3, "SECRET", secretColor);
        drawLegendItem(4, "GOAL", goalColor);

        constexpr std::string_view prompt = "RELEASE TAB";
        DrawHudText(
            *hudSpriteBatch_,
            *hudPixel_,
            viewport.getXProperty() + width / 2 - HudTextWidth(prompt) / 2,
            viewport.getYProperty() + height - 25,
            prompt,
            Color(255, 233, 136, 255));
        hudSpriteBatch_->End();
    }

    void WolfGame::DrawMenu()
    {
        if (!hudSpriteBatch_ || !hudPixel_)
            return;

        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const int width = viewport.getWidthProperty();
        const int height = viewport.getHeightProperty();
        const int left = viewport.getXProperty() + std::max(16, width / 2 - 160);
        const int top = viewport.getYProperty() + std::max(16, height / 2 - 130);
        const Color background(8, 18, 48, 255);
        const Color border(92, 150, 225, 255);
        const Color title(255, 211, 104, 255);
        const Color normal(202, 223, 255, 255);
        const Color selected(255, 236, 137, 255);

        hudSpriteBatch_->Begin();
        const Rectangle viewportRectangle(
            viewport.getXProperty(), viewport.getYProperty(), width, height);
        if (screen_ == Screen::Splash && titleBackground_)
        {
            const int imageWidth = titleBackground_->getWidthProperty();
            const int imageHeight = titleBackground_->getHeightProperty();
            int sourceWidth = imageWidth;
            int sourceHeight = imageHeight;
            int sourceX = 0;
            int sourceY = 0;
            if (width * imageHeight > height * imageWidth)
            {
                sourceHeight = std::max(1, imageWidth * height / std::max(1, width));
                sourceY = (imageHeight - sourceHeight) / 2;
            }
            else
            {
                sourceWidth = std::max(1, imageHeight * width / std::max(1, height));
                sourceX = (imageWidth - sourceWidth) / 2;
            }
            hudSpriteBatch_->Draw(
                *titleBackground_,
                viewportRectangle,
                Rectangle(sourceX, sourceY, sourceWidth, sourceHeight),
                Color(255, 255, 255, 255));
            hudSpriteBatch_->Draw(*hudPixel_, viewportRectangle, Color(2, 5, 12, 82));

            const int titleScale = std::clamp(width / 72, 5, 10);
            constexpr std::string_view splashTitle = "WOLF CNA";
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                viewport.getXProperty() + width / 2 - HudTextWidth(splashTitle, titleScale) / 2,
                viewport.getYProperty() + std::max(28, height / 7),
                splashTitle,
                title,
                titleScale);

            constexpr int buttonWidth = 180;
            constexpr int buttonHeight = 42;
            const int buttonX = viewport.getXProperty() + (width - buttonWidth) / 2;
            const int buttonY = viewport.getYProperty() + height * 3 / 4;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(buttonX, buttonY, buttonWidth, buttonHeight),
                Color(10, 24, 54, 224));
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(buttonX, buttonY, buttonWidth, 3),
                border);
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(buttonX, buttonY + buttonHeight - 3, buttonWidth, 3),
                border);
            constexpr std::string_view enterText = "ENTER";
            DrawHudText(
                *hudSpriteBatch_,
                *hudPixel_,
                buttonX + buttonWidth / 2 - HudTextWidth(enterText, 3) / 2,
                buttonY + 11,
                enterText,
                selected,
                3);
            hudSpriteBatch_->End();
            return;
        }
        else
        {
            hudSpriteBatch_->Draw(*hudPixel_, viewportRectangle, Color(4, 8, 21, 255));
        }
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, 320, 260), background);
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, 320, 3), border);
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top + 257, 320, 3), border);

        const auto centered = [&](int y, std::string_view text, Color color)
        {
            DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 160 - HudTextWidth(text) / 2, y, text, color);
        };
        if (screen_ == Screen::Title)
        {
            centered(top + 22, "MAIN MENU", title);
            centered(top + 58, "BUNKER OPERATIONS", normal);
            const std::array<std::string, 5> options{
                "START RUN",
                "CONTROLS",
                "SOUND " + std::to_string(soundVolumeStep_ * 25) + "%",
                "VIEW " + std::to_string(fieldOfViewDegrees_) + " DEG",
                "QUIT"};
            for (int index = 0; index < static_cast<int>(options.size()); ++index)
            {
                const int y = top + 88 + index * 27;
                const Color color = menuSelection_ == index ? selected : normal;
                if (menuSelection_ == index)
                    DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 45, y, ">", selected);
                centered(y, options[static_cast<std::size_t>(index)], color);
            }
            centered(top + 220, "ARROWS SELECT", normal);
            centered(top + 238, "ENTER SELECT", normal);
        }
        else if (screen_ == Screen::SectorSelect)
        {
            centered(top + 22, "BUNKER 1987", title);
            centered(top + 58, "SELECT SECTOR", normal);
            for (int index = 0; index < static_cast<int>(CampaignLevelNames.size()); ++index)
            {
                const bool unlocked = index <= highestUnlockedLevel_;
                const std::string option = std::string(CampaignLevelNames[static_cast<std::size_t>(index)]) +
                    (unlocked ? "" : " LOCKED");
                const int y = top + 92 + index * 34;
                const Color color = unlocked
                    ? menuSelection_ == index ? selected : normal
                    : Color(83, 99, 128, 255);
                if (menuSelection_ == index)
                    DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 22, y, ">", selected);
                centered(y, option, color);
            }
            centered(top + 216, "COMPLETE PRIOR SECTOR", normal);
            centered(top + 238, "ESC BACK", normal);
        }
        else if (screen_ == Screen::Difficulty)
        {
            centered(top + 22, "BUNKER 1987", title);
            centered(top + 58, "SELECT DIFFICULTY", normal);
            constexpr std::array<std::string_view, 3> options{"SCOUT", "OPERATIVE", "VETERAN"};
            for (int index = 0; index < static_cast<int>(options.size()); ++index)
            {
                const int y = top + 96 + index * 30;
                const Color color = menuSelection_ == index ? selected : normal;
                if (menuSelection_ == index)
                    DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 45, y, ">", selected);
                centered(y, options[static_cast<std::size_t>(index)], color);
            }
            centered(top + 220, "FOES AND SUPPLIES CHANGE", normal);
            centered(top + 238, "ESC BACK", normal);
        }
        else
        {
            centered(top + 22, "BUNKER 1987", title);
            centered(top + 58, "CONTROLS", normal);
            centered(top + 84, "UP DOWN WALK SHIFT RUN", normal);
            centered(top + 105, "LEFT RIGHT TURN", normal);
            centered(top + 126, "SPACE ACTION", normal);
            centered(top + 147, "CTRL ATTACK", normal);
            centered(top + 168, "1 2 3 4 WEAPONS", normal);
            centered(top + 189, "TAB MAP", normal);
            centered(top + 210, "P OR ESC PAUSE  F11 SCREEN", normal);
            centered(top + 238, "ENTER OR ESC BACK", selected);
        }
        hudSpriteBatch_->End();
    }

    Vector3 WolfGame::LookDirection() const
    {
        return Vector3(
            std::sin(yaw_),
            0.0f,
            -std::cos(yaw_));
    }

    Matrix WolfGame::ViewMatrix() const
    {
        return Matrix::CreateLookAt(
            playerPosition_,
            playerPosition_ + LookDirection(),
            Vector3::Up);
    }

    Matrix WolfGame::ProjectionMatrix()
    {
        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();

        return Matrix::CreatePerspectiveFieldOfView(
            MathHelper::ToRadians(static_cast<float>(fieldOfViewDegrees_)),
            viewport.getAspectRatioProperty(),
            0.03f,
            100.0f);
    }

    void WolfGame::TryMove(float dx, float dz)
    {
        const float targetX = playerPosition_.X + dx;
        if (!world_.Collides(targetX, playerPosition_.Z, PlayerRadius))
            playerPosition_.X = targetX;

        const float targetZ = playerPosition_.Z + dz;
        if (!world_.Collides(playerPosition_.X, targetZ, PlayerRadius))
            playerPosition_.Z = targetZ;
    }

    void WolfGame::ResetRun()
    {
        health_ = 100;
        ammo_ = GetDifficultyProfile(difficulty_).startingAmmunition;
        score_ = 0;
        lives_ = 3;
        nextExtraLifeScore_ = 40000;
        weapon_ = Weapon::Sidearm;
        lastFirearm_ = Weapon::Sidearm;
        hasRepeater_ = false;
        hasHeavyWeapon_ = false;
        LoadCampaignLevel(selectedLevelIndex_);
    }

    void WolfGame::LoadCampaignLevel(int index)
    {
        const int maximumIndex = static_cast<int>(CampaignLevelFiles.size()) - 1;
        levelIndex_ = std::clamp(index, 0, maximumIndex);
        level_ = LevelDefinition::LoadFromFile(std::string(CampaignLevelFiles[static_cast<std::size_t>(levelIndex_)]));
        world_ = World(level_, difficulty_);
        exploration_.Reset(level_);
        world_.Upload(getGraphicsDeviceProperty());
        if (atlas_)
            CreateProceduralAtlas();
        playerPosition_ = world_.PlayerStart();
        static_cast<void>(exploration_.Visit(playerPosition_.X, playerPosition_.Z));
        yaw_ = 0.0f;
        hasSecurityCard_ = false;
        completed_ = false;
        levelElapsedSeconds_ = 0.0f;
        screen_ = Screen::Playing;
        pauseMenuSelection_ = 0;
        actionWasDown_ = false;
        attackWasDown_ = false;
        weaponFlashSeconds_ = 0.0f;
        playerImpactFlashSeconds_ = 0.0f;
        playerFireCooldownSeconds_ = 0.0f;
        ilmWasDown_ = false;
        goalCheatWasDown_ = false;
        pauseWasDown_ = false;
        cheatMessageSeconds_ = 0.0f;
        objectiveMessage_.clear();
        objectiveMessageSeconds_ = 0.0f;
    }

    void WolfGame::AdvanceCampaign()
    {
        if (levelIndex_ + 1 < static_cast<int>(CampaignLevelFiles.size()))
            LoadCampaignLevel(levelIndex_ + 1);
        else
        {
            completed_ = false;
            screen_ = Screen::Title;
            menuSelection_ = 0;
        }
    }

    void WolfGame::UnlockNextLevel()
    {
        const int nextLevel = std::min(
            levelIndex_ + 1,
            static_cast<int>(CampaignLevelFiles.size()) - 1);
        if (nextLevel <= highestUnlockedLevel_)
            return;

        highestUnlockedLevel_ = nextLevel;
        SaveCampaignProfile();
    }

    void WolfGame::CompleteLevel()
    {
        if (completed_)
            return;

        completed_ = true;
        UnlockNextLevel();
        AwardScore(1000);
        if (exitSound_)
            static_cast<void>(exitSound_->Play(0.38f, 0.4f, 0.0f));
        if (completionFanfareSound_)
            static_cast<void>(completionFanfareSound_->Play(0.42f, 0.0f, 0.0f));
    }

    void WolfGame::SaveCampaignProfile() const
    {
        CampaignProgress::Save(
            std::string(ProgressFile),
            CampaignProfile{
                .highestUnlocked = highestUnlockedLevel_,
                .soundVolume = soundVolumeStep_,
                .difficulty = static_cast<int>(difficulty_),
                .fieldOfView = fieldOfViewDegrees_},
            static_cast<int>(CampaignLevelFiles.size()));
    }

    void WolfGame::AwardScore(int points)
    {
        if (points <= 0)
            return;

        score_ += points;
        while (score_ >= nextExtraLifeScore_)
        {
            ++lives_;
            nextExtraLifeScore_ += 40000;
            if (extraLifeSound_)
                static_cast<void>(extraLifeSound_->Play(0.34f, 0.3f, 0.0f));
        }
    }

    void WolfGame::HandleMenuInput()
    {
        const KeyboardState keyboard = Keyboard::GetState();
        const MouseState mouse = Mouse::GetState();
        const bool upIsDown = keyboard.IsKeyDown(Keys::Up);
        const bool downIsDown = keyboard.IsKeyDown(Keys::Down);
        const bool confirmIsDown = keyboard.IsKeyDown(Keys::Enter) || keyboard.IsKeyDown(Keys::Space);
        const bool escapeIsDown = keyboard.IsKeyDown(Keys::Escape);
        const bool mouseIsDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;

        if (screen_ == Screen::Splash)
        {
            const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
            constexpr int buttonWidth = 180;
            constexpr int buttonHeight = 42;
            const int buttonX = viewport.getXProperty() + (viewport.getWidthProperty() - buttonWidth) / 2;
            const int buttonY = viewport.getYProperty() + viewport.getHeightProperty() * 3 / 4;
            const bool clicked = mouseIsDown && !mouseWasDown_ &&
                mouse.getXProperty() >= buttonX && mouse.getXProperty() < buttonX + buttonWidth &&
                mouse.getYProperty() >= buttonY && mouse.getYProperty() < buttonY + buttonHeight;
            if ((confirmIsDown && !confirmWasDown_) || clicked)
            {
                screen_ = Screen::Title;
                menuSelection_ = 0;
            }
            if (escapeIsDown && !escapeWasDown_)
                Exit();
        }
        else if (screen_ == Screen::Title)
        {
            if (upIsDown && !upWasDown_)
                menuSelection_ = (menuSelection_ + 4) % 5;
            if (downIsDown && !downWasDown_)
                menuSelection_ = (menuSelection_ + 1) % 5;
            if (confirmIsDown && !confirmWasDown_)
            {
                if (menuSelection_ == 0)
                {
                    screen_ = Screen::SectorSelect;
                    menuSelection_ = selectedLevelIndex_;
                }
                else if (menuSelection_ == 1)
                {
                    screen_ = Screen::Controls;
                }
                else if (menuSelection_ == 2)
                {
                    soundVolumeStep_ = (soundVolumeStep_ + 1) % 5;
                    SoundEffect::setMasterVolumeProperty(
                        static_cast<float>(soundVolumeStep_) / 4.0f);
                    SaveCampaignProfile();
                }
                else if (menuSelection_ == 3)
                {
                    fieldOfViewDegrees_ = fieldOfViewDegrees_ >= 96
                        ? 60
                        : fieldOfViewDegrees_ + 12;
                    SaveCampaignProfile();
                }
                else
                {
                    Exit();
                }
            }
            if (escapeIsDown && !escapeWasDown_)
                Exit();
        }
        else if (screen_ == Screen::SectorSelect)
        {
            if (upIsDown && !upWasDown_)
                menuSelection_ = (menuSelection_ + static_cast<int>(CampaignLevelFiles.size()) - 1) %
                    static_cast<int>(CampaignLevelFiles.size());
            if (downIsDown && !downWasDown_)
                menuSelection_ = (menuSelection_ + 1) % static_cast<int>(CampaignLevelFiles.size());
            if (confirmIsDown && !confirmWasDown_ && menuSelection_ <= highestUnlockedLevel_)
            {
                selectedLevelIndex_ = menuSelection_;
                screen_ = Screen::Difficulty;
                menuSelection_ = static_cast<int>(difficulty_);
            }
            if (escapeIsDown && !escapeWasDown_)
            {
                screen_ = Screen::Title;
                menuSelection_ = 0;
            }
        }
        else if (screen_ == Screen::Difficulty)
        {
            if (upIsDown && !upWasDown_)
                menuSelection_ = (menuSelection_ + 2) % 3;
            if (downIsDown && !downWasDown_)
                menuSelection_ = (menuSelection_ + 1) % 3;
            if (confirmIsDown && !confirmWasDown_)
            {
                difficulty_ = static_cast<Difficulty>(menuSelection_);
                SaveCampaignProfile();
                ResetRun();
            }
            if (escapeIsDown && !escapeWasDown_)
            {
                screen_ = Screen::SectorSelect;
                menuSelection_ = selectedLevelIndex_;
            }
        }
        else if ((confirmIsDown && !confirmWasDown_) || (escapeIsDown && !escapeWasDown_))
        {
            screen_ = Screen::Title;
            menuSelection_ = 0;
        }

        upWasDown_ = upIsDown;
        downWasDown_ = downIsDown;
        confirmWasDown_ = confirmIsDown;
        escapeWasDown_ = escapeIsDown;
        mouseWasDown_ = mouseIsDown;
    }

    void WolfGame::HandleInput(float elapsedSeconds)
    {
        const KeyboardState keyboard = Keyboard::GetState();
        const bool mapIsDown = keyboard.IsKeyDown(Keys::Tab);
        const bool ilmIsDown =
            keyboard.IsKeyDown(Keys::I) &&
            keyboard.IsKeyDown(Keys::L) &&
            keyboard.IsKeyDown(Keys::M);
        const bool goalCheatIsDown =
            keyboard.IsKeyDown(Keys::G) &&
            keyboard.IsKeyDown(Keys::O) &&
            keyboard.IsKeyDown(Keys::A) &&
            keyboard.IsKeyDown(Keys::L);
        const bool actionIsDown = keyboard.IsKeyDown(Keys::Space);
        const bool confirmIsDown = actionIsDown || keyboard.IsKeyDown(Keys::Enter);
        const bool pauseIsDown = keyboard.IsKeyDown(Keys::P);
        const bool escapeIsDown = keyboard.IsKeyDown(Keys::Escape);
        const bool upIsDown = keyboard.IsKeyDown(Keys::Up);
        const bool downIsDown = keyboard.IsKeyDown(Keys::Down);
        if (screen_ == Screen::Map)
        {
            if (escapeIsDown && !escapeWasDown_)
            {
                screen_ = Screen::Paused;
                pauseMenuSelection_ = 0;
                upWasDown_ = upIsDown;
                downWasDown_ = downIsDown;
                confirmWasDown_ = confirmIsDown;
            }
            else if (!mapIsDown)
                screen_ = Screen::Playing;
            escapeWasDown_ = escapeIsDown;
            return;
        }
        if (screen_ == Screen::Paused)
        {
            if ((pauseIsDown && !pauseWasDown_) ||
                (escapeIsDown && !escapeWasDown_))
            {
                screen_ = Screen::Playing;
            }
            else
            {
                if (upIsDown && !upWasDown_)
                    pauseMenuSelection_ = (pauseMenuSelection_ + 3) % 4;
                if (downIsDown && !downWasDown_)
                    pauseMenuSelection_ = (pauseMenuSelection_ + 1) % 4;
                if (confirmIsDown && !confirmWasDown_)
                {
                    if (pauseMenuSelection_ == 0)
                    {
                        screen_ = Screen::Playing;
                    }
                    else if (pauseMenuSelection_ == 1)
                    {
                        soundVolumeStep_ = (soundVolumeStep_ + 1) % 5;
                        SoundEffect::setMasterVolumeProperty(
                            static_cast<float>(soundVolumeStep_) / 4.0f);
                        SaveCampaignProfile();
                    }
                    else if (pauseMenuSelection_ == 2)
                    {
                        fieldOfViewDegrees_ = fieldOfViewDegrees_ >= 96
                            ? 60
                            : fieldOfViewDegrees_ + 12;
                        SaveCampaignProfile();
                    }
                    else
                    {
                        screen_ = Screen::Title;
                        menuSelection_ = 0;
                    }
                }
            }
            upWasDown_ = upIsDown;
            downWasDown_ = downIsDown;
            confirmWasDown_ = confirmIsDown;
            actionWasDown_ = actionIsDown;
            pauseWasDown_ = pauseIsDown;
            escapeWasDown_ = escapeIsDown;
            return;
        }
        if (screen_ == Screen::GameOver)
        {
            if ((actionIsDown && !actionWasDown_) ||
                (escapeIsDown && !escapeWasDown_))
            {
                screen_ = Screen::Title;
                menuSelection_ = 0;
            }
            actionWasDown_ = actionIsDown;
            escapeWasDown_ = escapeIsDown;
            return;
        }

        if (escapeIsDown && !escapeWasDown_)
        {
            screen_ = Screen::Paused;
            pauseMenuSelection_ = 0;
            upWasDown_ = upIsDown;
            downWasDown_ = downIsDown;
            confirmWasDown_ = confirmIsDown;
            escapeWasDown_ = true;
            return;
        }
        escapeWasDown_ = escapeIsDown;

        if (pauseIsDown && !pauseWasDown_)
        {
            screen_ = Screen::Paused;
            pauseMenuSelection_ = 0;
            upWasDown_ = upIsDown;
            downWasDown_ = downIsDown;
            confirmWasDown_ = confirmIsDown;
            pauseWasDown_ = true;
            return;
        }
        pauseWasDown_ = pauseIsDown;
        if (completed_)
        {
            if (actionIsDown && !actionWasDown_)
                AdvanceCampaign();
            actionWasDown_ = actionIsDown;
            return;
        }

        if (mapIsDown)
        {
            screen_ = Screen::Map;
            return;
        }

        playerFireCooldownSeconds_ = std::max(0.0f, playerFireCooldownSeconds_ - elapsedSeconds);

        if (ilmIsDown && !ilmWasDown_)
        {
            health_ = 100;
            ammo_ = MaxAmmo;
            score_ = 0;
            nextExtraLifeScore_ = 40000;
            hasSecurityCard_ = true;
            hasRepeater_ = true;
            hasHeavyWeapon_ = true;
            weapon_ = Weapon::HeavyAutomatic;
            lastFirearm_ = Weapon::HeavyAutomatic;
            cheatMessageSeconds_ = 2.0f;
        }
        ilmWasDown_ = ilmIsDown;

        if (goalCheatIsDown && !goalCheatWasDown_)
        {
            const std::optional<World::ExitApproach> approach = world_.GetExitApproach();
            if (approach &&
                !world_.Collides(approach->position.X, approach->position.Z, PlayerRadius))
            {
                playerPosition_ = approach->position;
                yaw_ = std::atan2(
                    approach->lookDirection.X,
                    -approach->lookDirection.Z);
                static_cast<void>(exploration_.Visit(playerPosition_.X, playerPosition_.Z));
                objectiveMessage_ = "GOAL APPROACH";
                objectiveMessageSeconds_ = 2.0f;
                if (secretSound_)
                    static_cast<void>(secretSound_->Play(0.24f, 0.2f, 0.0f));
            }
        }
        goalCheatWasDown_ = goalCheatIsDown;

        if (actionIsDown && !actionWasDown_)
        {
            const World::InteractionResult activation =
                world_.TryActivate(playerPosition_, LookDirection(), hasSecurityCard_);
            if (activation == World::InteractionResult::DoorOpened && doorSound_)
                static_cast<void>(doorSound_->Play(0.68f, -0.15f, 0.0f));
            else if (activation == World::InteractionResult::DoorLocked && lockedSound_)
                static_cast<void>(lockedSound_->Play(0.24f, -0.7f, 0.0f));
            else if (activation == World::InteractionResult::ExitActivated)
            {
                CompleteLevel();
                actionWasDown_ = actionIsDown;
                return;
            }
            else if (activation == World::InteractionResult::TerminalActivated)
            {
                if (terminalSound_)
                    static_cast<void>(terminalSound_->Play(0.32f, 0.25f, 0.0f));
                objectiveMessage_ = world_.AreObjectivesComplete()
                    ? "SYSTEMS COMPLETE"
                    : "TERMINAL ONLINE";
                objectiveMessageSeconds_ = 2.0f;
            }
            else if (activation == World::InteractionResult::RelayActivated)
            {
                if (terminalSound_)
                    static_cast<void>(terminalSound_->Play(0.38f, -0.2f, 0.0f));
                objectiveMessage_ = world_.AreObjectivesComplete()
                    ? "SYSTEMS COMPLETE"
                    : "POWER ONLINE";
                objectiveMessageSeconds_ = 2.0f;
            }
            else if (activation == World::InteractionResult::SecretRevealed)
            {
                AwardScore(500);
                if (secretSound_)
                    static_cast<void>(secretSound_->Play(0.3f, 0.45f, 0.0f));
            }
        }
        actionWasDown_ = actionIsDown;

        if (ammo_ <= 0)
            weapon_ = Weapon::Knife;
        if (keyboard.IsKeyDown(Keys::D1))
            weapon_ = Weapon::Knife;
        if (ammo_ > 0 && keyboard.IsKeyDown(Keys::D2))
        {
            weapon_ = Weapon::Sidearm;
            lastFirearm_ = Weapon::Sidearm;
        }
        if (ammo_ > 0 && hasRepeater_ && keyboard.IsKeyDown(Keys::D3))
        {
            weapon_ = Weapon::Repeater;
            lastFirearm_ = Weapon::Repeater;
        }
        if (ammo_ > 0 && hasHeavyWeapon_ && keyboard.IsKeyDown(Keys::D4))
        {
            weapon_ = Weapon::HeavyAutomatic;
            lastFirearm_ = Weapon::HeavyAutomatic;
        }

        const bool attackIsDown =
            keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        const bool automaticWeapon =
            weapon_ == Weapon::Repeater || weapon_ == Weapon::HeavyAutomatic;
        const bool attackTriggered = attackIsDown &&
            (!attackWasDown_ || (automaticWeapon && playerFireCooldownSeconds_ <= 0.0f));
        if (attackIsDown && !attackWasDown_ && weapon_ == Weapon::Knife)
        {
            const World::AttackResult attack = world_.FireHitscan(
                playerPosition_,
                LookDirection(),
                0.9f,
                false);
            AwardScore(attack.score);
            weaponFlashSeconds_ = KnifeAttackVisualSeconds;
            if (knifeSound_)
                static_cast<void>(knifeSound_->Play(0.62f, -0.2f, 0.0f));
            if (attack.score > 0 && enemyDefeatedSound_)
                static_cast<void>(enemyDefeatedSound_->Play(0.28f, -0.3f, 0.0f));
        }
        else if (attackTriggered && ammo_ > 0)
        {
            if (automaticWeapon)
            {
                const bool isHeavy = weapon_ == Weapon::HeavyAutomatic;
                const int shotCount = std::min(ammo_, isHeavy ? 5 : 3);
                ammo_ -= shotCount;
                int defeatedScore = 0;
                const float spreadStep = isHeavy ? 0.055f : 0.09f;
                for (int shot = 0; shot < shotCount; ++shot)
                {
                    const float spread =
                        (static_cast<float>(shot) - static_cast<float>(shotCount - 1) * 0.5f) * spreadStep;
                    const float shotYaw = yaw_ + spread;
                    defeatedScore += world_.FireHitscan(
                        playerPosition_,
                        Vector3(std::sin(shotYaw), 0.0f, -std::cos(shotYaw))).score;
                }
                AwardScore(defeatedScore);
                playerFireCooldownSeconds_ = isHeavy ? 0.42f : 0.28f;
                weaponFlashSeconds_ = isHeavy
                    ? HeavyAttackVisualSeconds
                    : RepeaterAttackVisualSeconds;
                if (shotSound_)
                    static_cast<void>(shotSound_->Play(isHeavy ? 1.0f : 0.95f, isHeavy ? 0.08f : -0.04f, 0.0f));
                if (defeatedScore > 0 && enemyDefeatedSound_)
                    static_cast<void>(enemyDefeatedSound_->Play(0.32f, -0.22f, 0.0f));
            }
            else
            {
                --ammo_;
                const World::AttackResult attack = world_.FireHitscan(playerPosition_, LookDirection());
                AwardScore(attack.score);
                weaponFlashSeconds_ = SidearmAttackVisualSeconds;
                if (shotSound_)
                    static_cast<void>(shotSound_->Play(0.9f, -0.12f, 0.0f));
                if (attack.score > 0 && enemyDefeatedSound_)
                    static_cast<void>(enemyDefeatedSound_->Play(0.28f, -0.3f, 0.0f));
            }
        }
        if (ammo_ <= 0)
            weapon_ = Weapon::Knife;
        attackWasDown_ = attackIsDown;

        const float turnStep = KeyboardTurnSpeed * elapsedSeconds;

        if (keyboard.IsKeyDown(Keys::Left))
            yaw_ -= turnStep;
        if (keyboard.IsKeyDown(Keys::Right))
            yaw_ += turnStep;

        float forwardInput = 0.0f;

        if (keyboard.IsKeyDown(Keys::Up))
            forwardInput += 1.0f;
        if (keyboard.IsKeyDown(Keys::Down))
            forwardInput -= 1.0f;

        if (forwardInput == 0.0f)
            return;

        const float forwardX = std::sin(yaw_);
        const float forwardZ = -std::cos(yaw_);

        const bool isRunning =
            keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
        const float speed = WalkSpeed * (isRunning ? RunSpeedMultiplier : 1.0f);
        const float distance = speed * elapsedSeconds;
        const float dx = forwardX * forwardInput * distance;
        const float dz = forwardZ * forwardInput * distance;

        TryMove(dx, dz);
    }

    void WolfGame::Update(GameTime& gameTime)
    {
        const float elapsed =
            static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

        // Clamp unusually long frames so a debugger pause cannot launch the player through walls.
        const float clampedElapsed = std::min(elapsed, 0.05f);
        const KeyboardState keyboard = Keyboard::GetState();
        const bool fullScreenIsDown = keyboard.IsKeyDown(Keys::F11);
        if (fullScreenIsDown && !fullScreenWasDown_)
            graphics_->ToggleFullScreen();
        fullScreenWasDown_ = fullScreenIsDown;

        if (screen_ == Screen::Splash || screen_ == Screen::Title || screen_ == Screen::SectorSelect ||
            screen_ == Screen::Difficulty || screen_ == Screen::Controls)
        {
            HandleMenuInput();
            Game::Update(gameTime);
            return;
        }

        if (screen_ == Screen::Map || screen_ == Screen::Paused ||
            screen_ == Screen::GameOver || completed_)
        {
            HandleInput(clampedElapsed);
            Game::Update(gameTime);
            return;
        }

        levelElapsedSeconds_ += clampedElapsed;
        cheatMessageSeconds_ = std::max(0.0f, cheatMessageSeconds_ - clampedElapsed);
        objectiveMessageSeconds_ = std::max(0.0f, objectiveMessageSeconds_ - clampedElapsed);
        weaponFlashSeconds_ = std::max(0.0f, weaponFlashSeconds_ - clampedElapsed);
        playerImpactFlashSeconds_ = std::max(0.0f, playerImpactFlashSeconds_ - clampedElapsed);
        const int incomingDamage = world_.Update(clampedElapsed, playerPosition_);
        if (world_.ConsumeGuardShotCount() > 0 && guardShotSound_)
            static_cast<void>(guardShotSound_->Play(0.18f, 0.12f, 0.0f));
        const World::EnemyAudioEvents enemyAudioEvents = world_.ConsumeEnemyAudioEvents();
        if (enemyAudioEvents.guardAlerts > 0 && guardAlertSound_)
            static_cast<void>(guardAlertSound_->Play(0.2f, -0.1f, 0.0f));
        if (enemyAudioEvents.houndAlerts > 0 && houndAlertSound_)
            static_cast<void>(houndAlertSound_->Play(0.22f, -0.3f, 0.0f));
        if (enemyAudioEvents.houndAttacks > 0 && houndAttackSound_)
            static_cast<void>(houndAttackSound_->Play(0.25f, -0.4f, 0.0f));
        if (enemyAudioEvents.projectileImpacts > 0 && enemyImpactSound_)
            static_cast<void>(enemyImpactSound_->Play(0.2f, 0.18f, 0.0f));
        if (enemyAudioEvents.doorsOpened > 0 && doorSound_)
            static_cast<void>(doorSound_->Play(0.35f, 0.0f, 0.0f));
        health_ -= incomingDamage;
        if (incomingDamage > 0 && hurtSound_)
            static_cast<void>(hurtSound_->Play(0.3f, -0.25f, 0.0f));
        if (incomingDamage > 0)
            playerImpactFlashSeconds_ = 0.18f;
        if (health_ <= 0)
        {
            lives_ = std::max(0, lives_ - 1);
            if (lives_ == 0)
            {
                screen_ = Screen::GameOver;
                actionWasDown_ = false;
            }
            else
            {
                health_ = 100;
                ammo_ = GetDifficultyProfile(difficulty_).startingAmmunition;
                weapon_ = lastFirearm_;
                playerPosition_ = world_.PlayerStart();
                completed_ = false;
            }
        }
        HandleInput(clampedElapsed);
        static_cast<void>(exploration_.Visit(playerPosition_.X, playerPosition_.Z));
        const World::PickupResult pickups = world_.CollectPickups(playerPosition_, health_);
        const bool wasOutOfAmmo = ammo_ <= 0;
        health_ = std::min(100, health_ + pickups.health);
        ammo_ = std::min(MaxAmmo, ammo_ + pickups.ammo);
        if (pickups.ammo > 0 && wasOutOfAmmo && ammo_ > 0)
            weapon_ = lastFirearm_;
        if (pickups.repeaterWeapons > 0)
        {
            hasRepeater_ = true;
            weapon_ = Weapon::Repeater;
            lastFirearm_ = Weapon::Repeater;
        }
        if (pickups.heavyWeapons > 0)
        {
            hasHeavyWeapon_ = true;
            weapon_ = Weapon::HeavyAutomatic;
            lastFirearm_ = Weapon::HeavyAutomatic;
        }
        AwardScore(pickups.gold);
        hasSecurityCard_ = hasSecurityCard_ || pickups.accessCards > 0;
        if (pickups.ammo > 0 && ammoPickupSound_)
            static_cast<void>(ammoPickupSound_->Play(0.72f, 0.0f, 0.0f));
        if ((pickups.health + pickups.gold + pickups.accessCards +
            pickups.repeaterWeapons + pickups.heavyWeapons) > 0 && pickupSound_)
            static_cast<void>(pickupSound_->Play(0.28f, 0.0f, 0.0f));
        if (!completed_ && world_.ReachedExit(playerPosition_))
            CompleteLevel();

        Game::Update(gameTime);
    }

    void WolfGame::Draw(const GameTime& gameTime)
    {
        auto& device = getGraphicsDeviceProperty();

        device.Clear(Color(18, 20, 24, 255));
        device.setDepthStencilStateProperty(DepthStencilState::Default);
        device.setBlendStateProperty(BlendState::Opaque);
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.getSamplerStatesProperty()[0] = SamplerState::PointClamp;

        if ((screen_ == Screen::Playing || screen_ == Screen::Map ||
            screen_ == Screen::Paused || screen_ == Screen::GameOver) &&
            effect_ && atlas_ && guardSprite_ && houndSprite_ && bloodDecal_ &&
            rapidTrooperSprite_ && heavyUnitSprite_ &&
            guardAttackSprite_ && houndAttackSprite_ &&
            rapidTrooperAttackSprite_ && heavyUnitAttackSprite_ &&
            guardPainSprite_ && houndPainSprite_ &&
            rapidTrooperPainSprite_ && heavyUnitPainSprite_ &&
            defeatedGuardSprite_ && defeatedHoundSprite_ &&
            defeatedRapidTrooperSprite_ && defeatedHeavyUnitSprite_ &&
            ammoPickupSprite_ && healthPickupSprite_ && goldBarsSprite_ &&
            goldenGobletSprite_ && peaceMedallionSprite_ &&
            accessCardSprite_ && repeaterPickupSprite_ && heavyWeaponPickupSprite_ &&
            terminalSprite_ && relaySprite_ && exitSprite_ && enemyProjectileSprite_ &&
            enemyImpactSprite_ &&
            paintingTexture_ && peaceBannerTexture_ && ceilingLampTexture_ && lampLightTexture_ &&
            storagePlantSprite_ && foundryPlantSprite_ && labsPlantSprite_ && archivePlantSprite_)
        {
            world_.Draw(
                device,
                *effect_,
                ViewMatrix(),
                ProjectionMatrix(),
                *atlas_,
                *guardSprite_,
                *houndSprite_,
                *rapidTrooperSprite_,
                *heavyUnitSprite_,
                *guardAttackSprite_,
                *houndAttackSprite_,
                *rapidTrooperAttackSprite_,
                *heavyUnitAttackSprite_,
                *guardPainSprite_,
                *houndPainSprite_,
                *rapidTrooperPainSprite_,
                *heavyUnitPainSprite_,
                *defeatedGuardSprite_,
                *defeatedHoundSprite_,
                *defeatedRapidTrooperSprite_,
                *defeatedHeavyUnitSprite_,
                *ammoPickupSprite_,
                *healthPickupSprite_,
                *goldBarsSprite_,
                *goldenGobletSprite_,
                *peaceMedallionSprite_,
                *accessCardSprite_,
                *repeaterPickupSprite_,
                *heavyWeaponPickupSprite_,
                *terminalSprite_,
                *relaySprite_,
                *exitSprite_,
                *enemyProjectileSprite_,
                *enemyImpactSprite_,
                *bloodDecal_,
                *paintingTexture_,
                *peaceBannerTexture_,
                *ceilingLampTexture_,
                *lampLightTexture_,
                levelIndex_ == 0
                    ? *storagePlantSprite_
                    : levelIndex_ == 1
                        ? *foundryPlantSprite_
                        : levelIndex_ == 2 ? *labsPlantSprite_ : *archivePlantSprite_,
                playerPosition_);
        }

        if (screen_ == Screen::Splash || screen_ == Screen::Title || screen_ == Screen::SectorSelect ||
            screen_ == Screen::Difficulty || screen_ == Screen::Controls)
            DrawMenu();
        else
        {
            DrawHud();
            if (screen_ == Screen::Map)
                DrawAutomap();
        }

        Game::Draw(gameTime);
    }
}
