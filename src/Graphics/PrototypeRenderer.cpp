#include "IronShadows/Graphics/PrototypeRenderer.hpp"

#include "IronShadows/World/PrototypeWorld.hpp"

#include "CNA/Extended/ECS/Entity.hpp"
#include "CNA/Extended/ECS/World.hpp"
#include "CNA/Extended/ECS/WorldBuilder.hpp"
#include "CNA/Extended/World3DEXT/ModelAnimationComponentEXT.hpp"
#include "CNA/Extended/World3DEXT/ModelAnimationSystem3DEXT.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"
#include "System/TimeSpan.hpp"

#include <utility>

namespace IronShadows
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

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

        if (characterModel_.has_value())
        {
            auto* skinningData = static_cast<SkinningData*>(characterModel_->getTagProperty());
            if (skinningData != nullptr)
            {
                characterAnimComponent_ =
                    std::make_unique<CNA::Extended::World3DEXT::ModelAnimationComponentEXT>(*skinningData);
                characterAnimComponent_->ModelEXT = &characterModel_.value();

                CNA::Extended::ECS::WorldBuilder builder;
                builder.AddSystem(std::make_unique<CNA::Extended::World3DEXT::ModelAnimationSystem3DEXT>());
                characterWorld_ = builder.Build();
                characterWorld_->Initialize();
                CNA::Extended::ECS::Entity& entity = characterWorld_->CreateEntity();
                entity.Attach(characterAnimComponent_.get());
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

        effect_ = std::make_unique<BasicEffect>(device);
        effect_->VertexColorEnabled = true;
        device.setRasterizerStateProperty(RasterizerState::CullNone);
        device.SetDepthTestEnabled(true);
    }

    void PrototypeRenderer::UpdateCharacterAnimation(float deltaSeconds, const std::string& clipName)
    {
        if (!characterWorld_ || !characterAnimComponent_)
        {
            return;
        }
        characterAnimComponent_->ClipNameEXT = clipName;
        GameTime gameTime(System::TimeSpan::Zero, System::TimeSpan::FromSeconds(deltaSeconds));
        characterWorld_->Update(gameTime);
    }

    void PrototypeRenderer::RebuildStaticGeometry(GraphicsDevice& device, const PrototypeWorld& world)
    {
        MeshBuilder cityBuilder;
        for (const WorldBox& box : world.GetBoxes())
        {
            if (warehouseModel_.has_value() && box.name == "warehouse")
            {
                warehousePosition_ = box.center;
                continue;
            }
            cityBuilder.AddBox(box.center, box.size, box.color);
        }
        staticCityMesh_.Upload(device, cityBuilder);
    }

    void PrototypeRenderer::DrawMesh(GraphicsDevice& device,
                                     PrimitiveMesh& mesh,
                                     const Matrix& worldMatrix)
    {
        effect_->World = worldMatrix;
        for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        {
            pass.Apply();
            mesh.Draw(device);
        }
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

        DrawMesh(device, staticCityMesh_, Matrix::getIdentityProperty());

        const Matrix vehicleWorld = Matrix::CreateRotationY(vehicleYaw) * Matrix::CreateTranslation(vehiclePosition);
        if (vehicleModels_.has_value())
        {
            const Vector3 kCabinOffset{0.0F, 0.58F, -0.15F};
            const Vector3 kWindshieldOffset{0.0F, 0.62F, -0.35F};
            const Vector3 kWheelOffsets[4] = {
                {-1.05F, -0.20F, -1.35F}, {1.05F, -0.20F, -1.35F},
                {-1.05F, -0.20F, 1.35F},  {1.05F, -0.20F, 1.35F},
            };

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
            DrawMesh(device, vehicleMesh_, vehicleWorld);
        }

        if (warehouseModel_.has_value())
        {
            warehouseModel_->Draw(Matrix::CreateTranslation(warehousePosition_), view, projection);
        }

        if (drawPlayer)
        {
            if (characterModel_.has_value() && characterAnimComponent_)
            {
                // Push freshly computed bone poses onto every skinned effect this model's meshes
                // use, then draw through the same direct Model::Draw() call warehouseModel_/
                // vehicleModels_ already use (no RenderSystem3DEXT/Camera3DEXT: this renderer
                // already has view/projection ready, and needs no other ECS-driven drawing).
                const auto& skinTransforms = characterAnimComponent_->PlayerEXT.GetSkinTransforms();
                for (ModelMesh* mesh : characterModel_->getMeshesProperty())
                {
                    for (Effect* effect : mesh->getEffectsPropertyMutable())
                    {
                        if (auto* skinnedEffect = dynamic_cast<SkinnedEffect*>(effect))
                        {
                            skinnedEffect->SetBoneTransforms(skinTransforms);
                        }
                        else if (auto* skinnedPbrEffect = dynamic_cast<SkinnedPbrEffect*>(effect))
                        {
                            skinnedPbrEffect->SetBoneTransforms(skinTransforms);
                        }
                    }
                }
                characterModel_->Draw(Matrix::CreateRotationY(playerYaw) * Matrix::CreateTranslation(playerPosition),
                                      view, projection);
            }
            else
            {
                const Vector3 playerBodyPosition = playerPosition + Vector3(0.0F, -0.95F, 0.0F);
                DrawMesh(device, playerMesh_, Matrix::CreateRotationY(playerYaw) * Matrix::CreateTranslation(playerBodyPosition));
            }
        }
    }
}
