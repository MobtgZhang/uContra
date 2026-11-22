#include "Player.h"
#include "Map.h"
#include "CFG.h"

#include <algorithm>

namespace {
constexpr int spawnProtectionFrames = 100;
constexpr Player::WeaponConfig weaponConfigs[] = {
    {3.0f, 4.0f, 4, 0, 0, 0},
    {3.0f, 4.0f, 6, 6, 8, 16},
    {1.5f, 2.0f, 4, 0, 0, 0},
    {3.0f, 3.5f, 10, 0, 0, 0},
    {4.0f, 4.0f, 4, 0, 0, 0},
};
constexpr Player::WeaponConfig baseWeaponConfigs[] = {
    {3.0f, 4.0f, 2, 0, 10, 0},
    {3.0f, 4.0f, 2, 0, 6, 0},
    {1.5f, 2.0f, 2, 0, 10, 0},
    {3.0f, 4.0f, 2, 0, 14, 0},
    {4.0f, 5.0f, 2, 0, 17, 0},
};
}

const Player::WeaponConfig& Player::ConfigFor(Weapon weapon, bool indoor) {
    return indoor ? baseWeaponConfigs[weapon] : weaponConfigs[weapon];
}

void Player::Reset(int startingLives) {
    *this = Player{};
    lives = startingLives;
}

void Player::Respawn(float startX, float startY, bool onGround) {
    worldX = startX;
    worldY = startY;
    verticalSpeed = 0;
    respawnTimer = dropTimer = shotTimer = burstShots = shockTimer = firePoseTimer = 0;
    protection = spawnProtectionFrames;
    aimX = facing = 1;
    aimY = 0;
    prone = swimming = submerged = walking = jumpHeld = fireHeld = tumbling = jumpLeft = false;
    grounded = onGround;
    runFrame = jumpFrame = airDirection = 0;
}

bool Player::EnterBaseRoom() {
    ++frame;
    protection = shockTimer = firePoseTimer = 0;
    prone = false;
    grounded = true;
    tumbling = false;
    walking = std::abs(worldX - 128.0f) > 2.0f;
    if (walking) {
        facing = worldX < 128.0f ? 1 : -1;
        worldX += std::max(-3.5f, std::min(128.0f - worldX, 3.5f));
        runFrame = (runFrame + 1) % 48;
    } else {
        worldX = 128.0f;
        worldY = std::max(113.0f, worldY - 2.5f);
    }
    return !walking && worldY == 113.0f;
}

bool Player::WalkIntoExit(Map& map, int index, float jumpX, float entranceX, bool& jumped) {
    Input input;
    input.right = worldX < entranceX;
    protection = barrier = 0;
    if (!jumped && grounded && !swimming && worldX >= jumpX) {
        jumpHeld = false;
        input.jump = jumped = true;
        facing = 1;
    }
    Update(map, input, index);
    return jumped && grounded && worldX >= entranceX;
}

void Player::Update(Map& map, const Input& input, int index) {
    if (!Active()) {
        return;
    }
    ++frame;
    protection -= protection > 0;
    barrier -= barrier > 0;
    shotTimer -= shotTimer > 0;
    if (!input.fire && shotTimer == 0) burstShots = 0;
    firePoseTimer -= firePoseTimer > 0;
    dropTimer -= dropTimer > 0;
    const bool firePressed = input.fire && !fireHeld;
    fireHeld = input.fire;

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
        const float floor = indoor ? float(map.BaseGroundY()) : map.FindFloor(worldX, worldY, nextY, dropTimer > 0, true);
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

    submerged = swimming && input.down;
    if (swimming && !submerged && input.fire && aimY <= 0) firePoseTimer = 15;
    const auto& config = ConfigFor(weapon, indoor);
    const bool firing = indoor || config.burstCount ? input.fire && shotTimer == 0 : firePressed;
    if (firing && !submerged && (!swimming || aimY <= 0)) {
        float muzzleX = worldX + aimX * 7.0f;
        float muzzleY = worldY - (prone ? 6.0f : 20.0f) + (indoor ? 0 : aimY * 5.0f);
        if (indoor && !tumbling) {
            static const SDL_Point walkingMuzzles[] = {{0, -40}, {-1, -41}, {-1, -43}};
            static const int mirroredMuzzles[] = {-1, 0, 1};
            SDL_Point muzzle = prone ? SDL_Point{0, -32} : SDL_Point{1, -45};
            if (walking) {
                const int pose = BaseWalkPose();
                muzzle = walkingMuzzles[pose];
                if (facing < 0) muzzle.x = mirroredMuzzles[pose];
            }
            muzzleX = worldX + muzzle.x;
            muzzleY = worldY + muzzle.y;
        } else if (swimming) {
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
            if (indoor) {
                shotTimer = rapid ? std::max(4, config.shotInterval - 3) : config.shotInterval;
            } else if (config.burstCount) {
                if (++burstShots == config.burstCount) {
                    burstShots = 0;
                    shotTimer = config.burstPause;
                } else {
                    shotTimer = config.shotInterval;
                }
            }
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
    grounded = swimming = submerged = false;
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
    const float floor = map.IsBase() ? float(map.BaseGroundY()) : map.FindFloor(worldX, worldY, nextY, false, true);
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

void Player::Collect(int item, bool indoor) {
    switch (item) {
        case 0: rapid = true; break;
        case MachineGun: case Fireball: case Spread: case Laser:
            if (weapon != item) {
                weapon = static_cast<Weapon>(item);
                if (!indoor) {
                    rapid = false;
                    shotTimer = burstShots = 0;
                }
            }
            break;
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
    if (lives > 0 && lives < CCFG::MaxLives && (firstLife || score / 50000 > previousMilestone)) {
        ++lives;
        CCFG::getMusic()->PlayChunk(Music::cP_1UP);
    }
}

SDL_Rect Player::Bounds() const {
    const int height = prone || swimming ? 10 : grounded ? 25 : 19;
    return {int(worldX) - (prone ? 11 : 6), int(worldY) - height, prone ? 22 : 12, height};
}

int Player::BaseWalkPose() const {
    return ((runFrame + 47) % 48) / 8 % 3;
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
    if (!Active()) return;
    if (submerged) {
        SDL_BlendMode previousBlend = SDL_BLENDMODE_NONE;
        SDL_Color previousColor{};
        SDL_GetRenderDrawBlendMode(renderer, &previousBlend);
        SDL_GetRenderDrawColor(renderer, &previousColor.r, &previousColor.g, &previousColor.b, &previousColor.a);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        const int spread = frame / 8 % 2;
        const int rippleY = renderY + 5;
        SDL_SetRenderDrawColor(renderer, 96, 207, 248, 96);
        SDL_RenderDrawLine(renderer, renderX - 3 - spread, rippleY, renderX - 1, rippleY);
        SDL_RenderDrawLine(renderer, renderX + 1, rippleY, renderX + 3 + spread, rippleY);
        SDL_SetRenderDrawColor(renderer, 96, 207, 248, 56);
        SDL_RenderDrawLine(renderer, renderX - 2, rippleY + 2 + spread, renderX + 2, rippleY + 2 + spread);

        SDL_SetRenderDrawColor(renderer, previousColor.r, previousColor.g, previousColor.b, previousColor.a);
        SDL_SetRenderDrawBlendMode(renderer, previousBlend);
        return;
    }
    if (protection > 0 && (spawnProtectionFrames - protection) / 4 % 2 != 0) {
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
        sprite = shockTimer ? 0x55 : prone ? 0x54 : walking ?
            (firePoseTimer ? 0x51 + BaseWalkPose() : Running[stride]) : 0x50;
        flipX = walking && !shockTimer && !prone && facing < 0;
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
        sprite = firePoseTimer ? 0x0d + stride % 3 : Running[stride];
    }
    sprites.Draw(renderer, sprite, renderX + xOffset, renderY + yOffset, flipX, second, feet, flipY);
    if (barrier && (barrier > 80 || frame / 4 % 2)) {
        SDL_SetRenderDrawColor(renderer, 76, 192, 255, 255);
        SDL_Rect shield{renderX - 12, renderY - 31, 24, 32};
        SDL_RenderDrawRect(renderer, &shield);
    }
}
