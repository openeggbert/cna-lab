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

        std::array<std::string_view, 5> Glyph(char c)
        {
            switch (c)
            {
            case 'A': return {"010", "101", "111", "101", "101"};
            case 'C': return {"111", "100", "100", "100", "111"};
            case 'E': return {"111", "100", "110", "100", "111"};
            case 'H': return {"101", "101", "111", "101", "101"};
            case 'I': return {"111", "010", "010", "010", "111"};
            case 'L': return {"100", "100", "100", "100", "111"};
            case 'M': return {"101", "111", "111", "101", "101"};
            case 'O': return {"111", "101", "101", "101", "111"};
            case 'R': return {"110", "101", "110", "101", "101"};
            case 'S': return {"111", "100", "111", "001", "111"};
            case 'V': return {"101", "101", "101", "101", "010"};
            case '0': return {"111", "101", "101", "101", "111"};
            case '1': return {"010", "110", "010", "010", "111"};
            case '2': return {"111", "001", "111", "100", "111"};
            case '3': return {"111", "001", "111", "001", "111"};
            case '4': return {"101", "101", "111", "001", "001"};
            case '5': return {"111", "100", "111", "001", "111"};
            case '6': return {"111", "100", "111", "101", "111"};
            case '7': return {"111", "001", "010", "010", "010"};
            case '8': return {"111", "101", "111", "101", "111"};
            case '9': return {"111", "101", "111", "001", "111"};
            case '%': return {"101", "001", "010", "100", "101"};
            default: return {"000", "000", "000", "000", "000"};
            }
        }

        void DrawHudText(SpriteBatch& batch, Texture2D& pixel, int x, int y, std::string_view text, Color color)
        {
            constexpr int scale = 3;
            for (char character : text)
            {
                const auto glyph = Glyph(character);
                for (int row = 0; row < 5; ++row)
                    for (int column = 0; column < 3; ++column)
                        if (glyph[row][column] == '1')
                            batch.Draw(pixel, Rectangle(x + column * scale, y + row * scale, scale, scale), color);
                x += 12;
            }
        }

        int HudTextWidth(std::string_view text)
        {
            return text.empty() ? 0 : static_cast<int>(text.size()) * 12 - 3;
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
    }

    void WolfGame::DrawHud()
    {
        if (!hudSpriteBatch_ || !hudPixel_ || !weaponIcon_ || !knifeIcon_)
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
        drawReadout(0, "LEVEL", "1");
        drawReadout(1, "SCORE", std::to_string(score_ + gold_));
        drawReadout(2, "LIVES", std::to_string(lives_));
        drawReadout(3, "HEALTH", std::to_string(health_) + "%");
        drawReadout(4, "AMMO", std::to_string(ammo_));
        const int weaponCenter = viewport.getXProperty() + viewport.getWidthProperty() * 11 / 12;
        hudSpriteBatch_->Draw(
            weapon_ == Weapon::Sidearm ? *weaponIcon_ : *knifeIcon_,
            Rectangle(weaponCenter - 30, panelY + 12, 60, 60),
            Color(255, 255, 255, 255));
        if (completed_)
            hudSpriteBatch_->Draw(*hudPixel_, Rectangle(0, 8, viewport.getWidthProperty(), 4), Color(92, 226, 244, 255));
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

    void WolfGame::HandleInput(float elapsedSeconds)
    {
        const KeyboardState keyboard = Keyboard::GetState();

        if (keyboard.IsKeyDown(Keys::Escape))
        {
            Exit();
            return;
        }

        const bool fullScreenIsDown = keyboard.IsKeyDown(Keys::F11);
        if (fullScreenIsDown && !fullScreenWasDown_)
            graphics_->ToggleFullScreen();
        fullScreenWasDown_ = fullScreenIsDown;

        const bool actionIsDown = keyboard.IsKeyDown(Keys::Space);
        if (actionIsDown && !actionWasDown_)
        {
            const World::DoorActivation activation =
                world_.TryActivate(playerPosition_, LookDirection(), hasSecurityCard_);
            if (activation == World::DoorActivation::Opened && doorSound_)
                static_cast<void>(doorSound_->Play(0.22f, -0.2f, 0.0f));
            else if (activation == World::DoorActivation::Locked && lockedSound_)
                static_cast<void>(lockedSound_->Play(0.24f, -0.7f, 0.0f));
        }
        actionWasDown_ = actionIsDown;

        if (keyboard.IsKeyDown(Keys::D1))
            weapon_ = Weapon::Knife;
        if (keyboard.IsKeyDown(Keys::D2))
            weapon_ = Weapon::Sidearm;

        const bool attackIsDown =
            keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        if (attackIsDown && !attackWasDown_ && weapon_ == Weapon::Knife)
        {
            static_cast<void>(world_.FireHitscan(playerPosition_, LookDirection(), 0.9f));
            if (shotSound_)
                static_cast<void>(shotSound_->Play(0.18f, -0.45f, 0.0f));
        }
        else if (attackIsDown && !attackWasDown_ && ammo_ > 0)
        {
            --ammo_;
            static_cast<void>(world_.FireHitscan(playerPosition_, LookDirection()));
            if (shotSound_)
                static_cast<void>(shotSound_->Play(0.35f, 0.0f, 0.0f));
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
        const int incomingDamage = world_.Update(clampedElapsed, playerPosition_);
        health_ -= incomingDamage;
        if (incomingDamage > 0 && hurtSound_)
            static_cast<void>(hurtSound_->Play(0.3f, -0.25f, 0.0f));
        if (health_ <= 0)
        {
            lives_ = std::max(0, lives_ - 1);
            health_ = 100;
            ammo_ = 12;
            gold_ = 0;
            playerPosition_ = world_.PlayerStart();
            completed_ = false;
        }
        HandleInput(clampedElapsed);
        const World::PickupResult pickups = world_.CollectPickups(playerPosition_);
        health_ = std::min(100, health_ + pickups.health);
        ammo_ = std::min(12, ammo_ + pickups.ammo);
        gold_ += pickups.gold;
        hasSecurityCard_ = hasSecurityCard_ || pickups.accessCards > 0;
        if ((pickups.health + pickups.ammo + pickups.gold + pickups.accessCards) > 0 && pickupSound_)
            static_cast<void>(pickupSound_->Play(0.28f, 0.0f, 0.0f));
        completed_ = completed_ || world_.ReachedExit(playerPosition_);

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

        if (effect_ && atlas_)
        {
            world_.Draw(
                device,
                *effect_,
                ViewMatrix(),
                ProjectionMatrix(),
                *atlas_);
        }

        DrawHud();

        Game::Draw(gameTime);
    }
}
