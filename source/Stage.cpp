#include "Stage.h"

#include <SDL2/SDL_image.h>
#include <algorithm>
#include <fstream>

namespace {
const char* AssetDirectory = "files/stages/";
}

Stage::~Stage() {
    Clear();
}

void Stage::Clear() {
    SDL_DestroyTexture(damagedExit);
    damagedExit = nullptr;
    for (const auto& chunk : chunks) {
        SDL_DestroyTexture(chunk.texture);
    }
    chunks.clear();
    tiles.clear();
    spawns.clear();
    soldierProfiles.clear();
    width = height = 0;
}

bool Stage::Load(SDL_Renderer* renderer, int stageNumber) {
    Clear();
    number = stageNumber;
    const std::string filename = AssetDirectory + std::to_string(number);
    std::ifstream data(filename + ".txt");
    if (!(data >> width >> height) || width < ViewWidth || height < ViewHeight ||
        width > 8192 || height > 8192 || width % TileSize || height % TileSize ||
        (IsBase() && (width % ViewWidth || height != ViewHeight))) {
        return false;
    }
    if (!IsBase()) {
        for (int index = 0; index < height / TileSize; ++index) {
            std::string row;
            if (!(data >> row) || row.size() != static_cast<unsigned>(width / TileSize) ||
                row.find_first_not_of(".=~#") != std::string::npos) {
                return false;
            }
            tiles.push_back(row);
        }
    }
    std::string marker;
    int profiles;
    if (!(data >> marker >> profiles) || marker != "soldiers" || profiles < 0 || profiles > 32) return false;
    for (int index = 0; index < profiles; ++index) {
        int value;
        if (!(data >> value) || value < 0 || value > 255) return false;
        soldierProfiles.push_back(value);
    }
    if (!(data >> marker) || marker != "entities-v2") return false;
    Spawn spawn{};
    while (data >> spawn.worldX) {
        if (!(data >> spawn.worldY >> spawn.type >> spawn.attribute >> spawn.room >> spawn.trigger) || spawn.trigger < 0 ||
            spawn.worldX < 0 || spawn.worldX >= (IsBase() ? ViewWidth : width) ||
            spawn.worldY < 0 || spawn.worldY >= height || spawn.room < 0 || spawn.room >= RoomCount()) {
            return false;
        }
        spawns.push_back(spawn);
    }
    if (!data.eof()) {
        return false;
    }

    SDL_Surface* original = IMG_Load((filename + ".png").c_str());
    if (!original || original->w != width || original->h != height) {
        SDL_FreeSurface(original);
        return false;
    }
    SDL_Surface* background = SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(original);
    if (!background) {
        return false;
    }

    for (int chunkY = 0; chunkY < height; chunkY += 256) {
        for (int chunkX = 0; chunkX < width; chunkX += 256) {
            SDL_Rect bounds{chunkX, chunkY, std::min(256, width - chunkX), std::min(256, height - chunkY)};
            SDL_Texture* texture = SDL_CreateTexture(renderer, background->format->format,
                                                     SDL_TEXTUREACCESS_STATIC, bounds.w, bounds.h);
            auto* pixels = static_cast<const Uint8*>(background->pixels);
            if (!texture || SDL_UpdateTexture(texture, nullptr, pixels + chunkY * background->pitch + chunkX * 4,
                                              background->pitch) < 0) {
                SDL_DestroyTexture(texture);
                SDL_FreeSurface(background);
                Clear();
                return false;
            }
            chunks.push_back({bounds, texture});
        }
    }
    SDL_FreeSurface(background);
    if (number == 1) {
        damagedExit = IMG_LoadTexture(renderer, (filename + "-exit.png").c_str());
        if (damagedExit) SDL_QueryTexture(damagedExit, nullptr, nullptr, &damagedExitSize.w, &damagedExitSize.h);
    }
    return true;
}

void Stage::Draw(SDL_Renderer* renderer, int cameraX, int cameraY) const {
    DrawArea(renderer, {cameraX, cameraY, ViewWidth, ViewHeight}, 0, 0);
}

void Stage::DrawDamagedExit(SDL_Renderer* renderer, int centerX, int centerY) const {
    if (!damagedExit) return;
    SDL_Rect destination = damagedExitSize;
    destination.x = centerX - destination.w / 2;
    destination.y = centerY - destination.h / 2;
    SDL_RenderCopy(renderer, damagedExit, nullptr, &destination);
}

void Stage::DrawArea(SDL_Renderer* renderer, const SDL_Rect& area, int destX, int destY) const {
    for (const auto& chunk : chunks) {
        SDL_Rect intersection;
        if (SDL_IntersectRect(&area, &chunk.bounds, &intersection)) {
            SDL_Rect source{intersection.x - chunk.bounds.x, intersection.y - chunk.bounds.y,
                            intersection.w, intersection.h};
            SDL_Rect destination{destX + intersection.x - area.x, destY + intersection.y - area.y,
                                 intersection.w, intersection.h};
            SDL_RenderCopy(renderer, chunk.texture, &source, &destination);
        }
    }
}

int Stage::SoldierProfile(int page) const {
    return page > 0 && page <= static_cast<int>(soldierProfiles.size()) ? soldierProfiles[page - 1] : 255;
}

char Stage::TileAt(int worldX, int worldY) const {
    if (tiles.empty() || worldX < 0 || worldY < 0 || worldX >= width || worldY >= height ||
        worldY / TileSize >= static_cast<int>(tiles.size())) {
        return '.';
    }
    return tiles[worldY / TileSize][worldX / TileSize];
}

StageSprites::~StageSprites() {
    SDL_DestroyTexture(texture);
}

bool StageSprites::Load(SDL_Renderer* renderer) {
    texture = IMG_LoadTexture(renderer, (std::string(AssetDirectory) + "sprites.png").c_str());
    std::ifstream data(std::string(AssetDirectory) + "sprites.txt");
    std::string frame;
    SDL_Rect bounds;
    while (data >> frame >> bounds.x >> bounds.y >> bounds.w >> bounds.h) {
        frames[frame] = bounds;
    }
    return texture && frames.count("05");
}

void StageSprites::Draw(SDL_Renderer* renderer, int frame, int centerX, int anchorY,
                        bool flip, bool second, bool feet, bool flipY) const {
    static const char hex[] = "0123456789abcdef";
    if (frame < 0 || frame > 255) {
        return;
    }
    const std::string key{hex[frame >> 4], hex[frame & 15]};
    auto found = frames.find(second ? key + "_p2" : key);
    if (found == frames.end() && second) {
        found = frames.find(key);
    }
    if (found == frames.end()) {
        return;
    }
    const auto& source = found->second;
    SDL_Rect destination{centerX - source.w / 2, anchorY - (feet ? source.h : source.h / 2), source.w, source.h};
    const auto orientation = static_cast<SDL_RendererFlip>((flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) |
                                                         (flipY ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE));
    SDL_RenderCopyEx(renderer, texture, &source, &destination, 0, nullptr, orientation);
}

void StageSprites::DrawRunner(SDL_Renderer* renderer, int centerX, int feetY, int direction, int stride) const {
    static const int runFrames[] = {0x3b, 0x3c, 0x3d, 0x3f, 0x3c, 0x3e};
    const int frame = frames.count("3f") ? runFrames[stride] : 0x3c;
    Draw(renderer, frame, centerX, feetY, direction > 0, false, true);
}

StageSprites::RiflemanPose StageSprites::GetRiflemanPose(int centerX, int bottomY, int direction, int aim, int coverFrame) const {
    struct PoseFrame {
        int frame, offset;
        SDL_Point muzzlePixel;
        const char* key;
    };
    static const PoseFrame poses[] = {
        {0x41, 4, {1, 17}, "41"}, {0x42, 1, {1, 1}, "42"}, {0x43, 4, {1, 4}, "43"},
        {0x44, 0, {0, 0}, "44"}, {0x45, 3, {1, 8}, "45"}, {0x46, 3, {1, 4}, "46"}
    };
    const int frame = coverFrame ? coverFrame : !frames.count("43") || aim < 0 ? 0x42 : aim > 0 ? 0x41 : 0x43;
    const auto& pose = poses[frame - 0x41];
    const SDL_Point anchor{centerX + direction * pose.offset, bottomY};
    const auto found = frames.find(pose.key);
    if (found == frames.end()) return {frame, anchor, {centerX, bottomY}};
    const auto& bounds = found->second;
    const int muzzleX = direction > 0 ? bounds.w - 1 - pose.muzzlePixel.x : pose.muzzlePixel.x;
    return {frame, anchor, {anchor.x - bounds.w / 2 + muzzleX, anchor.y - bounds.h + pose.muzzlePixel.y}};
}
