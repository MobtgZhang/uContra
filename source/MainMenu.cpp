#include "MainMenu.h"
#include "Application.h"
#include "CFG.h"

namespace {
constexpr Uint32 flashHalfPeriod = 250;
constexpr Uint32 confirmationDuration = 3 * 2 * flashHalfPeriod;
}

MainMenu::MainMenu() {
    lMO.push_back(new MenuOption("START", 256, 288));
    lMO.push_back(new MenuOption("OPTIONS", 256, 320));
    lMO.push_back(new MenuOption("ABOUT", 256, 352));
    numOfMenuOptions = lMO.size();
}

void MainMenu::Draw(SDL_Renderer* display) {
    CCFG::getLogo()->Draw(display, 160, 0);
    CCFG::getText()->Draw(display, "AUTHOR:MOBTGZHANG", 5, CCFG::GAME_HEIGHT - 16, 8);
    if (phase == Phase::Title) {
        Menu::Draw(display);
        return;
    }
    SDL_SetRenderDrawColor(display, 4, 4, 4, 255);
    SDL_RenderFillRect(display, &rSelectLevel);
    SDL_SetRenderDrawColor(display, 255, 255, 255, 255);
    SDL_RenderDrawRect(display, &rSelectLevel);
    const std::string label = "SELECT LEVEL";
    auto* text = CCFG::getText();
    text->Draw(display, label, rSelectLevel.x + (rSelectLevel.w - text->getTextWidth(label)) / 2, rSelectLevel.y + 16);
    const bool showSelected = phase != Phase::ConfirmLevel ||
        ((SDL_GetTicks() - openingCue.startedAt) / flashHalfPeriod) % 2 != 0;
    for (int index = 0; index < Map::StageCount; ++index) {
        const bool selected = index == activeLevelID;
        if (selected && !showSelected) continue;
        const int color = selected ? 255 : 105;
        text->Draw(display, std::to_string(index + 1), rSelectLevel.x + 32 + index * 32,
                   rSelectLevel.y + 42, 16, color, color, selected ? 0 : color);
    }
}

void MainMenu::escape() {
    if (phase == Phase::ConfirmLevel) CCFG::getMusic()->StopMusic();
    phase = Phase::Title;
}

void MainMenu::enter() {
    if (phase == Phase::SelectLevel) {
        openingCue = LoadingMenu::PlayOpeningCue();
        phase = Phase::ConfirmLevel;
        return;
    }
    if (phase != Phase::Title) return;

    auto* menus = CCFG::getMM();
    switch (activeMenuOption) {
        case Start:
            activeLevelID = 0;
            phase = Phase::SelectLevel;
            break;
        case Options: {
            auto* options = menus->getOptionsMenu();
            options->setEscapeToMainMenu(true);
            menus->resetGameState(MenuManager::eOptions);
            options->updateVolumeRect();
            menus->setGameState(MenuManager::eOptions);
            break;
        }
        case About:
            menus->setGameState(MenuManager::eAbout);
            break;
    }
}

void MainMenu::Update() {
    if (phase != Phase::ConfirmLevel || SDL_GetTicks() - openingCue.startedAt < confirmationDuration) return;

    auto* map = Application::getMap();
    map->NewGame(activeLevelID + 1, 1);
    auto* menus = CCFG::getMM();
    auto* loading = menus->getLoadingMenu();
    if (map->GetResult() == Map::Result::MissingAssets) {
        loading->Begin(LoadingMenu::Mode::MissingAssets);
    } else {
        loading->BeginStage(openingCue);
    }
    phase = Phase::Title;
    menus->setGameState(MenuManager::eGameLoading);
}

void MainMenu::updateActiveButton(int direction) {
    if (phase == Phase::Title) {
        Menu::updateActiveButton(direction);
        return;
    }
    if (phase != Phase::SelectLevel) return;
    if (direction == iLEFT_ITEM) {
        activeLevelID = (activeLevelID + Map::StageCount - 1) % Map::StageCount;
    } else if (direction == iRIGHT_ITEM) {
        activeLevelID = (activeLevelID + 1) % Map::StageCount;
    }
}
