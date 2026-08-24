#include "explore2d/CnaGame.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace explore2d::cna {
namespace {

using Microsoft::Xna::Framework::Input::Keys;
[[nodiscard]] constexpr Keys key(const int code) noexcept { return static_cast<Keys>(code); }

constexpr int kBackspace = 8;
constexpr int kEnter = 13;
constexpr int kEscape = 27;
constexpr int kSpace = 32;
constexpr int kLeft = 37;
constexpr int kUp = 38;
constexpr int kRight = 39;
constexpr int kDown = 40;
constexpr int kD1 = 49;
constexpr int kD2 = 50;
constexpr int kD3 = 51;
constexpr int kL = 76;
constexpr int kM = 77;
constexpr int kQ = 81;
constexpr int kS = 83;

} // namespace

AdventureGame::AdventureGame(
    WorldDefinition world,
    SessionConfig sessionConfig,
    RendererTheme rendererTheme,
    HostConfig hostConfig)
    : world_{std::move(world)},
      sessionConfig_{sessionConfig},
      rendererTheme_{rendererTheme},
      hostConfig_{std::move(hostConfig)},
      graphics_{std::make_unique<Microsoft::Xna::Framework::GraphicsDeviceManager>(this)},
      session_{world_, sessionConfig_},
      renderer_{world_, sessionConfig_, rendererTheme_}
{
    const int scale = std::max(1, hostConfig_.presentationScale);
    graphics_->setPreferredBackBufferWidthProperty(sessionConfig_.logicalWidth * scale);
    graphics_->setPreferredBackBufferHeightProperty(sessionConfig_.logicalHeight * scale);
    getWindowProperty().setTitleProperty(hostConfig_.windowTitle);
}

AdventureGame::~AdventureGame() = default;

void AdventureGame::Initialize() {
    Microsoft::Xna::Framework::Game::Initialize();
    getWindowProperty().setAllowUserResizingProperty(true);
}

void AdventureGame::LoadContent() {
    Microsoft::Xna::Framework::Game::LoadContent();
    frameTexture_ = std::make_unique<Microsoft::Xna::Framework::Graphics::Texture2D>(
        getGraphicsDeviceProperty(), sessionConfig_.logicalWidth, sessionConfig_.logicalHeight);
    spriteBatch_ = std::make_unique<Microsoft::Xna::Framework::Graphics::SpriteBatch>(getGraphicsDeviceProperty());
    pointClamp_ = std::make_unique<Microsoft::Xna::Framework::Graphics::SamplerState>(
        Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp);
}

bool AdventureGame::down(
    const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
    const int virtualKey) const
{
    return keyboard.IsKeyDown(key(virtualKey));
}

bool AdventureGame::pressed(
    const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
    const int virtualKey) const
{
    return down(keyboard, virtualKey) && !previousKeyboard_.IsKeyDown(key(virtualKey));
}

void AdventureGame::quickSave() {
    std::string error;
    if (saveSnapshot(session_.snapshot(), hostConfig_.savePath, &error)) {
        session_.showSystemMessage("Game saved to " + hostConfig_.savePath.string());
    } else {
        session_.showSystemMessage("Save failed: " + error);
    }
}

void AdventureGame::quickLoad() {
    LoadResult loaded = loadSnapshot(hostConfig_.savePath);
    if (!loaded) {
        session_.showSystemMessage("Load failed: " + loaded.error);
        return;
    }
    if (!session_.restore(*loaded.snapshot)) {
        session_.showSystemMessage("Load failed: save does not match this world.");
        return;
    }
    session_.showSystemMessage("Game loaded.");
}

void AdventureGame::updateWorldInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard) {
    if (pressed(keyboard, kUp)) session_.cycleVerb(1);
    if (pressed(keyboard, kDown)) session_.performSelectedVerb();
    if (pressed(keyboard, kD1)) session_.performVerb(Verb::use);
    if (pressed(keyboard, kD2)) session_.performVerb(Verb::examine);
    if (pressed(keyboard, kD3)) session_.performVerb(Verb::take);
    if (pressed(keyboard, kEnter) || pressed(keyboard, kSpace)) session_.jumpOrContext();
    if (pressed(keyboard, kM)) session_.openMap();
    if (pressed(keyboard, kS)) quickSave();
    if (pressed(keyboard, kL)) quickLoad();

    const bool repeatFrame = (updateCounter_ % 5U) == 0U;
    if (down(keyboard, kLeft) && (pressed(keyboard, kLeft) || repeatFrame)) session_.walk(Direction::left);
    if (down(keyboard, kRight) && (pressed(keyboard, kRight) || repeatFrame)) session_.walk(Direction::right);
}

void AdventureGame::Update(Microsoft::Xna::Framework::GameTime& gameTime) {
    ++updateCounter_;
    const auto keyboard = Microsoft::Xna::Framework::Input::Keyboard::GetState();

    if (pressed(keyboard, kQ)) {
        Exit();
    } else {
        switch (session_.mode()) {
        case SessionMode::world:
            updateWorldInput(keyboard);
            break;
        case SessionMode::choice:
        case SessionMode::map:
            if (pressed(keyboard, kUp)) session_.menuMove(-1);
            if (pressed(keyboard, kDown)) session_.menuMove(1);
            if (pressed(keyboard, kEnter) || pressed(keyboard, kSpace)) session_.confirm();
            if (pressed(keyboard, kEscape) || pressed(keyboard, kBackspace)) session_.cancel();
            break;
        case SessionMode::message:
            if (pressed(keyboard, kEnter) || pressed(keyboard, kSpace)) session_.advanceMessage();
            if (pressed(keyboard, kEscape)) session_.cancel();
            break;
        case SessionMode::dead:
        case SessionMode::won:
            if (pressed(keyboard, kEnter) || pressed(keyboard, kSpace)) session_.restart();
            break;
        }
        session_.tick(1.0F / 60.0F);
    }

    previousKeyboard_ = keyboard;
    Microsoft::Xna::Framework::Game::Update(gameTime);
}

void AdventureGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime) {
    getGraphicsDeviceProperty().Clear(Microsoft::Xna::Framework::Color::Black);
    renderer_.render(session_);
    if (frameTexture_ != nullptr && spriteBatch_ != nullptr) {
        const auto bytes = renderer_.canvas().bytes();
        frameTexture_->SetDataRGBA(bytes.data(), sessionConfig_.logicalWidth * sessionConfig_.logicalHeight);
        const auto& params = getGraphicsDeviceProperty().getPresentationParametersProperty();
        const int width = static_cast<int>(params.getBackBufferWidthProperty());
        const int height = static_cast<int>(params.getBackBufferHeightProperty());
        const float sx = static_cast<float>(width) / static_cast<float>(sessionConfig_.logicalWidth);
        const float sy = static_cast<float>(height) / static_cast<float>(sessionConfig_.logicalHeight);
        const float scale = std::max(1.0F, std::floor(std::min(sx, sy)));
        const int drawWidth = static_cast<int>(static_cast<float>(sessionConfig_.logicalWidth) * scale);
        const int drawHeight = static_cast<int>(static_cast<float>(sessionConfig_.logicalHeight) * scale);
        const int x = (width - drawWidth) / 2;
        const int y = (height - drawHeight) / 2;
        spriteBatch_->Begin(
            Microsoft::Xna::Framework::Graphics::SpriteSortMode::Deferred,
            Microsoft::Xna::Framework::Graphics::BlendState::Opaque,
            pointClamp_.get(), nullptr, nullptr);
        spriteBatch_->Draw(
            *frameTexture_,
            Microsoft::Xna::Framework::Rectangle{x, y, drawWidth, drawHeight},
            Microsoft::Xna::Framework::Rectangle{0, 0, sessionConfig_.logicalWidth, sessionConfig_.logicalHeight},
            Microsoft::Xna::Framework::Color::White);
        spriteBatch_->End();
    }
    Microsoft::Xna::Framework::Game::Draw(gameTime);
    ++renderedFrames_;
    if (hostConfig_.exitAfterFrames != 0U && renderedFrames_ >= hostConfig_.exitAfterFrames) Exit();
}

void AdventureGame::UnloadContent() {
    frameTexture_.reset();
    spriteBatch_.reset();
    pointClamp_.reset();
    Microsoft::Xna::Framework::Game::UnloadContent();
}

} // namespace explore2d::cna
