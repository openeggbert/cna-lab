// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MeshWorldApp — menu + 3D world explorer.
//
// Controls (explore mode):
//   Mouse            — look around (free-look; moves the mouse to glance without changing
//                      the player's own facing direction — moving/turning snaps the camera
//                      back to standard; disabled while the map view, N below, is open)
//   Arrow Up/Down    — move forward / back (pans the map view instead, see N below, while it's open)
//   Arrow Left/Right — turn left / right (pans the map view instead, see N below, while it's open)
//   Left Shift       — sprint (4× speed)
//   Left Ctrl        — jump
//   M                — toggle minimap (small always-on corner overlay, legacy chunk zone colors only)
//   N                — toggle map view (full pannable/zoomable planetary map — mouse wheel zooms,
//                      left-click recenters, arrow keys pan; shows real place-name labels)
//   F11              — toggle fullscreen
//   ESC              — return to main menu

#include "BuiltinMaterials.hpp"
#include "BuiltinStyles.hpp"
#include "CelestialPosition.hpp"
#include "ComposerAssets.hpp"
#include "ContentPackLoader.hpp"
#include "Map/MapPipeline.hpp"
#include "MapTileFetchQueue.hpp"
#include "MapView.hpp"
#include "Mc3Renderer.hpp"          // also defines MeshWorld::FPCamera
#include "Model3DStreamer.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "PersistentWorldMap.hpp"
#include "PlanetMapLogic.hpp"      // is_existing_planet_world()/planet_params_from_config()
#include "PlanetWorld.hpp"
#include "ParticleSystem.hpp"
#include "PlayerCollision.hpp"
#include "SkyColor.hpp"
#include "SnowAccumulation.hpp"
#include "TimeOfDay.hpp"
#include "Weather.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "WorldRenderer.hpp"

#include <Microsoft/Xna/Framework/Game.hpp>
#include <Microsoft/Xna/Framework/GameTime.hpp>
#include <Microsoft/Xna/Framework/GraphicsDeviceManager.hpp>
#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/Mouse.hpp>
#include <Microsoft/Xna/Framework/Input/MouseState.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace MeshWorld;

// ---------------------------------------------------------------------------
// SDL event watcher — forwards every SDL event to ImGui
// ---------------------------------------------------------------------------
static bool SDLCALL imgui_event_watch(void*, SDL_Event* e) {
    ImGui_ImplSDL3_ProcessEvent(e);
    return true;
}

// ---------------------------------------------------------------------------
// Helpers: world directory scanning
// ---------------------------------------------------------------------------
static std::vector<std::string> scan_worlds() {
    std::vector<std::string> names;
    std::error_code ec;
    if (fs::is_directory("saves", ec)) {
        for (const auto& entry : fs::directory_iterator("saves", ec))
            if (!ec && entry.is_directory(ec) && !ec)
                names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

static std::string next_world_name() {
    auto existing = scan_worlds();
    for (int n = 1; n <= 9999; ++n) {
        std::string c = "world" + std::to_string(n);
        if (std::find(existing.begin(), existing.end(), c) == existing.end())
            return c;
    }
    return "world1";
}

static constexpr const char* kCityShowcaseConfigPath = "examples/city_showcase.json";
static constexpr const char* kCityShowcaseSaveName = "city_showcase";
static constexpr const char* kBiomeShowcaseConfigPath = "examples/biome_showcase.json";
static constexpr const char* kBiomeShowcaseSaveName = "biome_showcase";
// A curated demo must not reuse chunks that may have been cached by an
// earlier procedural version of this same save directory. R134 changes the
// physical road-edge graph, so the old r114 geometry would otherwise keep
// showing visual road stubs despite the fixed generator. Bump this whenever
// an authored showcase layout or its generated geometry changes.
static constexpr const char* kCityShowcaseChunkCacheDir = "saves/city_showcase/r134_road_graph_chunks";
// Kept separate from the city cache so this visual tour remains reproducible
// and never inherits arbitrary chunks from an ordinary persistent world.
static constexpr const char* kBiomeShowcaseChunkCacheDir = "saves/biome_showcase/r142_biome_showcase_chunks";

// Ordinary persistent worlds need the same cache separation: their SQLite
// region history is retained, but their old MC3 chunks must not outlive a
// generator/topology correction.
static constexpr const char* kChunkCacheGeneration = "r142_biome_family_routing";
// Chunk XML is derived data, not world persistence. Bound the interactive
// cache to keep a long walk from filling the SSD; the 13 resident chunks fit
// comfortably inside this 256-entry working set.
static constexpr std::size_t kInteractiveChunkCacheMaxEntries = 256;

// ---------------------------------------------------------------------------
// WorldApp
// ---------------------------------------------------------------------------
class WorldApp : public Game {
public:
    WorldApp() {
        register_builtin_materials();
        ObjectDefinitionLibrary::instance().load_all();
        register_builtin_styles();
        // The app renders generated <instance> references through
        // ObjectDefinitionLibrary, but BuildingComposer queries the separate
        // AssetRegistry first. Without this startup registration an opted-in
        // composer world silently falls back to its legacy generators.
        register_composer_assets();
        // §5 #17's fix (MeshWorld/MeshWorldExport/MeshWorldPlanet) applies
        // here too, now that start_explore() attaches a real Map::MapPipeline
        // — without this, every Lua map/chunk generator silently falls back
        // to its C++ counterpart.
        ContentPackLoader{}.load_auto(".", "meshworld_content.sqlite");
        graphics_ = std::make_unique<GraphicsDeviceManager>(this);
        graphics_->setPreferredBackBufferWidthProperty(1280);
        graphics_->setPreferredBackBufferHeightProperty(720);
    }

    ~WorldApp() override {
        stop_explore_internal(); // joins background threads
        shutdown_imgui();
    }

protected:
    // -----------------------------------------------------------------------
    void Initialize() override {
        Game::Initialize();
        mc3_renderer_ = std::make_unique<Mc3Renderer>(getGraphicsDeviceProperty());
        init_imgui();
        setIsMouseVisibleProperty(true);
        world_list_ = scan_worlds();
    }

    void LoadContent() override {}

    // -----------------------------------------------------------------------
    void Update(GameTime& gt) override {
        const float dt = static_cast<float>(
            gt.getElapsedGameTimeProperty().getTotalSecondsProperty());

        handle_f11();

        // Execute deferred menu commands (to avoid modifying world_list_ inside ImGui loops)
        if (pending_enter_) {
            pending_enter_ = false;
            start_explore(pending_world_name_);
            return; // skip further update this frame
        }
        if (pending_create_) {
            pending_create_ = false;
            const std::string wn = next_world_name();
            fs::create_directories("saves/" + wn + "/chunks");
            world_list_ = scan_worlds();
            start_explore(wn);
            return;
        }
        if (pending_showcase_) {
            pending_showcase_ = false;
            start_city_showcase();
            return;
        }
        if (pending_biome_showcase_) {
            pending_biome_showcase_ = false;
            start_biome_showcase();
            return;
        }
        if (pending_rename_) {
            pending_rename_ = false;
            do_rename(pending_old_name_, pending_new_name_);
        }
        if (pending_delete_) {
            pending_delete_ = false;
            do_delete(pending_world_name_);
        }

        if (state_ == State::MENU)
            update_menu();
        else if (dt > 0.f && dt < 0.25f)
            update_explore(dt);
    }

    // -----------------------------------------------------------------------
    void Draw(const GameTime&) override {
        // S202 -- the sky background now follows the day/night cycle
        // (S101/S201) instead of a flat clear color, while exploring; the
        // menu screen keeps its own fixed dark background (not meant to
        // represent an in-game sky).
        if (state_ == State::EXPLORE) {
            const auto sky = sky_color(time_of_day_.hours());
            getGraphicsDeviceProperty().Clear(Color(
                static_cast<int>(sky[0] * 255.0f), static_cast<int>(sky[1] * 255.0f),
                static_cast<int>(sky[2] * 255.0f), 255));
        } else {
            getGraphicsDeviceProperty().Clear(Color(15, 15, 25, 255));
        }

        if (state_ == State::EXPLORE && world_renderer_) {
            // Free-look (2026-07-11) -- the temporary look offset only ever
            // affects what's actually rendered, never camera_ itself (the
            // player's committed position/facing, used for movement and
            // model-streaming queries) -- see look_yaw_offset_'s own doc
            // comment on the member field.
            FPCamera render_cam = camera_;
            render_cam.yaw   += look_yaw_offset_;
            render_cam.pitch += look_pitch_offset_;

            // S301/S302 -- the sun, drawn "at infinity" (camera-relative,
            // far past any chunk/placement render distance) before the
            // world geometry below, same "background first" order a
            // skybox would use.
            const double hours = time_of_day_.hours();
            const SkyAngle sun_angle  = sun_position(hours);
            const SkyAngle moon_angle = moon_position(hours);
            // S501/S502/S503 -- stars, drawn first (farthest/most
            // "background" of the three sky layers -- a no-op in daylight,
            // see visible_star_count()).
            world_renderer_->render_stars(*mc3_renderer_, render_cam, star_field_, sun_angle);
            world_renderer_->render_sun(*mc3_renderer_, render_cam, sun_angle);
            // S401/S402/S403 -- same "background first" placement as the sun.
            world_renderer_->render_moon(*mc3_renderer_, render_cam, moon_angle, sun_angle,
                                          moon_phase_fraction(time_of_day_.day(), hours));
            // S901 -- shared "in-game hours since world start" clock for
            // both cloud drift and tree sway below (S602/S602 already use
            // the same day()*24+hours() combination for other purposes).
            const double total_game_hours = time_of_day_.day() * 24.0 + hours;

            // S701/S702/S703/S904 -- clouds, nearer than the sun/moon/stars
            // but still drawn before the world geometry below (same
            // "background first" order). A no-op on WeatherState::Clear.
            world_renderer_->render_clouds(*mc3_renderer_, render_cam, weather_.state(),
                                            total_game_hours, weather_.wind());

            // S902/S903 -- tree sway (see WorldRenderer.hpp's own doc
            // comment on apply_tree_sway() for the known v1 "every tree
            // sways in lockstep" limitation). Must be called every frame
            // unconditionally, even at zero wind -- see that method's own
            // doc comment on why.
            world_renderer_->apply_tree_sway(*mc3_renderer_, weather_.wind(), total_game_hours);

            world_renderer_->render(*mc3_renderer_, render_cam);
            // MAP11 M175-177 — 3D model placements (trees/props/etc.),
            // streamed independently of chunk geometry above.
            if (model_streamer_)
                world_renderer_->render_placements(
                    *mc3_renderer_, render_cam, camera_.x, camera_.y, camera_.z,
                    model_streamer_->loaded_placements());
            // S1001-S1005 -- snow-cap overlays on roofs/placements. A no-op
            // when snow_.depth() is 0 (bare ground).
            if (model_streamer_)
                world_renderer_->render_snow_accumulation(
                    *mc3_renderer_, render_cam, camera_.x, camera_.y, camera_.z, snow_.depth(),
                    model_streamer_->loaded_placements());
            // S801/S802/S803 -- precipitation, drawn last (nearest the
            // camera of every sky/weather layer, unlike sky/sun/moon/stars/
            // clouds above which are all deliberately "background first").
            // A no-op unless weather_.state() is Rain/Snow.
            world_renderer_->render_particles(*mc3_renderer_, render_cam, particles_.particles(),
                                               weather_.state());
        }

        if (state_ == State::MENU)
            draw_menu();
        else if (state_ == State::EXPLORE)
            draw_hud();
    }

private:
    // -----------------------------------------------------------------------
    enum class State { MENU, EXPLORE };
    State state_{State::MENU};

    // MAP12 map-view layout, shared between draw_map_view() and
    // update_map_view_input() (M197) so mouse hit-testing matches what's
    // actually drawn.
    static constexpr int   kMapViewTilesW = 15;
    static constexpr int   kMapViewTilesH = 15;
    static constexpr float kMapViewCellPx = 24.0f;

    std::unique_ptr<GraphicsDeviceManager> graphics_;
    std::unique_ptr<Mc3Renderer>           mc3_renderer_;

    // --- Explore ---
    std::unique_ptr<WorldMap>           world_map_;
    std::unique_ptr<PersistentWorldMap> persistent_map_;
    std::unique_ptr<WorldRenderer>      world_renderer_;
    // MAP11 M175-177 -- streams 3D model placements (trees/props/etc, the
    // models/<rx>_<rz>.db region shards) around the player independently of
    // world_renderer_'s own 2D chunk streaming; see start_explore() for why
    // it points at the same map_dir as the planetary map layer.
    std::unique_ptr<Model3DStreamer>    model_streamer_;
    WorldConfig   cfg_;
    FPCamera      camera_;
    float         vel_y_{0.f};
    bool          on_ground_{true};
    bool          minimap_visible_{true};
    bool          m_prev_{false};
    // S101 (sky/day-night/weather) -- reset fresh every time a world is
    // (re-)entered (start_explore()), same "explore-session state, not
    // saved world state" precedent camera_/vel_y_/on_ground_ above already
    // establish; advanced once per frame in update_explore().
    TimeOfDay     time_of_day_;

    // S501 -- generated once per start_explore() call (seeded from the
    // world's own name -- see start_explore() for why), then held for the
    // rest of the session and reused every Draw() frame unchanged: the
    // whole point of generate_star_field()'s own determinism is "not a
    // reshuffle every frame".
    std::vector<SkyAngle> star_field_;

    // S601-S604 -- re-seeded fresh every start_explore() call from
    // steady_clock (same "time-based, non-reproducible entropy" pattern
    // PlanetWorld/PersistentWorldMap already use -- see Weather.hpp's own
    // doc comment on why weather doesn't reuse generate_star_field()'s
    // deterministic-seed pattern instead). The placeholder seed here is
    // never actually used to pick real weather -- start_explore() always
    // reassigns this before EXPLORE state is entered.
    Weather weather_{0};

    // S801-S805 -- same "re-seeded fresh every start_explore(), placeholder
    // seed here never actually used" reasoning as weather_ above.
    ParticleSystem particles_{0};
    // S804 -- fixed at Medium for now (the task's own proposed default);
    // no in-app quality setting UI exists yet to change this at runtime.
    static constexpr ParticleQuality kParticleQuality = ParticleQuality::Medium;

    // S1001-S1005 -- reset fresh every start_explore() call, same
    // "explore-session state, not saved world state" precedent
    // time_of_day_/weather_/particles_ already establish. No seed needed
    // (purely deterministic accumulate/melt, no randomness).
    SnowAccumulation snow_;

    // Free-look (2026-07-11, user request): a TEMPORARY yaw/pitch offset
    // applied only when actually rendering (Draw()), never to camera_.yaw/
    // pitch itself -- so glancing around never changes the player's own
    // committed facing direction (movement in update_explore() always uses
    // camera_.yaw, unaffected). Reset to 0 whenever movement/turning input
    // fires ("kdyz hrac zase popojde kamera se vrati do standardniho
    // rezimu" -- when the player moves again, the camera returns to
    // standard mode) or whenever the map view opens (its own click-to-pan,
    // M197, needs a normal absolute-position cursor, mutually exclusive
    // with free-look's relative mouse mode).
    float         look_yaw_offset_{0.0f};
    float         look_pitch_offset_{0.0f};

    static constexpr float kEyeHeightM    = 1.7f;
    static constexpr float kPlayerRadiusM = 0.35f;

    // --- MAP12: zoomable planetary map view ---
    // Own PlanetWorld/MapPipeline, separate from WorldRenderer's internal
    // per-worker-thread ones (WorldStreamer's own doc comment explains why
    // sharing one across threads isn't safe) -- this pair is only ever
    // touched from the main/UI thread, driving MapView's queries.
    std::unique_ptr<PlanetWorld>         map_view_world_;
    std::unique_ptr<Map::MapPipeline>    map_view_pipeline_;
    // M192-193 — background generation for tiles the map view wants to show
    // but aren't persisted yet (own PlanetWorld/MapPipeline internally, see
    // MapTileFetchQueue's own doc comment for why it can't share
    // map_view_world_/map_view_pipeline_ above).
    std::unique_ptr<MapTileFetchQueue>  map_view_fetch_queue_;
    MapView       map_view_;
    bool          map_view_visible_{false};
    bool          n_prev_{false};
    int           map_view_scroll_prev_{0};      // M197 — Mouse::ScrollWheelValue is cumulative
    bool          map_view_left_button_prev_{false};
    bool          map_view_pan_up_prev_{false};
    bool          map_view_pan_down_prev_{false};
    bool          map_view_pan_left_prev_{false};
    bool          map_view_pan_right_prev_{false};

    // --- Menu ---
    std::vector<std::string> world_list_;
    bool  imgui_ready_{false};
    bool  f11_prev_{false};

    // Popup state — captured during PushID loop, popup opened/rendered outside it
    char  rename_buf_[128]{};
    std::string popup_world_name_; // which world the current popup targets
    std::string open_popup_this_frame_; // "##rename_modal" or "##delete_modal" or ""

    // Deferred commands (set in Draw, executed in Update)
    bool        pending_enter_{false};
    bool        pending_create_{false};
    bool        pending_showcase_{false};
    bool        pending_biome_showcase_{false};
    bool        pending_rename_{false};
    bool        pending_delete_{false};
    std::string pending_world_name_;
    std::string pending_old_name_;
    std::string pending_new_name_;

    // -----------------------------------------------------------------------
    // Free-look (2026-07-11) -- SDL_Window* accessor, shared with init_imgui()
    // below.
    // -----------------------------------------------------------------------
    SDL_Window* sdl_window() {
        return reinterpret_cast<SDL_Window*>(
            static_cast<std::uintptr_t>(getWindowProperty().getHandleProperty()));
    }

    // -----------------------------------------------------------------------
    // ImGui
    // -----------------------------------------------------------------------
    void init_imgui() {
        SDL_Window*   win = sdl_window();
        SDL_GLContext ctx = SDL_GL_GetCurrentContext();
        if (!win || !ctx) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(1.4f);
        ImGui::GetIO().FontGlobalScale = 1.3f;

        ImGui_ImplSDL3_InitForOpenGL(win, ctx);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        SDL_AddEventWatch(imgui_event_watch, nullptr);
        imgui_ready_ = true;
    }

    void shutdown_imgui() {
        if (!imgui_ready_) return;
        SDL_RemoveEventWatch(imgui_event_watch, nullptr);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imgui_ready_ = false;
    }

    // -----------------------------------------------------------------------
    // F11 fullscreen (edge-triggered)
    // -----------------------------------------------------------------------
    void handle_f11() {
        bool f11_now = Keyboard::GetState().IsKeyDown(Keys::F11);
        if (f11_now && !f11_prev_)
            graphics_->ToggleFullScreen();
        f11_prev_ = f11_now;
    }

    // -----------------------------------------------------------------------
    // Menu update (deferred commands are handled at top of Update)
    // -----------------------------------------------------------------------
    void update_menu() {
        // Input is handled by ImGui; deferred commands are processed in Update
    }

    // -----------------------------------------------------------------------
    // Menu draw
    // -----------------------------------------------------------------------
    void draw_menu() {
        if (!imgui_ready_) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        const float sw = static_cast<float>(vp.getWidthProperty());
        const float sh = static_cast<float>(vp.getHeightProperty());

        ImGui::SetNextWindowPos(ImVec2(sw * 0.5f, sh * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Always);
        ImGui::Begin("##mw_menu", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar);

        ImGui::SetWindowFontScale(1.7f);
        ImGui::TextUnformatted("MeshWorld");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        if (world_list_.empty())
            ImGui::TextDisabled("No worlds yet. Create one below.");

        // Capture click events during the loop; open popup AFTER the loop
        // so that OpenPopup and BeginPopupModal share the same ID stack context.
        open_popup_this_frame_.clear();

        for (const auto& name : world_list_) {
            ImGui::PushID(name.c_str());

            if (ImGui::Button(("  " + name + "  ").c_str()))
                queue_enter(name);

            ImGui::SameLine();
            if (ImGui::SmallButton("Rename")) {
                popup_world_name_ = name;
                std::strncpy(rename_buf_, name.c_str(), sizeof(rename_buf_) - 1);
                rename_buf_[sizeof(rename_buf_) - 1] = '\0';
                open_popup_this_frame_ = "##rename_modal";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                popup_world_name_ = name;
                open_popup_this_frame_ = "##delete_modal";
            }

            ImGui::PopID();
        }

        // Open popup OUTSIDE PushID scope so IDs are stable and consistent.
        if (!open_popup_this_frame_.empty())
            ImGui::OpenPopup(open_popup_this_frame_.c_str());

        // --- Rename modal ---
        if (ImGui::BeginPopupModal("##rename_modal", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Rename \"%s\" to:", popup_world_name_.c_str());
            ImGui::SetNextItemWidth(280);
            ImGui::InputText("##rn", rename_buf_, sizeof(rename_buf_));
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(110, 0))) {
                queue_rename(popup_world_name_, rename_buf_);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // --- Delete modal ---
        if (ImGui::BeginPopupModal("##delete_modal", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete world \"%s\"?", popup_world_name_.c_str());
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "This cannot be undone.");
            ImGui::Spacing();
            if (ImGui::Button("Delete", ImVec2(110, 0))) {
                queue_delete(popup_world_name_);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("+ Create World"))
            queue_create();
        ImGui::SameLine();
        if (ImGui::Button("Explore City Showcase"))
            queue_city_showcase();
        ImGui::SameLine();
        if (ImGui::Button("Explore Biome Showcase"))
            queue_biome_showcase();
        ImGui::SameLine();
        if (ImGui::Button("Quit"))
            Exit();

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // -----------------------------------------------------------------------
    // Command queuing (set in Draw, executed next Update)
    // -----------------------------------------------------------------------
    void queue_enter(const std::string& name) {
        pending_enter_      = true;
        pending_world_name_ = name;
    }
    void queue_create() {
        pending_create_ = true;
    }
    void queue_city_showcase() {
        pending_showcase_ = true;
    }
    void queue_biome_showcase() {
        pending_biome_showcase_ = true;
    }
    void queue_rename(const std::string& old_name, const std::string& new_name) {
        pending_rename_   = true;
        pending_old_name_ = old_name;
        pending_new_name_ = new_name;
    }
    void queue_delete(const std::string& name) {
        pending_delete_     = true;
        pending_world_name_ = name;
    }

    // -----------------------------------------------------------------------
    // World management
    // -----------------------------------------------------------------------
    void do_rename(const std::string& old_name, const std::string& new_name_raw) {
        std::string new_name = new_name_raw;
        while (!new_name.empty() && new_name.front() == ' ') new_name.erase(new_name.begin());
        while (!new_name.empty() && new_name.back()  == ' ') new_name.pop_back();
        if (new_name.empty() || new_name == old_name) return;
        std::error_code ec;
        fs::rename("saves/" + old_name, "saves/" + new_name, ec);
        world_list_ = scan_worlds();
    }

    void do_delete(const std::string& name) {
        std::error_code ec;
        fs::remove_all("saves/" + name, ec);
        world_list_ = scan_worlds();
    }

    // -----------------------------------------------------------------------
    // Explore: start
    // -----------------------------------------------------------------------
    void start_explore(const std::string& world_name) {
        stop_explore_internal();

        cfg_ = WorldConfig{};
        cfg_.style        = "central_europe_small_city";
        cfg_.grid_w       = 200;
        cfg_.grid_h       = 200;
        cfg_.chunk_size_m = 64;
        cfg_.procedural   = false;
        // Keep WorldConfig's library-wide default conservative for existing
        // tools and callers, but make the interactive app actually present
        // the registered reusable MC3 city assets in its ordinary worlds.
        // Regions without composer coverage still use their established
        // Lua/C++ fallback path.
        cfg_.use_world_composer = true;

        world_map_ = std::make_unique<WorldMap>(cfg_);

        const std::string world_dir = "saves/" + world_name;
        const std::string chunk_cache_dir = world_dir + "/chunks/" + kChunkCacheGeneration;
        fs::create_directories(chunk_cache_dir);

        persistent_map_ = std::make_unique<PersistentWorldMap>(
            world_dir, *world_map_, /*level=*/0);

        // Player spawns at center of the 200×200 grid
        int cx = cfg_.grid_w / 2;   // 100
        int cy = cfg_.grid_h / 2;   // 100
        camera_    = FPCamera{};

        // Pre-populate 15-chunk radius around spawn before streamer starts.
        persistent_map_->ensure_region(cx, cy, 15);

        // A new persistent world may place the geometric map centre in any
        // biome. Start near a real composer-ready city parcel when one was
        // generated in the preloaded neighbourhood so ordinary app entry
        // immediately demonstrates the reusable MC3 assets instead of a
        // random ocean or empty field. Keep the geometric centre as a safe
        // fallback for sparse worlds with no such parcel nearby.
        const int spawn_cx = cx;
        const int spawn_cy = cy;
        int best_distance_sq = 16 * 16;
        for (int y = spawn_cy - 15; y <= spawn_cy + 15; ++y) {
            for (int x = spawn_cx - 15; x <= spawn_cx + 15; ++x) {
                const int dx = x - spawn_cx;
                const int dy = y - spawn_cy;
                const int distance_sq = dx * dx + dy * dy;
                if (distance_sq > 15 * 15 || distance_sq >= best_distance_sq) continue;

                const ChunkInfo info = world_map_->info(x, y);
                const bool has_street = info.exits.north_road || info.exits.south_road ||
                                        info.exits.east_road || info.exits.west_road;
                const bool composer_ready = info.region == RegionType::square ||
                    ((info.region == RegionType::small_house_block ||
                      info.region == RegionType::apartment_block ||
                      info.region == RegionType::shop_street) && has_street);
                if (!composer_ready) continue;

                cx = x;
                cy = y;
                best_distance_sq = distance_sq;
            }
        }

        camera_.x  = static_cast<float>(cx * cfg_.chunk_size_m + 32);
        camera_.y  = 1.7f;
        camera_.z  = static_cast<float>(cy * cfg_.chunk_size_m + 32);

        // Attach the planetary map layer (Map::MapPipeline) so
        // ChunkContext.map_context is populated for generated chunks, same
        // hand-off MeshWorldPlanet's CLI already exercises. The world.json
        // must be created here, once, on this (single) thread before
        // WorldRenderer spawns WorldStreamer's worker threads — each of
        // those only ever open_existing()s this same directory (see
        // WorldStreamer's own constructor doc comment); two threads racing
        // to create_new() on first launch would corrupt which entropy the
        // world actually settles on. Nothing to do if it already exists.
        const std::string map_dir = world_dir + "/map";
        if (!is_existing_planet_world(map_dir)) PlanetWorld::create_new(map_dir, cfg_);

        world_renderer_ = std::make_unique<WorldRenderer>(
            cfg_, *world_map_, chunk_cache_dir, /*load_radius=*/2,
            map_dir, planet_params_from_config(cfg_), kInteractiveChunkCacheMaxEntries);

        // MAP11 M175-177 — 3D model placement streaming. Points at the same
        // map_dir as the planetary map layer above: map.md §10.1's own
        // storage layout puts map_level{z}.db and models/<rx>_<rz>.db as
        // siblings under one world directory, not two separate ones.
        // horizontal_radius_chunks matches world_renderer_'s own load_radius
        // (2 chunks); vertical_radius_m is a generous but bounded window
        // (a handful of MAP11's own 64 m altitude bands, Map::ALT_BAND_M)
        // since this demo's own player movement stays close to the ground.
        model_streamer_ = std::make_unique<Model3DStreamer>(
            map_dir, /*horizontal_radius_chunks=*/2, /*vertical_radius_m=*/200.0,
            /*thread_count=*/2, cfg_.chunk_size_m);

        // MAP12 — a second, independent PlanetWorld/MapPipeline for the
        // zoomable map view's own queries (main/UI thread only; never shared
        // with WorldStreamer's per-worker-thread ones, same reasoning as
        // WorldRenderer's own construction above). Opens centered on the
        // player's own spawn tile at level 12 ("City" scale, map.md's own
        // level table) rather than always starting at the planet root and
        // requiring 12 zoom-ins by hand.
        map_view_world_    = std::make_unique<PlanetWorld>(PlanetWorld::open_existing(map_dir));
        map_view_pipeline_ = std::make_unique<Map::MapPipeline>(
            *map_view_world_, planet_params_from_config(cfg_));
        map_view_fetch_queue_ =
            std::make_unique<MapTileFetchQueue>(map_dir, planet_params_from_config(cfg_));
        map_view_ = MapView{};
        map_view_.set_view(Map::TileCoord::from_world(camera_.x, camera_.z, /*level=*/12));

        vel_y_             = 0.f;
        on_ground_         = true;
        time_of_day_       = TimeOfDay{};  // S101 -- fresh clock each time a world is (re-)entered
        // S501 -- seeded from the world's own name (always available here,
        // unlike PlanetWorld's internal entropy, which isn't exposed to this
        // app) so the same world reliably shows the same stars session to
        // session; std::hash isn't guaranteed stable across process runs by
        // the standard, but glibc's implementation is deterministic in
        // practice, and stars are cosmetic -- not worth plumbing a real
        // entropy accessor through PlanetWorld just for this.
        star_field_ = generate_star_field(std::hash<std::string>{}(world_name), /*count=*/800);
        // S601-S604 -- fresh, non-reproducible weather each time a world is
        // (re-)entered (see weather_'s own doc comment on the member field).
        weather_ = Weather(
            static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
            TimeOfDay::kDefaultDayLengthRealMinutes);
        // S801-S805 -- fresh particle pool each time a world is
        // (re-)entered, same non-reproducible-seed reasoning as weather_
        // above.
        particles_ = ParticleSystem(
            static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        // S1001-S1005 -- fresh (bare-ground) snow state each time a world
        // is (re-)entered.
        snow_ = SnowAccumulation{};
        look_yaw_offset_   = 0.0f;
        look_pitch_offset_ = 0.0f;
        state_             = State::EXPLORE;
        setIsMouseVisibleProperty(false);
        // Free-look -- relative mouse mode captures the cursor and reports
        // motion deltas instead of absolute position, exactly what
        // look-around needs. Map view's own click-to-pan (M197) needs the
        // opposite (a normal, visible, absolute-position cursor) -- see the
        // N-key handling in update_explore() for why this gets toggled off
        // again whenever the map view opens.
        if (SDL_Window* win = sdl_window()) SDL_SetWindowRelativeMouseMode(win, true);
    }

    // The deterministic showcase intentionally keeps the planetary map
    // available for the app's map UI, but isolates its 3D content from the
    // curated chunk scene. PersistentWorldMap mutates WorldMap in place, and
    // MapPipeline/Model3DStreamer supply planet-derived zones and placements;
    // all three are right for ordinary worlds but would overwrite or obscure
    // this exact house/apartment/shop/square layout.
    void start_city_showcase() {
        start_authored_showcase(kCityShowcaseConfigPath, kCityShowcaseSaveName,
                                kCityShowcaseChunkCacheDir);
    }

    void start_biome_showcase() {
        start_authored_showcase(kBiomeShowcaseConfigPath, kBiomeShowcaseSaveName,
                                kBiomeShowcaseChunkCacheDir);
    }

    void start_authored_showcase(const char* config_path, const char* save_name,
                                 const char* chunk_cache_dir) {
        WorldConfig showcase;
        if (!showcase.load_from_file(config_path) || !showcase.is_consistent()) {
            std::fprintf(stderr, "MeshWorldApp: cannot load %s\n", config_path);
            return;
        }

        stop_explore_internal();
        cfg_ = std::move(showcase);
        world_map_ = std::make_unique<WorldMap>(cfg_);

        const std::string world_dir = std::string("saves/") + save_name;
        fs::create_directories(chunk_cache_dir);
        // stop_explore_internal() normally cleared this already. Keep the
        // invariant explicit: update_explore() must never let the legacy
        // persistent procedural map overwrite the authored WorldConfig.
        persistent_map_.reset();

        const int cx = cfg_.grid_w / 2;
        const int cy = cfg_.grid_h / 2;
        camera_ = FPCamera{};
        camera_.x = static_cast<float>(cx * cfg_.chunk_size_m + cfg_.chunk_size_m / 2);
        camera_.y = 1.7f;
        camera_.z = static_cast<float>(cy * cfg_.chunk_size_m + cfg_.chunk_size_m / 2);

        const std::string map_dir = world_dir + "/map";
        if (!is_existing_planet_world(map_dir)) PlanetWorld::create_new(map_dir, cfg_);

        world_renderer_ = std::make_unique<WorldRenderer>(
            cfg_, *world_map_, chunk_cache_dir, /*load_radius=*/2,
            "", MeshWorld::Map::PlanetParams{}, kInteractiveChunkCacheMaxEntries);

        // The full-screen map remains available below, but its independent
        // planet-scale 3D placements intentionally stay disabled here.
        model_streamer_.reset();

        map_view_world_ = std::make_unique<PlanetWorld>(PlanetWorld::open_existing(map_dir));
        map_view_pipeline_ = std::make_unique<Map::MapPipeline>(
            *map_view_world_, planet_params_from_config(cfg_));
        map_view_fetch_queue_ = std::make_unique<MapTileFetchQueue>(
            map_dir, planet_params_from_config(cfg_));
        map_view_ = MapView{};
        map_view_.set_view(Map::TileCoord::from_world(camera_.x, camera_.z, /*level=*/12));

        vel_y_ = 0.f;
        on_ground_ = true;
        time_of_day_ = TimeOfDay{};
        star_field_ = generate_star_field(std::hash<std::string>{}(save_name), /*count=*/800);
        weather_ = Weather(static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()),
            TimeOfDay::kDefaultDayLengthRealMinutes);
        particles_ = ParticleSystem(static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        snow_ = SnowAccumulation{};
        look_yaw_offset_ = 0.0f;
        look_pitch_offset_ = 0.0f;
        state_ = State::EXPLORE;
        setIsMouseVisibleProperty(false);
        if (SDL_Window* win = sdl_window()) SDL_SetWindowRelativeMouseMode(win, true);
    }

    // -----------------------------------------------------------------------
    // Explore: stop (called from destructor and when returning to menu)
    // -----------------------------------------------------------------------
    void stop_explore_internal() {
        if (world_renderer_) {
            world_renderer_->clear_doc_cache();
            world_renderer_.reset();
        }
        model_streamer_.reset();        // joins its background threads
        map_view_fetch_queue_.reset();  // joins its background thread
        map_view_pipeline_.reset();
        map_view_world_.reset();
        persistent_map_.reset();
        world_map_.reset();
    }

    void stop_explore() {
        stop_explore_internal();
        state_ = State::MENU;
        setIsMouseVisibleProperty(true);
        // Free-look -- release the cursor back to normal so the menu's own
        // ImGui buttons are clickable.
        if (SDL_Window* win = sdl_window()) SDL_SetWindowRelativeMouseMode(win, false);
        world_list_ = scan_worlds();
    }

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------
    // R133 -- moves the player through the core-tested capsule/AABB slide
    // resolver. Obstacles include both legacy inline structural boxes and
    // resolved MC3 instances whose authored metadata opts into a box proxy.
    // Falls back to unconditional movement if no rendered chunk is nearby.
    // -----------------------------------------------------------------------
    void try_move(float dx, float dz) {
        if (dx == 0.0f && dz == 0.0f) return;
        if (!world_renderer_) { camera_.x += dx; camera_.z += dz; return; }

        const float query_radius = cfg_.chunk_size_m * 1.5f;
        const auto  obstacles    = world_renderer_->nearby_collision_boxes(camera_.x, camera_.z, query_radius);
        if (obstacles.empty()) { camera_.x += dx; camera_.z += dz; return; }

        const PlayerMoveResult moved = resolve_player_capsule_slide(
            camera_.x, camera_.z, dx, dz, camera_.y - kEyeHeightM, camera_.y + 0.3f,
            kPlayerRadiusM, obstacles);
        camera_.x = moved.x;
        camera_.z = moved.z;
    }

    // -----------------------------------------------------------------------
    // S603 -- the map layer's own temperature field, sampled at the
    // player's current position, for Weather::advance()'s snow/rain gate.
    // Same tile-lookup + fractional-grid-sample technique
    // ChunkPipeline.cpp's populate_map_context() already uses for
    // elevation_m/biome_ordinal (MAX_LEVEL tile, normalize world position
    // into the tile's [0,1) extent, scale to the field's own resolution).
    // 0.0 (freezing) if the map layer isn't ready yet or the field is
    // empty -- errs toward snow rather than crashing/asserting, since this
    // is only ever used for a cosmetic weather roll, not gameplay-critical.
    // -----------------------------------------------------------------------
    double local_temperature_c() const {
        if (!map_view_pipeline_) return 0.0;

        const double world_x = static_cast<double>(camera_.x);
        const double world_z = static_cast<double>(camera_.z);
        const Map::TileCoord tile = Map::TileCoord::from_world(world_x, world_z, Map::MAX_LEVEL);
        const Map::MapTilePayload payload = map_view_pipeline_->get(tile);
        if (payload.temperature.empty()) return 0.0;

        const Map::WorldBounds bounds = tile.world_bounds();
        const double u = (world_x - bounds.min_x) / (bounds.max_x - bounds.min_x);
        const double v = (world_z - bounds.min_z) / (bounds.max_z - bounds.min_z);
        const int gx = std::clamp(static_cast<int>(u * payload.temperature.w), 0, payload.temperature.w - 1);
        const int gy = std::clamp(static_cast<int>(v * payload.temperature.h), 0, payload.temperature.h - 1);
        return static_cast<double>(payload.temperature.at(gx, gy));
    }

    // -----------------------------------------------------------------------
    // MAP12 (M197): mouse wheel zooms the map view, left-click recenters it
    // on the clicked tile, arrow keys pan it (edge-triggered — one tile per
    // press, not continuous like player movement uses the same keys for).
    // Only called while the map view is open (update_explore() gates it).
    // -----------------------------------------------------------------------
    void update_map_view_input() {
        auto kb = Keyboard::GetState();
        auto ms = Mouse::GetState();

        const int scroll_now   = ms.getScrollWheelValueProperty();
        const int scroll_delta = scroll_now - map_view_scroll_prev_;
        map_view_scroll_prev_  = scroll_now;
        if (scroll_delta > 0) map_view_.zoom_in();
        else if (scroll_delta < 0) map_view_.zoom_out();

        const bool left_now = ms.getLeftButtonProperty() == ButtonState::Pressed;
        if (left_now && !map_view_left_button_prev_) {
            auto&       vp      = getGraphicsDeviceProperty().getViewportProperty();
            const float sw      = static_cast<float>(vp.getWidthProperty());
            const float sh      = static_cast<float>(vp.getHeightProperty());
            const float panel_w = kMapViewCellPx * kMapViewTilesW;
            const float panel_h = kMapViewCellPx * kMapViewTilesH;
            const float ox      = (sw - panel_w) * 0.5f;
            const float oy      = (sh - panel_h) * 0.5f;

            const float mx = static_cast<float>(ms.getXProperty());
            const float my = static_cast<float>(ms.getYProperty());
            if (mx >= ox && mx < ox + panel_w && my >= oy && my < oy + panel_h) {
                const int col = static_cast<int>((mx - ox) / kMapViewCellPx);
                const int row = static_cast<int>((my - oy) / kMapViewCellPx);
                map_view_.pan(col - kMapViewTilesW / 2, row - kMapViewTilesH / 2);
            }
        }
        map_view_left_button_prev_ = left_now;

        const auto edge = [](bool now, bool& prev) {
            const bool fired = now && !prev;
            prev = now;
            return fired;
        };
        if (edge(kb.IsKeyDown(Keys::Up),    map_view_pan_up_prev_))    map_view_.pan(0, -1);
        if (edge(kb.IsKeyDown(Keys::Down),  map_view_pan_down_prev_))  map_view_.pan(0, 1);
        if (edge(kb.IsKeyDown(Keys::Left),  map_view_pan_left_prev_))  map_view_.pan(-1, 0);
        if (edge(kb.IsKeyDown(Keys::Right), map_view_pan_right_prev_)) map_view_.pan(1, 0);
    }

    // -----------------------------------------------------------------------
    // Explore: update
    // -----------------------------------------------------------------------
    void update_explore(float dt) {
        auto kb = Keyboard::GetState();

        if (kb.IsKeyDown(Keys::Escape)) {
            stop_explore();
            return;
        }

        // S101 -- advance the day/night clock every explore-mode frame,
        // before anything below reads it (sky/sun/moon rendering, once
        // built, will do the same).
        time_of_day_.advance(static_cast<double>(dt));
        // S601-S604 -- advance the weather state machine too, sampling the
        // player's CURRENT local temperature each frame (not just at
        // world-load time) so a transition rolled after moving to a colder/
        // warmer biome uses that biome's own temperature.
        const double local_temp_c = local_temperature_c();
        weather_.advance(static_cast<double>(dt), local_temp_c);
        // S801-S805/S904 -- advance the precipitation particle pool with the
        // player's TRUE world position (camera_.x/y/z, not render_cam's
        // look-around-offset copy built later in Draw()) so particles stay
        // anchored in world space regardless of free-look. weather_.wind()
        // drives horizontal drift (S904).
        particles_.advance(dt, weather_.state(), kParticleQuality, weather_.wind(), camera_.x, camera_.y,
                            camera_.z);
        // S1001-S1005 -- advance snow accumulation with the SAME local
        // temperature just sampled for weather_ above.
        snow_.advance(static_cast<double>(dt), weather_.state(), local_temp_c);

        const float sprint     = kb.IsKeyDown(Keys::LeftShift) ? 4.0f : 1.0f;
        const float move_speed = 8.0f * dt * sprint;
        const float turn_speed = 1.8f * dt;
        const float gravity    = -20.0f;
        const float jump_vel   =  7.0f;

        const float fwd_x =  std::sin(camera_.yaw);
        const float fwd_z = -std::cos(camera_.yaw);

        // Free-look (2026-07-11) -- always drain SDL's relative-motion
        // accumulator, even while the map view is open (relative mouse mode
        // is toggled off then, see the N-key handling below, but draining
        // here too means no stale delta is left waiting to cause a jarring
        // snap the next time free-look resumes).
        float mouse_dx = 0.0f, mouse_dy = 0.0f;
        SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);

        // M197 — while the map view is open, arrow keys pan it (discrete,
        // one tile per press) instead of moving the player — avoids a
        // control conflict over the same 4 keys.
        if (!map_view_visible_) {
            constexpr float kLookSensitivity = 0.0025f;  // radians per pixel of mouse motion
            constexpr float kMaxPitchRad     = 1.5f;     // ~86 degrees -- avoids a full vertical flip
            look_yaw_offset_ += mouse_dx * kLookSensitivity;
            // Real bug found via user report (2026-07-11): SDL reports
            // mouse-down as a POSITIVE y delta, but Mc3Renderer.cpp's own
            // view-direction math uses sin(cam.pitch) for the Y (up)
            // component -- positive pitch means "looking up". Adding
            // mouse_dy directly meant moving the mouse down made the camera
            // look UP, inverted from the standard (non-inverted) convention
            // every other game uses. Subtracting instead fixes it: mouse
            // down -> pitch decreases -> looking down.
            look_pitch_offset_ = std::clamp(look_pitch_offset_ - mouse_dy * kLookSensitivity,
                                             -kMaxPitchRad, kMaxPitchRad);

            // Moving or turning cancels the glance and returns the camera
            // to the player's own standard/committed facing direction (user
            // request: "kdyz hrac zase popojde kamera se vrati do
            // standardniho rezimu").
            const bool moving_or_turning =
                kb.IsKeyDown(Keys::Left) || kb.IsKeyDown(Keys::Right) ||
                kb.IsKeyDown(Keys::Up)   || kb.IsKeyDown(Keys::Down);
            if (moving_or_turning) {
                look_yaw_offset_   = 0.0f;
                look_pitch_offset_ = 0.0f;
            }

            if (kb.IsKeyDown(Keys::Left))  camera_.yaw -= turn_speed;
            if (kb.IsKeyDown(Keys::Right)) camera_.yaw += turn_speed;

            // M-fix (player physics): movement now goes through try_move()
            // (collision-tested against nearby buildings/walls) instead of
            // writing camera_.x/z directly.
            float move_dx = 0.0f, move_dz = 0.0f;
            if (kb.IsKeyDown(Keys::Up))   { move_dx += fwd_x * move_speed; move_dz += fwd_z * move_speed; }
            if (kb.IsKeyDown(Keys::Down)) { move_dx -= fwd_x * move_speed; move_dz -= fwd_z * move_speed; }
            try_move(move_dx, move_dz);
        }

        bool m_now = kb.IsKeyDown(Keys::M);
        if (m_now && !m_prev_) minimap_visible_ = !minimap_visible_;
        m_prev_ = m_now;

        bool n_now = kb.IsKeyDown(Keys::N);
        if (n_now && !n_prev_) {
            map_view_visible_ = !map_view_visible_;
            // Free-look's relative mouse mode and the map view's own
            // click-to-pan (M197, needs a normal absolute-position cursor)
            // are mutually exclusive -- toggle together.
            if (SDL_Window* win = sdl_window())
                SDL_SetWindowRelativeMouseMode(win, !map_view_visible_);
            if (map_view_visible_) {
                look_yaw_offset_   = 0.0f;
                look_pitch_offset_ = 0.0f;
            }
        }
        n_prev_ = n_now;

        if (map_view_visible_) update_map_view_input();

        // M-fix, REVERTED (2026-07-10): a previous version of this code set
        // camera_.y from ground_elevation_at()'s ABSOLUTE map-layer
        // elevation (hundreds of meters, sea-level-relative). That's wrong
        // for this renderer: WorldRenderer::render() only ever offsets a
        // loaded chunk's local_cam by its (x, z) chunk-grid position (see
        // that function's own local_cam.x -= / .z -= lines) and NEVER
        // touches Y -- every chunk's own geometry (ground plane at y=0,
        // walls/boxes anchored at their own small local y) is built and
        // rendered in a purely CHUNK-LOCAL vertical space, independent of
        // what real-world elevation that region represents (MountainGenerator
        // etc. use elevation to scale FEATURE height -- e.g. a taller cliff
        // box -- never to offset the whole chunk's own origin). Setting
        // camera_.y to the real elevation (often hundreds of meters) put the
        // camera far above/below where the actual rendered geometry lives,
        // which read as "just a blue screen, no 3D world" once the map
        // layer's tile finished generating and a nonzero elevation kicked
        // in. Back to the flat, chunk-local eye height until a real
        // chunk-local height-field (not the abstract map elevation value)
        // is available to sample -- see NEXT.md's own bug entry for this.
        constexpr float eye_height = kEyeHeightM;

        if (kb.IsKeyDown(Keys::LeftControl) && on_ground_) {
            vel_y_     = jump_vel;
            on_ground_ = false;
        }
        if (!on_ground_) {
            vel_y_    += gravity * dt;
            camera_.y += vel_y_ * dt;
        }
        if (camera_.y <= eye_height) {
            camera_.y  = eye_height;
            vel_y_     = 0.f;
            on_ground_ = true;
        }

        auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        if (vp.getHeightProperty() > 0)
            camera_.aspect = static_cast<float>(vp.getWidthProperty()) /
                             static_cast<float>(vp.getHeightProperty());

        // Ordinary worlds use PersistentWorldMap to populate their WorldMap
        // ahead of the player. Curated worlds intentionally have no such
        // mutable map, but both kinds must advance WorldStreamer every frame.
        if (world_renderer_) {
            auto cc = ChunkCoord::from_world(camera_.x, camera_.z, cfg_.chunk_size_m);
            if (persistent_map_)
                persistent_map_->ensure_region(cc.x, cc.y, 10);
            world_renderer_->update(camera_.x, camera_.z);
        }
        // MAP11 M175-177 — non-blocking; the actual region-shard query runs
        // on model_streamer_'s own background thread(s), same shape as
        // world_renderer_'s own update() above. camera_.x/y/z are this
        // demo's only notion of world position (not a separate
        // planet-scale double track) — consistent with how the rest of
        // this app already treats them, see WorldRenderer.hpp's own doc
        // comment on why that's a real precision limit at true planet
        // scale, just not one this specific demo world exercises.
        if (model_streamer_) model_streamer_->update(camera_.x, camera_.y, camera_.z);
    }

    // S601 -- display name for the HUD readout below; no rendering meaning
    // yet (S7xx/S8xx, clouds/precipitation, are separate later tasks).
    static const char* weather_state_name(WeatherState s) {
        switch (s) {
            case WeatherState::Clear: return "clear";
            case WeatherState::PartlyCloudy: return "partly cloudy";
            case WeatherState::Overcast: return "overcast";
            case WeatherState::Rain: return "rain";
            case WeatherState::Snow: return "snow";
        }
        return "?";
    }

    // -----------------------------------------------------------------------
    // HUD (explore mode): minimap overlay via ImGui foreground draw list
    // -----------------------------------------------------------------------
    void draw_hud() {
        if (!imgui_ready_) return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // S101/S103/S104 -- day/night clock + sun/moon position readout. A
        // minimal debug HUD block so the clock and celestial math are both
        // visually verifiable even before S2xx/S3xx/S4xx's real
        // sky/sun/moon rendering exist; not meant to be the final in-game
        // clock UI.
        {
            const double hours  = time_of_day_.hours();
            const int    hour   = static_cast<int>(hours);
            const int    minute = static_cast<int>((hours - hour) * 60.0);
            const SkyAngle sun  = sun_position(hours);
            const SkyAngle moon = moon_position(hours);

            char buf[160];
            std::snprintf(buf, sizeof(buf), "Day %d, %02d:%02d", time_of_day_.day(), hour, minute);
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            fg->AddText({10.0f, 10.0f}, IM_COL32(255, 255, 255, 220), buf);

            std::snprintf(buf, sizeof(buf), "Sun  el=%+.0f az=%.0f%s", sun.elevation_deg, sun.azimuth_deg,
                          sun.elevation_deg > 0.0 ? " (up)" : " (down)");
            fg->AddText({10.0f, 28.0f}, IM_COL32(255, 220, 140, 200), buf);

            // S402/S403 -- phase readout. compute_moon_render_state() is
            // pure logic (no renderer needed), so it's fine to call here
            // just for its illuminated_fraction, same as render_moon()
            // itself will separately do in Draw() for the real geometry.
            const double phase = moon_phase_fraction(time_of_day_.day(), hours);
            const float illum  = compute_moon_render_state(moon, sun, phase).illuminated_fraction;
            std::snprintf(buf, sizeof(buf), "Moon el=%+.0f az=%.0f%s phase=%.0f%%", moon.elevation_deg,
                          moon.azimuth_deg, moon.elevation_deg > 0.0 ? " (up)" : " (down)", illum * 100.0f);
            fg->AddText({10.0f, 46.0f}, IM_COL32(190, 200, 255, 200), buf);

            // S502/S503 -- visible star count readout.
            const int visible_stars = visible_star_count(static_cast<int>(star_field_.size()), sun);
            std::snprintf(buf, sizeof(buf), "Stars %d/%d visible", visible_stars,
                          static_cast<int>(star_field_.size()));
            fg->AddText({10.0f, 64.0f}, IM_COL32(220, 220, 255, 200), buf);

            // S601-S604 -- weather readout. During a crossfade, shows both
            // the state being faded from and the target, with progress;
            // once settled, just the current state.
            if (weather_.transition_progress() >= 1.0f) {
                std::snprintf(buf, sizeof(buf), "Weather %s", weather_state_name(weather_.state()));
            } else {
                std::snprintf(buf, sizeof(buf), "Weather %s -> %s (%.0f%%)",
                              weather_state_name(weather_.previous_state()),
                              weather_state_name(weather_.state()),
                              weather_.transition_progress() * 100.0f);
            }
            fg->AddText({10.0f, 82.0f}, IM_COL32(200, 220, 210, 200), buf);

            // S702 -- cloud puff count readout.
            std::snprintf(buf, sizeof(buf), "Clouds %d puffs", cloud_count_for_weather(weather_.state()));
            fg->AddText({10.0f, 100.0f}, IM_COL32(210, 210, 215, 200), buf);

            // S801-S805 -- active particle count readout.
            std::snprintf(buf, sizeof(buf), "Particles %d active",
                          static_cast<int>(particles_.particles().size()));
            fg->AddText({10.0f, 118.0f}, IM_COL32(200, 215, 225, 200), buf);

            // S901 -- wind readout.
            const WindState wind = weather_.wind();
            std::snprintf(buf, sizeof(buf), "Wind dir=%.0f strength=%.0f%%", wind.direction_deg,
                          wind.strength * 100.0f);
            fg->AddText({10.0f, 136.0f}, IM_COL32(215, 225, 210, 200), buf);

            // S1001-S1005 -- snow accumulation readout.
            std::snprintf(buf, sizeof(buf), "Snow %.0f%% accumulated", snow_.depth() * 100.0f);
            fg->AddText({10.0f, 154.0f}, IM_COL32(230, 230, 235, 200), buf);
        }

        if (minimap_visible_ && world_map_) {
            auto& vp = getGraphicsDeviceProperty().getViewportProperty();
            const float sw = static_cast<float>(vp.getWidthProperty());

            constexpr float cell   = 1.5f;
            const int       gw     = cfg_.grid_w;
            const int       gh     = cfg_.grid_h;
            constexpr float margin = 10.0f;
            const float     ox     = sw - cell * gw - margin;
            const float     oy     = margin;

            ImDrawList* dl = ImGui::GetForegroundDrawList();

            // Run-length encode each row to stay well under ImGui's 16-bit
            // vertex index limit (65 536). 200×200 individual rects = 160k
            // vertices; merged horizontal runs of same-colored cells reduces
            // this to ~500–2000 rects for a typical zone map.
            for (int y = 0; y < gh; ++y) {
                int   run_x  = 0;
                auto  run_c  = WorldMap::zone_color(world_map_->zone(0, y));
                for (int x = 1; x <= gw; ++x) {
                    auto cur_c = (x < gw)
                        ? WorldMap::zone_color(world_map_->zone(x, y))
                        : std::array<float,3>{-1.f, -1.f, -1.f};
                    if (cur_c != run_c) {
                        ImU32 col = IM_COL32(
                            static_cast<int>(run_c[0] * 255),
                            static_cast<int>(run_c[1] * 255),
                            static_cast<int>(run_c[2] * 255),
                            200);
                        dl->AddRectFilled(
                            {ox + run_x * cell, oy + y * cell},
                            {ox + x      * cell, oy + (y + 1) * cell},
                            col);
                        run_x = x;
                        run_c = cur_c;
                    }
                }
            }

            // Player marker: white dot
            auto cc = ChunkCoord::from_world(camera_.x, camera_.z, cfg_.chunk_size_m);
            dl->AddCircleFilled(
                {ox + (cc.x + 0.5f) * cell, oy + (cc.y + 0.5f) * cell},
                3.0f, IM_COL32(255, 255, 255, 255));

            // Thin border around the minimap
            dl->AddRect({ox - 1.f, oy - 1.f},
                        {ox + cell * gw + 1.f, oy + cell * gh + 1.f},
                        IM_COL32(120, 120, 120, 200));
        }

        draw_map_view();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // -----------------------------------------------------------------------
    // MAP12 (M190-196): renders MapView::visible_tiles() as colored cells
    // (biome color, sampled from each tile's own BiomeGrid center — one color
    // per whole tile, not per cell, matching a zoomed-out map's resolution),
    // plus feature overlays (rivers/mountain ranges/borders/roads/streets,
    // M194), zoom-filtered labels (M195/M202 — MapView::should_show_label()),
    // and a "you are here" player marker (M196). A not-yet-persisted tile
    // triggers background generation via map_view_fetch_queue_ and shows a
    // dim "generating…" placeholder until it's ready, rather than staying
    // blank forever. Toggled by N (update_explore()'s n_prev_ edge
    // detection), independent of the legacy minimap's M toggle. No
    // pan/zoom controls yet (M197, not built) — the view only ever shows
    // where it opened, centered on the player's spawn tile.
    // -----------------------------------------------------------------------
    void draw_map_view() {
        if (!map_view_visible_ || !map_view_pipeline_) return;

        auto& vp = getGraphicsDeviceProperty().getViewportProperty();
        const float sw = static_cast<float>(vp.getWidthProperty());
        const float sh = static_cast<float>(vp.getHeightProperty());

        constexpr int   kTilesW = kMapViewTilesW;
        constexpr int   kTilesH = kMapViewTilesH;
        constexpr float cell    = kMapViewCellPx;  // pixels per tile
        const float     panel_w = cell * kTilesW;
        const float     panel_h = cell * kTilesH;
        const float     ox      = (sw - panel_w) * 0.5f;
        const float     oy      = (sh - panel_h) * 0.5f;

        ImDrawList* dl    = ImGui::GetForegroundDrawList();
        const int   level = map_view_.level();

        // Per-tile screen rect + loaded payload, gathered in the base-color
        // pass below and reused for the feature-overlay/label pass (M194-196)
        // afterward, so each tile is loaded from disk only once per frame.
        struct TileDraw {
            Map::TileCoord      tile;
            float               sx0, sy0, sx1, sy1;
            Map::MapTilePayload payload;
        };
        std::vector<TileDraw> loaded;

        // Up to kTilesW*kTilesH = 225 rects (~900 vertices) — comfortably
        // under ImGui's 16-bit vertex-index budget without needing the
        // minimap's own run-length merge (that grid is 200x200=40 000
        // cells; this one is two orders of magnitude smaller by design,
        // since each cell here is a whole map tile, not a single chunk).
        const auto tiles = map_view_.visible_tiles(kTilesW, kTilesH);

        // M217 (MAP14) — drop any background request left over from a
        // previous frame's viewport that fast pan/zoom has since scrolled
        // away from, before it wastes worker time generating a tile this
        // frame no longer wants.
        if (map_view_fetch_queue_) map_view_fetch_queue_->cancel_unneeded(tiles);

        for (const auto& tile : tiles) {
            // M-fix: MapView::visible_tiles()'s own before_w/before_h (and
            // the center-tile marker/click-to-pan math further below, both
            // of which already use plain integer kTilesW/2) place the
            // center tile at grid COLUMN kTilesW/2 (7, for width 15) --
            // dx=0 must map to px=7, not 7.5. This used to divide by the
            // floating-point literal 2.0, shifting every tile's fill rect
            // half a cell right/down from the grid it's actually drawn
            // into: a visible seam at the left/top edge and the last
            // column/row overhanging the panel's own border, with the
            // white "center tile" outline drawn a half-cell off from the
            // tile it's supposed to highlight.
            const double px = static_cast<double>(tile.x - map_view_.center().x) + kTilesW / 2;
            const double py = static_cast<double>(tile.y - map_view_.center().y) + kTilesH / 2;
            const float  sx0 = ox + static_cast<float>(px) * cell;
            const float  sy0 = oy + static_cast<float>(py) * cell;
            const float  sx1 = ox + static_cast<float>(px + 1) * cell;
            const float  sy1 = oy + static_cast<float>(py + 1) * cell;

            auto& store = map_view_world_->tile_store(tile.level);
            if (!store.has(tile)) {
                // M192-193 — not generated/persisted yet: kick off
                // background generation (no-op if already queued/done from
                // an earlier frame) and show a dim "generating…" placeholder
                // instead of leaving the cell blank forever.
                if (map_view_fetch_queue_) map_view_fetch_queue_->request(tile);
                const bool pending = map_view_fetch_queue_ && map_view_fetch_queue_->is_pending(tile);
                const ImU32 placeholder = pending ? IM_COL32(70, 70, 80, 180) : IM_COL32(40, 40, 40, 140);
                dl->AddRectFilled({sx0, sy0}, {sx1, sy1}, placeholder);
                continue;
            }
            auto payload = store.load(tile);
            if (!payload || payload->biome.empty()) continue;

            const auto& biome = payload->biome;
            const auto  zone  = static_cast<ZoneType>(biome.at(biome.w / 2, biome.h / 2));
            const auto  c     = WorldMap::zone_color(zone);
            const ImU32 col   = IM_COL32(static_cast<int>(c[0] * 255), static_cast<int>(c[1] * 255),
                                          static_cast<int>(c[2] * 255), 220);
            dl->AddRectFilled({sx0, sy0}, {sx1, sy1}, col);

            loaded.push_back({tile, sx0, sy0, sx1, sy1, std::move(*payload)});
        }

        // M194/M195 — feature overlays (coastlines/rivers/roads/borders) and
        // labels, drawn on top of every tile's base color. World-space
        // points are mapped into that tile's own screen rect via its
        // world_bounds() — features/labels stay within their own tile's
        // rect, matching how the map pipeline generates them tile-local in
        // the first place.
        for (const auto& ld : loaded) {
            const auto   bounds = ld.tile.world_bounds();
            const double bw     = bounds.max_x - bounds.min_x;
            const double bh     = bounds.max_z - bounds.min_z;
            if (bw <= 0.0 || bh <= 0.0) continue;

            const auto to_screen = [&](double wx, double wz) {
                const float fx = static_cast<float>((wx - bounds.min_x) / bw);
                const float fz = static_cast<float>((wz - bounds.min_z) / bh);
                return ImVec2{ld.sx0 + fx * (ld.sx1 - ld.sx0), ld.sy0 + fz * (ld.sy1 - ld.sy0)};
            };

            for (const auto& f : ld.payload.features) {
                ImU32 col;
                float thickness = 1.5f;
                switch (f.type) {
                    case Map::FeatureType::River:
                    case Map::FeatureType::Coastline:    col = IM_COL32( 70, 130, 220, 210); break;
                    case Map::FeatureType::MountainRange: col = IM_COL32(140, 115,  90, 200); break;
                    case Map::FeatureType::Border:       col = IM_COL32(230, 120,  60, 220); thickness = 2.0f; break;
                    case Map::FeatureType::Road:         col = IM_COL32( 90,  90,  90, 220); break;
                    case Map::FeatureType::Street:       col = IM_COL32(150, 150, 150, 160); break;
                    default:                              col = IM_COL32(200, 200, 200, 200); break;
                }

                if (f.points.size() == 1) {
                    dl->AddCircleFilled(to_screen(f.points[0][0], f.points[0][1]), 2.5f, col);
                    continue;
                }
                for (std::size_t i = 1; i < f.points.size(); ++i) {
                    dl->AddLine(to_screen(f.points[i - 1][0], f.points[i - 1][1]),
                                to_screen(f.points[i][0], f.points[i][1]), col, thickness);
                }
            }

            // M195/M202 — only show labels appropriate for the current zoom
            // level, so shallow zooms aren't cluttered with every village name.
            for (const auto& label : ld.payload.labels) {
                if (!MapView::should_show_label(label.kind, level)) continue;
                const ImVec2 p = to_screen(label.pos[0], label.pos[1]);
                dl->AddText({p.x + 3.0f, p.y - 6.0f}, IM_COL32(255, 255, 255, 230), label.name.c_str());
            }
        }

        // M196 — "you are here" marker at the player's current world
        // position, if it falls within the currently rendered tile set (it
        // may not, once M197's pan/zoom controls exist and the view drifts
        // away from the player).
        {
            const Map::TileCoord player_tile = Map::TileCoord::from_world(camera_.x, camera_.z, level);
            for (const auto& ld : loaded) {
                if (ld.tile != player_tile) continue;
                const auto   bounds = ld.tile.world_bounds();
                const double bw     = bounds.max_x - bounds.min_x;
                const double bh     = bounds.max_z - bounds.min_z;
                if (bw <= 0.0 || bh <= 0.0) break;
                const float  fx = static_cast<float>((camera_.x - bounds.min_x) / bw);
                const float  fz = static_cast<float>((camera_.z - bounds.min_z) / bh);
                const ImVec2 p{ld.sx0 + fx * (ld.sx1 - ld.sx0), ld.sy0 + fz * (ld.sy1 - ld.sy0)};
                dl->AddCircleFilled(p, 4.0f, IM_COL32(255, 255, 0, 255));
                dl->AddCircle(p, 6.0f, IM_COL32(0, 0, 0, 200), 0, 1.5f);
                break;
            }
        }

        // Center-tile marker (the tile the view is currently focused on).
        dl->AddRect({ox + (kTilesW / 2) * cell, oy + (kTilesH / 2) * cell},
                    {ox + (kTilesW / 2 + 1) * cell, oy + (kTilesH / 2 + 1) * cell},
                    IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

        dl->AddRect({ox - 1.f, oy - 1.f}, {ox + panel_w + 1.f, oy + panel_h + 1.f},
                    IM_COL32(120, 120, 120, 200));

        // M199 — "1:N" display scale label under the panel.
        const double scale_n = MapView::scale_denominator(level, kTilesW);
        if (scale_n > 0.0) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "1 : %.0f", scale_n);
            dl->AddText({ox, oy + panel_h + 4.0f}, IM_COL32(220, 220, 220, 230), buf);
        }
    }
};

// ---------------------------------------------------------------------------
int main() {
    WorldApp app;
    app.Run();
    return 0;
}
