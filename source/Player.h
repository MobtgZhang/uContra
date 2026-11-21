# ifndef PLAYER_H
# define PLAYER_H

#include "Stage.h"

class Map;

class Player {
public:
    struct Input {
        bool left = false, right = false, up = false, down = false, jump = false, fire = false;
    };
    enum Weapon { Rifle, MachineGun, Fireball, Spread, Laser };

    void Reset(int startingLives);
    void Respawn(float worldX, float worldY, bool onGround = false);
    void EnterBaseRoom();
    void Update(Map& map, const Input& input, int index);
    void Draw(SDL_Renderer* renderer, const StageSprites& sprites, int cameraX, int cameraY,
              bool indoor, bool second, int groundInset) const;
    bool Hit(bool pit = false);
    void UpdateDefeat(const Map& map);
    void Collect(int item);
    void AddScore(int points);
    SDL_Rect Bounds() const;
    bool Active() const { return lives > 0 && respawnTimer == 0; }
    bool Defeated() const { return respawnTimer > 0; }

    float worldX = 0, worldY = 0;
    int lives = 0, score = 0, respawnTimer = 0;
    Weapon weapon = Rifle;

private:
    float verticalSpeed = 0;
    bool grounded = false, prone = false, swimming = false, walking = false;
    bool jumpHeld = false, rapid = false, tumbling = false, jumpLeft = false;
    int facing = 1, aimY = 0, aimX = 1, airDirection = 0;
    int runFrame = 0, jumpFrame = 0;
    int frame = 0, cooldown = 0, protection = 0, barrier = 0, dropTimer = 0, shockTimer = 0;
    int firePoseTimer = 0;
    int defeatFrame = 0;
    bool defeatLanded = false, defeatHidden = false;
};

# endif
