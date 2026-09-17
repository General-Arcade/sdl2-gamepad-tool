#ifndef GAMEPADMAPPING_H
#define GAMEPADMAPPING_H

#include <QString>

namespace GamepadMapping {

// Return the GUID+CRC identity used to replace one controller mapping without
// collapsing controller variants that share a base GUID.
QString key(const QString &mapping);

} // namespace GamepadMapping

#endif // GAMEPADMAPPING_H
