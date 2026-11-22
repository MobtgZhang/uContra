#ifndef LOADINGMENU_H
#define LOADINGMENU_H

#include "Menu.h"

class LoadingMenu : public Menu {
public:
    enum class Mode { Stage, NextStage, Continue, GameOver, Ending, MissingAssets };
    struct OpeningCue {
        Uint32 startedAt;
        bool playing;
    };
    LoadingMenu();
    static OpeningCue PlayOpeningCue();
    void BeginStage(OpeningCue cue);
    void Begin(Mode next);
    void Draw(SDL_Renderer* renderer) override;
    void Update() override;
    void updateActiveButton(int direction) override;
    void enter();
    void escape();

private:
    bool IsScorePage() const;
    void ReturnToTitle();
    Mode mode = Mode::Stage;
    Uint32 start = 0;
    OpeningCue openingCue{0, false};
    bool waitingAfterCue = false;
    Uint32 cueEndedAt = 0;
};

#endif
