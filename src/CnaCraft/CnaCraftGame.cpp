#include "CnaCraftGame.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include "Render/TextureAtlas.hpp"
#include "Worlds/NoiseGenerator.hpp"
#include "Worlds/VoxelRaycast.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;

namespace CnaCraft {

namespace {
constexpr float kPiOver4 = 0.78539816339744830962f;
// Zoom (plan.md §11.4): narrows the FOV while Left Shift is held, mirroring
// Craft's own hold-to-zoom behavior (`g->fov = ... ? 15 : 65` in main.c) —
// not a toggle. Craft's absolute FOV numbers don't carry over 1:1 since our
// base FOV (kPiOver4, 45 degrees) already differs from Craft's default (65
// degrees) for unrelated reasons; kZoomFov keeps the same "much narrower"
// relationship instead.
constexpr float kZoomFov = 0.26179938779914943654f; // 15 degrees, in radians
constexpr float kMouseSensitivity = 0.0025f;
constexpr float kMaxReach = 6.0f;
constexpr std::uint32_t kWorldSeed = 1337;
// Orthographic toggle (plan.md §11.4): also hold-to-activate in Craft
// (`g->ortho = ... ? 64 : 0`), not a toggle despite the backlog's wording.
constexpr float kOrthoViewHeight = 24.0f; // world units (blocks) of vertical view extent
}

CnaCraftGame::CnaCraftGame() : graphics_(this) {
    static constexpr int kFps = 60;
    setTargetElapsedTimeProperty(System::TimeSpan::FromTicks(static_cast<long>(10000000L / kFps)));
}

const std::string& CnaCraftGame::GetTypeName() const {
    static const std::string name = "CnaCraftGame";
    return name;
}

void CnaCraftGame::Initialize() {
    Game::Initialize();

    auto& device = getGraphicsDeviceProperty();
    device.SetDepthTestEnabled(true);

    Mouse::setIsRelativeMouseModeEXTProperty(true);

    world_.Generate(kWorldSeed);

    chunkRenderers_.reserve(
        static_cast<std::size_t>(Worlds::WORLD_CHUNKS_X) * Worlds::WORLD_CHUNKS_Y * Worlds::WORLD_CHUNKS_Z);
    for (int cz = 0; cz < Worlds::WORLD_CHUNKS_Z; ++cz) {
        for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
            for (int cx = 0; cx < Worlds::WORLD_CHUNKS_X; ++cx) {
                chunkRenderers_.emplace_back(
                    cx * Worlds::CHUNK_SIZE, cy * Worlds::CHUNK_SIZE, cz * Worlds::CHUNK_SIZE);
            }
        }
    }
    RebuildDirtyChunks();

    effect_ = std::make_unique<BasicEffect>(device);
    effect_->VertexColorEnabled = false;
    effect_->setTextureEnabledProperty(true);
    effect_->EnableDefaultLighting();
    // EnableDefaultLighting()'s 3-light rig leaves faces angled away from all
    // three lights essentially unlit (visibly black on flat terrain faces);
    // floor it with a moderate ambient term so no face ever goes pure black,
    // matching Craft's block_fragment.glsl ("ambient = value*0.3+0.2", never
    // zero) — see THIRD_PARTY_NOTICES.md.
    effect_->setAmbientLightColorProperty(Vector3(0.5f, 0.5f, 0.5f));

    atlasTexture_ = std::make_unique<Texture2D>(Render::BuildPlaceholderAtlas(device));
    effect_->setTextureProperty(atlasTexture_.get());

    const float spawnX = static_cast<float>(Worlds::WORLD_SIZE_X) / 2.0f;
    const float spawnZ = static_cast<float>(Worlds::WORLD_SIZE_Z) / 2.0f;
    const int spawnHeight =
        Worlds::NoiseGenerator::Height(kWorldSeed, static_cast<int>(spawnX), static_cast<int>(spawnZ));
    player_ = std::make_unique<Worlds::PlayerController>(
        Core::Vec3f{spawnX, static_cast<float>(spawnHeight + 2), spawnZ});

    hud_ = std::make_unique<Render::Hud>(device);
    hotbarSlotNames_.reserve(Worlds::Hotbar::kSlots.size());
    for (Worlds::BlockType type : Worlds::Hotbar::kSlots) {
        hotbarSlotNames_.emplace_back(Worlds::GetBlockName(type));
    }
    hud_->RebuildHotbar(device, hotbarSlotNames_.data(), static_cast<int>(hotbarSlotNames_.size()),
                        hotbar_.SelectedIndex(), player_->IsFlying());
}

void CnaCraftGame::RebuildDirtyChunks() {
    auto& device = getGraphicsDeviceProperty();
    std::size_t i = 0;
    for (int cz = 0; cz < Worlds::WORLD_CHUNKS_Z; ++cz) {
        for (int cy = 0; cy < Worlds::WORLD_CHUNKS_Y; ++cy) {
            for (int cx = 0; cx < Worlds::WORLD_CHUNKS_X; ++cx, ++i) {
                Worlds::Chunk& chunk = world_.ChunkAt(cx, cy, cz);
                if (chunk.IsDirty()) {
                    chunkRenderers_[i].Rebuild(device, world_);
                    chunk.ClearDirty();
                }
            }
        }
    }
}

void CnaCraftGame::Update(GameTime& gameTime) {
    Game::Update(gameTime);

    if (smokeFramesLeft_ > 0) {
        if (--smokeFramesLeft_ == 0) {
            Exit();
            return;
        }
    }

    const float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());

    const auto kb = Keyboard::GetState();
    if (kb.IsKeyDown(Keys::Escape)) {
        Exit();
        return;
    }

    const auto mouse = Mouse::GetState();

    Worlds::PlayerInput input;
    if (kb.IsKeyDown(Keys::W)) input.moveForward += 1.0f;
    if (kb.IsKeyDown(Keys::S)) input.moveForward -= 1.0f;
    if (kb.IsKeyDown(Keys::D)) input.moveRight += 1.0f;
    if (kb.IsKeyDown(Keys::A)) input.moveRight -= 1.0f;
    input.jumpPressed = kb.IsKeyDown(Keys::Space);
    // Fly mode only (PlayerController ignores this in game mode): Space rises,
    // Left Ctrl descends. Left Shift is left free for the "Zoom" backlog item.
    if (kb.IsKeyDown(Keys::Space)) input.moveUp += 1.0f;
    if (kb.IsKeyDown(Keys::LeftControl)) input.moveUp -= 1.0f;
    input.lookDeltaYaw = static_cast<float>(mouse.getXProperty()) * kMouseSensitivity;
    input.lookDeltaPitch = -static_cast<float>(mouse.getYProperty()) * kMouseSensitivity;

    const auto rebuildHud = [this]() {
        hud_->RebuildHotbar(getGraphicsDeviceProperty(), hotbarSlotNames_.data(),
                            static_cast<int>(hotbarSlotNames_.size()), hotbar_.SelectedIndex(),
                            player_->IsFlying());
    };

    // Screenshot capture (plan.md §11.7): F12 is not a Craft key (its README's
    // "Screenshot" section is just a marketing image, not a documented
    // hotkey) but it's a common convention and CNA already exposes the
    // needed GraphicsDevice::GetBackBufferData/Texture2D::SaveAsPng — the
    // actual capture happens in Draw() once the frame is fully rendered.
    const bool f12Down = kb.IsKeyDown(Keys::F12);
    if (f12Down && !f12WasDown_) {
        screenshotPending_ = true;
    }
    f12WasDown_ = f12Down;

    const bool tabDown = kb.IsKeyDown(Keys::Tab);
    if (tabDown && !tabWasDown_) {
        player_->ToggleFlying();
        std::printf("Flying: %s\n", player_->IsFlying() ? "on" : "off");
        std::fflush(stdout);
        rebuildHud();
    }
    tabWasDown_ = tabDown;

    player_->Update(world_, input, dt);

    const int previousHotbarIndex = hotbar_.SelectedIndex();
    const int numberKeySlots = std::min(Worlds::Hotbar::kMaxNumberKeySlots, Worlds::Hotbar::SlotCount());
    for (int slot = 1; slot <= numberKeySlots; ++slot) {
        if (kb.IsKeyDown(static_cast<Keys>(static_cast<int>(Keys::D1) + slot - 1))) {
            hotbar_.SelectSlot(slot);
        }
    }
    const bool eDown = kb.IsKeyDown(Keys::E);
    if (eDown && !eKeyWasDown_) {
        hotbar_.CycleNext();
    }
    eKeyWasDown_ = eDown;
    if (hotbar_.SelectedIndex() != previousHotbarIndex) {
        std::printf("Selected block: %s\n", Worlds::GetBlockName(hotbar_.Selected()));
        std::fflush(stdout);
        rebuildHud();
    }

    const bool leftDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;
    const bool rightDown = mouse.getRightButtonProperty() == ButtonState::Pressed;

    if (leftDown && !leftClickWasDown_) {
        if (auto hit = Worlds::VoxelRaycast::Cast(
                world_, player_->EyePosition(), player_->LookDirection(), kMaxReach)) {
            world_.SetBlock(hit->x, hit->y, hit->z, Worlds::BlockType::Air);
        }
    }
    if (rightDown && !rightClickWasDown_) {
        if (auto hit = Worlds::VoxelRaycast::Cast(
                world_, player_->EyePosition(), player_->LookDirection(), kMaxReach)) {
            world_.SetBlock(hit->x + hit->nx, hit->y + hit->ny, hit->z + hit->nz, hotbar_.Selected());
        }
    }
    leftClickWasDown_ = leftDown;
    rightClickWasDown_ = rightDown;

    RebuildDirtyChunks();
}

void CnaCraftGame::Draw(const GameTime&) {
    auto& device = getGraphicsDeviceProperty();
    device.Clear(Color(135, 196, 235, 255), 1.0f);
    device.SetDepthTestEnabled(true);
    // Nearest-neighbor sampling: the atlas has no padding between tiles, so
    // linear filtering bleeds each tile's neighbor color (visible as magenta
    // speckling from the unused-tile fallback color) across every tile edge.
    // Craft's own texture.png atlas is sampled the same way (GL_NEAREST, see
    // main.c) — see THIRD_PARTY_NOTICES.md.
    device.getSamplerStatesProperty()[0] = SamplerState::PointClamp;

    const auto& vp = device.getViewportProperty();
    const float aspect = (vp.getHeightProperty() > 0)
        ? static_cast<float>(vp.getWidthProperty()) / static_cast<float>(vp.getHeightProperty())
        : 1.0f;

    const Core::Vec3f eye = player_->EyePosition();
    const Core::Vec3f dir = player_->LookDirection();
    const Vector3 eyeVec(eye.x, eye.y, eye.z);
    const Vector3 targetVec(eye.x + dir.x, eye.y + dir.y, eye.z + dir.z);

    effect_->View = Matrix::CreateLookAt(eyeVec, targetVec, Vector3::Up);

    const auto kb = Keyboard::GetState();
    if (kb.IsKeyDown(Keys::F)) {
        effect_->Projection = Matrix::CreateOrthographic(
            kOrthoViewHeight * aspect, kOrthoViewHeight, 0.1f, 500.0f);
    } else {
        const float fov = kb.IsKeyDown(Keys::LeftShift) ? kZoomFov : kPiOver4;
        effect_->Projection = Matrix::CreatePerspectiveFieldOfView(fov, aspect, 0.1f, 500.0f);
    }

    // Per-chunk frustum culling (plan.md §11.2): only draw chunks whose AABB
    // intersects the current view frustum. Cheap at today's fixed 32-chunk
    // world size, but the point is to stay correct once the world is bigger
    // (§9 M7) — mirrors Craft's own naive AABB-vs-frustum test in
    // src/main.c's render_chunks.
    const BoundingFrustum frustum(effect_->View * effect_->Projection);
    for (auto& renderer : chunkRenderers_) {
        if (!frustum.Intersects(renderer.Bounds())) continue;
        renderer.DrawOpaque(device, *effect_);
    }

    // Transparent geometry (plan.md §11.2 "Transparency for glass") is drawn
    // last, with blending on and depth writes off, so the opaque scene
    // behind it shows through — same order/state as house3d_demo.cpp's
    // solid-then-glass pass.
    device.SetBlendEnabled(true);
    device.SetDepthWriteEnabled(false);
    for (auto& renderer : chunkRenderers_) {
        if (!frustum.Intersects(renderer.Bounds())) continue;
        renderer.DrawTransparent(device, *effect_);
    }
    device.SetDepthWriteEnabled(true);
    device.SetBlendEnabled(false);

    hud_->Draw(device);

    if (screenshotPending_) {
        CaptureScreenshot(device);
        screenshotPending_ = false;
    }
}

void CnaCraftGame::CaptureScreenshot(GraphicsDevice& device) {
    const auto& vp = device.getViewportProperty();
    const int width = vp.getWidthProperty();
    const int height = vp.getHeightProperty();
    if (width <= 0 || height <= 0) return;

    std::vector<Color> pixels(static_cast<std::size_t>(width) * height, Color(0, 0, 0, 255));
    device.GetBackBufferData(pixels.data(), static_cast<int>(pixels.size()));

    Texture2D frame(device, width, height);
    frame.SetData(pixels.data(), static_cast<int>(pixels.size()));

    std::filesystem::create_directories("screenshots");
    char filename[64];
    std::snprintf(filename, sizeof(filename), "screenshots/cnacraft_%04d.png", ++screenshotCounter_);
    frame.SaveAsPng(filename);

    std::printf("Screenshot saved: %s\n", filename);
    std::fflush(stdout);
}

}
