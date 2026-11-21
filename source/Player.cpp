#include "Player.h"
#include "Map.h"
#include "CFG.h"

#include <algorithm>

namespace {
constexpr int spawnProtectionFrames = 100;
}

void Player::Reset(int startingLives) {
    *this = Player{};
    lives = startingLives;
}

void Player::Respawn(float startX, float startY, bool onGround) {
    worldX = startX;
    worldY = startY;
    verticalSpeed = 0;
    respawnTimer = dropTimer = cooldown = shockTimer = firePoseTimer = 0;
    protection = spawnProtectionFrames;
    aimX = facing = 1;
    aimY = 0;
    prone = swimming = walking = jumpHeld = tumbling = jumpLeft = false;
    grounded = onGround;
    runFrame = jumpFrame = airDirection = 0;
}

void Player::EnterBaseRoom() {
    ++frame;
    worldX += std::max(-2.0f, std::min(128.0f - worldX, 2.0f));
    worldY = std::max(119.0f, worldY - 2.25f);
    shockTimer = 0;
    prone = false;
    walking = grounded = true;
}

void Player::Update(Map& map, const Input& input, int index) {
    if (!Active()) {
        return;
    }
    ++frame;
    protection -= protection > 0;
    barrier -= barrier > 0;
    cooldown -= cooldown > 0;
    firePoseTimer -= firePoseTimer > 0;
    dropTimer -= dropTimer > 0;

    const int direction = static_cast<int>(input.right) - static_cast<int>(input.left);
    if (direction) facing = direction;
    const bool indoor = map.IsBase();
    const bool jump = input.jump && !jumpHeld;
    jumpHeld = input.jump;
    if (indoor && map.HasBaseGate() && !map.BaseGateOpen()) {
        if (shockTimer > 0) {
            --shockTimer;
            return;
        }
        if (input.up && !input.down && !direction && !jump && grounded) {
            shockTimer = 48;
            walking = prone = false;
            CCFG::getMusic()->PlayChunk(Music::cP_SHOCK);
            return;
        }
    }
    const bool drop = jump && !indoor && input.down && !direction;
    const bool launch = jump && grounded && !swimming && (!drop || map.CanDrop(worldX, worldY));
    prone = input.down && !direction && grounded && !swimming;
    walking = grounded && direction && !launch && !(swimming && input.down);
    aimY = indoor || input.up ? -1 : input.down && (tumbling || direction) ? 1 : 0;
    aimX = indoor || (aimY && !direction) ? 0 : facing;

    if (launch) {
        if (drop) {
            dropTimer = 18;
            verticalSpeed = 0;
        } else {
            verticalSpeed = (indoor ? -880.0f : -1040.0f) / 256;
            tumbling = true;
            jumpLeft = facing < 0;
            jumpFrame = 0;
        }
        airDirection = 0;
        grounded = false;
    } else {
        if (tumbling) {
            ++jumpFrame;
            if (direction) airDirection = direction;
        }
        const int movement = tumbling ? airDirection : walking || (!grounded && !dropTimer) ? direction : 0;
        if (movement) worldX = map.MovePlayerX(worldX, worldY, float(movement));

        if (verticalSpeed < 0 && !indoor && map.HitsCeiling(worldX, worldY)) verticalSpeed = 0;
        verticalSpeed = std::min(verticalSpeed + 35.0f / 256, 8.0f);
        const float nextY = worldY + verticalSpeed;
        const float floor = indoor ? float(Map::BaseFloor) : map.FindFloor(worldX, worldY, nextY, dropTimer > 0, true);
        if (verticalSpeed >= 0 && nextY >= floor) {
            if (tumbling || dropTimer) CCFG::getMusic()->PlayChunk(Music::cP_LANDING);
            worldY = floor;
            verticalSpeed = 0;
            tumbling = false;
            grounded = true;
            swimming = !indoor && map.IsWater(int(worldX), int(worldY));
            dropTimer = 0;
        } else {
            worldY = nextY;
            grounded = swimming = false;
        }
    }
    runFrame = walking && grounded ? (runFrame + 1) % 48 : 0;
    if (map.BelowView(worldY)) {
        Hit(true);
        return;
    }

    if (swimming && input.down && !input.up) aimY = 1;
    if (swimming && input.fire && aimY <= 0) firePoseTimer = 15;
    if (input.fire && cooldown == 0 && (!swimming || aimY <= 0)) {
        float muzzleX = worldX + aimX * 7.0f;
        float muzzleY = worldY - (prone ? 6.0f : 20.0f) + (indoor ? 0 : aimY * 5.0f);
        if (swimming) {
            if (aimY == 0) {
                muzzleX = worldX + facing * 17.0f;
                muzzleY = worldY - 1.0f;
            } else if (aimX != 0) {
                muzzleX = worldX + aimX * 13.0f;
                muzzleY = worldY - 16.0f;
            } else {
                muzzleX = worldX + facing * 5.0f;
                muzzleY = worldY - 26.0f;
            }
        }
        if (map.Fire(index, muzzleX, muzzleY, aimX, aimY, weapon, rapid)) {
            cooldown = weapon == MachineGun ? 6 : weapon == Laser ? 17 : weapon == Spread ? 14 : 10;
            if (rapid) cooldown = std::max(4, cooldown - 3);
            firePoseTimer = 15;
        }
    }
}

bool Player::Hit(bool pit) {
    if (!Active() || (!pit && (protection || barrier))) {
        return false;
    }
    --lives;
    respawnTimer = pit ? 32 : 80;
    defeatFrame = 0;
    defeatHidden = pit;
    defeatLanded = false;
    grounded = swimming = false;
    verticalSpeed = -2.65f;
    weapon = Rifle;
    rapid = false;
    barrier = 0;
    CCFG::getMusic()->PlayChunk(Music::cP_DEATH);
    return true;
}

void Player::UpdateDefeat(const Map& map) {
    ++defeatFrame;
    --respawnTimer;
    if (defeatHidden || defeatLanded || respawnTimer == 0) return;
    if (!map.IsBase()) worldX = map.MovePlayerX(worldX, worldY, -0.65f * facing);
    verticalSpeed = std::min(verticalSpeed + 0.16f, 6.0f);
    const float nextY = worldY + verticalSpeed;
    const float floor = map.IsBase() ? float(Map::BaseFloor) : map.FindFloor(worldX, worldY, nextY, false, true);
    if (verticalSpeed >= 0 && nextY >= floor) {
        worldY = floor;
        defeatLanded = true;
        grounded = true;
        respawnTimer = 32;
    } else {
        worldY = nextY;
        if (map.BelowView(worldY)) {
            defeatHidden = true;
            respawnTimer = 24;
        }
    }
}

void Player::Collect(int item) {
    switch (item) {
        case 0: rapid = true; break;
        case 1: weapon = MachineGun; break;
        case 2: weapon = Fireball; break;
        case 3: weapon = Spread; break;
        case 4: weapon = Laser; break;
        case 5: barrier = 480; break;
        default: break;
    }
    AddScore(500);
    CCFG::getMusic()->PlayChunk(Music::cBONUS);
}

void Player::AddScore(int points) {
    const int previousMilestone = score / 50000;
    const bool firstLife = score < 30000 && score + points >= 30000;
    score += points;
    if (lives && (firstLife || score / 50000 > previousMilestone)) {
        lives = std::min(lives + 1, 9);
        CCFG::getMusic()->PlayChunk(Music::cP_1UP);
    }
}

SDL_Rect Player::Bounds() const {
    const int height = prone || swimming ? 10 : grounded ? 25 : 19;
    return {int(worldX) - (prone ? 11 : 6), int(worldY) - height, prone ? 22 : 12, height};
}

void Player::Draw(SDL_Renderer* renderer, const StageSprites& sprites, int cameraX, int cameraY,
                  bool indoor, bool second, int groundInset) const {
    const int renderX = int(worldX) - (indoor ? 0 : cameraX);
    const int renderY = int(worldY) - cameraY + (grounded ? groundInset : 0);
    if (Defeated() && !defeatHidden) {
        if (defeatLanded) {
            sprites.Draw(renderer, 0x0c, renderX, renderY, facing < 0, second, true);
        } else {
            const int pose = (defeatFrame / 7) % 4;
            sprites.Draw(renderer, pose % 2 ? 0x0b : 0x0a, renderX, renderY - 10,
                         (facing < 0) != (pose >= 2), second, false, pose >= 2);
        }
        return;
    }
    if (!Active() || (protection > 0 && (spawnProtectionFrames - protection) / 4 % 2 != 0)) {
        return;
    }
    static const int Running[] = {0x02, 0x03, 0x04, 0x05, 0x03, 0x06};
    const int stride = ((runFrame + 47) % 48) / 8;
    int sprite = 0x0f;
    int xOffset = 0, yOffset = 0;
    bool flipX = facing < 0, flipY = false, feet = true;
    if (tumbling) {
        const int roll = jumpFrame / 5 % 4;
        sprite = 0x08 + (roll & 1);
        flipY = roll >= 2;
        flipX = jumpLeft != flipY;
        yOffset = -12 + ((roll & 1) ? 0 : flipY ? 4 : -4);
        feet = false;
    } else if (indoor) {
        sprite = prone ? 0x54 : shockTimer ? 0x55 : walking ?
            (cooldown ? 0x51 + stride % 3 : Running[stride]) : 0x50;
        flipX = walking && !cooldown && facing < 0;
    } else if (swimming) {
        sprite = 0x19;
        if (firePoseTimer && aimY <= 0) {
            sprite = aimY == 0 ? 0x1d : aimX != 0 ? 0x1c : 0x1b;
            xOffset = facing * (sprite == 0x1d ? 4 : sprite == 0x1c ? 1 : 0);
        }
        yOffset = 5;
    } else if (!grounded) {
        sprite = 0x05;
    } else if (prone) {
        sprite = 0x17;
    } else if (aimY < 0) {
        sprite = walking ? 0x10 + stride % 3 : 0x16;
    } else if (aimY > 0 && walking) {
        sprite = 0x13 + stride % 3;
    } else if (walking) {
        sprite = cooldown ? 0x0d + stride % 3 : Running[stride];
    }
    sprites.Draw(renderer, sprite, renderX + xOffset, renderY + yOffset, flipX, second, feet, flipY);
    if (barrier && (barrier > 80 || frame / 4 % 2)) {
        SDL_SetRenderDrawColor(renderer, 76, 192, 255, 255);
        SDL_Rect shield{renderX - 12, renderY - 31, 24, 32};
        SDL_RenderDrawRect(renderer, &shield);
    }
}
