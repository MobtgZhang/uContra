#include "Map.h"
#include "CFG.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
const float NoFloor = std::numeric_limits<float>::infinity();
const char* const StageNames[] = {
    "JUNGLE", "BASE 1", "WATERFALL", "BASE 2", "SNOW FIELD", "ENERGY ZONE", "HANGAR", "ALIEN'S LAIR"
};
const Music::eMusic StageMusic[] = {
    Music::mJUNGLE, Music::mMAZEFORTRESS, Music::mWATERFALL, Music::mMAZEFORTRESS,
    Music::mFORTRESSICE, Music::mFORTRESSFLAME, Music::mJUNGLE, Music::mHORRIBLEHEARTBEAT
};
}

Map::Map(SDL_Renderer* display) {
    loadGameData(display);
}

Map::~Map() {
    SDL_DestroyTexture(gameView);
}

void Map::loadGameData(SDL_Renderer* display) {
    renderer = display;
    gameView = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                 Stage::ViewWidth, Stage::ViewHeight);
    spritesReady = sprites.Load(renderer);
}

const char* Map::StageName() const {
    return StageNames[stageNumber - 1];
}

void Map::NewGame(int selectedStage, int count) {
    demo = false;
    playerCount = std::max(1, std::min(count, 2));
    stageNumber = std::max(1, std::min(selectedStage, int(StageCount)));
    continues = 3;
    for (int index = 0; index < 2; ++index) {
        players[index].Reset(index < playerCount ? startingLives : 0);
    }
    loadLVL();
}

void Map::StartDemo() {
    demo = true;
    playerCount = 0;
    demoRandom.seed(std::random_device{}());
    LoadJungle();
}

void Map::Continue() {
    if (result != Result::Lost || continues <= 0) {
        return;
    }
    --continues;
    for (int index = 0; index < playerCount; ++index) {
        players[index].Reset(startingLives);
    }
    loadLVL();
}

void Map::NextStage() {
    if (result == Result::Cleared && stageNumber < StageCount) {
        ++stageNumber;
        loadLVL();
    }
}

void Map::LoadStage(int number) {
    stageNumber = number;
    clearTimer = roomTimer = frame = cameraX = room = 0;
    arenaScrolling = false;
    soldierTimer = soldierPage = soldiersOnPage = previousScroll = 0;
    demoSpawnTimer = demoWave = nextVolley = 0;
    demoAimIndex = demoAims.size();
    inputs = {};
    pickups.clear();
    bursts.clear();
    shots.clear();
    enemies.clear();
    result = Result::MissingAssets;
    if (!gameView || !spritesReady || !stage.Load(renderer, number)) {
        SDL_Log("Could not load Contra stage %d. Run 'make assets': %s", number, SDL_GetError());
        return;
    }
    cameraY = stage.IsVertical() ? stage.Height() - Stage::ViewHeight : 0;
    result = Result::Playing;
    PopulateEnemies();
    for (int index = 0; index < playerCount; ++index) {
        RespawnPlayer(index, true);
    }
}

void Map::loadLVL() {
    using Loader = void (Map::*)();
    static const Loader loaders[] = {&Map::LoadJungle, &Map::LoadMazeBaseA, &Map::LoadMazeWaterFall,
                                     &Map::LoadMazeBaseB, &Map::LoadMazeSnowField, &Map::LoadMazeEnergyZone,
                                     &Map::LoadMazeHangar, &Map::LoadMazeAliensLair};
    (this->*loaders[stageNumber - 1])();
}

void Map::LoadJungle() { LoadStage(1); }
void Map::LoadMazeBaseA() { LoadStage(2); }
void Map::LoadMazeWaterFall() { LoadStage(3); }
void Map::LoadMazeBaseB() { LoadStage(4); }
void Map::LoadMazeSnowField() { LoadStage(5); }
void Map::LoadMazeEnergyZone() { LoadStage(6); }
void Map::LoadMazeHangar() { LoadStage(7); }
void Map::LoadMazeAliensLair() { LoadStage(8); }

void Map::PlayStageMusic() const {
    if (result == Result::Playing) {
        CCFG::getMusic()->PlayMusic(StageMusic[stageNumber - 1]);
    }
}

void Map::SetInput(int index, const Player::Input& input) {
    inputs[index] = input;
}

void Map::RespawnPlayer(int index, bool initial) {
    auto& player = players[index];
    if (!player.lives) {
        return;
    }
    if (IsBase()) {
        player.Respawn(playerCount == 1 ? 128 : 108 + index * 40, BaseFloor, true);
        return;
    }
    const int preferred = initial ? 48 + index * 26 : int(player.worldX) - 22;
    const int startX = std::max(cameraX + 25, std::min(preferred, cameraX + 230));
    const int startY = cameraY + (initial && stage.IsVertical() ? 90 : 24);
    const bool enterFromAir = initial && stageNumber != 1;
    const int firstX = cameraX + 25;
    const int lastX = std::min(cameraX + 230, stage.Width() - 26);
    for (bool allowWater : {false, true}) {
        if (!allowWater && stageNumber != 1) continue;
        for (int distance = 0; distance < 182; distance += 8) {
            for (int direction : {1, -1}) {
                const int worldX = startX + direction * distance;
                if (worldX < firstX || worldX > lastX) continue;
                const float floor = FindFloor(worldX, startY, cameraY + 216, false, allowWater);
                if (std::isfinite(floor) && TerrainAt(worldX, int(floor) - 10) != '#') {
                    player.Respawn(worldX, enterFromAir ? std::max(float(startY), floor - 24) : floor, !enterFromAir);
                    return;
                }
            }
        }
    }
    player.Respawn(cameraX + 42 + index * 24, cameraY + 30);
}

void Map::UpdateDefeatedPlayers() {
    for (int index = 0; index < playerCount; ++index) {
        auto& player = players[index];
        if (!player.Defeated()) continue;
        player.UpdateDefeat(*this);
        if (!player.Defeated() && player.lives > 0) RespawnPlayer(index, false);
    }
}

void Map::StartRoom() {
    roomTimer = 0;
    requiredRemaining = 0;
    shots.clear();
    pickups.clear();
    bursts.clear();
    for (const auto& enemy : enemies) {
        if (enemy.room == room && enemy.required && enemy.alive) {
            ++requiredRemaining;
        }
    }
    for (int index = 0; index < playerCount; ++index) {
        RespawnPlayer(index, true);
    }
    if (room == stage.RoomCount() - 1) {
        CCFG::getMusic()->PlayMusic(Music::mMAZEFORTRESSBOSS);
    }
}

int Map::ScrollPosition() const {
    return stage.IsVertical() ? stage.Height() - Stage::ViewHeight - cameraY : cameraX;
}

int Map::ScrollLimit() const {
    return stage.IsVertical() ? (stage.Height() - Stage::ViewHeight) / 240 * 240 : stage.Width() - Stage::ViewWidth;
}

int Map::HorizontalScrollPoint() const {
    return players[0].lives > 0 && players[1].lives > 0 ? 176 : 128;
}

void Map::UpdateCamera() {
    if (IsBase()) return;
    const bool vertical = stage.IsVertical();
    const int current = ScrollPosition();
    const int limit = ScrollLimit();
    const int arenaStart = limit - (vertical ? 64 : 96);
    int desired = current;
    for (const auto& player : players) {
        if (player.Active()) {
            const int target = vertical ? stage.Height() - Stage::ViewHeight + 92 - int(std::floor(player.worldY)) :
                                          int(player.worldX) - HorizontalScrollPoint();
            desired = std::max(desired, target);
        }
    }
    if (desired > current && current >= arenaStart && current < limit) arenaScrolling = true;
    const int next = arenaScrolling ? std::min(limit, current + 1) : std::min(std::max(current, arenaStart), desired);
    if (vertical) {
        cameraY -= next - current;
    } else {
        cameraX = next;
        for (auto& player : players) {
            if (player.Active()) {
                player.worldX = std::max(player.worldX, float(cameraX + 25));
            }
        }
    }
}

void Map::Update() {
    if (result != Result::Playing) {
        return;
    }
    ++frame;
    for (auto& burst : bursts) {
        ++burst.age;
    }
    bursts.erase(std::remove_if(bursts.begin(), bursts.end(), [](const Burst& burst) { return burst.age >= 24; }), bursts.end());
    if (demo) {
        cameraX = std::min(frame, ScrollLimit());
        if (frame >= ScrollLimit() + 240) {
            LoadJungle();
            return;
        }
        ActivateSpawns();
        SpawnDemoEnemies();
        UpdateEnemies();
        UpdateShots();
        UpdatePickups();
        return;
    }
    if (clearTimer) {
        if (--clearTimer == 0) {
            result = stageNumber == StageCount ? Result::Finished : Result::Cleared;
        }
        return;
    }
    UpdateDefeatedPlayers();
    if (players[0].lives + players[1].lives == 0 && !players[0].Defeated() && !players[1].Defeated()) {
        result = Result::Lost;
        return;
    }
    if (roomTimer) {
        for (auto& player : players) {
            if (player.Active()) player.EnterBaseRoom();
        }
        if (--roomTimer == 0) {
            ++room;
            StartRoom();
        }
        return;
    }
    previousScroll = ScrollPosition();
    UpdatePlatforms();
    for (int index = 0; index < playerCount; ++index) {
        players[index].Update(*this, inputs[index], index);
    }
    UpdateCamera();
    ActivateSpawns();
    if (!BaseGateOpen()) {
        UpdateEnemies();
        UpdateShots();
        UpdatePickups();
    }
    highScore = std::max(highScore, std::max(players[0].score, players[1].score));
    if (BaseGateOpen()) {
        shots.clear();
        for (int index = 0; index < playerCount; ++index) {
            if (players[index].Active() && inputs[index].up && !inputs[index].left && !inputs[index].right) {
                roomTimer = 38;
                break;
            }
        }
    } else if (requiredRemaining == 0) {
        SetCleared();
        return;
    }
}

void Map::SetCleared() {
    clearTimer = 105;
    shots.clear();
    CCFG::getMusic()->PlayMusic(stageNumber == StageCount ? Music::mTRIUMPHANTBOSS : Music::mTRIUMPHANT, 0);
}

bool Map::OnScreen(float worldX, float worldY, int margin) const {
    const int left = IsBase() ? 0 : cameraX;
    return worldX >= left - margin && worldX < left + Stage::ViewWidth + margin &&
           worldY >= cameraY - margin && worldY < cameraY + Stage::ViewHeight + margin;
}

int Map::ClosestPlayer(float worldX, float worldY) const {
    int closest = -1;
    float distance = NoFloor;
    for (int index = 0; index < playerCount; ++index) {
        const auto& player = players[index];
        if (player.Active()) {
            const float delta = std::abs(player.worldX - worldX) + std::abs(player.worldY - 15 - worldY);
            if (delta < distance) {
                closest = index;
                distance = delta;
            }
        }
    }
    return closest;
}

bool Map::IsWater(int worldX, int worldY) const {
    return stage.TileAt(worldX, worldY) == '~';
}

bool Map::CanDrop(float worldX, float feet) const {
    if (feet >= cameraY + 198 || TerrainAt(int(worldX), int(feet)) != '=') return false;
    return std::isfinite(FindFloor(worldX, feet + Stage::TileSize, cameraY + 222, false, true));
}

bool Map::BelowView(float feet) const {
    return feet > cameraY + Stage::ViewHeight + 14;
}

float Map::MovePlayerX(float worldX, float feet, float movement) const {
    static const int ArenaRight[] = {144, 255, 255, 255, 160, 208, 176, 176};
    const int left = IsBase() ? (HasBaseGate() ? 47 : 31) : cameraX + 25;
    int right = IsBase() ? (HasBaseGate() ? 208 : 224) : cameraX + 230;
    if (!IsBase() && !stage.IsVertical() && cameraX == ScrollLimit()) {
        right = std::min(right, cameraX + ArenaRight[stageNumber - 1]);
    } else if (!IsBase() && !stage.IsVertical() && !arenaScrolling && players[0].Active() && players[1].Active() &&
               std::min(players[0].worldX, players[1].worldX) < cameraX + 33) {
        right = cameraX + HorizontalScrollPoint();
    }
    if ((movement < 0 && (worldX <= left || (!IsBase() && !CCFG::canMoveBackward && worldX <= cameraX + 128))) ||
        (movement > 0 && worldX >= right)) return worldX;
    const float targetX = std::max(float(left), std::min(worldX + movement, float(right)));
    const int edge = int(worldX) + (movement < 0 ? -8 : 8);
    if (!IsBase()) {
        for (int offset : {1, 9, 17, 25, 29}) {
            if (TerrainAt(edge, int(std::floor(feet)) - offset) == '#') return worldX;
        }
    }
    return targetX;
}

bool Map::HitsCeiling(float worldX, float feet) const {
    return TerrainAt(int(worldX), int(std::floor(feet)) - 30) == '#';
}

float Map::FindFloor(float worldX, float currentY, float nextY, bool dropping, bool canSwim) const {
    float nearest = NoFloor;
    const int first = std::max(0, int(std::ceil(currentY / Stage::TileSize)) * Stage::TileSize);
    for (int row = first; row <= nextY && row < stage.Height(); row += Stage::TileSize) {
        for (int offset : {0, -4, 4}) {
            const int feetX = int(worldX) + offset;
            const char tile = TerrainAt(feetX, row);
            const char top = TerrainAt(feetX, row - 1);
            if ((tile == '=' && top != '=' && !dropping) || (tile == '#' && top != '#') ||
                (tile == '~' && top != '~' && canSwim && !dropping)) {
                nearest = std::min(nearest, float(row));
            }
        }
    }
    if (!dropping) {
        for (const auto& enemy : enemies) {
            if (enemy.role == Role::Platform && enemy.active && enemy.alive && std::abs(worldX - enemy.worldX) <= 15 &&
                currentY <= enemy.RideHeight() && nextY >= enemy.RideHeight()) {
                nearest = std::min(nearest, enemy.RideHeight());
            }
        }
    }
    return nearest;
}

char Map::TerrainAt(int worldX, int worldY) const {
    const char tile = stage.TileAt(worldX, worldY);
    if ((tile == '=' || tile == '#') && stageNumber == 1 && BridgeOpen(worldX, worldY)) return '.';
    if (tile == '#' && (stageNumber == 7 || stageNumber == 8)) {
        for (const auto& enemy : enemies) {
            const bool wall = stageNumber == 7 && (enemy.type == 0x11 || enemy.type == 0x12);
            const bool guardian = stageNumber == 8 && enemy.type == 0x10;
            if (!enemy.alive && (wall || guardian) && std::abs(worldX - enemy.anchorX) < (wall ? 16 : 49)) return '.';
        }
    }
    return tile;
}

int Map::getStartBlock() const { return CameraX() / Stage::TileSize; }
int Map::getEndBlock() const { return (CameraX() + Stage::ViewWidth) / Stage::TileSize; }

void Map::Draw(SDL_Renderer* display) {
    if (result == Result::MissingAssets || !gameView) {
        return;
    }
    if (SDL_SetRenderTarget(display, gameView) < 0) {
        return;
    }
    SDL_SetRenderDrawColor(display, 0, 0, 0, 255);
    SDL_RenderClear(display);
    stage.Draw(display, CameraX(), cameraY);
    DrawBridgeGaps(display);
    if (BaseGateOpen()) DrawBaseGate(display);
    else DrawEnemies(display);
    DrawCombat(display);
    for (int index = 0; index < playerCount; ++index) {
        const auto& player = players[index];
        const int groundInset = stageNumber == 1 && TerrainAt(int(player.worldX), int(player.worldY)) == '=' ? 2 : 0;
        player.Draw(display, sprites, cameraX, cameraY, IsBase(), index != 0, groundInset);
    }
    if (!demo) DrawHUD(display);
    SDL_SetRenderTarget(display, nullptr);
    const int scale = std::max(1, std::min(CCFG::GAME_WIDTH / Stage::ViewWidth, CCFG::GAME_HEIGHT / Stage::ViewHeight));
    const SDL_Rect viewport{(CCFG::GAME_WIDTH - Stage::ViewWidth * scale) / 2,
                            (CCFG::GAME_HEIGHT - Stage::ViewHeight * scale) / 2,
                            Stage::ViewWidth * scale, Stage::ViewHeight * scale};
    SDL_RenderCopy(display, gameView, nullptr, &viewport);
}

void Map::DrawBaseGate(SDL_Renderer* display) const {
    SDL_SetRenderDrawColor(display, 5, 8, 16, 255);
    const SDL_Rect door{113, 75, 30, 42};
    SDL_RenderFillRect(display, &door);
    SDL_SetRenderDrawColor(display, 24, 60, 92, 255);
    for (int row : {138, 154}) {
        const SDL_Rect field{56, row, 144, 5};
        SDL_RenderFillRect(display, &field);
    }
}

void Map::DrawHUD(SDL_Renderer* display) const {
    auto* text = CCFG::getText();
    SDL_SetRenderDrawColor(display, 0, 0, 0, 255);
    const SDL_Rect header{0, 0, Stage::ViewWidth, 15};
    SDL_RenderFillRect(display, &header);
    text->Draw(display, "1P " + std::to_string(players[0].score) + " X" + std::to_string(players[0].lives), 5, 4, 8);
    if (playerCount == 2) {
        text->Draw(display, "2P " + std::to_string(players[1].score) + " X" + std::to_string(players[1].lives), 150, 4, 8);
    } else {
        text->Draw(display, std::string(StageName()), 165, 4, 8);
    }
    const char* message = clearTimer ? "STAGE CLEAR" : roomTimer ? "GO" : nullptr;
    if (message) {
        const int width = text->getTextWidth(message, 8);
        const SDL_Rect backing{(Stage::ViewWidth - width) / 2 - 5, 22, width + 10, 15};
        SDL_RenderFillRect(display, &backing);
        text->Draw(display, message, backing.x + 5, 26, 8);
    }
}
