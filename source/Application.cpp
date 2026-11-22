#include "Application.h"
#include "CFG.h"

#include <SDL2/SDL_image.h>

Map* Application::oMap = nullptr;
bool Application::quitGame = false;

Map* Application::getMap() {
    return oMap;
}

Application::Application() {
    quitGame = false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("Cannot initialize SDL: %s", SDL_GetError());
        quitGame = true;
        return;
    }
    IMG_Init(IMG_INIT_PNG);
    m_window = SDL_CreateWindow("uContra", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                CCFG::GAME_WIDTH, CCFG::GAME_HEIGHT, SDL_WINDOW_SHOWN);
    if (m_window) {
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        if (!m_renderer) m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    }
    if (!m_renderer) {
        SDL_Log("Cannot open uContra window: %s", SDL_GetError());
        quitGame = true;
        return;
    }
    if (auto* icon = SDL_LoadBMP("files/images/ico.bmp")) {
        SDL_SetWindowIcon(m_window, icon);
        SDL_FreeSurface(icon);
    }
    CCFG::keyIDA = SDLK_j;
    CCFG::keyIDB = SDLK_k;
    CCFG::keyIDX = SDLK_l;
    CCFG::keyIDY = SDLK_i;
    CCFG::keyIDRight = SDLK_d;
    CCFG::keyIDLeft = SDLK_a;
    CCFG::keyIDUp = SDLK_w;
    CCFG::keyIDDown = SDLK_s;
    oMap = new Map(m_renderer);
    CCFG::getMM()->setActiveOption(m_renderer);
    CCFG::getLogo()->setImg("contra", m_renderer);
    CCFG::getText()->setFont(m_renderer, "font");
}

Application::~Application() {
    delete oMap;
    oMap = nullptr;
    CCFG::Release();
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    IMG_Quit();
    SDL_Quit();
}

void Application::mainloop() {
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previous = SDL_GetPerformanceCounter();
    Uint64 accumulated = frequency;
    while (!quitGame) {
        const Uint64 now = SDL_GetPerformanceCounter();
        accumulated += SDL_min((now - previous) * 60, frequency * 4);
        previous = now;
        Input();
        if (quitGame) break;
        if (accumulated >= frequency) {
            do {
                Update();
                accumulated -= frequency;
            } while (accumulated >= frequency);
            Draw();
            SDL_RenderPresent(m_renderer);
        } else {
            SDL_Delay(1);
        }
    }
}

void Application::Input() {
    auto* menu = CCFG::getMM();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            quitGame = true;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            if (menu->getGameState() == MenuManager::eGame) menu->escape();
            if (menu->getGameState() == MenuManager::eOptions) menu->getOptionsMenu()->StopLivesIncrease();
        } else if (event.type == SDL_KEYUP && menu->getGameState() == MenuManager::eOptions) {
            menu->getOptionsMenu()->StopLivesIncrease(event.key.keysym.scancode);
        } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            const SDL_Keycode key = event.key.keysym.sym;
            if (menu->getGameState() == MenuManager::eGame) {
                if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) menu->escape();
            } else if (menu->getGameState() != MenuManager::eOptions ||
                       !menu->getOptionsMenu()->HandleLivesIncrease(event.key.keysym)) {
                InputMenu(key);
            }
        }
    }
    if (menu->getGameState() == MenuManager::eGame) InputLayer();
}

void Application::InputMenu(SDL_Keycode key) {
    auto* menu = CCFG::getMM();
    if (menu->getGameState() == MenuManager::eOptions && menu->getOptionsMenu()->IsBindingKey()) {
        menu->setKey(key);
        return;
    }
    switch (key) {
        case SDLK_s: case SDLK_DOWN: menu->keyPressed(Menu::iDOWN_ITEM); break;
        case SDLK_w: case SDLK_UP: menu->keyPressed(Menu::iUP_ITEM); break;
        case SDLK_a: case SDLK_LEFT: menu->keyPressed(Menu::iLEFT_ITEM); break;
        case SDLK_d: case SDLK_RIGHT: menu->keyPressed(Menu::iRIGHT_ITEM); break;
        case SDLK_RETURN: case SDLK_KP_ENTER: menu->enter(); break;
        case SDLK_ESCAPE: menu->escape(); break;
        default: break;
    }
}

void Application::InputLayer() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const auto held = [keys](SDL_Keycode key) {
        const auto scancode = SDL_GetScancodeFromKey(key);
        return scancode != SDL_SCANCODE_UNKNOWN && keys[scancode] != 0;
    };
    Player::Input first;
    first.left = held(CCFG::keyIDLeft);
    first.right = held(CCFG::keyIDRight);
    first.up = held(CCFG::keyIDUp);
    first.down = held(CCFG::keyIDDown);
    first.jump = held(CCFG::keyIDA);
    first.fire = held(CCFG::keyIDB);
    Player::Input second;
    second.left = held(SDLK_LEFT);
    second.right = held(SDLK_RIGHT);
    second.up = held(SDLK_UP);
    second.down = held(SDLK_DOWN);
    second.jump = held(SDLK_RCTRL);
    second.fire = held(SDLK_RSHIFT);
    oMap->SetInput(0, first);
    oMap->SetInput(1, second);
}

void Application::Update() {
    CCFG::getMM()->Update();
}

void Application::Draw() {
    CCFG::getMM()->setBackgroundColor(m_renderer);
    SDL_RenderClear(m_renderer);
    CCFG::getMM()->Draw(m_renderer);
}
