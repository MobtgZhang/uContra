#include "LoadingMenu.h"
#include "Application.h"
#include "CFG.h"

namespace {
constexpr Uint32 openingCueDuration = 5107;
constexpr Uint32 openingSilenceDuration = 500;
constexpr Uint32 scoreBlinkHalfPeriod = 250;
}

LoadingMenu::LoadingMenu() {
    lMO.push_back(new MenuOption("CONTINUE", 337, 305));
    lMO.push_back(new MenuOption("END", 337, 339));
    numOfMenuOptions = lMO.size();
}

void LoadingMenu::Begin(Mode next) {
    mode = next;
    activeMenuOption = next == Mode::GameOver && Application::getMap()->Continues() == 0 ? 1 : 0;
    switch (next) {
        case Mode::Stage: BeginStage(PlayOpeningCue()); break;
        case Mode::GameOver: CCFG::getMusic()->PlayMusic(Music::mGAMEOVER, 0); break;
        case Mode::Ending: CCFG::getMusic()->PlayMusic(Music::mSANDINISTA, 0); break;
        case Mode::MissingAssets: CCFG::getMusic()->StopMusic(); break;
    }
}

LoadingMenu::OpeningCue LoadingMenu::PlayOpeningCue() {
    const Uint32 startedAt = SDL_GetTicks();
    return {startedAt, CCFG::getMusic()->PlayMusic(Music::mTITLE, 0)};
}

void LoadingMenu::BeginStage(OpeningCue cue) {
    openingCue = cue;
    waitingAfterCue = false;
    mode = Mode::Stage;
    start = SDL_GetTicks();
}

void LoadingMenu::Update() {
    if (mode != Mode::Stage) return;
    const Uint32 now = SDL_GetTicks();
    if (!waitingAfterCue) {
        if (openingCue.playing ? CCFG::getMusic()->IsMusicPlaying() :
                                 now - openingCue.startedAt < openingCueDuration) return;
        cueEndedAt = openingCue.playing ? now : openingCue.startedAt + openingCueDuration;
        waitingAfterCue = true;
    }
    if (now - cueEndedAt < openingSilenceDuration) return;
    Application::getMap()->PlayStageMusic();
    CCFG::getMM()->setGameState(MenuManager::eGame);
}

void LoadingMenu::Draw(SDL_Renderer* display) {
    auto* map = Application::getMap();
    auto* text = CCFG::getText();
    text->Draw(display, "1P", 85, 84);
    text->Draw(display, "HI", 580, 84);
    if (mode != Mode::Stage || ((SDL_GetTicks() - start) / scoreBlinkHalfPeriod) % 2 == 0) {
        text->Draw(display, std::to_string(map->Score()), 133, 84);
        text->Draw(display, std::to_string(map->HighScore()), 628, 84);
    }
    switch (mode) {
        case Mode::Stage:
            text->Draw(display, "REST " + std::to_string(map->Lives()), 85, 115);
            text->DrawCenterX(display, "STAGE " + std::to_string(map->StageNumber()), 219);
            text->DrawCenterX(display, map->StageName(), 255);
            break;
        case Mode::GameOver:
            text->DrawCenterX(display, "GAME OVER", 222);
            text->DrawCenterX(display, "CONTINUES " + std::to_string(map->Continues()), 255);
            Menu::Draw(display);
            break;
        case Mode::Ending:
            text->DrawCenterX(display, "MISSION COMPLETE", 198, 24);
            text->DrawCenterX(display, "RED FALCON HAS BEEN DEFEATED", 242);
            text->DrawCenterX(display, "PRESS ENTER", 350);
            break;
        case Mode::MissingAssets:
            text->DrawCenterX(display, "STAGE ASSETS MISSING", 210);
            text->DrawCenterX(display, "RUN MAKE ASSETS THEN MAKE BUILD", 250);
            text->DrawCenterX(display, "PRESS ENTER", 350);
            break;
    }
}

void LoadingMenu::updateActiveButton(int direction) {
    if (mode == Mode::GameOver && Application::getMap()->Continues() > 0) Menu::updateActiveButton(direction);
}

void LoadingMenu::enter() {
    auto* map = Application::getMap();
    if (mode == Mode::Stage) return;
    if (mode == Mode::GameOver && activeMenuOption == 0 && map->Continues() > 0) {
        map->Continue();
        Begin(map->GetResult() == Map::Result::MissingAssets ? Mode::MissingAssets : Mode::Stage);
    } else {
        ReturnToTitle();
    }
}

void LoadingMenu::escape() {
    ReturnToTitle();
}

void LoadingMenu::ReturnToTitle() {
    CCFG::getMM()->setGameState(MenuManager::eMainMenu);
    CCFG::getMusic()->StopMusic();
}
