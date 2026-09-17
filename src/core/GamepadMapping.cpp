#include "GamepadMapping.h"

namespace GamepadMapping {

QString key(const QString &mapping)
{
    QString result;
    const int commaPosition = mapping.indexOf(',');
    if (commaPosition > 0) {
        result = mapping.left(commaPosition);
    }

    static const QString crcTag = QStringLiteral(",crc:");
    int crcPosition = mapping.indexOf(crcTag);
    if (crcPosition >= 0) {
        ++crcPosition;
        int crcEnd = mapping.indexOf(',', crcPosition);
        if (crcEnd < 0) {
            crcEnd = mapping.length();
        }
        result += ',' + mapping.mid(crcPosition, crcEnd - crcPosition);
    }
    return result;
}

} // namespace GamepadMapping
