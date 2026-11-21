#include "AboutMenu.h"
#include "CFG.h"
#include "Map.h"

#include <cstdlib>
#include <ctime>

AboutMenu::AboutMenu() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

AboutMenu::~AboutMenu() = default;

void AboutMenu::Draw(SDL_Renderer* renderer) {
    if (!preview) {
        preview.reset(new Map(renderer));
        preview->StartDemo();
    }
    preview->Draw(renderer);
    auto* text = CCFG::getText();
    text->Draw(renderer, preview->GetResult() == Map::Result::MissingAssets ? "RUN MAKE ASSETS" : "JUNGLE TEST MODE", 8, 4, 8);
    text->Draw(renderer, "ESC / ENTER: MAIN MENU", 8, CCFG::GAME_HEIGHT - 12, 8);
}

void AboutMenu::Update() {
    if (preview) preview->Update();
}

void AboutMenu::enter() {
    preview.reset();
    auto* menus = CCFG::getMM();
    menus->resetGameState(MenuManager::eMainMenu);
    menus->setGameState(MenuManager::eMainMenu);
    CCFG::getMusic()->StopMusic();
}
