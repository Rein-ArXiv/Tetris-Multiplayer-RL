#include "menu.h"

Menu::Menu()
{

}

void Menu::Draw()
{
    DrawText("Press SPACE or ENTER to Start", 100, 300, 20, WHITE);
}

void Menu::HandleInput(bool &gameStart)
{
    int keyPressed = GetKeyPressed();
    if (keyPressed == KEY_ENTER || keyPressed == KEY_SPACE) {
        gameStart = true;
    }
}