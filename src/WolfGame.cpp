#include "WolfGame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
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
    using namespace Microsoft::Xna::Framework::Graphics;
    using namespace Microsoft::Xna::Framework::Input;

    namespace
    {
        constexpr int PanelSize = 32;
        constexpr int PanelCount = 3;
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
            }
        }

        atlas_->SetData(pixels.data(), static_cast<int>(pixels.size()));
    }

    Vector3 WolfGame::LookDirection() const
    {
        const float cosPitch = std::cos(pitch_);

        return Vector3(
            std::sin(yaw_) * cosPitch,
            std::sin(pitch_),
            -std::cos(yaw_) * cosPitch);
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

        const float lookStep = KeyboardLookSpeed * elapsedSeconds;

        if (keyboard.IsKeyDown(Keys::Left))
            yaw_ -= lookStep;
        if (keyboard.IsKeyDown(Keys::Right))
            yaw_ += lookStep;
        if (keyboard.IsKeyDown(Keys::Up))
            pitch_ += lookStep;
        if (keyboard.IsKeyDown(Keys::Down))
            pitch_ -= lookStep;

        pitch_ = std::clamp(
            pitch_,
            MathHelper::ToRadians(-75.0f),
            MathHelper::ToRadians(75.0f));

        const float speed =
            (keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift))
                ? RunSpeed
                : WalkSpeed;

        float forwardInput = 0.0f;
        float strafeInput = 0.0f;

        if (keyboard.IsKeyDown(Keys::W))
            forwardInput += 1.0f;
        if (keyboard.IsKeyDown(Keys::S))
            forwardInput -= 1.0f;
        if (keyboard.IsKeyDown(Keys::D))
            strafeInput += 1.0f;
        if (keyboard.IsKeyDown(Keys::A))
            strafeInput -= 1.0f;

        const float inputLength = std::sqrt(
            forwardInput * forwardInput + strafeInput * strafeInput);

        if (inputLength <= 0.0f)
            return;

        // Prevent diagonal movement from being faster.
        forwardInput /= inputLength;
        strafeInput /= inputLength;

        const float forwardX = std::sin(yaw_);
        const float forwardZ = -std::cos(yaw_);
        const float rightX = std::cos(yaw_);
        const float rightZ = std::sin(yaw_);

        const float distance = speed * elapsedSeconds;
        const float dx = (forwardX * forwardInput + rightX * strafeInput) * distance;
        const float dz = (forwardZ * forwardInput + rightZ * strafeInput) * distance;

        TryMove(dx, dz);
    }

    void WolfGame::Update(GameTime& gameTime)
    {
        const float elapsed =
            static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

        // Clamp unusually long frames so a debugger pause cannot launch the player through walls.
        HandleInput(std::min(elapsed, 0.05f));

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

        Game::Draw(gameTime);
    }
}
