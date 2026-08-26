#pragma once

#include "IronGang/Graphics/LightmapMesh.hpp"
#include "IronGang/Graphics/PrimitiveMesh.hpp"
#include "IronGang/Graphics/VideoMemoryAccounting.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace IronGang
{
    class PrototypeWorld;
    struct CharacterAnimationState;

    // Gate M9: the minimal per-actor state PrototypeRenderer needs to draw one traffic vehicle,
    // pedestrian, or police car this frame -- position/yaw only, matching how the player/vehicle
    // boxes are already drawn in Draw() below.
    // A traffic light to draw: where it stands and what colour it is showing.
    struct SignalLight
    {
        Microsoft::Xna::Framework::Vector3 position;
        Microsoft::Xna::Framework::Color color;
    };

    struct ActorPose
    {
        Microsoft::Xna::Framework::Vector3 position;
        float yaw{0.0F};
        // Pedestrians only (plan_20 IG-20-003): the locomotion flags that pick this one's
        // animation clip, resolved through SelectPedestrianAnimation(). Vehicles ignore them.
        bool moving{true};
        bool turningInPlace{false};
        bool fleeing{false};
        // Whether to draw this pedestrian as the skinned character rather than a coloured box.
        // The **caller** decides, because the caller knows where the player is looking from and
        // the renderer does not; see IronGangGame's nearest-N policy.
        bool skinned{true};
    };

    // The current MC3 -> glTF -> CNJ pipeline does not bake per-object node transforms into
    // vertex data, so a multi-object MC3 scene loaded as one CNJ Model loses each object's
    // relative position (confirmed empirically; see plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md).
    // Until a scene compiler closes that gap, a multi-part prop like the sedan is authored as one
    // single-object MC3 file per part, and Iron Gang itself composes the parts with its own
    // local-offset transforms -- the same composition PrototypeRenderer already did for the
    // procedural boxes, just per CNJ model instead of per procedural box.
    struct VehicleModelSet
    {
        Microsoft::Xna::Framework::Graphics::Model body;
        Microsoft::Xna::Framework::Graphics::Model cabin;
        Microsoft::Xna::Framework::Graphics::Model windshield;
        Microsoft::Xna::Framework::Graphics::Model wheel;
    };

    struct RendererVideoMemoryBreakdown
    {
        std::size_t gameOwnedBytes{0};
        VideoMemoryBreakdown importedModels;

        [[nodiscard]] std::size_t TotalBytes() const noexcept
        {
            return gameOwnedBytes + importedModels.TotalBytes();
        }
    };

    // Exact game/front-end 3D submission counts for one frame. StateChanges counts explicit
    // EffectPass applications plus vertex/index-buffer and blend-state binding calls; a backend
    // may deduplicate them. VisibleObjects means submitted scene objects: the prototype has no
    // frustum/occlusion rejection yet, so every submitted object is treated as visible.
    struct RenderWorkload
    {
        std::uint64_t drawCalls{0};
        std::uint64_t stateChanges{0};
        std::uint64_t vertices{0};
        std::uint64_t triangles{0};
        std::uint64_t instances{0};
        std::uint64_t visibleObjects{0};
    };

    class PrototypeRenderer final
    {
    public:
        PrototypeRenderer();
        // Declared (not defaulted) here and defined `= default` in the .cpp: characterAnimation_
        // is a unique_ptr to a forward-declared type, which needs the type complete wherever the
        // destructor is actually instantiated -- the .cpp includes the full definition, this header's
        // other includers do not.
        ~PrototypeRenderer();

        // warehouseModel/vehicleModels replace the procedural warehouse box and sedan with
        // converted CNJ models (MC3 -> glTF -> CNJ) when supplied, proving the Mesh Craft -> CNA
        // runtime loop for production assets. Every other box keeps using the procedural
        // debug renderer.
        // characterModel additionally replaces the procedural on-foot player box with a real
        // skinned CNJ character (gate M6) when supplied; see UpdateCharacterAnimation for how its
        // Idle/Walk clip is driven. characterModel's asset is hand-authored glTF, not MC3 -- Mesh
        // Craft has no rigging/skinning authoring support (see assets/source/gltf/test_character.gltf's
        // own provenance note) -- unlike warehouseModel/vehicleModels, which are real MC3 content.
        void Initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const PrototypeWorld& world,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel = std::nullopt,
                        std::optional<VehicleModelSet> vehicleModels = std::nullopt,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> characterModel = std::nullopt);

        // Advances the skinned character's animation state (gate M6): switches to clipName if it
        // differs from the currently playing clip, crossfades over 0.25 seconds, and ticks it by
        // deltaSeconds through CNA::GraphicsCore's AnimationPlayer. A no-op if characterModel was
        // not supplied to Initialize() (the procedural player box stays in place instead). Call
        // once per frame from gameplay Update(), not Draw() -- Draw() has no time step of its own.
        void UpdateCharacterAnimation(float deltaSeconds, const std::string& clipName);

        // plan_20 IG-20-003: advances one animation state per pedestrian, creating them as needed
        // and dropping any surplus. Each is started at its own phase, because a crowd of skinned
        // characters stepping in perfect unison looks worse than the coloured boxes they replace.
        // A no-op when the skinned character model is unavailable -- the boxes stay in that case.
        // Call once per frame from Update(), like UpdateCharacterAnimation.
        void UpdatePedestrianAnimations(float deltaSeconds, const std::vector<ActorPose>& pedestrians);

        // Rebuilds just the static city mesh for a newly loaded district (a district transition,
        // see DistrictManager) without touching the vehicle/player meshes or reloading CNJ
        // content. warehouseModel_/vehicleModels_ keep applying by box name, so re-entering the
        // district that has the warehouse CNJ model still shows it; a district with no box named
        // "warehouse" (e.g. the countryside) naturally never matches and stays fully procedural.
        void RebuildStaticGeometry(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                   const PrototypeWorld& world);

        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                  const Microsoft::Xna::Framework::Matrix& view,
                  const Microsoft::Xna::Framework::Matrix& projection,
                  const Microsoft::Xna::Framework::Vector3& playerPosition,
                  float playerYaw,
                  bool drawPlayer,
                  const Microsoft::Xna::Framework::Vector3& vehiclePosition,
                  float vehicleYaw);

        // Gate M9 (plan_21/plan_20/plan_22): drawn as a separate pass right after Draw() each
        // frame, reusing the view/projection Draw() already set up. Each box-based mesh below is
        // deliberately shaped/colored distinctly from the player's own sedan/character meshes so
        // the three actor kinds (and the player) stay visually distinguishable in a screenshot
        // despite all being colored-box debug geometry.
        // plan_21 IG-21-003: one small box per signal, coloured by its phase, drawn in the same
        // pass as the ambient actors.
        void DrawTrafficSignals(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                const Microsoft::Xna::Framework::Matrix& view,
                                const Microsoft::Xna::Framework::Matrix& projection,
                                const std::vector<SignalLight>& lights);

        void DrawTraffic(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         const std::vector<ActorPose>& trafficVehicles,
                         const std::vector<ActorPose>& pedestrians,
                         const std::vector<ActorPose>& policeCars);

        // Counts game-created resources plus imported CNJ model buffers/textures through CNA's
        // public capacity/format APIs. Backend effect programs, swapchain/depth allocations and
        // driver padding remain outside this value; reports label that limitation explicitly.
        [[nodiscard]] RendererVideoMemoryBreakdown GetTrackedVideoMemory() const;

        // Enabled only for --profile runs. HUD SpriteBatch batching is backend-internal and is
        // intentionally outside these exact 3D counters.
        void BeginFrameWorkloadTracking() noexcept;
        [[nodiscard]] const RenderWorkload& GetFrameWorkload() const noexcept { return frameWorkload_; }

    private:
        // tint multiplies vertex color (see SunLight.hpp's own comment on why this, rather than
        // CNA's built-in lighting, drives gate M10's "dynamic sun"). Defaults to full brightness
        // for static geometry; dynamic actors pass ComputeSunBrightness() explicitly.
        void DrawMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                      PrimitiveMesh& mesh,
                      const Microsoft::Xna::Framework::Matrix& worldMatrix,
                      const Microsoft::Xna::Framework::Vector3& tint =
                          Microsoft::Xna::Framework::Vector3(1.0F, 1.0F, 1.0F));

        // Gate M10: a period-appropriate "blob shadow" -- a flat, dark, semi-transparent decal on
        // the ground beneath an actor -- standing in for real shadow-mapping, which is not
        // achievable on the SOFTWARE backend without modifying CNA itself (no shadow-map support,
        // fixed per-backend effect formulas rather than a custom-shader system; confirmed by
        // reading its source). Scoped to just the player and their own vehicle, matching gate
        // M10's own "limited shadows" wording -- not extended to every traffic/pedestrian/police
        // actor.
        void DrawShadowDecal(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                             const Microsoft::Xna::Framework::Vector3& position,
                             float yaw,
                             float width,
                             float depth);

        // Gate M10 baked lighting: draws staticCityLightmapMesh_ through lightmapEffect_ (a
        // DualTextureEffect, not the shared BasicEffect effect_ every other mesh uses) -- see
        // RebuildStaticGeometry()'s own comment for how the lightmap atlas is built.
        void DrawStaticCityMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                const Microsoft::Xna::Framework::Matrix& view,
                                const Microsoft::Xna::Framework::Matrix& projection);

        void DrawModel(Microsoft::Xna::Framework::Graphics::Model& model,
                       const Microsoft::Xna::Framework::Matrix& world,
                       const Microsoft::Xna::Framework::Matrix& view,
                       const Microsoft::Xna::Framework::Matrix& projection);
        void RecordPrimitiveDraw(const PrimitiveMesh& mesh) noexcept;
        void RecordLightmapDraw() noexcept;
        void RecordModelDraw(const Microsoft::Xna::Framework::Graphics::Model& model);

        LightmapPrimitiveMesh staticCityLightmapMesh_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::DualTextureEffect> lightmapEffect_;
        // texture0 for lightmapEffect_: a flat, near-50%-gray 1x1 texture so DualTextureEffect's
        // `texture0*2` term is close to an identity multiplier (128/255*2 ~= 1.004, a negligible
        // ~0.4% error) -- the real per-face brightness lives entirely in texture1 (the baked
        // lightmap atlas, rebuilt per district in RebuildStaticGeometry()). Both are
        // std::shared_ptr so lightmapEffect_'s SetOwnedTexture()/SetOwnedTexture2() (which take
        // shared ownership, matching real XNA's GC-tracked Effect.Texture) can keep them alive.
        std::shared_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> lightmapNeutralTexture_;
        std::size_t lightmapTextureBytes_{0};
        std::size_t staticPrimitiveObjectCount_{0};

        PrimitiveMesh vehicleMesh_;
        PrimitiveMesh playerMesh_;
        PrimitiveMesh trafficVehicleMesh_;
        PrimitiveMesh pedestrianMesh_;
        PrimitiveMesh policeCarMesh_;
        PrimitiveMesh signalLightMesh_;
        PrimitiveMesh shadowDecalMesh_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;

        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel_;
        Microsoft::Xna::Framework::Vector3 warehousePosition_{};

        std::optional<VehicleModelSet> vehicleModels_;

        // Gate M6: one small game-owned playback state over CNA's AnimationPlayer. Pulling in the
        // entire cna-extended ECS for this single character would defeat CNA's modular consumer
        // model; rendering stays on the direct Model::Draw() path used by the other CNJ assets.
        std::optional<Microsoft::Xna::Framework::Graphics::Model> characterModel_;
        std::unique_ptr<CharacterAnimationState> characterAnimation_;
        // One per pedestrian, in the same order DrawTraffic receives them. The model itself is
        // shared: only the bone palette differs per instance, and it is pushed into the effect
        // immediately before each draw.
        std::vector<std::unique_ptr<CharacterAnimationState>> pedestrianAnimations_;

        bool workloadTrackingEnabled_{false};
        RenderWorkload frameWorkload_;
    };
}
