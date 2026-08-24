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
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"

#include "World.hpp"
#include "CampaignProgress.hpp"
#include "ExplorationMap.hpp"

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
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> guardSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> houndSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> rapidTrooperSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyUnitSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> guardAttackSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> houndAttackSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> rapidTrooperAttackSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyUnitAttackSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> guardPainSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> houndPainSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> rapidTrooperPainSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyUnitPainSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> defeatedGuardSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> defeatedHoundSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> defeatedRapidTrooperSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> defeatedHeavyUnitSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> ammoPickupSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> healthPickupSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> goldBarsSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> goldenGobletSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> peaceMedallionSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> accessCardSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> repeaterPickupSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyWeaponPickupSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> terminalSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> relaySprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> exitSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> enemyProjectileSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> enemyImpactSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> bloodDecal_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> paintingTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> peaceBannerTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> ceilingLampTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> lampLightTexture_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> storagePlantSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> foundryPlantSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> labsPlantSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> archivePlantSprite_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> titleBackground_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> hudSpriteBatch_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> hudPixel_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> sidearmView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> knifeView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> repeaterView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyWeaponView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> knifeAttackView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> sidearmAttackView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> repeaterAttackView_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> heavyWeaponAttackView_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> shotSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> knifeSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> pickupSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> ammoPickupSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> doorSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> lockedSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> hurtSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> enemyDefeatedSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> terminalSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> guardShotSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> enemyImpactSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> secretSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> guardAlertSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> houndAlertSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> houndAttackSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> extraLifeSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> exitSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> completionFanfareSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffect> ambientSound_;
        std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> ambientInstance_;

        LevelDefinition level_{LevelDefinition::LoadFromFile("assets/levels/starter.level")};
        World world_{level_};
        ExplorationMap exploration_{level_};
        Microsoft::Xna::Framework::Vector3 playerPosition_;

        float yaw_ = 0.0f;
        int health_ = 100;
        int ammo_ = 12;
        int score_ = 0;
        int lives_ = 3;
        int nextExtraLifeScore_ = 40000;
        int levelIndex_ = 0;
        int selectedLevelIndex_ = 0;
        int highestUnlockedLevel_ = 0;
        float levelElapsedSeconds_ = 0.0f;
        bool hasSecurityCard_ = false;
        bool completed_ = false;
        enum class Screen
        {
            Splash,
            Title,
            SectorSelect,
            Difficulty,
            Controls,
            Playing,
            Map,
            Paused,
            GameOver
        };
        enum class Difficulty
        {
            Scout,
            Operative,
            Veteran
        };
        Screen screen_ = Screen::Splash;
        Difficulty difficulty_ = Difficulty::Operative;
        int menuSelection_ = 0;
        int pauseMenuSelection_ = 0;
        enum class Weapon { Knife, Sidearm, Repeater, HeavyAutomatic };
        Weapon weapon_ = Weapon::Sidearm;
        Weapon lastFirearm_ = Weapon::Sidearm;
        bool hasRepeater_ = false;
        bool hasHeavyWeapon_ = false;
        bool actionWasDown_ = false;
        bool attackWasDown_ = false;
        bool fullScreenWasDown_ = false;
        bool pauseWasDown_ = false;
        bool ilmWasDown_ = false;
        bool goalCheatWasDown_ = false;
        bool upWasDown_ = false;
        bool downWasDown_ = false;
        bool confirmWasDown_ = false;
        bool escapeWasDown_ = false;
        bool mouseWasDown_ = false;
        int soundVolumeStep_ = 4;
        int fieldOfViewDegrees_ = 72;
        float cheatMessageSeconds_ = 0.0f;
        std::string objectiveMessage_;
        float objectiveMessageSeconds_ = 0.0f;
        float weaponFlashSeconds_ = 0.0f;
        float playerImpactFlashSeconds_ = 0.0f;
        float playerFireCooldownSeconds_ = 0.0f;

        static constexpr float PlayerRadius = 0.22f;
        static constexpr float WalkSpeed = 2.4f;
        static constexpr float RunSpeedMultiplier = 1.65f;
        static constexpr float KeyboardTurnSpeed = 1.65f;
        static constexpr int MaxAmmo = 99;

        void HandleInput(float elapsedSeconds);
        void HandleMenuInput();
        void TryMove(float dx, float dz);
        void ResetRun();
        void LoadCampaignLevel(int index);
        void AdvanceCampaign();
        void UnlockNextLevel();
        void CompleteLevel();
        void SaveCampaignProfile() const;
        void AwardScore(int points);

        [[nodiscard]] float DamageMultiplier() const;

        [[nodiscard]] Microsoft::Xna::Framework::Vector3 LookDirection() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix ViewMatrix() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix ProjectionMatrix();

        void CreateProceduralAtlas();
        void CreateProceduralBloodDecal();
        void CreateProceduralDecorationTextures();
        void CreateProceduralEnemyImpactTexture();
        void CreateHudResources();
        void CreateSoundEffects();
        void DrawHud();
        void DrawAutomap();
        void DrawMenu();
    };
}
