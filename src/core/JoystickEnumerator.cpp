#include "JoystickEnumerator.h"
#include "GamepadGuid.h"
#include "SDL_gamecontroller.h"

std::vector<JoystickInfo> enumerateJoysticks() {
    std::vector<JoystickInfo> result;
    int count = SDL_NumJoysticks();

    for (int i = 0; i < count; ++i) {
        SDL_Joystick *joystick = SDL_JoystickOpen(i);
        if (!joystick) {
            continue;
        }

        JoystickInfo info;
        info.index = i;
        info.isGameController = SDL_IsGameController(i);

        SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
        GamepadGuid::zeroCrc(guid);
        char guidString[33];
        SDL_JoystickGetGUIDString(guid, guidString, sizeof(guidString));
        info.guid = QString::fromLatin1(guidString);

        if (info.isGameController) {
            SDL_GameController *gamepad = SDL_GameControllerOpen(i);
            if (gamepad) {
                info.name = QString(SDL_GameControllerName(gamepad));
                SDL_GameControllerClose(gamepad);
            } else {
                info.name = QString(SDL_JoystickName(joystick));
            }
        } else {
            info.name = QString(SDL_JoystickName(joystick));
        }

        SDL_JoystickClose(joystick);
        result.push_back(std::move(info));
    }

    return result;
}
