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

namespace WolfCna
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Audio;
    using namespace Microsoft::Xna::Framework::Graphics;
    using namespace Microsoft::Xna::Framework::Input;

    namespace
    {
        constexpr int PanelSize = 32;
        constexpr int PanelCount = 5;
        constexpr int AtlasWidth = PanelSize * PanelCount;
        constexpr int AtlasHeight = PanelSize;
        constexpr std::array<std::string_view, 3> CampaignLevelFiles = {
            "assets/levels/starter.level",
            "assets/levels/sector-02.level",
            "assets/levels/sector-03.level"
        };

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

        void DrawHudText(SpriteBatch& batch, Texture2D& pixel, int x, int y, std::string_view text, Color color)
        {
            constexpr int scale = 2;
            for (char character : text)
            {
                const auto glyph = Glyph(character);
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < 5; ++column)
                        if (glyph[row][column] == '1')
                            batch.Draw(pixel, Rectangle(x + column * scale, y + row * scale, scale, scale), color);
                x += 12;
            }
        }

        int HudTextWidth(std::string_view text)
        {
            return text.empty() ? 0 : static_cast<int>(text.size()) * 12 - 2;
        }
    }

    WolfGame::WolfGame()
        : playerPosition_(world_.PlayerStart())
    {
        getContentProperty().setRootDirectoryProperty("Content");

        graphics_ = std::make_unique<GraphicsDeviceManager>(this);
        graphics_->setIsFullScreenProperty(false);
        graphics_->ApplyChanges();
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
        CreateHudResources();
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

        for (int y = 0; y < PanelSize; ++y)
        {
            for (int x = 0; x < PanelSize; ++x)
            {
                const int noise = Noise(x, y);

                // Panel 0: warm bunker bricks.
                {
                    const int brickHeight = 8;
                    const int brickWidth = 16;
                    const int row = y / brickHeight;
                    const int shiftedX = (x + ((row & 1) ? brickWidth / 2 : 0)) % brickWidth;
                    const bool mortar = (y % brickHeight) == 0 || shiftedX == 0;

                    Color c(0, 0, 0, 255);
                    if (mortar)
                    {
                        c = Color(45, 47, 45, 255);
                    }
                    else
                    {
                        c = Color(
                            ByteClamp(113 + noise),
                            ByteClamp(58 + noise / 2),
                            ByteClamp(43 + noise / 3),
                            ByteClamp(255));
                    }

                    pixels[static_cast<std::size_t>(y * AtlasWidth + x)] = c;
                }

                // Panel 1: dark steel floor tiles.
                {
                    const int ax = x + PanelSize;
                    const bool seam = (x % 8) == 0 || (y % 8) == 0;
                    const int checker = ((x / 8) + (y / 8)) & 1;

                    Color c(0, 0, 0, 255);
                    if (seam)
                    {
                        c = Color(35, 39, 42, 255);
                    }
                    else
                    {
                        const int base = checker ? 68 : 58;
                        c = Color(
                            ByteClamp(base + noise / 3),
                            ByteClamp(base + 5 + noise / 3),
                            ByteClamp(base + 7 + noise / 3),
                            ByteClamp(255));
                    }

                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = c;
                }

                // Panel 2: pale concrete ceiling with panel seams.
                {
                    const int ax = x + PanelSize * 2;
                    const bool seam = (x % 16) == 0 || (y % 16) == 0;

                    Color c(0, 0, 0, 255);
                    if (seam)
                    {
                        c = Color(74, 76, 72, 255);
                    }
                    else
                    {
                        c = Color(
                            ByteClamp(135 + noise),
                            ByteClamp(137 + noise),
                            ByteClamp(129 + noise),
                            ByteClamp(255));
                    }

                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = c;
                }

                // Panel 3: blue bunker door.
                {
                    const int ax = x + PanelSize * 3;
                    const bool seam = x < 3 || x > PanelSize - 4 || (y % 8) == 0;
                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = seam
                        ? Color(31, 50, 78, 255)
                        : Color(ByteClamp(49 + noise), ByteClamp(86 + noise), ByteClamp(128 + noise), ByteClamp(255));
                }

                // Panel 4: red security door.
                {
                    const int ax = x + PanelSize * 4;
                    const bool seam = x < 3 || x > PanelSize - 4 || (y % 8) == 0;
                    pixels[static_cast<std::size_t>(y * AtlasWidth + ax)] = seam
                        ? Color(76, 25, 28, 255)
                        : Color(ByteClamp(132 + noise), ByteClamp(46 + noise), ByteClamp(50 + noise), ByteClamp(255));
                }
            }
        }

        atlas_->SetData(pixels.data(), static_cast<int>(pixels.size()));
    }

    void WolfGame::CreateHudResources()
    {
        auto& device = getGraphicsDeviceProperty();

        hudSpriteBatch_ = std::make_unique<SpriteBatch>(device);
        hudPixel_ = std::make_unique<Texture2D>(device, 1, 1);
        const Color pixel(255, 255, 255, 255);
        hudPixel_->SetData(&pixel, 1);

        constexpr int iconSize = 40;
        weaponIcon_ = std::make_unique<Texture2D>(device, iconSize, iconSize);
        std::vector<Color> iconPixels(iconSize * iconSize, Color(0, 0, 0, 0));
        for (int y = 7; y < 21; ++y)
            for (int x = 15; x < 30; ++x)
                if (y < 13 || x < 23)
                    iconPixels[y * iconSize + x] = Color(82, 88, 98, 255);
        for (int y = 18; y < 35; ++y)
            for (int x = 11; x < 24; ++x)
                if (x > 15 || y > 24)
                    iconPixels[y * iconSize + x] = Color(184, 126, 83, 255);
        weaponIcon_->SetData(iconPixels.data(), static_cast<int>(iconPixels.size()));

        knifeIcon_ = std::make_unique<Texture2D>(device, iconSize, iconSize);
        std::fill(iconPixels.begin(), iconPixels.end(), Color(0, 0, 0, 0));
        for (int y = 4; y < 25; ++y)
            for (int x = 20; x < 24; ++x)
                iconPixels[y * iconSize + x] = Color(202, 208, 218, 255);
        for (int y = 22; y < 35; ++y)
            for (int x = 13; x < 28; ++x)
                if (x > 16 || y > 27)
                    iconPixels[y * iconSize + x] = Color(184, 126, 83, 255);
        knifeIcon_->SetData(iconPixels.data(), static_cast<int>(iconPixels.size()));

        repeaterIcon_ = std::make_unique<Texture2D>(device, iconSize, iconSize);
        std::fill(iconPixels.begin(), iconPixels.end(), Color(0, 0, 0, 0));
        for (int y = 8; y < 20; ++y)
            for (int x = 8; x < 33; ++x)
                if (y < 13 || x < 26)
                    iconPixels[y * iconSize + x] = Color(72, 83, 94, 255);
        for (int y = 18; y < 35; ++y)
            for (int x = 14; x < 27; ++x)
                if (x > 17 || y > 24)
                    iconPixels[y * iconSize + x] = Color(166, 112, 72, 255);
        repeaterIcon_->SetData(iconPixels.data(), static_cast<int>(iconPixels.size()));
    }

    void WolfGame::CreateSoundEffects()
    {
        shotSound_ = std::make_unique<SoundEffect>(
            MakeTone(150.0f, 1800),
            22050,
            AudioChannels::Mono);
        pickupSound_ = std::make_unique<SoundEffect>(
            MakeTone(660.0f, 2400),
            22050,
            AudioChannels::Mono);
        doorSound_ = std::make_unique<SoundEffect>(
            MakeTone(210.0f, 3600),
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
        if (!hudSpriteBatch_ || !hudPixel_ || !weaponIcon_ || !knifeIcon_ || !repeaterIcon_)
            return;

        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const int centerX = viewport.getXProperty() + viewport.getWidthProperty() / 2;
        const int centerY = viewport.getYProperty() + viewport.getHeightProperty() / 2;
        const Color crosshairColor(238, 211, 132, 255);

        hudSpriteBatch_->Begin();
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(centerX - 8, centerY, 17, 1),
            crosshairColor);
        hudSpriteBatch_->Draw(
            *hudPixel_,
            Rectangle(centerX, centerY - 8, 1, 17),
            crosshairColor);
        const int panelHeight = 84;
        const int panelY = viewport.getYProperty() + viewport.getHeightProperty() - panelHeight;
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
            weapon_ == Weapon::Sidearm ? *weaponIcon_
                : weapon_ == Weapon::Knife ? *knifeIcon_ : *repeaterIcon_,
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
            constexpr std::string_view message = "PAUSED";
            constexpr std::string_view prompt = "P RESUME";
            const int messageWidth = HudTextWidth(message);
            const int messageX = centerX - messageWidth / 2;
            const int messageY = centerY - 34;
            hudSpriteBatch_->Draw(
                *hudPixel_,
                Rectangle(messageX - 20, messageY - 11, messageWidth + 40, 57),
                Color(17, 59, 116, 255));
            DrawHudText(*hudSpriteBatch_, *hudPixel_, messageX, messageY, message, Color(184, 238, 255, 255));
            DrawHudText(*hudSpriteBatch_, *hudPixel_, centerX - HudTextWidth(prompt) / 2, messageY + 21, prompt, Color(255, 233, 136, 255));
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
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(viewport.getXProperty(), viewport.getYProperty(), width, height), Color(4, 8, 21, 255));
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, 320, 260), background);
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top, 320, 3), border);
        hudSpriteBatch_->Draw(*hudPixel_, Rectangle(left, top + 257, 320, 3), border);

        const auto centered = [&](int y, std::string_view text, Color color)
        {
            DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 160 - HudTextWidth(text) / 2, y, text, color);
        };
        centered(top + 22, "BUNKER 1987", title);

        if (screen_ == Screen::Title)
        {
            centered(top + 58, "CNA OPERATIONS", normal);
            const std::array<std::string, 4> options{
                "START RUN",
                "CONTROLS",
                soundEnabled_ ? "SOUND ON" : "SOUND OFF",
                "QUIT"};
            for (int index = 0; index < static_cast<int>(options.size()); ++index)
            {
                const int y = top + 96 + index * 30;
                const Color color = menuSelection_ == index ? selected : normal;
                if (menuSelection_ == index)
                    DrawHudText(*hudSpriteBatch_, *hudPixel_, left + 45, y, ">", selected);
                centered(y, options[static_cast<std::size_t>(index)], color);
            }
            centered(top + 220, "ARROWS SELECT", normal);
            centered(top + 238, "ENTER SELECT", normal);
        }
        else if (screen_ == Screen::Difficulty)
        {
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
            centered(top + 220, "SCOUT LESS DAMAGE", normal);
            centered(top + 238, "ESC BACK", normal);
        }
        else
        {
            centered(top + 58, "CONTROLS", normal);
            centered(top + 94, "UP DOWN WALK", normal);
            centered(top + 118, "LEFT RIGHT TURN", normal);
            centered(top + 142, "SPACE ACTION", normal);
            centered(top + 166, "CTRL ATTACK", normal);
            centered(top + 190, "F11 FULLSCREEN", normal);
            centered(top + 232, "ENTER OR ESC BACK", selected);
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
            MathHelper::ToRadians(72.0f),
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
        ammo_ = 12;
        score_ = 0;
        lives_ = 3;
        nextExtraLifeScore_ = 40000;
        weapon_ = Weapon::Sidearm;
        LoadCampaignLevel(0);
    }

    void WolfGame::LoadCampaignLevel(int index)
    {
        const int maximumIndex = static_cast<int>(CampaignLevelFiles.size()) - 1;
        levelIndex_ = std::clamp(index, 0, maximumIndex);
        level_ = LevelDefinition::LoadFromFile(std::string(CampaignLevelFiles[static_cast<std::size_t>(levelIndex_)]));
        world_ = World(level_);
        world_.Upload(getGraphicsDeviceProperty());
        playerPosition_ = world_.PlayerStart();
        yaw_ = 0.0f;
        hasSecurityCard_ = false;
        completed_ = false;
        levelElapsedSeconds_ = 0.0f;
        screen_ = Screen::Playing;
        actionWasDown_ = false;
        attackWasDown_ = false;
        ilmWasDown_ = false;
        pauseWasDown_ = false;
        cheatMessageSeconds_ = 0.0f;
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

    float WolfGame::DamageMultiplier() const
    {
        switch (difficulty_)
        {
        case Difficulty::Scout: return 0.7f;
        case Difficulty::Operative: return 1.0f;
        case Difficulty::Veteran: return 1.4f;
        }
        return 1.0f;
    }

    void WolfGame::HandleMenuInput()
    {
        const KeyboardState keyboard = Keyboard::GetState();
        const bool upIsDown = keyboard.IsKeyDown(Keys::Up);
        const bool downIsDown = keyboard.IsKeyDown(Keys::Down);
        const bool confirmIsDown = keyboard.IsKeyDown(Keys::Enter) || keyboard.IsKeyDown(Keys::Space);
        const bool escapeIsDown = keyboard.IsKeyDown(Keys::Escape);

        if (screen_ == Screen::Title)
        {
            if (upIsDown && !upWasDown_)
                menuSelection_ = (menuSelection_ + 3) % 4;
            if (downIsDown && !downWasDown_)
                menuSelection_ = (menuSelection_ + 1) % 4;
            if (confirmIsDown && !confirmWasDown_)
            {
                if (menuSelection_ == 0)
                {
                    screen_ = Screen::Difficulty;
                    menuSelection_ = static_cast<int>(difficulty_);
                }
                else if (menuSelection_ == 1)
                {
                    screen_ = Screen::Controls;
                }
                else if (menuSelection_ == 2)
                {
                    soundEnabled_ = !soundEnabled_;
                    SoundEffect::setMasterVolumeProperty(soundEnabled_ ? 1.0f : 0.0f);
                }
                else
                {
                    Exit();
                }
            }
            if (escapeIsDown && !escapeWasDown_)
                Exit();
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
                ResetRun();
            }
            if (escapeIsDown && !escapeWasDown_)
            {
                screen_ = Screen::Title;
                menuSelection_ = 0;
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
    }

    void WolfGame::HandleInput(float elapsedSeconds)
    {
        const KeyboardState keyboard = Keyboard::GetState();

        if (keyboard.IsKeyDown(Keys::Escape))
        {
            Exit();
            return;
        }

        const bool actionIsDown = keyboard.IsKeyDown(Keys::Space);
        const bool pauseIsDown = keyboard.IsKeyDown(Keys::P);
        if (screen_ == Screen::Paused)
        {
            if (pauseIsDown && !pauseWasDown_)
                screen_ = Screen::Playing;
            pauseWasDown_ = pauseIsDown;
            return;
        }
        if (screen_ == Screen::GameOver)
        {
            if (actionIsDown && !actionWasDown_)
            {
                screen_ = Screen::Title;
                menuSelection_ = 0;
            }
            actionWasDown_ = actionIsDown;
            return;
        }

        if (pauseIsDown && !pauseWasDown_)
        {
            screen_ = Screen::Paused;
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

        const bool ilmIsDown =
            keyboard.IsKeyDown(Keys::I) && keyboard.IsKeyDown(Keys::L) && keyboard.IsKeyDown(Keys::M);
        if (ilmIsDown && !ilmWasDown_)
        {
            health_ = 100;
            ammo_ = 12;
            score_ = 0;
            nextExtraLifeScore_ = 40000;
            hasSecurityCard_ = true;
            weapon_ = Weapon::Repeater;
            cheatMessageSeconds_ = 2.0f;
        }
        ilmWasDown_ = ilmIsDown;

        if (actionIsDown && !actionWasDown_)
        {
            const World::InteractionResult activation =
                world_.TryActivate(playerPosition_, LookDirection(), hasSecurityCard_);
            if (activation == World::InteractionResult::DoorOpened && doorSound_)
                static_cast<void>(doorSound_->Play(0.22f, -0.2f, 0.0f));
            else if (activation == World::InteractionResult::DoorLocked && lockedSound_)
                static_cast<void>(lockedSound_->Play(0.24f, -0.7f, 0.0f));
            else if (activation == World::InteractionResult::TerminalActivated && terminalSound_)
                static_cast<void>(terminalSound_->Play(0.32f, 0.25f, 0.0f));
            else if (activation == World::InteractionResult::SecretRevealed)
            {
                AwardScore(500);
                if (secretSound_)
                    static_cast<void>(secretSound_->Play(0.3f, 0.45f, 0.0f));
            }
        }
        actionWasDown_ = actionIsDown;

        if (keyboard.IsKeyDown(Keys::D1))
            weapon_ = Weapon::Knife;
        if (keyboard.IsKeyDown(Keys::D2))
            weapon_ = Weapon::Sidearm;
        if (keyboard.IsKeyDown(Keys::D3))
            weapon_ = Weapon::Repeater;

        const bool attackIsDown =
            keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        if (attackIsDown && !attackWasDown_ && weapon_ == Weapon::Knife)
        {
            const World::AttackResult attack = world_.FireHitscan(playerPosition_, LookDirection(), 0.9f);
            AwardScore(attack.score);
            if (shotSound_)
                static_cast<void>(shotSound_->Play(0.18f, -0.45f, 0.0f));
            if (attack.score > 0 && enemyDefeatedSound_)
                static_cast<void>(enemyDefeatedSound_->Play(0.28f, -0.3f, 0.0f));
        }
        else if (attackIsDown && !attackWasDown_ && ammo_ > 0)
        {
            if (weapon_ == Weapon::Repeater && ammo_ >= 3)
            {
                ammo_ -= 3;
                int defeatedScore = 0;
                for (const float spread : {-0.09f, 0.0f, 0.09f})
                {
                    const float shotYaw = yaw_ + spread;
                    defeatedScore += world_.FireHitscan(
                        playerPosition_,
                        Vector3(std::sin(shotYaw), 0.0f, -std::cos(shotYaw))).score;
                }
                AwardScore(defeatedScore);
                if (shotSound_)
                    static_cast<void>(shotSound_->Play(0.48f, 0.16f, 0.0f));
                if (defeatedScore > 0 && enemyDefeatedSound_)
                    static_cast<void>(enemyDefeatedSound_->Play(0.32f, -0.22f, 0.0f));
            }
            else if (weapon_ != Weapon::Repeater)
            {
                --ammo_;
                const World::AttackResult attack = world_.FireHitscan(playerPosition_, LookDirection());
                AwardScore(attack.score);
                if (shotSound_)
                    static_cast<void>(shotSound_->Play(0.35f, 0.0f, 0.0f));
                if (attack.score > 0 && enemyDefeatedSound_)
                    static_cast<void>(enemyDefeatedSound_->Play(0.28f, -0.3f, 0.0f));
            }
        }
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

        const float distance = WalkSpeed * elapsedSeconds;
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

        if (screen_ == Screen::Title || screen_ == Screen::Difficulty || screen_ == Screen::Controls)
        {
            HandleMenuInput();
            Game::Update(gameTime);
            return;
        }

        if (screen_ == Screen::Paused || screen_ == Screen::GameOver || completed_)
        {
            HandleInput(clampedElapsed);
            Game::Update(gameTime);
            return;
        }

        levelElapsedSeconds_ += clampedElapsed;
        cheatMessageSeconds_ = std::max(0.0f, cheatMessageSeconds_ - clampedElapsed);
        const int incomingDamage = world_.Update(clampedElapsed, playerPosition_, DamageMultiplier());
        if (world_.ConsumeGuardShotCount() > 0 && guardShotSound_)
            static_cast<void>(guardShotSound_->Play(0.18f, 0.12f, 0.0f));
        const World::EnemyAudioEvents enemyAudioEvents = world_.ConsumeEnemyAudioEvents();
        if (enemyAudioEvents.guardAlerts > 0 && guardAlertSound_)
            static_cast<void>(guardAlertSound_->Play(0.2f, -0.1f, 0.0f));
        if (enemyAudioEvents.houndAlerts > 0 && houndAlertSound_)
            static_cast<void>(houndAlertSound_->Play(0.22f, -0.3f, 0.0f));
        if (enemyAudioEvents.houndAttacks > 0 && houndAttackSound_)
            static_cast<void>(houndAttackSound_->Play(0.25f, -0.4f, 0.0f));
        health_ -= incomingDamage;
        if (incomingDamage > 0 && hurtSound_)
            static_cast<void>(hurtSound_->Play(0.3f, -0.25f, 0.0f));
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
                ammo_ = 12;
                playerPosition_ = world_.PlayerStart();
                completed_ = false;
            }
        }
        HandleInput(clampedElapsed);
        const World::PickupResult pickups = world_.CollectPickups(playerPosition_);
        health_ = std::min(100, health_ + pickups.health);
        ammo_ = std::min(12, ammo_ + pickups.ammo);
        AwardScore(pickups.gold);
        hasSecurityCard_ = hasSecurityCard_ || pickups.accessCards > 0;
        if ((pickups.health + pickups.ammo + pickups.gold + pickups.accessCards) > 0 && pickupSound_)
            static_cast<void>(pickupSound_->Play(0.28f, 0.0f, 0.0f));
        if (!completed_ && world_.ReachedExit(playerPosition_))
        {
            completed_ = true;
            AwardScore(1000);
            if (exitSound_)
                static_cast<void>(exitSound_->Play(0.38f, 0.4f, 0.0f));
        }

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

        if ((screen_ == Screen::Playing || screen_ == Screen::Paused || screen_ == Screen::GameOver) && effect_ && atlas_)
        {
            world_.Draw(
                device,
                *effect_,
                ViewMatrix(),
                ProjectionMatrix(),
                *atlas_);
        }

        if (screen_ == Screen::Title || screen_ == Screen::Difficulty || screen_ == Screen::Controls)
            DrawMenu();
        else
            DrawHud();

        Game::Draw(gameTime);
    }
}
