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
#include "Worlds/DayNightCycle.hpp"
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

// Distance fog (CRAFT_PARITY.md §5.2): Craft's own block_vertex.glsl fades
// toward a sampled sky-texture color by camera distance
// (`fog_distance = render_radius * CHUNK_SIZE`, 320 units in real Craft).
// This project has no sky dome/texture yet (separate backlog item), so
// FogColor is set to the same flat sky clear color already computed below
// each frame instead — same "fade toward the sky" intent, simpler source.
// Verified NOT blocked by the shader-backend limitations in missing.md:
// BasicEffect's fog is standard XNA surface (FogEnabled/FogColor/FogStart/
// FogEnd), not a custom ShaderEffect — CNA has real fog support for the
// lit+textured BasicEffect path on EASYGL, VULKAN, and BGFX alike (see
// ../cna/examples/{easygl,vulkan,bgfx}_basiceffect_lit_fog_test.cpp).
// kFogStart/kFogEnd are scaled to this project's fixed 128x64x128 world
// (horizontal diagonal ~181 units) rather than Craft's streamed-world
// render radius, which has no equivalent here.
constexpr float kFogStart = 70.0f;
constexpr float kFogEnd = 150.0f;
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
    // zero — see THIRD_PARTY_NOTICES.md). Set here as the first frame's
    // starting value; Draw() recomputes it every frame from the day/night
    // cycle (plan.md §11.3) using the same formula with a live `daylight`.
    effect_->setAmbientLightColorProperty(Vector3(0.5f, 0.5f, 0.5f));

    atlasTexture_ = std::make_unique<Texture2D>(Render::BuildProceduralAtlas(device));
    effect_->setTextureProperty(atlasTexture_.get());

    // Bug fix: spawning at an *integer* coordinate puts the player's 0.6-wide
    // hitbox (kPlayerHalfWidth=0.3) exactly on the boundary between two
    // block columns, straddling both equally. With Simplex noise's steeper
    // local height changes (§11.1) a neighboring column can be much taller
    // right next to spawn, permanently wedging the player against it --
    // unable to reach its own column's true floor or move in any direction.
    // Spawning at block *center* (integer + 0.5) keeps the hitbox fully
    // inside its own column instead.
    const int spawnColumnX = Worlds::WORLD_SIZE_X / 2;
    const int spawnColumnZ = Worlds::WORLD_SIZE_Z / 2;
    const float spawnX = static_cast<float>(spawnColumnX) + 0.5f;
    const float spawnZ = static_cast<float>(spawnColumnZ) + 0.5f;
    const int spawnHeight = Worlds::NoiseGenerator::Height(kWorldSeed, spawnColumnX, spawnColumnZ);
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
    // Arrow keys as a keyboard alternative to mouse-look (some players don't
    // want to use the mouse for turning; also more reliable to test than
    // relative mouse motion). Additive with the mouse deltas above, same
    // rotSpeed formula as house3d_demo.cpp's Left/Right/Up/Down turning.
    const float rotSpeed = 1.6f * dt;
    if (kb.IsKeyDown(Keys::Left)) input.lookDeltaYaw -= rotSpeed;
    if (kb.IsKeyDown(Keys::Right)) input.lookDeltaYaw += rotSpeed;
    if (kb.IsKeyDown(Keys::Up)) input.lookDeltaPitch += rotSpeed;
    if (kb.IsKeyDown(Keys::Down)) input.lookDeltaPitch -= rotSpeed;

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

    // Raycast once per frame -- reused for break/place, the middle-click
    // eyedropper, and the visible targeted-block outline (CRAFT_PARITY.md
    // §2.4), instead of a separate cast per click as before.
    const auto hit = Worlds::VoxelRaycast::Cast(world_, player_->EyePosition(), player_->LookDirection(), kMaxReach);
    hasTargetedBlock_ = hit.has_value();
    if (hit) {
        targetedBlockX_ = hit->x;
        targetedBlockY_ = hit->y;
        targetedBlockZ_ = hit->z;
    }

    const int previousHotbarIndex = hotbar_.SelectedIndex();
    // Craft's on_key (CRAFT_PARITY.md §2.1) maps keys 1-9 to slots 0-8 and
    // key 0 to slot 9 (a 10th direct-key slot) -- kMaxNumberKeySlots stays
    // capped at 9 (Hotbar.hpp), key 0 is handled separately as slot 10.
    const int numberKeySlots = std::min(Worlds::Hotbar::kMaxNumberKeySlots, Worlds::Hotbar::SlotCount());
    for (int slot = 1; slot <= numberKeySlots; ++slot) {
        if (kb.IsKeyDown(static_cast<Keys>(static_cast<int>(Keys::D1) + slot - 1))) {
            hotbar_.SelectSlot(slot);
        }
    }
    if (Worlds::Hotbar::SlotCount() >= 10 && kb.IsKeyDown(Keys::D0)) {
        hotbar_.SelectSlot(10);
    }
    const bool eDown = kb.IsKeyDown(Keys::E);
    if (eDown && !eKeyWasDown_) {
        hotbar_.CycleNext();
    }
    eKeyWasDown_ = eDown;
    // R = reverse-cycle, mirrors Craft's E/R pair (CRAFT_PARITY.md §2.1).
    const bool rDown = kb.IsKeyDown(Keys::R);
    if (rDown && !rKeyWasDown_) {
        hotbar_.CyclePrev();
    }
    rKeyWasDown_ = rDown;
    // Scroll wheel also cycles the hotbar, matching Craft's on_scroll
    // (CRAFT_PARITY.md §2.1). CNA's ScrollWheelValue is cumulative (XNA
    // convention), so compare against the previous frame's value; the first
    // frame just captures a baseline (no synthetic cycle on startup).
    const int scrollWheelValue = mouse.getScrollWheelValueProperty();
    if (scrollWheelInitialized_) {
        const int scrollDelta = scrollWheelValue - previousScrollWheelValue_;
        if (scrollDelta > 0) {
            hotbar_.CycleNext();
        } else if (scrollDelta < 0) {
            hotbar_.CyclePrev();
        }
    }
    scrollWheelInitialized_ = true;
    previousScrollWheelValue_ = scrollWheelValue;

    const bool leftDown = mouse.getLeftButtonProperty() == ButtonState::Pressed;
    const bool rightDown = mouse.getRightButtonProperty() == ButtonState::Pressed;
    const bool middleDown = mouse.getMiddleButtonProperty() == ButtonState::Pressed;

    // Middle-click "eyedropper" (CRAFT_PARITY.md §2.7): selects the hotbar
    // slot matching the targeted block's type, ports Craft's real
    // on_middle_click. Silently does nothing if the target isn't in the
    // placeable roster (e.g. Bedrock), matching Craft's own behavior.
    if (hit && middleDown && !middleClickWasDown_) {
        hotbar_.SelectByBlockType(world_.GetBlock(hit->x, hit->y, hit->z));
    }
    middleClickWasDown_ = middleDown;

    if (hotbar_.SelectedIndex() != previousHotbarIndex) {
        std::printf("Selected block: %s\n", Worlds::GetBlockName(hotbar_.Selected()));
        std::fflush(stdout);
        rebuildHud();
    }

    if (hit && leftDown && !leftClickWasDown_) {
        // CRAFT_PARITY.md §2.5: only break blocks World::IsBreakable allows
        // (ports Craft's `is_destructable` guard in on_left_click) — Bedrock,
        // a cna-craft-only "world-boundary, not meant to be placed" block,
        // could previously be mined away with no protection at all.
        if (world_.IsBreakable(hit->x, hit->y, hit->z)) {
            world_.SetBlock(hit->x, hit->y, hit->z, Worlds::BlockType::Air);
        }
    }
    if (hit && rightDown && !rightClickWasDown_) {
        const int px = hit->x + hit->nx, py = hit->y + hit->ny, pz = hit->z + hit->nz;
        // CRAFT_PARITY.md §2.6: reject a placement that would overlap the
        // player's own body, matching Craft's on_right_click guard
        // (`!player_intersects_block(2, s->x,s->y,s->z, hx,hy,hz)`).
        if (!player_->IntersectsBlock(px, py, pz)) {
            world_.SetBlock(px, py, pz, hotbar_.Selected());
        }
    }
    leftClickWasDown_ = leftDown;
    rightClickWasDown_ = rightDown;

    RebuildDirtyChunks();
}

void CnaCraftGame::Draw(const GameTime& gameTime) {
    auto& device = getGraphicsDeviceProperty();

    // Day/night cycle (plan.md §11.3): a daylight value in [0, 1] driven by
    // GameTime::TotalGameTime, matching Craft's own get_daylight()/
    // time_of_day() curve shape (src/main.c) — dawn/dusk sigmoid transitions
    // bracketing long full-day/full-night plateaus. Drives BasicEffect's
    // ambient term with the same `value*0.3+0.2` formula as
    // block_fragment.glsl, and tints the (still-flat, no sky dome yet —
    // that's a separate backlog item) clear color between night and day.
    const float daylight = Worlds::ComputeDaylight(
        static_cast<float>(gameTime.getTotalGameTimeProperty().getTotalSecondsProperty()));
    const float ambient = daylight * 0.3f + 0.2f;
    effect_->setAmbientLightColorProperty(Vector3(ambient, ambient, ambient));

    const auto lerpChannel = [daylight](int night, int day) {
        return static_cast<int>(static_cast<float>(night) + static_cast<float>(day - night) * daylight);
    };
    const int skyR = lerpChannel(12, 135), skyG = lerpChannel(14, 196), skyB = lerpChannel(36, 235);
    // Zenith tint for the sky dome (CRAFT_PARITY.md §5.3) — a deeper blue
    // than the horizon color at both day and night, same lerp function.
    const int zenithR = lerpChannel(4, 60), zenithG = lerpChannel(6, 120), zenithB = lerpChannel(18, 200);
    device.Clear(Color(skyR, skyG, skyB, 255), 1.0f);
    device.SetDepthTestEnabled(true);

    // Distance fog (CRAFT_PARITY.md §5.2) — fades geometry toward the same
    // flat sky color used for the clear, so the fixed world's edges recede
    // instead of hard-cutting at the far clip plane. See the kFogStart/
    // kFogEnd comment above for why this isn't blocked by shader limits.
    effect_->setFogEnabledProperty(true);
    effect_->setFogColorProperty(Vector3(static_cast<float>(skyR) / 255.0f, static_cast<float>(skyG) / 255.0f,
                                          static_cast<float>(skyB) / 255.0f));
    effect_->setFogStartProperty(kFogStart);
    effect_->setFogEndProperty(kFogEnd);
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
        // Craft's own shader disables fog in ortho mode (`if (bool(ortho))
        // fog_factor = 0.0`, block_vertex.glsl) — matched here.
        effect_->setFogEnabledProperty(false);
    } else {
        const float fov = kb.IsKeyDown(Keys::LeftShift) ? kZoomFov : kPiOver4;
        effect_->Projection = Matrix::CreatePerspectiveFieldOfView(fov, aspect, 0.1f, 500.0f);
    }

    // Sky dome (CRAFT_PARITY.md §5.3) — a plain vertex-colored gradient
    // hemisphere replacing the flat clear color, drawn first with depth
    // writes off so it never occludes anything drawn afterward. Fog is
    // switched off for this draw (fading the sky into itself would be a
    // no-op at best, visibly wrong at worst); vertex-color/unlit mode is
    // used the same way SelectionOutline uses it below, then restored.
    skyDome_.Update(device, Color(skyR, skyG, skyB, 255), Color(zenithR, zenithG, zenithB, 255));
    device.SetDepthWriteEnabled(false);
    const bool fogWasEnabled = effect_->getFogEnabledProperty();
    effect_->setFogEnabledProperty(false);
    effect_->setTextureEnabledProperty(false);
    effect_->VertexColorEnabled = true;
    effect_->setLightingEnabledProperty(false);
    skyDome_.Draw(device, *effect_, eyeVec);
    effect_->setLightingEnabledProperty(true);
    effect_->VertexColorEnabled = false;
    effect_->setTextureEnabledProperty(true);
    effect_->setFogEnabledProperty(fogWasEnabled);
    device.SetDepthWriteEnabled(true);

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

    // Visible targeted-block outline (CRAFT_PARITY.md §2.4) — drawn right
    // after opaque geometry, same ordering as Craft's own render_wireframe
    // call (right after the solid-block render pass, before transparent
    // blocks/HUD). Temporarily switches the shared BasicEffect to plain
    // vertex-color/unlit mode (VertexPositionColor has no UV/normal), then
    // restores it for the textured/lit chunk geometry that follows.
    if (hasTargetedBlock_) {
        selectionOutline_.Update(device, targetedBlockX_, targetedBlockY_, targetedBlockZ_);
        effect_->setTextureEnabledProperty(false);
        effect_->VertexColorEnabled = true;
        effect_->setLightingEnabledProperty(false);
        selectionOutline_.Draw(device, *effect_);
        effect_->setLightingEnabledProperty(true);
        effect_->VertexColorEnabled = false;
        effect_->setTextureEnabledProperty(true);
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
