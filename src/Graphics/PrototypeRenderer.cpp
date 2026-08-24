#include "IronGang/Graphics/PrototypeRenderer.hpp"

#include "IronGang/Graphics/SunLight.hpp"
#include "IronGang/World/PrototypeWorld.hpp"

#include "Microsoft/Xna/Framework/Graphics/AnimationPlayer.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"
#include "System/TimeSpan.hpp"

#include <algorithm>
#include <utility>

namespace IronGang
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    struct CharacterAnimationState
    {
        explicit CharacterAnimationState(const SkinningData& data)
            : skinningData(&data), player(data), blendedSkinTransforms(player.GetSkinTransforms())
        {
        }

        void Update(float deltaSeconds, const std::string& requestedClip)
        {
            const auto clipIt = skinningData->AnimationClips.find(requestedClip);
            if (clipIt == skinningData->AnimationClips.end())
            {
                return;
            }

            const AnimationClip* playingClip = player.getCurrentClipProperty();
            if (playingClip != &clipIt->second)
            {
                if (playingClip != nullptr && blendDuration > 0.0F)
                {
                    blendFromSkinTransforms = player.GetSkinTransforms();
                    blendElapsed = 0.0F;
                }
                else
                {
                    blendFromSkinTransforms.clear();
                }
                player.StartClip(clipIt->second);
            }

            player.Update(System::TimeSpan::FromSeconds(deltaSeconds), true, true);
            const auto& target = player.GetSkinTransforms();
            if (blendFromSkinTransforms.empty())
            {
                blendedSkinTransforms = target;
                return;
            }

            blendElapsed += deltaSeconds;
            const float amount = std::clamp(blendElapsed / blendDuration, 0.0F, 1.0F);
            const std::size_t boneCount = std::min(blendFromSkinTransforms.size(), target.size());
            blendedSkinTransforms.resize(boneCount);
            for (std::size_t i = 0; i < boneCount; ++i)
            {
                blendedSkinTransforms[i] = Matrix::Lerp(blendFromSkinTransforms[i], target[i], amount);
            }
            if (amount >= 1.0F)
            {
                blendFromSkinTransforms.clear();
            }
        }

        const SkinningData* skinningData;
        AnimationPlayer player;
        float blendDuration{0.25F};
        float blendElapsed{0.0F};
        std::vector<Matrix> blendFromSkinTransforms;
        std::vector<Matrix> blendedSkinTransforms;
    };

    namespace
    {
        // Gate M10: applies the same CPU-computed sun-brightness tint (SunLight.hpp) to every
        // BasicEffect/PbrEffect/SkinnedEffect/SkinnedPbrEffect a CNJ Model's meshes use -- the
        // real-content equivalent of DrawMesh()'s `tint` parameter for the procedural boxes.
        void SetModelDiffuseColor(Model& model, const Vector3& tint)
        {
            for (ModelMesh* mesh : model.getMeshesProperty())
            {
                for (Effect* effect : mesh->getEffectsPropertyMutable())
                {
                    if (auto* basicEffect = dynamic_cast<BasicEffect*>(effect))
                    {
                        basicEffect->setDiffuseColorProperty(tint);
                    }
                    else if (auto* pbrEffect = dynamic_cast<PbrEffect*>(effect))
                    {
                        pbrEffect->setDiffuseColorProperty(tint);
                    }
                    else if (auto* skinnedEffect = dynamic_cast<SkinnedEffect*>(effect))
                    {
                        skinnedEffect->setDiffuseColorProperty(tint);
                    }
                    else if (auto* skinnedPbrEffect = dynamic_cast<SkinnedPbrEffect*>(effect))
                    {
                        skinnedPbrEffect->setDiffuseColorProperty(tint);
                    }
                }
            }
        }
    }

    PrototypeRenderer::PrototypeRenderer() = default;
    PrototypeRenderer::~PrototypeRenderer() = default;

    void PrototypeRenderer::Initialize(GraphicsDevice& device,
                                       const PrototypeWorld& world,
                                       std::optional<Model> warehouseModel,
                                       std::optional<VehicleModelSet> vehicleModels,
                                       std::optional<Model> characterModel)
    {
        warehouseModel_ = std::move(warehouseModel);
        vehicleModels_ = std::move(vehicleModels);
        characterModel_ = std::move(characterModel);

        // Gate M10 baked lighting: built before RebuildStaticGeometry() below, which assigns the
        // first district's baked lightmap atlas to lightmapEffect_'s second texture slot.
        lightmapNeutralTexture_ = std::make_shared<Texture2D>(device, 1, 1);
        const Color neutralGray(128, 128, 128, 255);
        lightmapNeutralTexture_->SetData(&neutralGray, 1);
        lightmapEffect_ = std::make_unique<DualTextureEffect>(device);
        lightmapEffect_->setVertexColorEnabledProperty(true);
        lightmapEffect_->SetOwnedTexture(lightmapNeutralTexture_);

        if (characterModel_.has_value())
        {
            auto* skinningData = static_cast<SkinningData*>(characterModel_->getTagProperty());
            if (skinningData != nullptr)
            {
                characterAnimation_ = std::make_unique<CharacterAnimationState>(*skinningData);
            }
            else
            {
                // Loaded, but with no SkinningData on Tag -- not a skinned model (e.g. a
                // malformed/regenerated asset). Fall back to the procedural player box rather
                // than drawing an un-animatable character.
                characterModel_.reset();
            }
        }

        RebuildStaticGeometry(device, world);

        MeshBuilder vehicleBuilder;
        vehicleBuilder.AddBox({0.0F, 0.0F, 0.0F}, {2.1F, 0.65F, 4.2F}, Color(116, 26, 30, 255));
        vehicleBuilder.AddBox({0.0F, 0.58F, -0.15F}, {1.75F, 0.75F, 2.05F}, Color(145, 42, 47, 255));
        vehicleBuilder.AddBox({0.0F, 0.62F, -0.35F}, {1.50F, 0.45F, 1.45F}, Color(95, 130, 145, 255));
        vehicleBuilder.AddBox({-1.05F, -0.20F, -1.35F}, {0.32F, 0.65F, 0.75F}, Color(25, 25, 27, 255));
        vehicleBuilder.AddBox({1.05F, -0.20F, -1.35F}, {0.32F, 0.65F, 0.75F}, Color(25, 25, 27, 255));
        vehicleBuilder.AddBox({-1.05F, -0.20F, 1.35F}, {0.32F, 0.65F, 0.75F}, Color(25, 25, 27, 255));
        vehicleBuilder.AddBox({1.05F, -0.20F, 1.35F}, {0.32F, 0.65F, 0.75F}, Color(25, 25, 27, 255));
        vehicleMesh_.Upload(device, vehicleBuilder);

        MeshBuilder playerBuilder;
        playerBuilder.AddBox({0.0F, 0.0F, 0.0F}, {0.55F, 1.25F, 0.38F}, Color(49, 69, 91, 255));
        playerBuilder.AddBox({0.0F, 0.85F, 0.0F}, {0.42F, 0.42F, 0.42F}, Color(209, 177, 143, 255));
        playerMesh_.Upload(device, playerBuilder);

        // Gate M9: a plainer, slightly smaller car than the player's own sedan (fewer parts, flat
        // slate-blue paint) so traffic reads as background rather than competing with it visually.
        MeshBuilder trafficVehicleBuilder;
        trafficVehicleBuilder.AddBox({0.0F, 0.0F, 0.0F}, {1.9F, 0.6F, 3.9F}, Color(70, 82, 97, 255));
        trafficVehicleBuilder.AddBox({0.0F, 0.55F, -0.10F}, {1.55F, 0.6F, 1.8F}, Color(88, 98, 112, 255));
        trafficVehicleMesh_.Upload(device, trafficVehicleBuilder);

        // A simple standing box distinct from the player's own two-tone body/head mesh (single
        // muted tone, no separate head box -- keeps ambient pedestrians cheap and visually minor).
        MeshBuilder pedestrianBuilder;
        pedestrianBuilder.AddBox({0.0F, 0.0F, 0.0F}, {0.5F, 1.15F, 0.35F}, Color(120, 108, 96, 255));
        pedestrianMesh_.Upload(device, pedestrianBuilder);

        // Black-and-white patrol livery so a police response reads clearly against ordinary
        // traffic even as plain colored boxes.
        MeshBuilder policeCarBuilder;
        policeCarBuilder.AddBox({0.0F, 0.0F, 0.0F}, {2.0F, 0.62F, 4.0F}, Color(245, 245, 245, 255));
        policeCarBuilder.AddBox({0.0F, 0.56F, -0.10F}, {1.6F, 0.62F, 1.9F}, Color(20, 20, 24, 255));
        policeCarBuilder.AddBox({0.0F, 0.92F, -0.10F}, {0.55F, 0.20F, 0.35F}, Color(200, 40, 40, 255));
        policeCarMesh_.Upload(device, policeCarBuilder);

        // Gate M10: a unit-footprint flat, dark, semi-transparent decal -- scaled/positioned per
        // actor at draw time (see DrawShadowDecal()) rather than baked per-size, so one mesh
        // covers both the player's and the vehicle's differently-shaped footprints.
        MeshBuilder shadowBuilder;
        shadowBuilder.AddBox({0.0F, 0.0F, 0.0F}, {1.0F, 0.02F, 1.0F}, Color(10, 10, 10, 110));
        shadowDecalMesh_.Upload(device, shadowBuilder);

        effect_ = std::make_unique<BasicEffect>(device);
        effect_->VertexColorEnabled = true;
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.SetDepthTestEnabled(true);
    }

    void PrototypeRenderer::UpdateCharacterAnimation(float deltaSeconds, const std::string& clipName)
    {
        if (!characterAnimation_)
        {
            return;
        }
        characterAnimation_->Update(deltaSeconds, clipName);
    }

    void PrototypeRenderer::RebuildStaticGeometry(GraphicsDevice& device, const PrototypeWorld& world)
    {
        // Gate M10 baked lighting: one flat-shaded lightmap tile per box face (LightmapMesh.hpp),
        // not per-vertex vertex color -- MC3-sourced models (warehouseModel_) have no lightmap UV
        // channel from the current pipeline and stay out of scope for this pass.
        LightmapMeshBuilder cityBuilder;
        for (const WorldBox& box : world.GetBoxes())
        {
            if (warehouseModel_.has_value() && box.name == "warehouse")
            {
                warehousePosition_ = box.center;
                continue;
            }
            cityBuilder.AddBox(box.center, box.size, box.color);
        }
        cityBuilder.Finalize();
        staticCityLightmapMesh_.Upload(device, cityBuilder);

        auto atlasTexture = std::make_shared<Texture2D>(device, cityBuilder.GetAtlasWidth(), cityBuilder.GetAtlasHeight());
        const auto& atlasPixels = cityBuilder.GetAtlasPixels();
        atlasTexture->SetData(atlasPixels.data(), static_cast<int>(atlasPixels.size()));
        lightmapEffect_->SetOwnedTexture2(std::move(atlasTexture));
        lightmapTextureBytes_ = sizeof(Color) + atlasPixels.size() * sizeof(Color);
    }

    RendererVideoMemoryBreakdown PrototypeRenderer::GetTrackedVideoMemory() const
    {
        VideoMemoryAccumulator importedModels;
        if (warehouseModel_)
        {
            importedModels.AddModel(*warehouseModel_);
        }
        if (vehicleModels_)
        {
            importedModels.AddModel(vehicleModels_->body);
            importedModels.AddModel(vehicleModels_->cabin);
            importedModels.AddModel(vehicleModels_->windshield);
            importedModels.AddModel(vehicleModels_->wheel);
        }
        if (characterModel_)
        {
            importedModels.AddModel(*characterModel_);
        }

        RendererVideoMemoryBreakdown result;
        result.gameOwnedBytes = staticCityLightmapMesh_.GetTrackedVideoMemoryBytes() +
                                vehicleMesh_.GetTrackedVideoMemoryBytes() +
                                playerMesh_.GetTrackedVideoMemoryBytes() +
                                trafficVehicleMesh_.GetTrackedVideoMemoryBytes() +
                                pedestrianMesh_.GetTrackedVideoMemoryBytes() +
                                policeCarMesh_.GetTrackedVideoMemoryBytes() +
                                shadowDecalMesh_.GetTrackedVideoMemoryBytes() + lightmapTextureBytes_;
        result.importedModels = importedModels.GetBreakdown();
        return result;
    }

    void PrototypeRenderer::DrawMesh(GraphicsDevice& device,
                                     PrimitiveMesh& mesh,
                                     const Matrix& worldMatrix,
                                     const Vector3& tint)
    {
        effect_->World = worldMatrix;
        effect_->setDiffuseColorProperty(tint);
        for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();
            mesh.Draw(device);
        }
    }

    void PrototypeRenderer::DrawStaticCityMesh(GraphicsDevice& device, const Matrix& view, const Matrix& projection)
    {
        lightmapEffect_->setWorldProperty(Matrix::getIdentityProperty());
        lightmapEffect_->setViewProperty(view);
        lightmapEffect_->setProjectionProperty(projection);
        for (auto& pass : lightmapEffect_->getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();
            staticCityLightmapMesh_.Draw(device);
        }
    }

    void PrototypeRenderer::DrawShadowDecal(GraphicsDevice& device,
                                            const Vector3& position,
                                            float yaw,
                                            float width,
                                            float depth)
    {
        // Ground surface sits at Y = -0.05 (SetGround's center -0.30 + half-height 0.25); this
        // matches the small clearance lane markings already use above it (Y = 0.045) to avoid
        // z-fighting.
        constexpr float kShadowGroundY = 0.03F;
        const Matrix world = Matrix::CreateScale(width, 1.0F, depth) *
                             Matrix::CreateRotationY(yaw) *
                             Matrix::CreateTranslation(Vector3(position.X, kShadowGroundY, position.Z));
        DrawMesh(device, shadowDecalMesh_, world);
    }

    void PrototypeRenderer::Draw(GraphicsDevice& device,
                                 const Matrix& view,
                                 const Matrix& projection,
                                 const Vector3& playerPosition,
                                 float playerYaw,
                                 bool drawPlayer,
                                 const Vector3& vehiclePosition,
                                 float vehicleYaw)
    {
        effect_->View = view;
        effect_->Projection = projection;

        // Gate M10: one shared brightness scalar for every dynamic actor this frame (see
        // SunLight.hpp). warehouseModel_ is deliberately left untinted here -- it has no lightmap
        // UV channel from the current pipeline and is out of scope for baked lighting this pass,
        // but it is also not a "dynamic actor", so it is simplest to leave it at full brightness
        // rather than half-applying either lighting model to it.
        const float sunBrightness = ComputeSunBrightness();
        const Vector3 sunTint(sunBrightness, sunBrightness, sunBrightness);

        // The static city mesh gets real per-face baked lighting (DrawStaticCityMesh(), a
        // DualTextureEffect) instead of the CPU brightness tint below.
        DrawStaticCityMesh(device, view, projection);

        const Matrix vehicleWorld = Matrix::CreateRotationY(vehicleYaw) * Matrix::CreateTranslation(vehiclePosition);
        if (vehicleModels_.has_value())
        {
            const Vector3 kCabinOffset{0.0F, 0.58F, -0.15F};
            const Vector3 kWindshieldOffset{0.0F, 0.62F, -0.35F};
            const Vector3 kWheelOffsets[4] = {
                {-1.05F, -0.20F, -1.35F}, {1.05F, -0.20F, -1.35F},
                {-1.05F, -0.20F, 1.35F},  {1.05F, -0.20F, 1.35F},
            };

            SetModelDiffuseColor(vehicleModels_->body, sunTint);
            SetModelDiffuseColor(vehicleModels_->cabin, sunTint);
            SetModelDiffuseColor(vehicleModels_->windshield, sunTint);
            SetModelDiffuseColor(vehicleModels_->wheel, sunTint);

            vehicleModels_->body.Draw(vehicleWorld, view, projection);
            vehicleModels_->cabin.Draw(Matrix::CreateTranslation(kCabinOffset) * vehicleWorld, view, projection);
            vehicleModels_->windshield.Draw(Matrix::CreateTranslation(kWindshieldOffset) * vehicleWorld, view, projection);
            for (const Vector3& wheelOffset : kWheelOffsets)
            {
                vehicleModels_->wheel.Draw(Matrix::CreateTranslation(wheelOffset) * vehicleWorld, view, projection);
            }
        }
        else
        {
            DrawMesh(device, vehicleMesh_, vehicleWorld, sunTint);
        }

        if (warehouseModel_.has_value())
        {
            warehouseModel_->Draw(Matrix::CreateTranslation(warehousePosition_), view, projection);
        }

        if (drawPlayer)
        {
            if (characterModel_.has_value() && characterAnimation_)
            {
                // Push freshly computed bone poses onto every skinned effect this model's meshes
                // use, then draw through the same direct Model::Draw() call warehouseModel_/
                // vehicleModels_ already use. The animation state uses CNA::GraphicsCore's
                // AnimationPlayer directly, so no separate ECS/rendering framework is needed.
                const auto& skinTransforms = characterAnimation_->blendedSkinTransforms;
                for (ModelMesh* mesh : characterModel_->getMeshesProperty())
                {
                    for (Effect* effect : mesh->getEffectsPropertyMutable())
                    {
                        if (auto* skinnedEffect = dynamic_cast<SkinnedEffect*>(effect))
                        {
                            skinnedEffect->SetBoneTransforms(skinTransforms);
                            skinnedEffect->setDiffuseColorProperty(sunTint);
                        }
                        else if (auto* skinnedPbrEffect = dynamic_cast<SkinnedPbrEffect*>(effect))
                        {
                            skinnedPbrEffect->SetBoneTransforms(skinTransforms);
                            skinnedPbrEffect->setDiffuseColorProperty(sunTint);
                        }
                    }
                }
                characterModel_->Draw(Matrix::CreateRotationY(playerYaw) * Matrix::CreateTranslation(playerPosition),
                                      view, projection);
            }
            else
            {
                const Vector3 playerBodyPosition = playerPosition + Vector3(0.0F, -0.95F, 0.0F);
                DrawMesh(device, playerMesh_,
                        Matrix::CreateRotationY(playerYaw) * Matrix::CreateTranslation(playerBodyPosition), sunTint);
            }
        }

        // Gate M10 "limited shadows": simple ground-decal blob shadows beneath the player and
        // their own vehicle only (not traffic/pedestrians/police -- see DrawShadowDecal()'s own
        // comment). Drawn last, with alpha blending, over the already-drawn opaque geometry.
        device.setBlendStateProperty(BlendState::AlphaBlend);
        if (drawPlayer)
        {
            DrawShadowDecal(device, playerPosition, playerYaw, 0.8F, 0.8F);
        }
        DrawShadowDecal(device, vehiclePosition, vehicleYaw, 1.9F, 3.7F);
        device.setBlendStateProperty(BlendState::Opaque);
    }

    void PrototypeRenderer::DrawTraffic(GraphicsDevice& device,
                                        const Matrix& view,
                                        const Matrix& projection,
                                        const std::vector<ActorPose>& trafficVehicles,
                                        const std::vector<ActorPose>& pedestrians,
                                        const std::vector<ActorPose>& policeCars)
    {
        effect_->View = view;
        effect_->Projection = projection;

        // Gate M10: the same shared sun-brightness tint as Draw()'s player/vehicle -- see
        // SunLight.hpp's own comment.
        const float sunBrightness = ComputeSunBrightness();
        const Vector3 sunTint(sunBrightness, sunBrightness, sunBrightness);

        for (const ActorPose& pose : trafficVehicles)
        {
            DrawMesh(device, trafficVehicleMesh_,
                    Matrix::CreateRotationY(pose.yaw) * Matrix::CreateTranslation(pose.position), sunTint);
        }
        for (const ActorPose& pose : pedestrians)
        {
            // Unlike the player mesh (whose GetPosition() is eye-height, offset down in Draw()
            // above), sidewalk WaypointPath points are already authored at this mesh's own center
            // height (see PrototypeWorld::BuildWarehouseBlock's sidewalkPaths_ comment) -- no
            // extra vertical offset needed here.
            DrawMesh(device, pedestrianMesh_,
                    Matrix::CreateRotationY(pose.yaw) * Matrix::CreateTranslation(pose.position), sunTint);
        }
        for (const ActorPose& pose : policeCars)
        {
            DrawMesh(device, policeCarMesh_,
                    Matrix::CreateRotationY(pose.yaw) * Matrix::CreateTranslation(pose.position), sunTint);
        }
    }
}
