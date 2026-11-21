#ifndef ABOUTMENU_H
#define ABOUTMENU_H

#include "Menu.h"

#include <memory>

class Map;

class AboutMenu : public Menu {
public:
    AboutMenu();
    ~AboutMenu();
    void Draw(SDL_Renderer* renderer);
    void Update();
    void enter();

private:
    std::unique_ptr<Map> preview;
};

#endif
