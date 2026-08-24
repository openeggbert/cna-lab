#pragma once

#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"

#include "World.hpp"

namespace WolfCna
{
    class WolfGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        WolfGame();

        [[nodiscard]] const std::string& GetTypeName() const override;

    protected:
        void Initialize() override;
        void LoadContent() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

    private:
        std::unique_ptr<Microsoft::Xna::Framework::GraphicsDeviceManager> graphics_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> atlas_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> hudSpriteBatch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> hudPixel_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> weaponIcon_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> knifeIcon_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> repeaterIcon_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> shotSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> pickupSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> doorSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> lockedSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> hurtSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> enemyDefeatedSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> terminalSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> guardShotSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> secretSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> guardAlertSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> houndAlertSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> houndAttackSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> extraLifeSound_;

        LevelDefinition level_{LevelDefinition::LoadFromFile("assets/levels/starter.level")};
        World world_{level_};
        Microsoft::Xna::Framework::Vector3 playerPosition_;

        float yaw_ = 0.0f;
        int health_ = 100;
        int ammo_ = 12;
        int score_ = 0;
        int lives_ = 3;
        int nextExtraLifeScore_ = 40000;
        bool hasSecurityCard_ = false;
        bool completed_ = false;
        enum class Screen
        {
            Title,
            Difficulty,
            Controls,
            Playing,
            GameOver
        };
        enum class Difficulty
        {
            Scout,
            Operative,
            Veteran
        };
        Screen screen_ = Screen::Title;
        Difficulty difficulty_ = Difficulty::Operative;
        int menuSelection_ = 0;
        enum class Weapon { Knife, Sidearm, Repeater };
        Weapon weapon_ = Weapon::Sidearm;
        bool actionWasDown_ = false;
        bool attackWasDown_ = false;
        bool fullScreenWasDown_ = false;
        bool upWasDown_ = false;
        bool downWasDown_ = false;
        bool confirmWasDown_ = false;
        bool escapeWasDown_ = false;

        static constexpr float PlayerRadius = 0.22f;
        static constexpr float WalkSpeed = 2.4f;
        static constexpr float KeyboardTurnSpeed = 1.65f;

        void HandleInput(float elapsedSeconds);
        void HandleMenuInput();
        void TryMove(float dx, float dz);
        void ResetRun();
        void AwardScore(int points);

        [[nodiscard]] float DamageMultiplier() const;

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 LookDirection() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix ViewMatrix() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix ProjectionMatrix();

        void CreateProceduralAtlas();
        void CreateHudResources();
        void CreateSoundEffects();
        void DrawHud();
        void DrawMenu();
    };
}
