#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    using Microsoft::Xna::Framework::Vector3;

    // plan_08 IG-08-014: the base colour an imported model should be drawn in.
    //
    // MC3 sources declare one per model (`<base_color>` in assets/source/mc3/*.mc3.xml), but
    // **none of it survives the pipeline**: a generated .cnj holds only vertices, indices, a
    // vertex stride and an effect name, and the vertex layout (stride 32 = position, normal, uv)
    // has no colour channel. Every MC3-sourced model therefore draws white unless the game says
    // otherwise -- which is why the warehouse rendered as a flat white slab brighter than anything
    // else in the scene, and why the sedan, authored dark red, rendered pale grey.
    //
    // So Iron Gang carries the colours itself, in shipped, versioned data rather than as literals
    // in the renderer, and a test keeps the file honest against the MC3 sources it mirrors.
    struct ModelMaterial
    {
        // The generated model's base name, as passed to ContentManager (e.g. "warehouse").
        std::string modelId;
        // Linear RGB in [0,1], matching the MC3 `<base_color>` it mirrors.
        Vector3 baseColor{1.0F, 1.0F, 1.0F};
    };

    inline constexpr int kModelMaterialsFileVersion = 1;

    class ModelMaterialTable final
    {
    public:
        // Validation refuses an unsupported version, a duplicate or empty model id, a colour that
        // is not three numbers in [0,1], and unknown fields. A file that fails to load leaves the
        // table empty, and every lookup then returns white -- exactly the behaviour that existed
        // before this data did, so a broken file degrades rather than blanks the screen.
        [[nodiscard]] bool LoadFromFile(const std::string& path, std::string& errorMessage);

        // White when the model has no entry. Callers multiply this by the shared sun brightness,
        // so an unknown model keeps rendering exactly as it did before.
        [[nodiscard]] Vector3 GetBaseColor(const std::string& modelId) const;
        [[nodiscard]] bool Contains(const std::string& modelId) const;
        [[nodiscard]] std::size_t GetCount() const noexcept { return materials_.size(); }
        [[nodiscard]] const std::vector<ModelMaterial>& GetMaterials() const noexcept { return materials_; }

    private:
        std::vector<ModelMaterial> materials_;
    };
}
