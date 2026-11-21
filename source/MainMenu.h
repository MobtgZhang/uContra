# pragma once
# ifndef MAINMENU_H
# define MAINMENU_H

# include"LoadingMenu.h"

class MainMenu:public Menu{
private:
    enum TitleOption { Start, Options, About };
    enum class Phase { Title, SelectLevel, ConfirmLevel };
    Phase phase = Phase::Title;
    LoadingMenu::OpeningCue openingCue{0, false};
    int activeLevelID = 0;
    SDL_Rect rSelectLevel{220, 275, 306, 72};
public:
    MainMenu();

    void escape();
    void enter();
    void Draw(SDL_Renderer* rR) override;
    void Update() override;
    void updateActiveButton(int iDir) override;
};

# endif
