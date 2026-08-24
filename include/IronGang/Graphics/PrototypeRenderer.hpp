#pragma once

#include "IronGang/Graphics/LightmapMesh.hpp"
#include "IronGang/Graphics/PrimitiveMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <cstddef>
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
    struct ActorPose
    {
        Microsoft::Xna::Framework::Vector3 position;
        float yaw{0.0F};
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
        void DrawTraffic(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         const std::vector<ActorPose>& trafficVehicles,
                         const std::vector<ActorPose>& pedestrians,
                         const std::vector<ActorPose>& policeCars);

        // Counts allocations Iron Gang creates with known dimensions. Imported CNJ model/effect
        // resources remain outside this value because CNA currently exposes no complete backend
        // residency counter; performance reports label that limitation explicitly.
        [[nodiscard]] std::size_t GetTrackedVideoMemoryBytes() const noexcept;

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

        PrimitiveMesh vehicleMesh_;
        PrimitiveMesh playerMesh_;
        PrimitiveMesh trafficVehicleMesh_;
        PrimitiveMesh pedestrianMesh_;
        PrimitiveMesh policeCarMesh_;
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
    };
}
