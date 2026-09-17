#include "GamepadMapper.h"
#include "GamepadGuid.h"

const int GamepadMapper::BINDING_ORDER[BINDING_COUNT] = {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_MISC1,
    SDL_CONTROLLER_BUTTON_PADDLE1,
    SDL_CONTROLLER_BUTTON_PADDLE2,
    SDL_CONTROLLER_BUTTON_PADDLE3,
    SDL_CONTROLLER_BUTTON_PADDLE4,
    SDL_CONTROLLER_BUTTON_TOUCHPAD,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTX_NEGATIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTX_POSITIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTY_NEGATIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTY_POSITIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTX_NEGATIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTX_POSITIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTY_NEGATIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTY_POSITIVE,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_TRIGGERRIGHT,
};

GamepadMapper::GamepadMapper()
    : m_arrBindings{},
      m_iCurrentBinding(0),
      m_unPendingAdvanceTime(0),
      m_bBindingComplete(SDL_FALSE)
{
}

void GamepadMapper::setButtonPressCallback(ButtonPressCallback callback)
{
    m_buttonPressCallback = std::move(callback);
}

void GamepadMapper::reset(SDL_Joystick *joystick)
{
    m_iCurrentBinding = 0;

    for (int i = 0; i < BINDING_COUNT; i++) {
        m_arrBindings[i].bindType = SDL_CONTROLLER_BINDTYPE_NONE;
        m_arrBindings[i].value.axis.axis = 0;
        m_arrBindings[i].value.axis.axis_max = 0;
        m_arrBindings[i].value.axis.axis_min = 0;
        m_arrBindings[i].value.button = 0;
        m_arrBindings[i].value.hat.hat = 0;
        m_arrBindings[i].value.hat.hat_mask = 0;
    }

    int nNumAxes = SDL_JoystickNumAxes(joystick);
    m_arrAxisState.assign(nNumAxes, AxisState{});
    for (int iIndex = 0; iIndex < nNumAxes; ++iIndex) {
        AxisState *pAxisState = &m_arrAxisState[iIndex];
        Sint16 nInitialValue = 0;
        pAxisState->m_bMoving = SDL_JoystickGetAxisInitialState(joystick, iIndex, &nInitialValue);
        pAxisState->m_nLastValue = nInitialValue;
        pAxisState->m_nStartingValue = nInitialValue;
        pAxisState->m_nFarthestValue = nInitialValue;
    }

    m_bBindingComplete = SDL_FALSE;
    m_unPendingAdvanceTime = 0;

    if (m_buttonPressCallback) {
        m_buttonPressCallback(m_iCurrentBinding, true);
    }
}

int GamepadMapper::currentBinding() const
{
    return m_iCurrentBinding;
}

bool GamepadMapper::isComplete() const
{
    return m_bBindingComplete == SDL_TRUE;
}

void GamepadMapper::setCurrentBinding(int iBinding)
{
    if (iBinding < 0) {
        return;
    }

    if (iBinding >= BINDING_COUNT) {
        m_bBindingComplete = SDL_TRUE;
        return;
    }

    if (BINDING_ORDER[iBinding] == -1) {
        setCurrentBinding(iBinding + 1);
        return;
    }

    if (m_buttonPressCallback) {
        m_buttonPressCallback(m_iCurrentBinding, false);
    }
    m_iCurrentBinding = iBinding;
    if (m_buttonPressCallback) {
        m_buttonPressCallback(m_iCurrentBinding, true);
    }

    SDL_GameControllerExtendedBind *pBinding = &m_arrBindings[BINDING_ORDER[m_iCurrentBinding]];
    SDL_zerop(pBinding);

    for (int iIndex = 0; iIndex < static_cast<int>(m_arrAxisState.size()); ++iIndex) {
        m_arrAxisState[iIndex].m_nFarthestValue = m_arrAxisState[iIndex].m_nStartingValue;
    }

    m_unPendingAdvanceTime = 0;
}

int GamepadMapper::standardizeAxisValue(int nValue)
{
    if (nValue > SDL_JOYSTICK_AXIS_MAX / 2) {
        return SDL_JOYSTICK_AXIS_MAX;
    } else if (nValue < SDL_JOYSTICK_AXIS_MIN / 2) {
        return SDL_JOYSTICK_AXIS_MIN;
    } else {
        return 0;
    }
}

void GamepadMapper::configureBinding(const SDL_GameControllerExtendedBind *pBinding)
{
    SDL_GameControllerExtendedBind *pCurrent;
    int iCurrentElement = BINDING_ORDER[m_iCurrentBinding];

    /* Do we already have this binding? */
    for (int iIndex = 0; iIndex < (int)SDL_arraysize(m_arrBindings); ++iIndex) {
        pCurrent = &m_arrBindings[iIndex];
        if (bBindingContainsBinding(pCurrent, pBinding)) {
            /* Already have this binding, ignore it */
            return;
        }
    }

    /* Should the new binding override the existing one? */
    pCurrent = &m_arrBindings[iCurrentElement];
    if (pCurrent->bindType != SDL_CONTROLLER_BINDTYPE_NONE) {
        SDL_bool bNativeDPad, bCurrentDPad;
        SDL_bool bNativeAxis, bCurrentAxis;

        bNativeDPad = (SDL_bool)(iCurrentElement == SDL_CONTROLLER_BUTTON_DPAD_UP ||
                       iCurrentElement == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
                       iCurrentElement == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                       iCurrentElement == SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        bCurrentDPad = (SDL_bool)(pCurrent->bindType == SDL_CONTROLLER_BINDTYPE_HAT);
        if (bNativeDPad && bCurrentDPad) {
            /* We already have a binding of the type we want, ignore the new one */
            return;
        }

        bNativeAxis = (SDL_bool)(iCurrentElement >= SDL_CONTROLLER_BUTTON_MAX);
        bCurrentAxis = (SDL_bool)(pCurrent->bindType == SDL_CONTROLLER_BINDTYPE_AXIS);
        if (bNativeAxis == bCurrentAxis &&
            (pBinding->bindType != SDL_CONTROLLER_BINDTYPE_AXIS ||
            pBinding->value.axis.axis != pCurrent->value.axis.axis)) {
            /* We already have a binding of the type we want, ignore the new one */
            return;
        }
    }

    *pCurrent = *pBinding;

    if (pBinding->committed) {
        m_unPendingAdvanceTime = SDL_GetTicks();
    } else {
        m_unPendingAdvanceTime = 0;
    }
}

SDL_bool GamepadMapper::bBindingContainsBinding(const SDL_GameControllerExtendedBind *pBindingA,
                                                 const SDL_GameControllerExtendedBind *pBindingB)
{
    if (pBindingA->bindType != pBindingB->bindType) {
        return SDL_FALSE;
    }
    switch (pBindingA->bindType) {
    case SDL_CONTROLLER_BINDTYPE_AXIS:
        if (pBindingA->value.axis.axis != pBindingB->value.axis.axis) {
            return SDL_FALSE;
        }

        if (!pBindingA->committed) {
            return SDL_FALSE;
        }

        {
            int minA = SDL_min(pBindingA->value.axis.axis_min, pBindingA->value.axis.axis_max);
            int maxA = SDL_max(pBindingA->value.axis.axis_min, pBindingA->value.axis.axis_max);
            int minB = SDL_min(pBindingB->value.axis.axis_min, pBindingB->value.axis.axis_max);
            int maxB = SDL_max(pBindingB->value.axis.axis_min, pBindingB->value.axis.axis_max);
            return (SDL_bool)(minA <= minB && maxA >= maxB);
        }
    default:
        return (SDL_bool)(SDL_memcmp(pBindingA, pBindingB, sizeof(*pBindingA)) == 0);
    }
}

SDL_bool GamepadMapper::bMergeAxisBindings(int iIndex)
{
    SDL_GameControllerExtendedBind *pBindingA = &m_arrBindings[iIndex];
    SDL_GameControllerExtendedBind *pBindingB = &m_arrBindings[iIndex + 1];
    if (pBindingA->bindType == SDL_CONTROLLER_BINDTYPE_AXIS &&
        pBindingB->bindType == SDL_CONTROLLER_BINDTYPE_AXIS &&
        pBindingA->value.axis.axis == pBindingB->value.axis.axis) {
        if (pBindingA->value.axis.axis_min == pBindingB->value.axis.axis_min) {
            pBindingA->value.axis.axis_min = pBindingA->value.axis.axis_max;
            pBindingA->value.axis.axis_max = pBindingB->value.axis.axis_max;
            pBindingB->bindType = SDL_CONTROLLER_BINDTYPE_NONE;
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

void GamepadMapper::processAxisMotion(int axis, int value, SDL_Joystick *joystick)
{
    if (axis < 0 || axis >= static_cast<int>(m_arrAxisState.size())) {
        return;
    }
    const int MAX_ALLOWED_JITTER = SDL_JOYSTICK_AXIS_MAX / 80;
    AxisState *pAxisState = &m_arrAxisState[axis];
    int nCurrentDistance, nFarthestDistance;

    if (!pAxisState->m_bMoving) {
        Sint16 nInitialValue = 0;
        pAxisState->m_bMoving = SDL_JoystickGetAxisInitialState(joystick, axis, &nInitialValue);
        pAxisState->m_nLastValue = value;
        pAxisState->m_nStartingValue = nInitialValue;
        pAxisState->m_nFarthestValue = nInitialValue;
    } else if (SDL_abs(value - pAxisState->m_nLastValue) <= MAX_ALLOWED_JITTER) {
        return;
    } else {
        pAxisState->m_nLastValue = value;
    }

    nCurrentDistance = SDL_abs(value - pAxisState->m_nStartingValue);
    nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
    if (nCurrentDistance > nFarthestDistance) {
        pAxisState->m_nFarthestValue = value;
        nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
    }

    if (nFarthestDistance >= AXIS_COMMIT_DISTANCE) {
        SDL_bool bCommitBinding = (nCurrentDistance <= AXIS_RETURN_DISTANCE) ? SDL_TRUE : SDL_FALSE;
        SDL_GameControllerExtendedBind binding;
        SDL_zero(binding);
        binding.bindType = SDL_CONTROLLER_BINDTYPE_AXIS;
        binding.value.axis.axis = axis;
        binding.value.axis.axis_min = standardizeAxisValue(pAxisState->m_nStartingValue);
        binding.value.axis.axis_max = standardizeAxisValue(pAxisState->m_nFarthestValue);
        binding.committed = bCommitBinding;
        configureBinding(&binding);
    }
}

bool GamepadMapper::checkPendingAdvance()
{
    if (m_unPendingAdvanceTime && SDL_GetTicks() - m_unPendingAdvanceTime >= PENDING_ADVANCE_DELAY_MS) {
        setCurrentBinding(m_iCurrentBinding + 1);
        return true;
    }
    return false;
}

void GamepadMapper::clearAxisState()
{
    m_arrAxisState.clear();
}

QString GamepadMapper::generateMappingString(SDL_JoystickID joystickId)
{
    SDL_JoystickGUID guid;
    char mapping[2048];
    char pszElement[12];

    SDL_Joystick *joystick = SDL_JoystickFromInstanceID(joystickId);
    if (!joystick) {
        clearAxisState();
        return QString();
    }
    guid = SDL_JoystickGetGUID(joystick);
    Uint16 crc = GamepadGuid::zeroCrc(guid);
    SDL_JoystickGetGUIDString(guid, mapping, SDL_arraysize(mapping));
    SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
    SDL_strlcat(mapping, "%GAMEPAD_NAME%", SDL_arraysize(mapping));
    SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
    SDL_strlcat(mapping, "platform:", SDL_arraysize(mapping));
    SDL_strlcat(mapping, SDL_GetPlatform(), SDL_arraysize(mapping));
    SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
    if (crc) {
        char crc_string[5];
        SDL_strlcat(mapping, "crc:", SDL_arraysize(mapping));
        SDL_snprintf(crc_string, sizeof(crc_string), "%.4x", crc);
        SDL_strlcat(mapping, crc_string, SDL_arraysize(mapping));
        SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
    }

    for (int iIndex = 0; iIndex < (int)SDL_arraysize(m_arrBindings); ++iIndex) {
        SDL_GameControllerExtendedBind *pBinding = &m_arrBindings[iIndex];
        if (pBinding->bindType == SDL_CONTROLLER_BINDTYPE_NONE) {
            continue;
        }

        if (iIndex < SDL_CONTROLLER_BUTTON_MAX) {
            SDL_GameControllerButton eButton = (SDL_GameControllerButton)iIndex;
            SDL_strlcat(mapping, SDL_GameControllerGetStringForButton(eButton), SDL_arraysize(mapping));
        } else {
            const char *pszAxisName = nullptr;
            switch (iIndex - SDL_CONTROLLER_BUTTON_MAX) {
            case SDL_CONTROLLER_BINDING_AXIS_LEFTX_NEGATIVE:
                if (!bMergeAxisBindings(iIndex)) {
                    SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                }
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_LEFTX);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_LEFTX_POSITIVE:
                SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_LEFTX);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_LEFTY_NEGATIVE:
                if (!bMergeAxisBindings(iIndex)) {
                    SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                }
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_LEFTY);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_LEFTY_POSITIVE:
                SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_LEFTY);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_RIGHTX_NEGATIVE:
                if (!bMergeAxisBindings(iIndex)) {
                    SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                }
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_RIGHTX);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_RIGHTX_POSITIVE:
                SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_RIGHTX);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_RIGHTY_NEGATIVE:
                if (!bMergeAxisBindings(iIndex)) {
                    SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                }
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_RIGHTY);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_RIGHTY_POSITIVE:
                SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_RIGHTY);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_TRIGGERLEFT:
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                break;
            case SDL_CONTROLLER_BINDING_AXIS_TRIGGERRIGHT:
                pszAxisName = SDL_GameControllerGetStringForAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
                break;
            }
            if (pszAxisName) {
                SDL_strlcat(mapping, pszAxisName, SDL_arraysize(mapping));
            }
        }
        SDL_strlcat(mapping, ":", SDL_arraysize(mapping));

        pszElement[0] = '\0';
        switch (pBinding->bindType) {
        case SDL_CONTROLLER_BINDTYPE_BUTTON:
            SDL_snprintf(pszElement, sizeof(pszElement), "b%d", pBinding->value.button);
            break;
        case SDL_CONTROLLER_BINDTYPE_AXIS:
            if (pBinding->value.axis.axis_min == 0 && pBinding->value.axis.axis_max == SDL_JOYSTICK_AXIS_MIN) {
                SDL_snprintf(pszElement, sizeof(pszElement), "-a%d", pBinding->value.axis.axis);
            } else if (pBinding->value.axis.axis_min == 0 && pBinding->value.axis.axis_max == SDL_JOYSTICK_AXIS_MAX) {
                SDL_snprintf(pszElement, sizeof(pszElement), "+a%d", pBinding->value.axis.axis);
            } else {
                SDL_snprintf(pszElement, sizeof(pszElement), "a%d", pBinding->value.axis.axis);
                if (pBinding->value.axis.axis_min > pBinding->value.axis.axis_max) {
                    SDL_strlcat(pszElement, "~", SDL_arraysize(pszElement));
                }
            }
            break;
        case SDL_CONTROLLER_BINDTYPE_HAT:
            SDL_snprintf(pszElement, sizeof(pszElement), "h%d.%d", pBinding->value.hat.hat, pBinding->value.hat.hat_mask);
            break;
        default:
            break;
        }
        SDL_strlcat(mapping, pszElement, SDL_arraysize(mapping));
        SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
    }

    clearAxisState();
    return QString::fromUtf8(mapping);
}
