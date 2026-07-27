#include "IronShadows/Graphics/PrototypeRenderer.hpp"

#include "IronShadows/World/PrototypeWorld.hpp"

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

#include <utility>

namespace IronShadows
{
    using namespace Microsoft::Xna::Framework;
    using namespace Microsoft::Xna::Framework::Graphics;

    void PrototypeRenderer::Initialize(GraphicsDevice& device,
                                       const PrototypeWorld& world,
                                       std::optional<Model> warehouseModel,
                                       std::optional<VehicleModelSet> vehicleModels)
    {
        warehouseModel_ = std::move(warehouseModel);
        vehicleModels_ = std::move(vehicleModels);

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
            const Vector3 playerBodyPosition = playerPosition + Vector3(0.0F, -0.95F, 0.0F);
            DrawMesh(device, playerMesh_, Matrix::CreateRotationY(playerYaw) * Matrix::CreateTranslation(playerBodyPosition));
        }
    }
}
