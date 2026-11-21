#ifndef STAGE_H
#define STAGE_H

#include <SDL2/SDL.h>
#include <array>
#include <map>
#include <string>
#include <vector>

class Stage {
public:
    enum { ViewWidth = 256, ViewHeight = 224, TileSize = 8 };
    struct Spawn {
        int worldX, worldY, type, attribute, room, trigger;
    };

    Stage() = default;
    ~Stage();
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;

    bool Load(SDL_Renderer* renderer, int number);
    void Draw(SDL_Renderer* renderer, int cameraX, int cameraY) const;
    void DrawArea(SDL_Renderer* renderer, const SDL_Rect& area, int destX, int destY) const;
    char TileAt(int worldX, int worldY) const;
    bool IsBase() const { return number == 2 || number == 4; }
    bool IsVertical() const { return number == 3; }
    int Width() const { return width; }
    int Height() const { return height; }
    int RoomCount() const { return width / ViewWidth; }
    const std::vector<Spawn>& Spawns() const { return spawns; }
    int SoldierProfile(int page) const;

private:
    struct Chunk { SDL_Rect bounds; SDL_Texture* texture; };
    void Clear();

    int number = 0;
    int width = 0;
    int height = 0;
    std::vector<std::string> tiles;
    std::vector<Spawn> spawns;
    std::vector<int> soldierProfiles;
    std::vector<Chunk> chunks;
};

class StageSprites {
public:
    StageSprites() = default;
    ~StageSprites();
    StageSprites(const StageSprites&) = delete;
    StageSprites& operator=(const StageSprites&) = delete;

    bool Load(SDL_Renderer* renderer);
    void Draw(SDL_Renderer* renderer, int frame, int centerX, int anchorY,
              bool flip = false, bool second = false, bool feet = false, bool flipY = false) const;
    void DrawRunner(SDL_Renderer* renderer, int centerX, int feetY, int direction, int stride) const;
    void DrawRifleman(SDL_Renderer* renderer, int centerX, int feetY, int direction, int aim) const;

private:
    SDL_Texture* texture = nullptr;
    std::map<std::string, SDL_Rect> frames;
};

#endif
