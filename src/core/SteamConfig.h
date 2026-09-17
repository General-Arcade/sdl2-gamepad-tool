#ifndef STEAMCONFIG_H
#define STEAMCONFIG_H

#include <QByteArray>
#include <QString>

class SteamConfig
{
public:
    static QString defaultPath();
    static QString backupPath(const QString &configPath);

    // Update InstallConfigStore/SDL_GamepadBind while preserving all unrelated
    // VDF content. Exposed separately so parsing and replacement are testable
    // without touching a user's Steam configuration.
    static bool updateMapping(const QByteArray &contents, const QString &mapping,
                              QByteArray *updatedContents, QString *error = nullptr);

    // Save atomically after writing a sibling .gamepad-tool.bak backup.
    static bool saveMapping(const QString &configPath, const QString &mapping,
                            QString *error = nullptr);
};

#endif // STEAMCONFIG_H
