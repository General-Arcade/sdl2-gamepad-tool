#include <QtTest>

#include "SteamConfig.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class TestSteamConfig : public QObject
{
    Q_OBJECT

private slots:
    void testReplaceMatchingMappingPreservesOtherContent();
    void testAppendMappingPreservesLineEndings();
    void testInsertMissingProperty();
    void testSaveCreatesBackup();
    void testMissingInstallConfigStoreRejected();
};

void TestSteamConfig::testReplaceMatchingMappingPreservesOtherContent()
{
    const QByteArray original =
        "\"InstallConfigStore\"\n"
        "{\n"
        "\t\"Unrelated\"\t\t\"keep\"\n"
        "\t\"SDL_GamepadBind\"\t\t\"03000000000000000000000000000000,Old Pad,a:b0,crc:1111,\n"
        "05000000000000000000000000000000,Other Pad,a:b1,\"\n"
        "\t\"Nested\"\n"
        "\t{\n"
        "\t\t\"SDL_GamepadBind\"\t\t\"nested-value\"\n"
        "\t}\n"
        "}\n";
    const QString replacement = QStringLiteral(
        "03000000000000000000000000000000,Updated Pad,a:b2,crc:1111,");

    QByteArray updated;
    QString error;
    QVERIFY2(SteamConfig::updateMapping(original, replacement, &updated, &error),
             qPrintable(error));

    QVERIFY(updated.contains("\"Unrelated\"\t\t\"keep\""));
    QVERIFY(updated.contains("03000000000000000000000000000000,Updated Pad,a:b2,crc:1111,"));
    QVERIFY(!updated.contains("03000000000000000000000000000000,Old Pad"));
    QVERIFY(updated.contains("05000000000000000000000000000000,Other Pad,a:b1,"));
    QVERIFY(updated.contains("\"SDL_GamepadBind\"\t\t\"nested-value\""));
}

void TestSteamConfig::testAppendMappingPreservesLineEndings()
{
    const QByteArray original =
        "\"InstallConfigStore\"\r\n"
        "{\r\n"
        "\t\"SDL_GamepadBind\"\t\t\"03000000000000000000000000000000,First Pad,a:b0,\r\n\"\r\n"
        "}\r\n";
    const QString mapping = QStringLiteral(
        "05000000000000000000000000000000,Second Pad,a:b1,");

    QByteArray updated;
    QString error;
    QVERIFY2(SteamConfig::updateMapping(original, mapping, &updated, &error),
             qPrintable(error));

    QVERIFY(updated.contains(
        "03000000000000000000000000000000,First Pad,a:b0,\r\n"
        "05000000000000000000000000000000,Second Pad,a:b1,\r\n"));
}

void TestSteamConfig::testInsertMissingProperty()
{
    const QByteArray original =
        "\"InstallConfigStore\"\n"
        "{\n"
        "\t\"Unrelated\"\t\t\"keep\"\n"
        "}\n";
    const QString mapping = QStringLiteral(
        "03000000000000000000000000000000,Inserted Pad,a:b0,");

    QByteArray updated;
    QString error;
    QVERIFY2(SteamConfig::updateMapping(original, mapping, &updated, &error),
             qPrintable(error));

    QCOMPARE(updated,
             QByteArray(
                 "\"InstallConfigStore\"\n"
                 "{\n"
                 "\t\"Unrelated\"\t\t\"keep\"\n"
                 "\t\"SDL_GamepadBind\"\t\t\"03000000000000000000000000000000,Inserted Pad,a:b0,\"\n"
                 "}\n"));
}

void TestSteamConfig::testSaveCreatesBackup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString configPath = QDir(temporaryDirectory.path()).filePath("config.vdf");
    const QByteArray original =
        "\"InstallConfigStore\"\n"
        "{\n"
        "\t\"SDL_GamepadBind\"\t\t\"\"\n"
        "}\n";
    QFile configFile(configPath);
    QVERIFY(configFile.open(QIODevice::WriteOnly));
    QCOMPARE(configFile.write(original), original.size());
    configFile.close();

    QString error;
    QVERIFY2(SteamConfig::saveMapping(
                 configPath,
                 QStringLiteral("03000000000000000000000000000000,Saved Pad,a:b0,"),
                 &error),
             qPrintable(error));

    QFile backupFile(SteamConfig::backupPath(configPath));
    QVERIFY(backupFile.open(QIODevice::ReadOnly));
    QCOMPARE(backupFile.readAll(), original);

    QVERIFY(configFile.open(QIODevice::ReadOnly));
    QVERIFY(configFile.readAll().contains(
        "03000000000000000000000000000000,Saved Pad,a:b0,"));
}

void TestSteamConfig::testMissingInstallConfigStoreRejected()
{
    const QByteArray original = "\"OtherObject\"\n{\n\t\"value\" \"keep\"\n}\n";
    QByteArray updated;
    QString error;

    QVERIFY(!SteamConfig::updateMapping(
        original,
        QStringLiteral("03000000000000000000000000000000,Pad,a:b0,"),
        &updated,
        &error));
    QVERIFY(error.contains("InstallConfigStore"));
}

QTEST_MAIN(TestSteamConfig)
#include "tst_steamconfig.moc"
