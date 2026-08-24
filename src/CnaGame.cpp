#include "explore2d/CnaGame.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioChannels.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
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
#include <cstdint>
#include <cmath>
#include <exception>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

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
constexpr int kF11 = 122;

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
    audioAvailable_ = hostConfig_.audioEnabled;
    const int scale = std::max(1, hostConfig_.presentationScale);
    graphics_->setPreferredBackBufferWidthProperty(ScreenMetrics::width * scale);
    graphics_->setPreferredBackBufferHeightProperty(ScreenMetrics::height * scale);
    getWindowProperty().setTitleProperty(hostConfig_.windowTitle);
}

AdventureGame::~AdventureGame() = default;

void AdventureGame::Initialize() {
    loadLanguageSetting();
    Microsoft::Xna::Framework::Game::Initialize();
    getWindowProperty().setAllowUserResizingProperty(true);
}

void AdventureGame::LoadContent() {
    Microsoft::Xna::Framework::Game::LoadContent();
    frameTexture_ = std::make_unique<Microsoft::Xna::Framework::Graphics::Texture2D>(
        getGraphicsDeviceProperty(), ScreenMetrics::width, ScreenMetrics::height);
    spriteBatch_ = std::make_unique<Microsoft::Xna::Framework::Graphics::SpriteBatch>(getGraphicsDeviceProperty());
    pointClamp_ = std::make_unique<Microsoft::Xna::Framework::Graphics::SamplerState>(
        Microsoft::Xna::Framework::Graphics::SamplerState::PointClamp);
    playSoundEffect(world_.presentation.sounds.title);
}

void AdventureGame::playSoundEffect(const std::string_view id) {
    if (!audioAvailable_ || id.empty()) return;
    const ToneEffectDefinition* effect = world_.soundEffect(id);
    if (effect == nullptr) return;
    const std::vector<std::int16_t> samples = synthesizeToneEffect(*effect);
    if (samples.empty()) return;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(samples.size() * 2U);
    for (const std::int16_t sample : samples) {
        const auto raw = static_cast<std::uint16_t>(sample);
        bytes.push_back(static_cast<std::uint8_t>(raw & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>((raw >> 8U) & 0xFFU));
    }
    try {
        // QBasic's PC speaker is monophonic: a fresh SOUND interrupts the old one.
        activeSound_.reset();
        activeSound_ = std::make_unique<Microsoft::Xna::Framework::Audio::SoundEffect>(
            bytes,
            qbasicSoundSampleRate,
            Microsoft::Xna::Framework::Audio::AudioChannels::Mono);
        if (!activeSound_->Play()) activeSound_.reset();
        activeSoundRemaining_ = toneEffectDurationSeconds(*effect) + 0.05F;
    } catch (const std::exception&) {
        activeSound_.reset();
        activeSoundRemaining_ = 0.0F;
        audioAvailable_ = false;
    }
}

void AdventureGame::drainSessionSounds() {
    for (const std::string& id : session_.takePendingSoundEffects()) playSoundEffect(id);
}

void AdventureGame::updateSound(const float seconds) {
    if (activeSound_ == nullptr) return;
    activeSoundRemaining_ -= std::max(0.0F, seconds);
    if (activeSoundRemaining_ <= 0.0F) {
        activeSound_.reset();
        activeSoundRemaining_ = 0.0F;
    }
}

void AdventureGame::updateTitleInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard) {
    if (pressed(keyboard, kUp)) {
        titleSelection_ = (titleSelection_ + 3U) % 4U;
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
    if (pressed(keyboard, kDown)) {
        titleSelection_ = (titleSelection_ + 1U) % 4U;
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
    if (pressed(keyboard, kEscape)) {
        Exit();
        return;
    }
    if (!pressed(keyboard, kEnter) && !pressed(keyboard, kSpace)) return;
    switch (titleSelection_) {
    case 0:
        playSoundEffect(world_.presentation.sounds.menuConfirm);
        session_.restart();
        shellMode_ = ShellMode::playing;
        break;
    case 1:
        quickLoad();
        shellMode_ = ShellMode::playing;
        break;
    case 2:
        playSoundEffect(world_.presentation.sounds.menuConfirm);
        settingsSelection_ = 0;
        shellMode_ = ShellMode::titleSettings;
        break;
    default:
        Exit();
        break;
    }
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
        playSoundEffect(world_.presentation.sounds.save);
        session_.showSystemMessage(world_.presentation.interfaceText.gameSaved);
    } else {
        playSoundEffect(world_.presentation.sounds.warning);
        session_.showSystemMessage(world_.presentation.interfaceText.saveFailed);
    }
}

void AdventureGame::quickLoad() {
    LoadResult loaded = loadSnapshot(hostConfig_.savePath);
    if (!loaded) {
        playSoundEffect(world_.presentation.sounds.warning);
        session_.showSystemMessage(world_.presentation.interfaceText.loadFailed);
        return;
    }
    if (!session_.restore(*loaded.snapshot)) {
        playSoundEffect(world_.presentation.sounds.warning);
        session_.showSystemMessage(world_.presentation.interfaceText.loadWorldMismatch);
        return;
    }
    playSoundEffect(world_.presentation.sounds.load);
    session_.showSystemMessage(world_.presentation.interfaceText.gameLoaded);
}

void AdventureGame::updateWorldInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard) {
    if (pressed(keyboard, kEscape)) {
        pauseSelection_ = 0;
        shellMode_ = ShellMode::pause;
        return;
    }
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

void AdventureGame::updatePauseInput(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard) {
    if (pressed(keyboard, kUp)) {
        pauseSelection_ = (pauseSelection_ + 2U) % 3U;
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
    if (pressed(keyboard, kDown)) {
        pauseSelection_ = (pauseSelection_ + 1U) % 3U;
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
    if (pressed(keyboard, kEscape)) {
        shellMode_ = ShellMode::playing;
        return;
    }
    if (!pressed(keyboard, kEnter) && !pressed(keyboard, kSpace)) return;
    playSoundEffect(world_.presentation.sounds.menuConfirm);
    switch (pauseSelection_) {
    case 0:
        shellMode_ = ShellMode::playing;
        break;
    case 1:
        settingsSelection_ = 0;
        shellMode_ = ShellMode::pauseSettings;
        break;
    default:
        titleSelection_ = 0;
        shellMode_ = ShellMode::title;
        break;
    }
}

void AdventureGame::cycleLanguage(const int delta) {
    const auto& languages = world_.localization.languages;
    if (languages.empty()) return;
    int current = 0;
    for (std::size_t index = 0; index < languages.size(); ++index) {
        if (languages[index].id == session_.language()) {
            current = static_cast<int>(index);
            break;
        }
    }
    const int size = static_cast<int>(languages.size());
    current = (current + delta) % size;
    if (current < 0) current += size;
    if (session_.setLanguage(languages[static_cast<std::size_t>(current)].id)) {
        saveLanguageSetting();
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
}

void AdventureGame::updateSettingsInput(
    const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
    const bool fromPause)
{
    if (pressed(keyboard, kUp) || pressed(keyboard, kDown)) {
        settingsSelection_ = (settingsSelection_ + 1U) % 2U;
        playSoundEffect(world_.presentation.sounds.menuMove);
    }
    if (settingsSelection_ == 0U && pressed(keyboard, kLeft)) cycleLanguage(-1);
    if (settingsSelection_ == 0U && pressed(keyboard, kRight)) cycleLanguage(1);
    if (pressed(keyboard, kEscape)) {
        shellMode_ = fromPause ? ShellMode::pause : ShellMode::title;
        return;
    }
    if (!pressed(keyboard, kEnter) && !pressed(keyboard, kSpace)) return;
    if (settingsSelection_ == 0U) {
        cycleLanguage(1);
    } else {
        playSoundEffect(world_.presentation.sounds.menuConfirm);
        shellMode_ = fromPause ? ShellMode::pause : ShellMode::title;
    }
}

void AdventureGame::loadLanguageSetting() {
    if (hostConfig_.settingsPath.empty()) return;
    std::ifstream input{hostConfig_.settingsPath};
    std::string label;
    std::string language;
    if (input >> label >> language && label == "LANGUAGE") {
        static_cast<void>(session_.setLanguage(language));
    }
}

void AdventureGame::saveLanguageSetting() const {
    if (hostConfig_.settingsPath.empty()) return;
    std::ofstream output{hostConfig_.settingsPath, std::ios::trunc};
    if (output) output << "LANGUAGE " << session_.language() << '\n';
}

void AdventureGame::Update(Microsoft::Xna::Framework::GameTime& gameTime) {
    ++updateCounter_;
    const auto keyboard = Microsoft::Xna::Framework::Input::Keyboard::GetState();

    if (pressed(keyboard, kF11)) graphics_->ToggleFullScreen();
    if (pressed(keyboard, kQ)) {
        Exit();
    } else {
        switch (shellMode_) {
        case ShellMode::title:
            updateTitleInput(keyboard);
            break;
        case ShellMode::titleSettings:
            updateSettingsInput(keyboard, false);
            break;
        case ShellMode::pause:
            updatePauseInput(keyboard);
            break;
        case ShellMode::pauseSettings:
            updateSettingsInput(keyboard, true);
            break;
        case ShellMode::playing:
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
            break;
        }
    }
    drainSessionSounds();
    updateSound(1.0F / 60.0F);

    previousKeyboard_ = keyboard;
    Microsoft::Xna::Framework::Game::Update(gameTime);
}

void AdventureGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime) {
    getGraphicsDeviceProperty().Clear(Microsoft::Xna::Framework::Color::Black);
    switch (shellMode_) {
    case ShellMode::title:
        renderer_.renderTitle(titleSelection_, session_.language());
        break;
    case ShellMode::titleSettings:
        renderer_.renderSettings(settingsSelection_, session_.language());
        break;
    case ShellMode::playing:
        renderer_.render(session_);
        break;
    case ShellMode::pause:
        renderer_.renderPause(session_, pauseSelection_);
        break;
    case ShellMode::pauseSettings:
        renderer_.renderSettings(settingsSelection_, session_.language(), &session_);
        break;
    }
    if (frameTexture_ != nullptr && spriteBatch_ != nullptr) {
        const auto bytes = renderer_.canvas().bytes();
        frameTexture_->SetDataRGBA(bytes.data(), ScreenMetrics::width * ScreenMetrics::height);
        const auto& params = getGraphicsDeviceProperty().getPresentationParametersProperty();
        const int width = static_cast<int>(params.getBackBufferWidthProperty());
        const int height = static_cast<int>(params.getBackBufferHeightProperty());
        const float sx = static_cast<float>(width) / static_cast<float>(ScreenMetrics::width);
        const float sy = static_cast<float>(height) / static_cast<float>(ScreenMetrics::height);
        const float scale = std::max(1.0F, std::floor(std::min(sx, sy)));
        const int drawWidth = static_cast<int>(static_cast<float>(ScreenMetrics::width) * scale);
        const int drawHeight = static_cast<int>(static_cast<float>(ScreenMetrics::height) * scale);
        const int x = (width - drawWidth) / 2;
        const int y = (height - drawHeight) / 2;
        spriteBatch_->Begin(
            Microsoft::Xna::Framework::Graphics::SpriteSortMode::Deferred,
            Microsoft::Xna::Framework::Graphics::BlendState::Opaque,
            pointClamp_.get(), nullptr, nullptr);
        spriteBatch_->Draw(
            *frameTexture_,
            Microsoft::Xna::Framework::Rectangle{x, y, drawWidth, drawHeight},
            Microsoft::Xna::Framework::Rectangle{0, 0, ScreenMetrics::width, ScreenMetrics::height},
            Microsoft::Xna::Framework::Color::White);
        spriteBatch_->End();
    }
    Microsoft::Xna::Framework::Game::Draw(gameTime);
    ++renderedFrames_;
    if (hostConfig_.exitAfterFrames != 0U && renderedFrames_ >= hostConfig_.exitAfterFrames) Exit();
}

void AdventureGame::UnloadContent() {
    activeSound_.reset();
    frameTexture_.reset();
    spriteBatch_.reset();
    pointClamp_.reset();
    Microsoft::Xna::Framework::Game::UnloadContent();
}

} // namespace explore2d::cna
