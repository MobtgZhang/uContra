#include "Map.h"
#include "CFG.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
const int PickupFrames[] = {0x33, 0x34, 0x31, 0x2f, 0x32, 0x30, 0x4e, 0x4e};
const Music::eChunk ShotSounds[] = {Music::cN_GUN, Music::cM_GUN, Music::cF_GUN, Music::cS_GUN, Music::cL_GUN};
const int OutdoorBoss[] = {0x11, -1, 0x14, -1, 0x14, 0x13, 0x16, 0x16};
constexpr int RiflemanFootOffset = 16;
constexpr int PlayerTargetHeight = 14;
constexpr int BridgeSections = 4;
constexpr int BridgeSectionSize = 32;
constexpr int BridgeSectionInterval = 56;
constexpr int BridgeBlastInterval = 8;
constexpr int BridgeCollapseDelay = 3 * BridgeBlastInterval;
constexpr int PickupBottom = 8;
constexpr int TankWidth = 96;
constexpr int TankHeight = 64;
constexpr int TankFirstFrame = 0xe0;
constexpr int TankMuzzleX = -46;
constexpr int TankMuzzleY = -52;
constexpr int TurretSize = 32;
constexpr int TurretFirstFrame = 0xe4;
constexpr int TurretOpeningStep = 8;
constexpr int TurretOpenAt = 3 * TurretOpeningStep;
constexpr int TurretFirstShotDelay = 8;
constexpr int TurretTurnInterval = 18;
constexpr int TurretDirections = 8;
constexpr float TurretAngleStep = 0.7853981634f;
constexpr float TurretMuzzleDistance = 13.0f;
constexpr int WallBoxFirstFrame = 0xee;
constexpr int WallBoxClosed = 40;
constexpr int WallBoxStep = 8;
constexpr int WallBoxOpen = 64;
constexpr int RedTurretFirstFrame = 0xf1;
constexpr int RedTurretStep = 8;
constexpr int RedTurretOpenAt = 3 * RedTurretStep;
constexpr int BombTurretFirstFrame = 0xf9;
constexpr int BombTurretRecoil = 8;
const SDL_Point AimDirections[] = {{-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}};
constexpr float JungleCapsuleSpeed = 1.125f;
constexpr float JungleWeaponCapsuleSpeed = 1.375f;
constexpr float JungleCapsuleAmplitude = 22.0f;
constexpr float JungleCapsuleFrequency = 0.04f;
constexpr int AmbushStep = 8;
constexpr int AmbushShotInterval = 24;
constexpr int BushRiflemanBottom = 10;
constexpr int BushRiflemanHeight = 16;
constexpr int BushRiflemanMuzzleY = BushRiflemanBottom - 8;
constexpr int FortressRiflemanCoverInset = 9;

struct RiflemanAmbush {
    int initialWait, repeatWait, shotDelay, shotCount;

    int FirstShot() const { return 2 * AmbushStep + shotDelay; }
    int Retreat() const { return FirstShot() + shotCount * AmbushShotInterval + 128; }
    int Phase(int age) const {
        const int elapsed = age - initialWait;
        return elapsed < 0 ? elapsed : elapsed % (Retreat() + 2 * AmbushStep + repeatWait);
    }
    bool Exposed(int age) const {
        const int phase = Phase(age);
        return phase >= 2 * AmbushStep && phase < Retreat();
    }
    bool Fires(int age) const {
        const int firing = Phase(age) - FirstShot();
        return firing >= 0 && firing < shotCount * AmbushShotInterval && firing % AmbushShotInterval == 0;
    }
    int Frame(int age) const {
        const int phase = Phase(age);
        if (phase < 0 || phase >= Retreat() + 2 * AmbushStep) return 0x44;
        if (phase < AmbushStep || phase >= Retreat() + AmbushStep) return 0x45;
        return 0x46;
    }
};

const RiflemanAmbush* JungleAmbush(int stageNumber, int attribute) {
    static const RiflemanAmbush bush{48, 96, 4, 1};
    static const RiflemanAmbush fortress{128, 128, 16, 3};
    if (stageNumber != 1) return nullptr;
    return attribute == 1 ? &bush : attribute == 2 ? &fortress : nullptr;
}

bool Overlaps(const SDL_Rect& bounds, float worldX, float worldY, int radius) {
    const SDL_Rect point{int(worldX) - radius, int(worldY) - radius, radius * 2 + 1, radius * 2 + 1};
    return SDL_HasIntersection(&bounds, &point);
}

int WallBoxFrame(int age, float screenX) {
    if (screenX <= 24) return 0;
    const int phase = age % (WallBoxClosed + 2 * WallBoxStep + WallBoxOpen);
    if (phase < WallBoxClosed) return 0;
    if (phase < WallBoxClosed + WallBoxStep || phase >= WallBoxClosed + WallBoxStep + WallBoxOpen) return 1;
    return 2;
}

float PlatformX(float origin, int attribute, int frame) {
    static const float left[] = {80, 112, 72, 72};
    static const float right[] = {176, 192, 184, 184};
    static const float speed[] = {-0.5f, 0.75f, -0.5f, 0.5f};
    const int variant = attribute & 3;
    const float span = right[variant] - left[variant];
    const float offset = std::max(0.0f, std::min(origin - left[variant], span));
    const float start = speed[variant] < 0 ? 2 * span - offset : offset;
    const float phase = std::fmod(start + std::abs(speed[variant]) * frame, 2 * span);
    return left[variant] + std::min(phase, 2 * span - phase);
}
}

SDL_Rect Map::Enemy::Bounds(bool indoor, int stageNumber) const {
    int width = 16, height = 16;
    switch (role) {
        case Role::Runner: width = 12; height = 22; break;
        case Role::Rifleman:
            if (stageNumber == 1 && attribute == 1) {
                return {int(worldX) - 8, int(worldY) + BushRiflemanBottom - BushRiflemanHeight, 16, BushRiflemanHeight};
            }
            width = 16;
            height = RiflemanFootOffset * 2;
            break;
        case Role::Turret: width = 24; height = 24; break;
        case Role::RedTurret: width = 25; height = 24; break;
        case Role::WallBox: width = height = 16; break;
        case Role::BombTurret: return {int(worldX) - 12, int(worldY) - 7, 16, 14};
        case Role::Tank:
            return {int(worldX) - TankWidth / 2, int(worldY) - TankHeight, TankWidth, TankHeight};
        case Role::Hazard:
            if (stageNumber == 6) {
                width = type == 0x10 ? 54 : 13;
                height = type == 0x10 ? 12 : 48;
            } else if (stageNumber == 7 && type != 0x10) {
                const int length = type == 0x11 ? 44 : (attribute & 2) ? 122 : 78;
                return {int(worldX) - 10, int(worldY) - length, 20, length + 6};
            }
            break;
        case Role::Boss:
            width = indoor ? 32 : stageNumber == 6 ? 40 : 28;
            height = stageNumber == 6 ? 52 : stageNumber == 8 ? 40 : 30;
            break;
        default: break;
    }
    return {int(worldX) - width / 2, int(worldY) - height / 2, width, height};
}

SDL_Rect Map::Enemy::BridgeSection(int index) const {
    return {int(anchorX) - BridgeSectionSize / 2 + index * BridgeSectionSize,
            int(anchorY) - BridgeSectionSize, BridgeSectionSize, BridgeSectionSize};
}

int Map::Enemy::CollapsedBridgeSections() const {
    if (age <= BridgeCollapseDelay) return 0;
    return std::min(1 + (age - 1 - BridgeCollapseDelay) / BridgeSectionInterval, BridgeSections);
}

void Map::PopulateEnemies() {
    requiredRemaining = 0;
    for (const auto& spawn : stage.Spawns()) {
        AddEnemy(spawn);
    }
    if (IsBase()) {
        StartRoom();
    } else {
        for (const auto& enemy : enemies) {
            requiredRemaining += enemy.required;
        }
    }
}

void Map::AddEnemy(const Stage::Spawn& spawn, bool generated) {
    const int type = spawn.type;
    Role role = Role::Runner;
    bool required = false;
    int hp = 1;
    if (IsBase()) {
        switch (type) {
            case 0x08: case 0x13: role = Role::Gun; hp = 5; break;
            case 0x0a: role = Role::Core; hp = 4; break;
            case 0x14: role = Role::Core; hp = (spawn.attribute & 8) ? 16 : 8; required = true; break;
            case 0x19: case 0x1a: case 0x20: role = Role::Generator; break;
            case 0x11: role = Role::Roller; hp = 2; break;
            case 0x10: case 0x1c:
                role = Role::Boss; hp = type == 0x10 ? 16 : 10; required = true; break;
            case 0x15: case 0x16: case 0x17: case 0x18: break;
            default: return;
        }
    } else if (type == OutdoorBoss[stageNumber - 1]) {
        static const int BossHealth[] = {32, 0, 32, 0, 36, 64, 32, 55};
        role = Role::Boss;
        required = true;
        hp = BossHealth[stageNumber - 1];
    } else if (type == 2) {
        role = Role::WallBox;
    } else if (type == 3) {
        role = Role::Capsule;
    } else if (type == 6) {
        role = Role::Rifleman;
    } else if (stageNumber == 1 && type == 4) {
        role = Role::Turret;
        hp = 8;
    } else if (stageNumber == 1 && type == 7) {
        role = Role::RedTurret;
        hp = 8;
    } else if (stageNumber == 1 && type == 0x10) {
        role = Role::BombTurret;
        hp = 16;
    } else if (type == 4 || type == 7 || type == 0x0e || (stageNumber == 7 && type == 0x17)) {
        role = Role::Gun;
        hp = type == 4 ? 8 : 6;
    } else if (type == 5 || type == 0x0c) {
        role = Role::Runner;
    } else if (stageNumber == 1 && type == 0x12) {
        role = Role::Bridge;
    } else if (stageNumber == 3 && type == 0x10) {
        role = Role::Platform;
    } else if (stageNumber == 5 && type == 0x12) {
        role = Role::Tank;
        hp = 24;
    } else if ((stageNumber == 7 && (type == 0x13 || type == 0x18)) || (stageNumber == 5 && type == 0x10)) {
        role = Role::Generator;
    } else if (stageNumber == 7 && (type == 0x14 || type == 0x15)) {
        role = Role::Roller;
        hp = 3;
    } else if (stageNumber == 8 && type >= 0x10) {
        role = Role::Alien;
        hp = type == 0x10 ? 28 : type == 0x15 ? 6 : 2;
    } else if (type >= 0x10) {
        role = Role::Hazard;
        hp = stageNumber == 7 && type != 0x10 ? 16 : 3;
    } else {
        return;
    }
    const float worldX = float(spawn.worldX);
    const bool groundMount = role == Role::Turret || role == Role::RedTurret || (role == Role::WallBox && stageNumber == 1);
    const float worldY = float(spawn.worldY - (groundMount ? TurretSize / 2 : 0));
    const int timer = role == Role::Turret ? TurretOpenAt + TurretFirstShotDelay : role == Role::RedTurret ? RedTurretOpenAt + 40 :
                      role == Role::BombTurret ? 96 : type == 5 && !IsBase() ? 16 : 40 + spawn.worldX % 55;
    const int direction = role == Role::Turret || role == Role::RedTurret ? 0 : type == 5 && (spawn.attribute & 1) ? 1 : -1;
    enemies.push_back({role, type, spawn.attribute, hp, timer,
                       0, spawn.room, direction,
                       required && !IsBase() ? std::min(spawn.trigger, ScrollLimit()) : spawn.trigger,
                       worldX, worldY, worldX, worldY, 0, generated, required, true, generated && type == 5, 0, -1, -1});
}

bool Map::Fire(int owner, float worldX, float worldY, int aimX, int aimY, Player::Weapon weapon, bool rapid) {
    if (!rapid) {
        unsigned previousVolley = 0;
        int volleys = 0;
        for (const auto& shot : shots) {
            if (shot.owner != owner || shot.life <= 0 || shot.volley == previousVolley) continue;
            previousVolley = shot.volley;
            if (++volleys == 2) return false;
        }
    }
    const int pellets = weapon == Player::Spread ? 5 : 1;
    const unsigned volley = ++nextVolley;
    const float heading = std::atan2(float(aimY), float(aimX));
    const float speed = weapon == Player::Fireball ? (rapid ? 2.0f : 1.5f) :
                        weapon == Player::Laser ? (rapid ? 5.0f : 4.0f) : (rapid ? 4.0f : 3.0f);
    const int sprite = weapon == Player::Fireball ? 0x79 : weapon == Player::Laser ? 0x99 :
                       (weapon == Player::MachineGun || weapon == Player::Spread) ? 0x1f : 0x1e;
    for (int pellet = 0; pellet < pellets; ++pellet) {
        const float angle = heading + (pellet - pellets / 2) * (IsBase() ? 0.09f : 0.16f);
        shots.push_back({worldX, worldY, speed * std::cos(angle), speed * std::sin(angle), owner,
                         weapon == Player::Laser ? 4 : weapon == Player::Fireball ? 2 : 1, 200, 0,
                         sprite, weapon, 0, volley});
    }
    CCFG::getMusic()->PlayChunk(ShotSounds[weapon]);
    return true;
}

void Map::EnemyShot(const Enemy& enemy, float targetX, float targetY, int spread, int sprite) {
    float worldX = enemy.worldX;
    float worldY = enemy.worldY;
    if (enemy.role == Role::Tank) {
        worldX += TankMuzzleX;
        worldY += TankMuzzleY;
    } else if (enemy.role == Role::Rifleman && stageNumber == 1 && enemy.attribute == 1) {
        worldX += enemy.direction * 11;
        worldY += BushRiflemanMuzzleY;
        if (enemy.demoAim < 0) {
            targetX = worldX + enemy.direction;
            targetY = worldY;
        }
    }
    float angle = std::atan2(targetY - worldY, targetX - worldX) + spread * 0.18f;
    if (enemy.role == Role::Turret) {
        angle = (enemy.direction + TurretDirections / 2) * TurretAngleStep;
        worldX += std::cos(angle) * TurretMuzzleDistance;
        worldY += std::sin(angle) * TurretMuzzleDistance;
    }
    if (enemy.role == Role::RedTurret) {
        static const float headings[] = {-4.0f, -3.0f, -2.5f};
        angle = headings[enemy.direction] * TurretAngleStep;
        worldX += std::cos(angle) * TurretMuzzleDistance;
        worldY += std::sin(angle) * TurretMuzzleDistance;
    }
    if (enemy.role == Role::BombTurret) {
        const float speed = 1.0f + (demo ? demoRandom() % 4 : std::rand() % 4) * 0.32f;
        shots.push_back({worldX - 8, worldY, -speed, -2.8f, -1, 1, 180, 0, 0x21, Player::Rifle, 0.10f, 0});
        return;
    }
    const float speed = sprite == 0x21 ? 1.75f : 2.1f;
    shots.push_back({worldX, worldY, std::cos(angle) * speed,
                     sprite == 0x21 && !IsBase() ? -3.2f : std::sin(angle) * speed,
                     -1, 1, 180, 0, sprite, Player::Rifle, sprite == 0x21 && !IsBase() ? 0.08f : 0.0f, 0});
}

void Map::UpdatePlatforms() {
    if (!stage.IsVertical()) return;
    for (auto& platform : enemies) {
        if (!platform.active || !platform.alive || platform.role != Role::Platform) continue;
        const float previousX = platform.worldX;
        platform.worldX = PlatformX(platform.anchorX, platform.attribute, ++platform.age);
        for (auto& player : players) {
            if (player.Active() && player.worldY == platform.RideHeight() && std::abs(player.worldX - previousX) <= 15) {
                player.worldX = MovePlayerX(player.worldX, player.worldY, platform.worldX - previousX);
            }
        }
    }
}

void Map::ActivateSpawns() {
    const int scroll = ScrollPosition();
    for (auto& enemy : enemies) {
        if (enemy.active || !enemy.alive || (IsBase() ? enemy.room != room : enemy.trigger > scroll)) continue;
        enemy.active = true;
        if (!IsBase() && enemy.type == 5) {
            if (enemy.direction == 1 && !stage.IsVertical()) enemy.worldX = float(cameraX + 10);
            const float floor = FindFloor(enemy.worldX, enemy.worldY, enemy.worldY + 16, false, false);
            if (!std::isfinite(floor)) enemy.alive = false;
            else {
                enemy.worldY = floor - 11;
                enemy.grounded = true;
            }
        } else if (enemy.role == Role::Rifleman) {
            const float floor = FindFloor(enemy.worldX, enemy.worldY - Stage::TileSize, enemy.worldY + Stage::TileSize, false, false);
            if (std::isfinite(floor)) enemy.worldY = floor - RiflemanFootOffset;
        } else if (enemy.role == Role::Tank) {
            const float floor = FindFloor(enemy.worldX, enemy.worldY, cameraY + Stage::ViewHeight - 1, false, false);
            if (std::isfinite(floor)) enemy.worldY = floor;
        } else if (!IsBase() && enemy.type == 3) {
            if (stage.IsVertical()) {
                enemy.worldX += 32;
                enemy.worldY = cameraY + Stage::ViewHeight;
            } else {
                enemy.anchorX = enemy.worldX = cameraX + 16;
                enemy.worldY += stageNumber == 1 ? JungleCapsuleAmplitude : 32.0f;
            }
        }
    }
}

void Map::GenerateSoldier(std::vector<Stage::Spawn>& generated) {
    static const int Delays[] = {144, 0, 216, 0, 208, 200, 192, 0};
    static const bool SpawnFromLeft[] = {false, true, false, false, true, false, true, false,
                                        true, true, true, false, false, true, true, false};
    static const int Attributes[][4] = {{0, 0, 0, 0}, {0, 0, 0, 4}, {0, 0, 4, 4}, {0, 4, 4, 4},
                                         {4, 4, 4, 4}, {0, 0, 0, 8}, {0, 0, 4, 8}, {0, 0, 4, 8}};
    const int delay = Delays[stageNumber - 1];
    if (!delay) return;
    const int scroll = ScrollPosition();
    const bool scrolling = scroll != previousScroll;
    const int page = scroll / (stage.IsVertical() ? 240 : 256);
    if (soldierPage != page) {
        soldierPage = page;
        soldiersOnPage = 0;
    }
    if (soldierTimer <= 0) soldierTimer = delay;
    soldierTimer -= scrolling && (frame & 1) ? 1 : 2;
    if (soldierTimer > 0) return;
    const int profile = stage.SoldierProfile(page);
    const int chance = (profile & 128) ? 2 : (profile & 64) ? 4 : 1;
    if (profile == 255 || std::rand() % chance != 0) return;

    const int target = ClosestPlayer(cameraX + 128, cameraY + 112);
    if (target < 0 || std::count_if(enemies.begin(), enemies.end(), [](const Enemy& enemy) {
            return enemy.type == 5 && enemy.active && enemy.alive;
        }) >= 7) return;
    const auto& player = players[target];
    const bool left = (stageNumber != 1 || soldiersOnPage >= 30) && SpawnFromLeft[(frame + std::rand() % 16) % 16];
    const float playerX = player.worldX - cameraX;
    if (soldiersOnPage < 30 && (left ? playerX < 64 : playerX >= 192)) return;
    if (stageNumber == 5 && page >= 17 && left) return;
    const int edgeX = cameraX + (left ? 10 : stageNumber == 1 ? 252 : 250);
    const int columns = stageNumber == 1 ? 7 : 1;
    const int mode = std::rand() % 4;
    const int start = mode == 0 ? 0 : mode == 1 ? 240 : int(player.worldY) - cameraY - (stageNumber == 1 ? 0 : 16);
    for (int step = 0; step < 16; ++step) {
        const int relative = (start + (mode == 0 ? step : -step) * 16 + 256) & 255;
        const int floor = (cameraY + relative) & ~15;
        const int worldY = floor - 16;
        if (worldY < cameraY + (stage.IsVertical() ? 0 : 64) || relative >= Stage::ViewHeight) continue;
        for (int column = 0; column < columns; ++column) {
            const int worldX = edgeX + column * (left ? 4 : -4);
            const int nearX = worldX + (left ? 8 : -8);
            if (TerrainAt(worldX, floor) != '=' || TerrainAt(worldX, floor - 1) == '=' ||
                TerrainAt(worldX, worldY) != '.' || TerrainAt(nearX, worldY) != '.' ||
                std::any_of(players.begin(), players.begin() + playerCount, [worldX](const Player& candidate) {
                    return candidate.Active() && std::abs(candidate.worldX - worldX) < 48;
                })) continue;
            const int attribute = Attributes[profile & 7][std::rand() % 4] | (std::rand() & 2) | (left ? 1 : 0);
            generated.push_back({worldX, floor - 11, 5, attribute, 0, 0});
            ++soldiersOnPage;
            return;
        }
    }
}

void Map::UpdateBridge(Enemy& bridge) {
    if (bridge.CollapsedBridgeSections() == BridgeSections) return;
    if (bridge.age == 0) {
        if (demo ? bridge.anchorX > cameraX + 160 :
            std::none_of(players.begin(), players.begin() + playerCount, [&bridge](const Player& player) {
                return player.Active() && std::abs(player.worldX - bridge.anchorX) < 24;
            })) return;
    }

    const int elapsed = bridge.age++;
    const int phase = elapsed % BridgeSectionInterval;
    if (phase > BridgeCollapseDelay || phase % BridgeBlastInterval != 0) return;

    static const SDL_Point blastOffsets[] = {{-16, 8}, {0, 0}, {16, 8}, {0, 8}};
    const SDL_Rect section = bridge.BridgeSection(elapsed / BridgeSectionInterval);
    const SDL_Point offset = blastOffsets[phase / BridgeBlastInterval];
    bursts.push_back({float(section.x + section.w / 2 + offset.x), float(section.y + offset.y), 0});
    if (phase < BridgeCollapseDelay) {
        CCFG::getMusic()->PlayChunk(Music::cBOMBING_1);
    }
}

void Map::SpawnDemoEnemies() {
    if (demoSpawnTimer-- > 0) return;
    demoSpawnTimer = std::uniform_int_distribution<int>(34, 52)(demoRandom);
    ++demoWave;
    const auto spawnOnLedge = [this](int type, int attribute) {
        constexpr int maxActors = 18;
        if (std::count_if(enemies.begin(), enemies.end(), [this](const Enemy& enemy) {
                return enemy.alive && enemy.active && OnScreen(enemy.worldX, enemy.worldY);
            }) >= maxActors) return false;
        for (int attempt = 0; attempt < 36; ++attempt) {
            const int worldX = cameraX + (type == 5 ? (attribute ? 28 : 231) :
                                std::uniform_int_distribution<int>(58, 212)(demoRandom));
            const int floor = std::uniform_int_distribution<int>(5, 25)(demoRandom) * Stage::TileSize;
            if (TerrainAt(worldX, floor) != '=' || TerrainAt(worldX, floor - 8) != '.' ||
                TerrainAt(worldX - 6, floor) != '=' || TerrainAt(worldX + 6, floor) != '=') continue;
            const int worldY = type == 5 ? floor - 11 : type == 6 ? floor - (attribute ? BushRiflemanBottom : RiflemanFootOffset) : floor;
            if (std::any_of(enemies.begin(), enemies.end(), [worldX, worldY](const Enemy& enemy) {
                    return enemy.alive && std::abs(enemy.worldX - worldX) < 26 && std::abs(enemy.worldY - worldY) < 26;
                })) continue;
            AddEnemy({worldX, worldY, type, attribute, 0, 0}, true);
            return true;
        }
        return false;
    };
    if (spawnOnLedge(6, 0)) {
        if (demoAimIndex == TurretDirections) {
            std::shuffle(demoAims.begin(), demoAims.end(), demoRandom);
            demoAimIndex = 0;
        }
        enemies.back().demoAim = demoAims[demoAimIndex++];
    }
    spawnOnLedge(5, demoRandom() & 1);
    if (demoWave % 2 == 0) spawnOnLedge(6, 1);
    if (demoWave % 4 == 0) spawnOnLedge(4, 0);
}

void Map::UpdateEnemies() {
    std::vector<Stage::Spawn> generated;
    for (auto& enemy : enemies) {
        if (!enemy.active || !enemy.alive || enemy.role == Role::Platform || (IsBase() && enemy.room != room)) {
            continue;
        }
        if (enemy.role == Role::Bridge) {
            UpdateBridge(enemy);
            continue;
        }
        const int target = ClosestPlayer(enemy.worldX, enemy.worldY);
        const bool hasTarget = target >= 0 || demo;
        if (!hasTarget && enemy.role != Role::Runner && enemy.role != Role::WallBox &&
            (enemy.role != Role::Capsule || enemy.type != 3)) {
            continue;
        }
        float targetX = target >= 0 ? players[target].worldX : demo ? cameraX + 56.0f : enemy.worldX;
        float targetY = target >= 0 ? players[target].worldY - PlayerTargetHeight : demo ? enemy.worldY - 24 : enemy.worldY;
        if (enemy.demoAim >= 0) {
            const auto aim = AimDirections[enemy.demoAim];
            targetX = enemy.worldX + 64 * aim.x;
            targetY = enemy.worldY + 64 * aim.y;
        }
        const int toward = targetX < enemy.worldX ? -1 : 1;
        ++enemy.age;
        const auto* ambush = enemy.role == Role::Rifleman ? JungleAmbush(stageNumber, enemy.attribute) : nullptr;
        const bool attack = hasTarget && !ambush && --enemy.timer <= 0;
        bool contact = false;
        switch (enemy.role) {
            case Role::Runner:
            case Role::Roller: {
                if (hasTarget && enemy.age == 1 && (enemy.type != 5 || IsBase())) {
                    enemy.direction = toward;
                }
                const float speed = enemy.type == 5 && !IsBase() ? (enemy.direction > 0 ? 1.25f : 1.0f) :
                    enemy.role == Role::Roller ? 1.65f : 1.18f;
                enemy.worldX += enemy.direction * speed;
                if (IsBase()) {
                    enemy.worldY += enemy.role == Role::Roller ? 1.1f : 0.77f;
                    if (enemy.worldX < 32 || enemy.worldX > 224) enemy.direction *= -1;
                    if (enemy.worldY > 222) enemy.alive = false;
                } else if (enemy.role == Role::Runner && enemy.type != 0x0c) {
                    const float feet = enemy.worldY + 11;
                    const float floor = FindFloor(enemy.worldX, feet, feet + enemy.velocityY, false, false);
                    if (enemy.velocityY >= 0 && std::isfinite(floor)) {
                        enemy.worldY = floor - 11;
                        enemy.velocityY = 0;
                        enemy.strideFrame = enemy.grounded ? (enemy.strideFrame + 1) % 48 : 0;
                        enemy.grounded = true;
                    } else {
                        if (enemy.grounded) enemy.velocityY = -1.65f;
                        enemy.grounded = false;
                        enemy.worldY += enemy.velocityY;
                        enemy.velocityY = std::min(enemy.velocityY + 0.19f, 6.0f);
                        if (BelowView(enemy.worldY + 11)) enemy.alive = false;
                    }
                }
                const bool armed = enemy.type != 5 || (stageNumber != 1 && (enemy.attribute & 12));
                if (attack && enemy.role == Role::Runner && armed &&
                    OnScreen(enemy.worldX, enemy.worldY)) {
                    EnemyShot(enemy, targetX, targetY);
                    enemy.timer = 105;
                }
                contact = true;
                break;
            }
            case Role::Rifleman:
            case Role::Gun:
                if (ambush) {
                    enemy.direction = toward;
                    contact = ambush->Exposed(enemy.age);
                    if (ambush->Fires(enemy.age) && OnScreen(enemy.worldX, enemy.worldY)) {
                        EnemyShot(enemy, targetX, targetY);
                    }
                    break;
                }
                enemy.direction = toward;
                if (attack && OnScreen(enemy.worldX, enemy.worldY)) {
                    const bool mortar = stageNumber == 7 && enemy.type == 0x17;
                    EnemyShot(enemy, targetX, targetY, 0, mortar ? 0x21 : 0x1e);
                    enemy.timer = mortar ? 115 : 75;
                }
                contact = enemy.role == Role::Rifleman;
                break;
            case Role::Turret: {
                if (enemy.age <= TurretOpenAt || !OnScreen(enemy.worldX, enemy.worldY)) break;
                const int halfTurn = TurretDirections / 2;
                const bool mayTurn = (enemy.age - TurretOpenAt) % TurretTurnInterval == 0;
                if (demo) {
                    if (mayTurn) {
                        enemy.direction = (enemy.direction + 1) % TurretDirections;
                        EnemyShot(enemy, targetX, targetY);
                    }
                    break;
                }
                const int octant = int(std::round(std::atan2(targetY - enemy.worldY, targetX - enemy.worldX) / TurretAngleStep));
                const int aim = (octant + halfTurn + TurretDirections) % TurretDirections;
                const int turn = (aim - enemy.direction + TurretDirections) % TurretDirections;
                if (turn && mayTurn) {
                    enemy.direction = (enemy.direction + (turn <= halfTurn ? 1 : TurretDirections - 1)) % TurretDirections;
                }
                if (attack && enemy.direction == aim) {
                    EnemyShot(enemy, targetX, targetY);
                    enemy.timer = 75;
                }
                break;
            }
            case Role::RedTurret: {
                if (enemy.age < RedTurretOpenAt || !OnScreen(enemy.worldX, enemy.worldY)) break;
                if (enemy.retractAge < 0 && enemy.worldX - cameraX <= 48) enemy.retractAge = enemy.age;
                if (enemy.retractAge >= 0) break;
                const float angle = std::atan2(enemy.worldY - targetY, enemy.worldX - targetX);
                const int aim = angle < 0.39f ? 0 : angle < 1.17f ? 1 : 2;
                if (enemy.age % 16 == 0) enemy.direction += (aim > enemy.direction) - (aim < enemy.direction);
                if (attack) {
                    if (targetX <= enemy.worldX && targetY <= enemy.worldY + 4) EnemyShot(enemy, targetX, targetY);
                    enemy.timer = ++enemy.strideFrame % 3 == 0 ? 80 : 16;
                }
                break;
            }
            case Role::BombTurret:
                if (!attack || !OnScreen(enemy.worldX, enemy.worldY)) break;
                if (enemy.strideFrame == 0) EnemyShot(enemy, targetX, targetY);
                enemy.strideFrame ^= 1;
                enemy.timer = enemy.strideFrame ? BombTurretRecoil : 40;
                break;
            case Role::WallBox:
                if (demo && enemy.age == 72) Damage(enemy, -1, enemy.hp);
                break;
            case Role::Capsule:
                if (enemy.type == 3) {
                    if (stage.IsVertical()) {
                        enemy.worldY -= 1.5f;
                        enemy.worldX = enemy.anchorX + std::cos(enemy.age * 0.088f) * 32.0f;
                    } else if (stageNumber == 1) {
                        const int item = enemy.attribute & 7;
                        enemy.worldX += item <= 1 || item == 3 ? JungleWeaponCapsuleSpeed : JungleCapsuleSpeed;
                        enemy.worldY = enemy.anchorY + std::cos(enemy.age * JungleCapsuleFrequency) * JungleCapsuleAmplitude;
                    } else {
                        enemy.worldX += 1.5f;
                        enemy.worldY = enemy.anchorY + std::cos(enemy.age * 0.088f) * 32.0f;
                    }
                    if (demo && enemy.age == 128) Damage(enemy, -1, enemy.hp);
                }
                break;
            case Role::Bridge:
            case Role::Platform: break;
            case Role::Core:
                break;
            case Role::Generator:
                if (!attack || !OnScreen(enemy.worldX, enemy.worldY, 12)) break;
                if (IsBase()) {
                    generated.push_back({int(enemy.worldX), int(enemy.worldY) + 10, enemy.type == 0x1a ? 0x11 : 0x15, 0, room, 0});
                } else if (stageNumber == 5) {
                    EnemyShot(enemy, targetX, targetY, 0, 0x21);
                } else if (enemy.type == 0x18 && requiredRemaining > 0) {
                    const bool left = targetX - cameraX >= 128;
                    generated.push_back({cameraX + (left ? 10 : 246), int(players[target].worldY) - 11, 5, left, 0, 0});
                } else if (enemy.type == 0x13) {
                    generated.push_back({int(enemy.worldX), int(enemy.worldY), 0x14, 0, 0, 0});
                }
                enemy.timer = 130;
                break;
            case Role::Hazard:
                if (stageNumber == 6) {
                    const int phase = (enemy.age + enemy.attribute * 24) % 150;
                    contact = phase >= 85 && phase < 130;
                } else if (stageNumber == 7) {
                    if (enemy.type == 0x10) {
                        enemy.worldY = enemy.anchorY + (1.0f - std::cos(enemy.age * 0.045f)) * 12.5f;
                    }
                    contact = true;
                } else if (stageNumber == 3) {
                    enemy.worldY = enemy.anchorY + (enemy.age * 2 % 108);
                    enemy.worldX = enemy.anchorX + std::sin(enemy.age * 0.055f) * 16.0f;
                    contact = true;
                    if (enemy.type == 0x15 && attack && OnScreen(enemy.worldX, enemy.worldY)) {
                        EnemyShot(enemy, targetX, targetY, 0, 0x79);
                        enemy.timer = 90;
                    }
                } else {
                    enemy.worldY = enemy.anchorY + std::sin(enemy.age * 0.05f) * 20;
                    contact = true;
                }
                break;
            case Role::Alien:
                if (enemy.type == 0x11 || enemy.type == 0x12 || enemy.type == 0x14) {
                    enemy.worldX += toward * 0.67f;
                    enemy.worldY += std::max(-0.42f, std::min(targetY - enemy.worldY, 0.42f));
                    contact = true;
                } else if (attack && OnScreen(enemy.worldX, enemy.worldY)) {
                    generated.push_back({int(enemy.worldX) + toward * 12, int(enemy.worldY),
                                         enemy.type == 0x10 ? 0x11 : 0x14, 0, 0, 0});
                    enemy.timer = 155;
                }
                break;
            case Role::Tank:
                enemy.worldX = enemy.anchorX + std::sin(enemy.age * 0.02f) * 19.0f;
                if (attack && OnScreen(enemy.worldX, enemy.worldY, TankWidth / 2) && targetX < enemy.worldX) {
                    EnemyShot(enemy, targetX, targetY, 0, 0x21);
                    enemy.timer = 100;
                }
                contact = true;
                break;
            case Role::Boss:
                enemy.direction = toward;
                if (IsBase() && enemy.type == 0x1c) {
                    enemy.worldX = enemy.anchorX + (enemy.anchorX < 128 ? -1 : 1) * std::sin(enemy.age * 0.035f) * 35.0f;
                } else if (stageNumber == 5) {
                    enemy.worldX = enemy.anchorX + std::sin(enemy.age * 0.03f) * 51;
                    enemy.worldY = enemy.anchorY + std::sin(enemy.age * 0.051f) * 24;
                } else if (stageNumber == 6) {
                    enemy.worldX = enemy.anchorX + std::sin(enemy.age * 0.023f) * 61;
                    enemy.worldY = enemy.anchorY - std::max(0.0f, std::sin(enemy.age * 0.046f)) * 56;
                    contact = true;
                }
                if (!attack || !OnScreen(enemy.worldX, enemy.worldY)) break;
                if (stageNumber == 8) {
                    generated.push_back({int(enemy.worldX) - 16, int(enemy.worldY), 0x14, 0, 0, 0});
                    enemy.timer = 92;
                } else if (stageNumber != 1 && stageNumber != 7) {
                    const int sprite = stageNumber == 3 ? 0x79 : stageNumber == 6 ? 0x7b : stageNumber == 5 ? 0x7c : 0x72;
                    for (int spread = -1; spread <= 1; ++spread) EnemyShot(enemy, targetX, targetY, spread * 2, sprite);
                    enemy.timer = stageNumber == 6 ? 80 : 105;
                }
                break;
        }
        if (contact && OnScreen(enemy.worldX, enemy.worldY, 16)) {
            const SDL_Rect bounds = enemy.Bounds(IsBase(), stageNumber);
            for (int index = 0; index < playerCount; ++index) {
                if (players[index].Active()) {
                    const SDL_Rect body = players[index].Bounds();
                    if (SDL_HasIntersection(&body, &bounds)) {
                        players[index].Hit();
                    }
                }
            }
        }
    }
    if (!IsBase() && !demo) GenerateSoldier(generated);
    for (const auto& spawn : generated) {
        if (spawn.worldX >= 0 && spawn.worldX < (IsBase() ? Stage::ViewWidth : stage.Width()) &&
            spawn.worldY > 0 && spawn.worldY < stage.Height()) AddEnemy(spawn, true);
    }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [this](const Enemy& enemy) {
        if (!enemy.active || enemy.required || enemy.role == Role::Bridge ||
            (stageNumber == 7 && (enemy.type == 0x11 || enemy.type == 0x12)) ||
            (stageNumber == 8 && enemy.type == 0x10)) return false;
        if (stageNumber == 1 && (enemy.role == Role::WallBox || enemy.role == Role::RedTurret || enemy.role == Role::BombTurret)) {
            return enemy.worldX < cameraX - 48;
        }
        if (!enemy.alive) return true;
        return !IsBase() && (enemy.worldX < cameraX - 80 || enemy.worldY > cameraY + Stage::ViewHeight + 90 ||
                             (!stage.IsVertical() && enemy.type == 5 && enemy.direction > 0 &&
                              enemy.worldX > cameraX + Stage::ViewWidth + 24) ||
                             (enemy.type == 3 && (enemy.worldX > cameraX + Stage::ViewWidth + 16 ||
                                                  enemy.worldY < cameraY - 16)) ||
                             enemy.worldX > stage.Width() + 35);
    }), enemies.end());
}

void Map::Damage(Enemy& enemy, int owner, int damage) {
    enemy.hp -= damage;
    if (enemy.hp > 0) return;
    enemy.alive = false;
    requiredRemaining -= enemy.required;
    if (owner >= 0) players[owner].AddScore(enemy.role == Role::Boss ? 10000 : enemy.role == Role::Core ? 1000 : 500);
    bursts.push_back({enemy.worldX, enemy.worldY, 0});
    if (enemy.role == Role::Boss) {
        bursts.push_back({enemy.worldX - 13, enemy.worldY - 10, -5});
        bursts.push_back({enemy.worldX + 13, enemy.worldY + 9, -10});
    }
    const bool explosion = enemy.role == Role::Runner || enemy.role == Role::Rifleman || enemy.role == Role::Turret ||
                           enemy.role == Role::RedTurret || enemy.role == Role::BombTurret ||
                           enemy.role == Role::Capsule || enemy.role == Role::WallBox;
    CCFG::getMusic()->PlayChunk(enemy.required ? Music::cBOMBING_2 : explosion ? Music::cBOMBING_0 : Music::cHITSND0);
    if (enemy.role == Role::Capsule || enemy.role == Role::WallBox) {
        const float drift = stage.IsVertical() ? (enemy.worldX < cameraX + Stage::ViewWidth / 2 ? 0.25f : -0.25f) : 0.5f;
        pickups.push_back({enemy.worldX, enemy.worldY, enemy.role == Role::WallBox ? 0.0f : drift,
                           enemy.role == Role::WallBox ? 0.0f : -3.0f, enemy.attribute & 7, 600});
    }
}

void Map::UpdateShots() {
    for (auto& shot : shots) {
        const int steps = 3;
        ++shot.age;
        --shot.life;
        shot.velocityY += shot.gravity;
        for (int step = 0; step < steps && shot.life > 0; ++step) {
            shot.worldX += shot.velocityX / steps;
            shot.worldY += shot.velocityY / steps;
            const float wave = shot.weapon == Player::Fireball && shot.owner >= 0 ? std::sin(shot.age * 0.42f) * 7 : 0;
            if (!OnScreen(shot.worldX, shot.worldY + wave, shot.owner < 0 ? 24 : 0)) {
                shot.life = 0;
            } else if (shot.owner < 0) {
                for (int index = 0; index < playerCount; ++index) {
                    if (players[index].Active() && Overlaps(players[index].Bounds(), shot.worldX, shot.worldY, shot.sprite == 0x1e ? 2 : 5) &&
                        players[index].Hit()) {
                        shot.life = 0;
                        break;
                    }
                }
            } else {
                for (auto& enemy : enemies) {
                    const auto* ambush = enemy.role == Role::Rifleman ? JungleAmbush(stageNumber, enemy.attribute) : nullptr;
                    if (!enemy.alive || !enemy.active || (IsBase() && enemy.room != room) ||
                        enemy.role == Role::Platform || enemy.role == Role::Bridge || enemy.role == Role::Generator ||
                        (enemy.role == Role::Hazard && stageNumber == 6) ||
                        (enemy.role == Role::WallBox && WallBoxFrame(enemy.age, stage.IsVertical() ? 128 : enemy.worldX - cameraX) != 2) ||
                        (enemy.role == Role::RedTurret && (enemy.age < RedTurretOpenAt || enemy.retractAge >= 0)) ||
                        (enemy.role == Role::Turret && enemy.age < TurretOpenAt) || (ambush && !ambush->Exposed(enemy.age)) ||
                        !Overlaps(enemy.Bounds(IsBase(), stageNumber), shot.worldX, shot.worldY + wave, 2)) continue;
                    const bool exposed = enemy.role != Role::Core || !enemy.required || enemy.age >= 45 + (enemy.attribute & 3) * 35;
                    if (exposed) Damage(enemy, shot.owner, shot.damage);
                    if (shot.weapon != Player::Laser || enemy.alive) {
                        shot.life = 0;
                        break;
                    }
                }
            }
        }
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& shot) { return shot.life <= 0; }), shots.end());
}

void Map::UpdatePickups() {
    for (auto& item : pickups) {
        --item.life;
        const float nextX = item.worldX + item.velocityX;
        if (nextX < 0 || nextX >= stage.Width() || TerrainAt(int(nextX), int(item.worldY)) == '#') {
            item.velocityX = -item.velocityX;
        } else {
            item.worldX = nextX;
        }
        const float nextY = item.worldY + item.velocityY;
        const float floor = IsBase() ? BaseFloor - 10.0f :
            FindFloor(item.worldX, item.worldY + PickupBottom, nextY + PickupBottom, false, true) - PickupBottom;
        if (item.velocityY >= 0 && nextY >= floor) {
            item.worldY = floor;
            item.velocityX = item.velocityY = 0;
        } else {
            item.worldY = nextY;
            item.velocityY = std::min(item.velocityY + 0.125f, 4.0f);
        }
        for (int index = 0; index < playerCount; ++index) {
            if (!players[index].Active() || !Overlaps(players[index].Bounds(), item.worldX, item.worldY, 7)) continue;
            players[index].Collect(item.type);
            if (item.type >= 6) {
                for (auto& enemy : enemies) {
                    if (enemy.alive && enemy.active && !enemy.required && enemy.hp > 0 &&
                        (enemy.role == Role::Runner || enemy.role == Role::Rifleman || enemy.role == Role::Gun ||
                         enemy.role == Role::Turret || enemy.role == Role::RedTurret || enemy.role == Role::BombTurret ||
                         enemy.role == Role::Alien) &&
                        OnScreen(enemy.worldX, enemy.worldY)) Damage(enemy, index, enemy.hp);
                }
            }
            item.life = 0;
            break;
        }
    }
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [this](const Pickup& item) {
        return item.life <= 0 || item.worldY > cameraY + Stage::ViewHeight + 36;
    }), pickups.end());
}

bool Map::BridgeOpen(int worldX, int worldY) const {
    const SDL_Point point{worldX, worldY};
    for (const auto& bridge : enemies) {
        if (bridge.role != Role::Bridge) continue;
        const int collapsed = bridge.CollapsedBridgeSections();
        if (collapsed == 0) continue;
        SDL_Rect gap = bridge.BridgeSection(0);
        gap.w *= collapsed;
        if (SDL_PointInRect(&point, &gap)) return true;
    }
    return false;
}

void Map::DrawBridgeGaps(SDL_Renderer* display) const {
    if (stageNumber != 1) return;
    for (const auto& bridge : enemies) {
        if (bridge.role != Role::Bridge) continue;
        const int collapsed = bridge.CollapsedBridgeSections();
        if (collapsed == 0) continue;
        SDL_Rect gap = bridge.BridgeSection(0);
        gap.x -= cameraX;
        gap.y -= cameraY;
        gap.w *= collapsed;
        SDL_SetRenderDrawColor(display, 0, 0, 0, 255);
        SDL_RenderFillRect(display, &gap);
    }
}

void Map::DrawEnemies(SDL_Renderer* display) const {
    for (const auto& enemy : enemies) {
        if ((IsBase() && enemy.room != room) || !enemy.active || !OnScreen(enemy.worldX, enemy.worldY, 48)) continue;
        const int screenX = int(enemy.worldX) - (IsBase() ? 0 : cameraX);
        const int screenY = int(enemy.worldY) - cameraY;
        const bool spikedWall = stageNumber == 7 && (enemy.type == 0x11 || enemy.type == 0x12);
        if (spikedWall) {
            if (!enemy.alive) {
                const int left = int(enemy.anchorX) - 20;
                const int top = enemy.type == 0x11 ? int(enemy.anchorY) - 80 : 0;
                int sample = left - 32;
                while (std::any_of(enemies.begin(), enemies.end(), [sample](const Enemy& other) {
                    return (other.type == 0x11 || other.type == 0x12) &&
                           std::abs(other.anchorX - (sample + 20)) < 32;
                })) sample -= 32;
                stage.DrawArea(display, {sample, std::max(0, top), 32, int(enemy.anchorY) - std::max(0, top)},
                               left - cameraX, std::max(0, top) - cameraY);
            }
            continue;
        }
        if (enemy.role == Role::WallBox) {
            if (enemy.alive) {
                const int frame = WallBoxFrame(enemy.age, stage.IsVertical() ? 128 : screenX);
                sprites.Draw(display, (stageNumber == 1 ? WallBoxFirstFrame : 0xd8) + frame, screenX, screenY);
            } else if (stageNumber == 1) {
                sprites.Draw(display, RedTurretFirstFrame + 5, screenX, screenY);
            }
            continue;
        }
        if (enemy.role == Role::RedTurret) {
            const int ground = 5 + (enemy.attribute & 1);
            int frame = ground;
            if (enemy.alive) {
                const int phase = enemy.retractAge < 0 ? enemy.age :
                    std::max(0, RedTurretOpenAt - 1 - (enemy.age - enemy.retractAge));
                if (phase >= RedTurretOpenAt) frame = enemy.direction;
                else if (phase >= 2 * RedTurretStep) frame = 7;
                else if (phase >= RedTurretStep) frame = 3 + (enemy.attribute & 1);
            }
            sprites.Draw(display, RedTurretFirstFrame + frame, screenX, screenY);
            continue;
        }
        if (enemy.role == Role::BombTurret) {
            const int frame = enemy.alive ? enemy.strideFrame : 2;
            const bool jungleSide = enemy.attribute & 1;
            sprites.Draw(display, BombTurretFirstFrame + (jungleSide ? 0 : 3) + frame,
                         screenX - (jungleSide ? 8 : 0), screenY);
            continue;
        }
        if (!enemy.alive) continue;
        int sprite = -1;
        switch (enemy.role) {
            case Role::Runner:
                if (IsBase()) {
                    sprite = 0x93 + enemy.age / 8 % 2;
                } else if (enemy.type == 0x0c) {
                    sprite = 0x45;
                } else if (enemy.grounded) {
                    const int groundInset = stageNumber == 1 && TerrainAt(int(enemy.worldX), int(enemy.worldY) + 11) == '=' ? 2 : 0;
                    sprites.DrawRunner(display, screenX, screenY + 11 + groundInset, enemy.direction, enemy.strideFrame / 8);
                } else {
                    sprites.Draw(display, 0x28, screenX, screenY + 11, enemy.direction > 0, false, true);
                }
                break;
            case Role::Rifleman: {
                const auto* ambush = JungleAmbush(stageNumber, enemy.attribute);
                if (ambush && (enemy.attribute == 1 || !ambush->Exposed(enemy.age))) {
                    const int frame = ambush->Frame(enemy.age);
                    const int bottom = enemy.attribute == 1 ? screenY + BushRiflemanBottom :
                                       int(enemy.anchorY) - cameraY - FortressRiflemanCoverInset;
                    sprites.Draw(display, frame, screenX + (frame == 0x44 ? 0 : enemy.direction * 3),
                                 bottom, enemy.direction > 0, false, true);
                    break;
                }
                int aim = 0;
                const int target = ClosestPlayer(enemy.worldX, enemy.worldY);
                if (enemy.demoAim >= 0) {
                    aim = AimDirections[enemy.demoAim].y;
                } else if (target >= 0) {
                    const float deltaY = players[target].worldY - PlayerTargetHeight - enemy.worldY;
                    const float threshold = std::abs(players[target].worldX - enemy.worldX) * 0.4f;
                    aim = deltaY < -threshold ? -1 : deltaY > threshold ? 1 : 0;
                }
                const int feetY = int(enemy.worldY) + RiflemanFootOffset;
                const int groundInset = stageNumber == 1 && TerrainAt(int(enemy.worldX), feetY) == '=' ? 2 : 0;
                sprites.DrawRifleman(display, screenX, feetY - cameraY + groundInset, enemy.direction, aim);
                break;
            }
            case Role::Gun: sprite = IsBase() ? 0x72 : 0x7a; break;
            case Role::Turret:
                sprites.Draw(display, TurretFirstFrame + (enemy.age < 2 * TurretOpeningStep ? 0 :
                             enemy.age < TurretOpenAt ? 1 : 2 + enemy.direction), screenX, screenY);
                break;
            case Role::Capsule: sprite = 0x4d; break;
            case Role::Platform:
                sprites.Draw(display, 0x48, screenX, screenY - 1);
                break;
            case Role::Core: sprite = enemy.required && enemy.age >= 45 + (enemy.attribute & 3) * 35 ? 0x72 : -1; break;
            case Role::Roller: sprite = IsBase() ? 0x7a : 0x2a + enemy.age / 8 % 2; break;
            case Role::Hazard:
                if (stageNumber == 6) {
                    const int phase = (enemy.age + enemy.attribute * 24) % 150;
                    if (phase >= 72 && phase < 139) {
                        const bool horizontal = enemy.type == 0x10;
                        const int length = phase < 85 || phase >= 130 ? 12 : 24;
                        for (int offset = -length; offset <= length; offset += 12) {
                            sprites.Draw(display, 0x49, screenX + (horizontal ? offset : 0), screenY + (horizontal ? 0 : offset));
                        }
                    }
                } else {
                    sprite = stageNumber == 3 ? (enemy.type == 0x15 ? 0x79 : 0x4a) : 0x7b;
                }
                break;
            case Role::Alien:
                if (enemy.type != 0x10) sprite = enemy.type == 0x15 ? 0xb6 : 0xb3 + enemy.age / 9 % 3;
                break;
            case Role::Tank:
                sprites.Draw(display, TankFirstFrame + enemy.age / 8 % 4, screenX, screenY, false, false, true);
                break;
            case Role::Boss:
                if (stageNumber == 2) sprite = 0x5d + enemy.age / 10 % 6;
                if (stageNumber == 4) sprite = 0x68 + enemy.age / 10 % 5;
                if (stageNumber == 5) sprite = 0xad + enemy.age / 8 % 3;
                if (stageNumber == 6) sprite = 0xb8 + enemy.age / 9 % 3;
                if (stageNumber == 8 && enemy.age % 25 < 8) sprite = 0x72;
                break;
            default: break;
        }
        if (sprite >= 0) sprites.Draw(display, sprite, screenX, screenY, enemy.direction > 0);
    }
}

void Map::DrawCombat(SDL_Renderer* display) const {
    const int offsetX = IsBase() ? 0 : cameraX;
    for (const auto& shot : shots) {
        const float wave = shot.weapon == Player::Fireball && shot.owner >= 0 ? std::sin(shot.age * 0.42f) * 7 : 0;
        if (shot.owner >= 0 && shot.weapon == Player::Laser) {
            SDL_SetRenderDrawColor(display, 255, 104, 104, 255);
            SDL_RenderDrawLine(display, int(shot.worldX) - offsetX, int(shot.worldY) - cameraY,
                               int(shot.worldX - shot.velocityX * 1.7f) - offsetX,
                               int(shot.worldY - shot.velocityY * 1.7f) - cameraY);
        } else {
            sprites.Draw(display, shot.sprite, int(shot.worldX) - offsetX, int(shot.worldY + wave) - cameraY);
        }
    }
    for (const auto& item : pickups) {
        if (item.life > 100 || item.life % 8 < 4) sprites.Draw(display, PickupFrames[item.type], int(item.worldX) - offsetX, int(item.worldY) - cameraY);
    }
    for (const auto& burst : bursts) {
        if (burst.age >= 0) sprites.Draw(display, 0x35 + burst.age / 8, int(burst.worldX) - offsetX, int(burst.worldY) - cameraY);
    }
}
