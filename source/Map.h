#ifndef MAP_H
#define MAP_H

#include "Player.h"
#include "Stage.h"

#include <array>
#include <random>
#include <vector>

class Map {
public:
    enum class Result { Playing, Cleared, Lost, Finished, MissingAssets };
    enum { BaseFloor = 184, StageCount = 8 };

    Map() = default;
    explicit Map(SDL_Renderer* renderer);
    ~Map();

    void NewGame(int selectedStage, int players);
    void StartDemo();
    void Continue();
    void NextStage();
    void SetInput(int index, const Player::Input& input);
    void Draw(SDL_Renderer* renderer);
    void Update();
    void PlayStageMusic() const;
    void loadGameData(SDL_Renderer* renderer);
    void loadLVL();
    void LoadJungle();
    void LoadMazeBaseA();
    void LoadMazeWaterFall();
    void LoadMazeBaseB();
    void LoadMazeSnowField();
    void LoadMazeEnergyZone();
    void LoadMazeHangar();
    void LoadMazeAliensLair();
    int getStartBlock() const;
    int getEndBlock() const;

    Result GetResult() const { return result; }
    int StageNumber() const { return stageNumber; }
    const char* StageName() const;
    int Score() const { return players[0].score; }
    int Lives() const { return players[0].lives; }
    int HighScore() const { return highScore; }
    int Continues() const { return continues; }

    bool IsBase() const { return stage.IsBase(); }
    bool HasBaseGate() const { return IsBase() && room + 1 < stage.RoomCount(); }
    int BaseGroundY() const { return HasBaseGate() ? BaseFloor : 204; }
    bool BaseGateOpen() const { return HasBaseGate() && requiredRemaining == 0; }
    bool IsWater(int worldX, int worldY) const;
    bool CanDrop(float worldX, float feet) const;
    bool BelowView(float feet) const;
    float MovePlayerX(float worldX, float feet, float movement) const;
    bool HitsCeiling(float worldX, float feet) const;
    float FindFloor(float worldX, float currentY, float nextY, bool dropping, bool canSwim) const;
    bool Fire(int owner, float worldX, float worldY, int aimX, int aimY, Player::Weapon weapon, bool rapid);

private:
    enum class Role { Runner, Rifleman, Gun, Turret, ShoulderCannon, RedTurret, BombTurret, WallBox, Capsule, Platform, Bridge,
                      Hazard, RockCave, Boulder, Core, Generator, BaseSoldier, Roller, Alien, Tank, Boss };
    struct Enemy {
        Role role;
        int type, attribute, hp, timer, age, room, direction, trigger;
        float worldX, worldY, anchorX, anchorY, velocityY;
        bool active, required, alive;
        bool grounded;
        int strideFrame;
        int retractAge;
        SDL_Point demoAim;
        int riflemanAim;
        std::array<unsigned, 2> lastLaserHits;
        int defeatAge;
        Player::Weapon finishingWeapon;
        float nextRockFloor;
        int baseActionAge;
        float RideHeight() const { return worldY - 16; }
        bool RecoilsOnDefeat() const;
        SDL_Rect Bounds(bool indoor, int stageNumber) const;
        SDL_Rect BridgeSection(int index) const;
        int CollapsedBridgeSections() const;
    };
    struct Shot {
        float worldX, worldY, velocityX, velocityY;
        int owner, damage, life, age, sprite;
        Player::Weapon weapon;
        float gravity;
        unsigned groupId;
        int launchDelay, orbitDirection;
        float orbitPhase;
        int slot, impactTimer;
        float LaserTailFrames(float ageInFrames) const;
        float PositionY(bool indoor) const;
    };
    struct Pickup { float worldX, worldY, velocityX, velocityY; int type, life; };
    struct Burst { float worldX, worldY; int age; };
    struct JungleExit {
        int age = 0;
        int entranceX = 0;
        SDL_Point machine{};
        SDL_Rect passage{};
        bool clearCueStarted = false;
        std::array<bool, 2> jumped{}, entered{};
    };

    void LoadStage(int number);
    void StartRoom();
    void RespawnPlayer(int index, bool initial);
    void UpdateDefeatedPlayers();
    int ScrollPosition() const;
    int ScrollLimit() const;
    int HorizontalScrollPoint() const;
    void UpdateCamera();
    void UpdatePlatforms();
    void ActivateSpawns();
    void GenerateSoldier(std::vector<Stage::Spawn>& generated);
    void SpawnDemoEnemies();
    void UpdateEnemies();
    bool UpdateBaseSoldier(Enemy& enemy, int target);
    void GenerateBaseWave(Enemy& generator, std::vector<Stage::Spawn>& generated);
    void UpdateBridge(Enemy& bridge);
    bool UpdateBoulder(Enemy& rock);
    void UpdateShots();
    void UpdatePickups();
    void PopulateEnemies();
    void AddEnemy(const Stage::Spawn& spawn, bool generated = false);
    StageSprites::RiflemanPose GetRiflemanPose(const Enemy& enemy) const;
    void EnemyShot(const Enemy& enemy, float targetX, float targetY, int spread = 0, int sprite = 0x1e);
    void Damage(Enemy& enemy, int owner, int damage, Player::Weapon weapon = Player::Rifle);
    void UpdateEnemyDefeat(Enemy& enemy);
    void FinishEnemyDefeat(Enemy& enemy);
    void DrawBridgeGaps(SDL_Renderer* renderer) const;
    void DrawEnemies(SDL_Renderer* renderer) const;
    void DrawCombat(SDL_Renderer* renderer) const;
    void DrawHUD(SDL_Renderer* renderer) const;
    void DrawBaseGate(SDL_Renderer* renderer) const;
    void StartJungleExit(const Enemy& boss);
    void UpdateJungleExit();
    int ClosestPlayer(float worldX, float worldY) const;
    bool OnScreen(float worldX, float worldY, int margin = 0) const;
    bool BridgeOpen(int worldX, int worldY) const;
    char TerrainAt(int worldX, int worldY) const;
    int CameraX() const { return IsBase() ? room * Stage::ViewWidth : cameraX; }
    void SetCleared();
    void StopBossAlarm();

    SDL_Renderer* renderer = nullptr;
    SDL_Texture* gameView = nullptr;
    Stage stage;
    StageSprites sprites;
    bool spritesReady = false;
    Result result = Result::MissingAssets;
    int stageNumber = 1, playerCount = 1, continues = 3, highScore = 20000;
    int cameraX = 0, cameraY = 0, room = 0, frame = 0, clearTimer = 0, requiredRemaining = 0;
    int roomTimer = 0, soldierTimer = 0;
    int soldierPage = 0, soldiersOnPage = 0, previousScroll = 0;
    bool arenaScrolling = false;
    bool bossAlarm = false;
    JungleExit jungleExit;
    bool demo = false;
    int demoSpawnTimer = 0, demoWave = 0, demoAimIndex = 0;
    unsigned nextShotGroup = 0;
    std::array<SDL_Point, 6> demoAims{{{-1, 0}, {-1, -1}, {1, -1}, {1, 0}, {1, 1}, {-1, 1}}};
    std::mt19937 demoRandom;
    std::array<Player, 2> players;
    std::array<Player::Input, 2> inputs;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<Pickup> pickups;
    std::vector<Burst> bursts;
};

#endif
