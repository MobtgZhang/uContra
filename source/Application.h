# ifndef APPLICATION_H
# define APPLICATION_H
# include <SDL2/SDL.h>
# include "Map.h"

class Application{
private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    static Map* oMap;
public:
    Application();
    ~Application();
    void mainloop();
    void Draw();
    void Update();
    void Input();
    //加载主菜单信息
    void InputMenu(SDL_Keycode key);
    // 加载玩家信息
    void InputLayer();
    // 设置游戏退出
    static bool quitGame;
    // 加载地图信息
    static Map* getMap();
};

# endif
