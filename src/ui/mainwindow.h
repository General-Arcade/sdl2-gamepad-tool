#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define APP_NAME "SDL2 Gamepad Tool v1.4.2"

#include <QMainWindow>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <memory>
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_gamecontroller.h"
#include "sdlpollevent.h"
#include "filedownloader.h"
#include "GamepadDatabase.h"
#include "Environment.h"

struct ImagesInfo {
    int index;
    const char *imagePath;
};

enum GamepadBgImage {
    GAMEPAD_BG_IMAGE_FRONT,
    GAMEPAD_BG_IMAGE_BACK,
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void showAppVersion();
    void searchJoysticks();
    void setAppMode(AppMode mode);
    void setupImages();
    void hideAllImages();
    void closeActiveGamepad();
    void updateDeviceIdentifiers();
    void copyDeviceIdentifier(QToolButton *button, const QString &name);
    void pauseSDLPolling();
    void resumeSDLPolling();
    void setGamepadBgImage(GamepadBgImage bgImage);
    void appendToLog(const QString &text);
    void updateDeleteLocalMappingState();

    std::unique_ptr<Ui::MainWindow> ui;
    SDL_Joystick *m_currentJoystick;
    SDL_GameController *m_currentGamepad;
    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<SDLPollEvent> m_worker;
    FileDownloader *m_downloader;
    GamepadDatabase m_database;
    std::unique_ptr<Environment> m_environment;
    GamepadBgImage m_gamepadBgImage;
    AppMode m_currentAppMode;
    QMenu *m_copyMenu;
    QLabel *m_images[SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX];
    QToolButton *m_vendorIdButton;
    QToolButton *m_productIdButton;
    QLabel *m_gamepadFacingIndicator;

    static const ImagesInfo s_imagesInfo[SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX];

private slots:
    void on_gamepadComboBox_currentIndexChanged(int index);
    void copyGuid();
    void copyMappingString();
    void on_deleteLocalMappingButton_clicked();
    void on_newBindings_clicked();
    void on_cancelButton_clicked();
    void on_skipButton_clicked();
    void on_previousButton_clicked();
    void on_setEnvVarButton_clicked();
    void on_resetEnvVarButton_clicked();
    void showGamepadImage(int index, bool show);
    void newMappingAvailable(QString mapping);
    void addJoystick(int index);
    void removeJoystick(int index);
    void dbFileDownloaded();
};

#endif // MAINWINDOW_H
