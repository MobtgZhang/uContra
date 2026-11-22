# pragma once
# ifndef OPTIONSMENU_H
# define OPTIONSMENU_H

# include "Menu.h"

class OptionsMenu:public Menu{
private:
    SDL_Rect rRect;
    SDL_Rect rSetKeyRect;
    SDL_Rect rVolumeBG;
    SDL_Rect rVolume;
    
    bool inSetKey,resetSetKey;

    bool escapeToMainMenu;
    SDL_Scancode livesRepeatKey = SDL_SCANCODE_UNKNOWN;
    Uint32 lastLivesIncrease = 0;
public:
    enum OptionMenuState{
        optionVOLUME,
        optionLEFT,
        optionDOWN,
        optionRIGHT,
        optionUP,
        optionA,
        optionB,
        optionX,
        optionY,
        optionMOVEBACKWARD,
        optionLIVES,
        optionMAINMENU
    };
    OptionsMenu();
    ~OptionsMenu();

    void escape();
    void enter();
    void setKey(int keyID);
    bool HandleLivesIncrease(const SDL_Keysym& key);
    void StopLivesIncrease(SDL_Scancode key = SDL_SCANCODE_UNKNOWN);
    bool IsBindingKey() const { return inSetKey; }
    void setEscapeToMainMenu(bool escapeToMainMenu);
    void updateVolumeRect();

    void updateActiveButton(int iDir);
    void Draw(SDL_Renderer* rR);
    void Update();

};
# endif
