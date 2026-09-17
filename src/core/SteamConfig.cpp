#include "SteamConfig.h"

#include "GamepadMapping.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStringList>
#include <QVector>
#include <cctype>

namespace {

enum class TokenType {
    String,
    OpenBrace,
    CloseBrace,
};

struct Token {
    TokenType type;
    int start;
    int end;
    QByteArray value;
};

struct ObjectLocation {
    int openToken = -1;
    int closeToken = -1;
};

struct PropertyLocation {
    int valueToken = -1;
};

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool tokenize(const QByteArray &contents, QVector<Token> *tokens, QString *error)
{
    int position = 0;
    while (position < contents.size()) {
        const unsigned char character = static_cast<unsigned char>(contents[position]);
        if (std::isspace(character)) {
            ++position;
            continue;
        }

        if (contents[position] == '/' && position + 1 < contents.size()) {
            if (contents[position + 1] == '/') {
                position += 2;
                while (position < contents.size() && contents[position] != '\n') {
                    ++position;
                }
                continue;
            }
            if (contents[position + 1] == '*') {
                const int commentEnd = contents.indexOf("*/", position + 2);
                if (commentEnd < 0) {
                    return fail(error, QStringLiteral("Unterminated VDF block comment"));
                }
                position = commentEnd + 2;
                continue;
            }
        }

        if (contents[position] == '{') {
            tokens->append({TokenType::OpenBrace, position, position + 1, {}});
            ++position;
            continue;
        }
        if (contents[position] == '}') {
            tokens->append({TokenType::CloseBrace, position, position + 1, {}});
            ++position;
            continue;
        }

        if (contents[position] == '"') {
            const int start = position++;
            QByteArray decoded;
            bool closed = false;
            while (position < contents.size()) {
                const char current = contents[position++];
                if (current == '"') {
                    closed = true;
                    break;
                }
                if (current != '\\' || position >= contents.size()) {
                    decoded.append(current);
                    continue;
                }

                const char escaped = contents[position++];
                switch (escaped) {
                case 'n': decoded.append('\n'); break;
                case 'r': decoded.append('\r'); break;
                case 't': decoded.append('\t'); break;
                case '"': decoded.append('"'); break;
                case '\\': decoded.append('\\'); break;
                default:
                    decoded.append('\\');
                    decoded.append(escaped);
                    break;
                }
            }
            if (!closed) {
                return fail(error, QStringLiteral("Unterminated quoted string in VDF"));
            }
            tokens->append({TokenType::String, start, position, decoded});
            continue;
        }

        const int start = position;
        while (position < contents.size()) {
            const char current = contents[position];
            if (std::isspace(static_cast<unsigned char>(current)) || current == '{' || current == '}') {
                break;
            }
            ++position;
        }
        if (position == start) {
            return fail(error, QStringLiteral("Unexpected character in VDF at byte %1").arg(position));
        }
        tokens->append({TokenType::String, start, position, contents.mid(start, position - start)});
    }
    return true;
}

int matchingClose(const QVector<Token> &tokens, int openToken)
{
    int depth = 0;
    for (int index = openToken; index < tokens.size(); ++index) {
        if (tokens[index].type == TokenType::OpenBrace) {
            ++depth;
        } else if (tokens[index].type == TokenType::CloseBrace && --depth == 0) {
            return index;
        }
    }
    return -1;
}

bool findObject(const QVector<Token> &tokens, int begin, int end, const QByteArray &name,
                ObjectLocation *location)
{
    int index = begin;
    while (index < end) {
        if (tokens[index].type != TokenType::String || index + 1 >= end) {
            ++index;
            continue;
        }

        const Token &key = tokens[index];
        const Token &value = tokens[index + 1];
        if (value.type != TokenType::OpenBrace) {
            index += 2;
            continue;
        }

        const int closeToken = matchingClose(tokens, index + 1);
        if (closeToken < 0 || closeToken >= end) {
            return false;
        }
        if (key.value == name) {
            location->openToken = index + 1;
            location->closeToken = closeToken;
            return true;
        }
        if (findObject(tokens, index + 2, closeToken, name, location)) {
            return true;
        }
        index = closeToken + 1;
    }
    return false;
}

bool findDirectProperty(const QVector<Token> &tokens, const ObjectLocation &object,
                        const QByteArray &name, PropertyLocation *location, QString *error)
{
    int index = object.openToken + 1;
    while (index < object.closeToken) {
        if (tokens[index].type != TokenType::String || index + 1 >= object.closeToken) {
            ++index;
            continue;
        }

        const Token &key = tokens[index];
        const Token &value = tokens[index + 1];
        if (value.type == TokenType::OpenBrace) {
            const int closeToken = matchingClose(tokens, index + 1);
            if (closeToken < 0 || closeToken > object.closeToken) {
                return fail(error, QStringLiteral("Malformed nested object in InstallConfigStore"));
            }
            if (key.value == name) {
                return fail(error, QStringLiteral("SDL_GamepadBind is an object, not a string"));
            }
            index = closeToken + 1;
            continue;
        }

        if (value.type != TokenType::String) {
            return fail(error, QStringLiteral("Malformed property in InstallConfigStore"));
        }
        if (key.value == name) {
            location->valueToken = index + 1;
            return true;
        }
        index += 2;
    }
    return true;
}

QByteArray quoteValue(const QString &value)
{
    QByteArray encoded = value.toUtf8();
    encoded.replace("\\", "\\\\");
    encoded.replace("\"", "\\\"");
    return '"' + encoded + '"';
}

bool updateBindings(const QString &existing, const QString &mapping, QString *updated,
                    QString *error)
{
    const QString cleanMapping = mapping.trimmed();
    if (cleanMapping.contains('\n') || cleanMapping.contains('\r')) {
        return fail(error, QStringLiteral("Controller mapping must be a single line"));
    }
    const QString mappingKey = GamepadMapping::key(cleanMapping);
    if (mappingKey.isEmpty()) {
        return fail(error, QStringLiteral("Controller mapping has no GUID"));
    }
    if (existing.isEmpty()) {
        *updated = cleanMapping;
        return true;
    }

    const QString lineEnding = existing.contains(QStringLiteral("\r\n"))
        ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QString normalized = existing;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace('\r', '\n');
    const QStringList lines = normalized.split('\n', Qt::KeepEmptyParts);

    QStringList result;
    bool replaced = false;
    for (const QString &line : lines) {
        if (GamepadMapping::key(line.trimmed()) == mappingKey) {
            if (!replaced) {
                result.append(cleanMapping);
                replaced = true;
            }
        } else {
            result.append(line);
        }
    }

    if (!replaced) {
        int insertionIndex = result.size();
        while (insertionIndex > 0 && result[insertionIndex - 1].trimmed().isEmpty()) {
            --insertionIndex;
        }
        result.insert(insertionIndex, cleanMapping);
    }

    *updated = result.join(lineEnding);
    return true;
}

bool isIndentation(const QByteArray &value)
{
    for (const char character : value) {
        if (character != ' ' && character != '\t') {
            return false;
        }
    }
    return true;
}

} // namespace

QString SteamConfig::defaultPath()
{
    const QDir home = QDir::home();

#ifdef Q_OS_WIN
    QStringList candidates;

    QSettings userRegistry(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"),
        QSettings::NativeFormat);
    const QString userSteamPath = userRegistry.value(QStringLiteral("SteamPath")).toString();
    if (!userSteamPath.isEmpty()) {
        candidates.append(QDir(userSteamPath).filePath(QStringLiteral("config/config.vdf")));
    }

    QSettings machineRegistry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam"),
        QSettings::Registry32Format);
    const QString machineSteamPath =
        machineRegistry.value(QStringLiteral("InstallPath")).toString();
    if (!machineSteamPath.isEmpty()) {
        candidates.append(QDir(machineSteamPath).filePath(QStringLiteral("config/config.vdf")));
    }

    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    if (!programFilesX86.isEmpty()) {
        candidates.append(
            QDir(programFilesX86).filePath(QStringLiteral("Steam/config/config.vdf")));
    }

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }
    return candidates.isEmpty()
        ? QStringLiteral("C:/Program Files (x86)/Steam/config/config.vdf")
        : QDir::cleanPath(candidates.first());
#elif defined(Q_OS_MACOS)
    return home.filePath(QStringLiteral("Library/Application Support/Steam/config/config.vdf"));
#else
    const QString deckPath = home.filePath(QStringLiteral(".steam/steam/config/config.vdf"));
    if (QFile::exists(deckPath)) {
        return deckPath;
    }

    const QString standardPath =
        home.filePath(QStringLiteral(".local/share/Steam/config/config.vdf"));
    return QFile::exists(standardPath) ? standardPath : deckPath;
#endif
}

QString SteamConfig::backupPath(const QString &configPath)
{
    return configPath + QStringLiteral(".gamepad-tool.bak");
}

bool SteamConfig::updateMapping(const QByteArray &contents, const QString &mapping,
                                QByteArray *updatedContents, QString *error)
{
    if (!updatedContents) {
        return fail(error, QStringLiteral("No destination provided for updated VDF"));
    }

    QVector<Token> tokens;
    if (!tokenize(contents, &tokens, error)) {
        return false;
    }

    ObjectLocation installConfigStore;
    if (!findObject(tokens, 0, tokens.size(), QByteArrayLiteral("InstallConfigStore"),
                    &installConfigStore)) {
        return fail(error, QStringLiteral("InstallConfigStore was not found in Steam config"));
    }

    PropertyLocation gamepadBind;
    if (!findDirectProperty(tokens, installConfigStore, QByteArrayLiteral("SDL_GamepadBind"),
                            &gamepadBind, error)) {
        return false;
    }

    if (gamepadBind.valueToken >= 0) {
        const Token &valueToken = tokens[gamepadBind.valueToken];
        QString updatedBindings;
        if (!updateBindings(QString::fromUtf8(valueToken.value), mapping, &updatedBindings, error)) {
            return false;
        }
        *updatedContents = contents;
        updatedContents->replace(valueToken.start, valueToken.end - valueToken.start,
                                 quoteValue(updatedBindings));
        return true;
    }

    QString updatedBindings;
    if (!updateBindings(QString(), mapping, &updatedBindings, error)) {
        return false;
    }

    const int closePosition = tokens[installConfigStore.closeToken].start;
    const int previousNewline = contents.lastIndexOf('\n', closePosition - 1);
    int insertionPosition = previousNewline < 0 ? closePosition : previousNewline + 1;
    QByteArray closeIndent = contents.mid(insertionPosition, closePosition - insertionPosition);
    QByteArray prefix;
    if (!isIndentation(closeIndent)) {
        insertionPosition = closePosition;
        closeIndent.clear();
        prefix = contents.contains("\r\n") ? "\r\n" : "\n";
    }

    const QByteArray lineEnding = contents.contains("\r\n") ? "\r\n" : "\n";
    const QByteArray propertyIndent = closeIndent + '\t';
    const QByteArray property = prefix + propertyIndent + "\"SDL_GamepadBind\"\t\t"
        + quoteValue(updatedBindings) + lineEnding;

    *updatedContents = contents;
    updatedContents->insert(insertionPosition, property);
    return true;
}

bool SteamConfig::saveMapping(const QString &configPath, const QString &mapping, QString *error)
{
    QFileInfo configInfo(configPath);
    if (!configInfo.exists() || !configInfo.isFile()) {
        return fail(error, QStringLiteral("Steam config was not found at %1").arg(configPath));
    }

    const QString resolvedPath = configInfo.canonicalFilePath();
    QFile configFile(resolvedPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        return fail(error, QStringLiteral("Unable to read %1: %2")
                     .arg(resolvedPath, configFile.errorString()));
    }
    const QByteArray originalContents = configFile.readAll();
    configFile.close();

    QByteArray updatedContents;
    if (!updateMapping(originalContents, mapping, &updatedContents, error)) {
        return false;
    }
    if (updatedContents == originalContents) {
        return true;
    }

    const QString resolvedBackupPath = backupPath(resolvedPath);
    QSaveFile backupFile(resolvedBackupPath);
    if (!backupFile.open(QIODevice::WriteOnly) || backupFile.write(originalContents) != originalContents.size()
        || !backupFile.commit()) {
        return fail(error, QStringLiteral("Unable to write backup %1: %2")
                     .arg(resolvedBackupPath, backupFile.errorString()));
    }

    QSaveFile outputFile(resolvedPath);
    if (!outputFile.open(QIODevice::WriteOnly) || outputFile.write(updatedContents) != updatedContents.size()
        || !outputFile.commit()) {
        return fail(error, QStringLiteral("Unable to update %1: %2")
                     .arg(resolvedPath, outputFile.errorString()));
    }
    return true;
}
