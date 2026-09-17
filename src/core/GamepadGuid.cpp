#include "GamepadGuid.h"

namespace GamepadGuid {

Uint16 zeroCrc(SDL_JoystickGUID &guid)
{
    Uint16 crc = 0;
    SDL_GetJoystickGUIDInfo(guid, nullptr, nullptr, nullptr, &crc);
    if (crc != 0) {
        guid.data[2] = 0;
        guid.data[3] = 0;
    }
    return crc;
}

} // namespace GamepadGuid
