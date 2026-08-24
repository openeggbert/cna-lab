#include "IronGang/Graphics/VideoMemoryAccounting.hpp"

#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EnvironmentMapEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/IShadowReceiverEXT.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/PbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SkinnedPbrEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture3D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCube.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

#include <algorithm>
#include <cstdint>

namespace IronGang
{
    using namespace Microsoft::Xna::Framework::Graphics;

    namespace
    {
        std::size_t BlocksForDimension(int extent, int blockExtent)
        {
            return static_cast<std::size_t>((std::max(1, extent) + blockExtent - 1) / blockExtent);
        }
    }

    std::size_t CalculateTextureStorageBytes(int width,
                                             int height,
                                             int depth,
                                             int faces,
                                             int levelCount,
                                             SurfaceFormat format)
    {
        if (width <= 0 || height <= 0 || depth <= 0 || faces <= 0 || levelCount <= 0)
        {
            return 0;
        }

        const int blockArea = Texture::GetBlockSizeSquaredEXT(format);
        const int blockExtent = blockArea == 16 ? 4 : 1;
        const std::size_t bytesPerBlock = static_cast<std::size_t>(Texture::GetFormatSizeEXT(format));
        std::size_t total = 0;
        for (int level = 0; level < levelCount; ++level)
        {
            total += BlocksForDimension(width, blockExtent) * BlocksForDimension(height, blockExtent) *
                     static_cast<std::size_t>(depth) * static_cast<std::size_t>(faces) * bytesPerBlock;
            width = std::max(1, width / 2);
            height = std::max(1, height / 2);
            depth = std::max(1, depth / 2);
        }
        return total;
    }

    void VideoMemoryAccumulator::AddModel(const Model& model)
    {
        for (const ModelMesh* mesh : model.getMeshesProperty())
        {
            if (mesh == nullptr)
            {
                continue;
            }
            // The mesh collection is the authoritative set after an effect replacement, while
            // individual parts are also consulted for models assembled through lower-level APIs.
            // effects_ deduplicates the common case where both views contain the same objects.
            for (const Effect* effect : mesh->getEffectsProperty())
            {
                AddEffect(effect);
            }
            for (const ModelMeshPart* part : mesh->getMeshPartsProperty())
            {
                if (part == nullptr)
                {
                    continue;
                }
                AddVertexBuffer(part->getVertexBufferProperty());
                AddIndexBuffer(part->getIndexBufferProperty());
                AddEffect(part->getEffectProperty());
            }
        }
    }

    void VideoMemoryAccumulator::AddVertexBuffer(const VertexBuffer* buffer)
    {
        if (buffer == nullptr || !vertexBuffers_.insert(buffer).second)
        {
            return;
        }
        const int stride = buffer->getVertexDeclarationProperty().getVertexStrideProperty();
        breakdown_.bufferBytes += static_cast<std::size_t>(std::max(0, buffer->getVertexCountProperty())) *
                                  static_cast<std::size_t>(std::max(0, stride));
    }

    void VideoMemoryAccumulator::AddIndexBuffer(const IndexBuffer* buffer)
    {
        if (buffer == nullptr || !indexBuffers_.insert(buffer).second)
        {
            return;
        }
        const std::size_t elementBytes =
            buffer->getIndexElementSizeProperty() == IndexElementSize::ThirtyTwoBits ? sizeof(std::uint32_t)
                                                                                     : sizeof(std::uint16_t);
        breakdown_.bufferBytes +=
            static_cast<std::size_t>(std::max(0, buffer->getIndexCountProperty())) * elementBytes;
    }

    void VideoMemoryAccumulator::AddEffect(const Effect* effect)
    {
        if (effect == nullptr || !effects_.insert(effect).second)
        {
            return;
        }
        for (const EffectParameter& parameter : effect->getParametersProperty())
        {
            AddEffectParameter(parameter);
        }

        // CNA's built-in effects expose texture slots as typed properties rather than generic
        // EffectParameters. Keep the generic traversal above for compiled/custom effects and
        // explicitly cover every built-in effect that can bind a texture here.
        if (const auto* alphaTest = dynamic_cast<const AlphaTestEffect*>(effect))
        {
            AddTexture(alphaTest->getTextureProperty());
        }
        if (const auto* basic = dynamic_cast<const BasicEffect*>(effect))
        {
            AddTexture(basic->getTextureProperty());
        }
        if (const auto* dual = dynamic_cast<const DualTextureEffect*>(effect))
        {
            AddTexture(dual->getTextureProperty());
            AddTexture(dual->getTexture2Property());
        }
        if (const auto* environment = dynamic_cast<const EnvironmentMapEffect*>(effect))
        {
            AddTexture(environment->getTextureProperty());
            AddTexture(environment->getEnvironmentMapProperty());
        }
        if (const auto* pbr = dynamic_cast<const PbrEffect*>(effect))
        {
            AddTexture(pbr->getTextureProperty());
            AddTexture(pbr->getNormalMapProperty());
            AddTexture(pbr->getMetallicRoughnessMapProperty());
            AddTexture(pbr->getEmissiveMapProperty());
            AddTexture(pbr->getOcclusionMapProperty());
            AddTexture(pbr->getSpecularMapEXTProperty());
            AddTexture(pbr->getSpecularColorMapEXTProperty());
            const ImageBasedLightEXT& imageBasedLight = pbr->getImageBasedLightEXT();
            AddTexture(imageBasedLight.Irradiance);
            AddTexture(imageBasedLight.PrefilteredSpecular);
            AddTexture(imageBasedLight.BrdfLut);
        }
        if (const auto* skinned = dynamic_cast<const SkinnedEffect*>(effect))
        {
            AddTexture(skinned->getTextureProperty());
        }
        if (const auto* skinnedPbr = dynamic_cast<const SkinnedPbrEffect*>(effect))
        {
            AddTexture(skinnedPbr->getTextureProperty());
            AddTexture(skinnedPbr->getNormalMapProperty());
            AddTexture(skinnedPbr->getMetallicRoughnessMapProperty());
            AddTexture(skinnedPbr->getEmissiveMapProperty());
            AddTexture(skinnedPbr->getOcclusionMapProperty());
            AddTexture(skinnedPbr->getSpecularMapEXTProperty());
            AddTexture(skinnedPbr->getSpecularColorMapEXTProperty());
            const ImageBasedLightEXT& imageBasedLight = skinnedPbr->getImageBasedLightEXT();
            AddTexture(imageBasedLight.Irradiance);
            AddTexture(imageBasedLight.PrefilteredSpecular);
            AddTexture(imageBasedLight.BrdfLut);
        }
        if (const auto* shadowReceiver = dynamic_cast<const IShadowReceiverEXT*>(effect))
        {
            AddTexture(shadowReceiver->getShadowMapEXT());
            const PunctualLightEXT& punctualLight = shadowReceiver->getPunctualLightEXT();
            AddTexture(punctualLight.ShadowMap);
            AddTexture(punctualLight.ShadowCube);
        }
    }

    void VideoMemoryAccumulator::AddEffectParameter(const EffectParameter& parameter)
    {
        AddTexture(parameter.GetValueTexture2D());
        AddTexture(parameter.GetValueTexture3D());
        AddTexture(parameter.GetValueTextureCube());
        for (const EffectParameter& element : parameter.getElementsProperty())
        {
            AddEffectParameter(element);
        }
        for (const EffectParameter& member : parameter.getStructureMembersProperty())
        {
            AddEffectParameter(member);
        }
    }

    void VideoMemoryAccumulator::AddTexture(const Texture2D* texture)
    {
        if (texture == nullptr || !textures2D_.insert(texture).second)
        {
            return;
        }
        breakdown_.textureBytes += CalculateTextureStorageBytes(
            texture->getWidthProperty(), texture->getHeightProperty(), 1, 1,
            texture->getLevelCountProperty(), texture->getFormatProperty());
    }

    void VideoMemoryAccumulator::AddTexture(const Texture3D* texture)
    {
        if (texture == nullptr || !textures3D_.insert(texture).second)
        {
            return;
        }
        breakdown_.textureBytes += CalculateTextureStorageBytes(
            texture->getWidthProperty(), texture->getHeightProperty(), texture->getDepthProperty(), 1,
            texture->getLevelCountProperty(), texture->getFormatProperty());
    }

    void VideoMemoryAccumulator::AddTexture(const TextureCube* texture)
    {
        if (texture == nullptr || !textureCubes_.insert(texture).second)
        {
            return;
        }
        breakdown_.textureBytes += CalculateTextureStorageBytes(
            texture->getSizeProperty(), texture->getSizeProperty(), 1, 6,
            texture->getLevelCountProperty(), texture->getFormatProperty());
    }
}
