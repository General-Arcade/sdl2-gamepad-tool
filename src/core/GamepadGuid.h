#ifndef GAMEPADGUID_H
#define GAMEPADGUID_H

#define SDL_MAIN_HANDLED
#include "SDL.h"

namespace GamepadGuid {

// Zero the name CRC embedded in an SDL joystick GUID and return the original
// CRC so callers can preserve it in a mapping's crc field.
Uint16 zeroCrc(SDL_JoystickGUID &guid);


} // namespace GamepadGuid

#endif // GAMEPADGUID_H
