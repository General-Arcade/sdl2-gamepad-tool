#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QClipboard>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QInputDialog>
#include "Environment.h"
#include "Logger.h"
#include "JoystickEnumerator.h"
#include "GamepadGuid.h"

const ImagesInfo MainWindow::s_imagesInfo[SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX] = {
    {SDL_CONTROLLER_BUTTON_A, ":/images/PadSetup-Button-3"},
    {SDL_CONTROLLER_BUTTON_B, ":/images/PadSetup-Button-2"},
    {SDL_CONTROLLER_BUTTON_X, ":/images/PadSetup-Button-4"},
    {SDL_CONTROLLER_BUTTON_Y, ":/images/PadSetup-Button-1"},
    {SDL_CONTROLLER_BUTTON_BACK, ":/images/PadSetup-Select"},
    {SDL_CONTROLLER_BUTTON_GUIDE, ":/images/PadSetup-X"},
    {SDL_CONTROLLER_BUTTON_START, ":/images/PadSetup-Start"},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK, ":/images/PadSetup-Stick-Left"},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, ":/images/PadSetup-Stick-Right"},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, ":/images/PadSetup-Shift-Left"},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, ":/images/PadSetup-Shift-Right"},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, ":/images/PadSetup-Dpad-Up"},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, ":/images/PadSetup-Dpad-Down"},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, ":/images/PadSetup-Dpad-Left"},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, ":/images/PadSetup-Dpad-Right"},
    {SDL_CONTROLLER_BUTTON_MISC1, ":/images/PadSetup-Share"},
    {SDL_CONTROLLER_BUTTON_PADDLE1, ":/images/PadSetup-BackBtn-1"},
    {SDL_CONTROLLER_BUTTON_PADDLE2, ":/images/PadSetup-BackBtn-2"},
    {SDL_CONTROLLER_BUTTON_PADDLE3, ":/images/PadSetup-BackBtn-3"},
    {SDL_CONTROLLER_BUTTON_PADDLE4, ":/images/PadSetup-BackBtn-4"},
    {SDL_CONTROLLER_BUTTON_TOUCHPAD, ":/images/PadSetup-Touch"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTX_NEGATIVE, ":/images/PadSetup-LStick-Arrow-Left"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTX_POSITIVE, ":/images/PadSetup-LStick-Arrow-Right"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTY_NEGATIVE, ":/images/PadSetup-LStick-Arrow-Up"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_LEFTY_POSITIVE, ":/images/PadSetup-LStick-Arrow-Down"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTX_NEGATIVE, ":/images/PadSetup-RStick-Arrow-Left"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTX_POSITIVE, ":/images/PadSetup-RStick-Arrow-Right"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTY_NEGATIVE, ":/images/PadSetup-RStick-Arrow-Up"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_RIGHTY_POSITIVE, ":/images/PadSetup-RStick-Arrow-Down"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_TRIGGERLEFT, ":/images/PadSetup-Trigger-Left"},
    {SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_TRIGGERRIGHT, ":/images/PadSetup-Trigger-Right"},
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>()),
    m_currentJoystick(nullptr),
    m_currentGamepad(nullptr),
    m_downloader(nullptr),
    m_environment(Environment::create()),
    m_gamepadBgImage(GAMEPAD_BG_IMAGE_FRONT),
    m_currentAppMode(APPMODE_TEST),
    m_vendorIdButton(nullptr),
    m_productIdButton(nullptr),
    m_gamepadFacingIndicator(nullptr) {

    memset(m_images, 0, sizeof(m_images));
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint
                   | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    ui->setupUi(this);
    statusBar()->setSizeGripEnabled(false);
    m_vendorIdButton = new QToolButton(this);
    m_productIdButton = new QToolButton(this);
    const auto setupIdentifierButton = [this](QToolButton *button, const QString &toolTip) {
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(toolTip);
        button->hide();
        ui->statusBar->addPermanentWidget(button);
    };
    setupIdentifierButton(m_vendorIdButton, "Copy the USB vendor ID");
    setupIdentifierButton(m_productIdButton, "Copy the USB product ID");
    connect(m_vendorIdButton, &QToolButton::clicked, this, [this]() {
        copyDeviceIdentifier(m_vendorIdButton, QStringLiteral("VID"));
    });
    connect(m_productIdButton, &QToolButton::clicked, this, [this]() {
        copyDeviceIdentifier(m_productIdButton, QStringLiteral("PID"));
    });

    connect(&Logger::instance(), &Logger::messageLogged, this, &MainWindow::appendToLog);
    m_database.initialize(QApplication::applicationDirPath());
    m_database.loadMappings();

    setupImages();
    setGamepadBgImage(GAMEPAD_BG_IMAGE_FRONT);
    showAppVersion();

    m_copyMenu = new QMenu(this);
    m_copyMenu->addAction("Copy Gamepad GUID", this, &MainWindow::copyGuid);
    m_copyMenu->addAction("Copy Mapping String", this, &MainWindow::copyMappingString);
    ui->copyButton->setMenu(m_copyMenu);

    searchJoysticks();

    m_thread = std::make_unique<QThread>();
    m_worker = std::make_unique<SDLPollEvent>();
    m_worker->moveToThread(m_thread.get());

    connect(m_worker.get(), &SDLPollEvent::buttonPressed, this, &MainWindow::showGamepadImage);
    connect(m_worker.get(), &SDLPollEvent::mappingReady, this, &MainWindow::newMappingAvailable);
    connect(m_worker.get(), &SDLPollEvent::joystickAdded, this, &MainWindow::addJoystick);
    connect(m_worker.get(), &SDLPollEvent::joystickRemoved, this, &MainWindow::removeJoystick);
    connect(m_worker.get(), &SDLPollEvent::workRequested, m_thread.get(), [this]() { m_thread->start(); });
    connect(m_thread.get(), &QThread::started, m_worker.get(), &SDLPollEvent::doWork);
    connect(m_worker.get(), &SDLPollEvent::finished, m_thread.get(), &QThread::quit, Qt::DirectConnection);
    setAppMode(APPMODE_TEST);

    QString env = m_environment->get("SDL_GAMECONTROLLERCONFIG");
    if (!env.isEmpty()) {
        Logger::instance().info(QString("Environment variable \"SDL_GAMECONTROLLERCONFIG\" is set to: \"%1\"").arg(env));
        ui->resetEnvVarButton->setEnabled(true);
    } else {
        Logger::instance().info(QStringLiteral("Environment variable \"SDL_GAMECONTROLLERCONFIG\" is not defined"));
        ui->resetEnvVarButton->setEnabled(false);
    }

    QUrl dburl(GamepadDatabase::DB_URL);
    m_downloader = new FileDownloader(dburl, this);
    connect(m_downloader, &FileDownloader::downloaded, this, &MainWindow::dbFileDownloaded);
    Logger::instance().info(QStringLiteral("Checking if new mappings available from github: https://github.com/mdqinc/SDL_GameControllerDB"));
}

MainWindow::~MainWindow() {
    m_worker->abort();
    m_thread->wait();
    Logger::instance().debug(QString("Deleting thread and worker in Thread %1")
                             .arg(reinterpret_cast<quintptr>(QObject::thread()->currentThreadId())));
    closeActiveGamepad();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->accept();
    QApplication::quit();
}

void MainWindow::appendToLog(const QString &text) {
    ui->logTextEdit->appendPlainText(text);
    ui->logTextEdit->ensureCursorVisible();
}

void MainWindow::showAppVersion() {
    SDL_version compiled;
    SDL_version linked_;
    SDL_GetVersion(&linked_);
    SDL_VERSION(&compiled);
    Logger::instance().info(QString("%1 by General Arcade (compiled with SDL version %2.%3.%4, DLL version %5.%6.%7)")
                            .arg(APP_NAME)
                            .arg(compiled.major).arg(compiled.minor).arg(compiled.patch)
                            .arg(linked_.major).arg(linked_.minor).arg(linked_.patch));
    Logger::instance().info(QStringLiteral("Website: http://generalarcade.com/gamepadtool/\n"));
}

void MainWindow::dbFileDownloaded() {
    if (m_currentAppMode != APPMODE_TEST) {
        return;
    }

    bool updated = m_database.processDownloadedData(m_downloader->downloadedData());
    if (!updated) {
        return;
    }

    QApplication::processEvents();
    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText("New mappings available. Do you want to load them?");
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::Yes);
    msg.setIcon(QMessageBox::Information);
    if (msg.exec() == QMessageBox::Yes) {
        QApplication::processEvents();
        m_database.loadMappings();
        pauseSDLPolling();
        searchJoysticks();
        resumeSDLPolling();
    }
}

void MainWindow::setGamepadBgImage(GamepadBgImage image) {
    m_gamepadBgImage = image;
    if (image == GAMEPAD_BG_IMAGE_FRONT) {
        ui->mainImage->setPixmap(QPixmap(":/images/PadSetup-Front"));
        m_gamepadFacingIndicator->setVisible(false);
    } else {
        ui->mainImage->setPixmap(QPixmap(":/images/PadSetup-Back"));
        m_gamepadFacingIndicator->setVisible(true);
    }
}

void MainWindow::setAppMode(AppMode mode) {
    ui->cancelButton->setVisible(mode == APPMODE_MAP);
    ui->previousButton->setVisible(mode == APPMODE_MAP);
    ui->skipButton->setVisible(mode == APPMODE_MAP);

    ui->copyButton->setVisible(mode != APPMODE_MAP);
    ui->deleteLocalMappingButton->setVisible(mode != APPMODE_MAP);
    ui->newBindings->setVisible(mode != APPMODE_MAP);

    ui->setEnvVarButton->setEnabled((mode != APPMODE_MAP) && m_currentGamepad);
    ui->resetEnvVarButton->setEnabled(mode != APPMODE_MAP
                                      && !m_environment->get("SDL_GAMECONTROLLERCONFIG").isEmpty());

    ui->gamepadComboBox->setEnabled(mode == APPMODE_TEST);

    m_currentAppMode = mode;
    m_worker->mode = mode;
    if (m_currentJoystick) {
        m_worker->joystickId = SDL_JoystickInstanceID(m_currentJoystick);
    }
    m_worker->abort();
    m_thread->wait();
    m_worker->requestWork();
}

void MainWindow::showGamepadImage(int index, bool show) {
    if (index < 0 || index >= (SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX)) {
        return;
    }
    if (m_images[index] == nullptr) {
        return;
    }
    GamepadBgImage previousBgImage = m_gamepadBgImage;
    if (show) {
        if (index >= SDL_CONTROLLER_BUTTON_PADDLE1 && index <= SDL_CONTROLLER_BUTTON_PADDLE4) {
            setGamepadBgImage(GAMEPAD_BG_IMAGE_BACK);
        } else {
            setGamepadBgImage(GAMEPAD_BG_IMAGE_FRONT);
        }
        if (previousBgImage != m_gamepadBgImage) {
            hideAllImages();
        }
    }
    m_images[index]->setVisible(show);
}

void MainWindow::closeActiveGamepad() {
    if (m_currentGamepad) {
        SDL_GameControllerClose(m_currentGamepad);
        m_currentGamepad = nullptr;
    }
    if (m_currentJoystick) {
        SDL_JoystickClose(m_currentJoystick);
        m_currentJoystick = nullptr;
    }
    updateDeviceIdentifiers();
}

void MainWindow::updateDeviceIdentifiers() {
    if (!m_vendorIdButton || !m_productIdButton) {
        return;
    }
    if (!m_currentJoystick) {
        m_vendorIdButton->hide();
        m_productIdButton->hide();
        return;
    }

    const auto formatIdentifier = [](Uint16 identifier) {
        return identifier == 0
            ? QStringLiteral("n/a")
            : QStringLiteral("0x%1").arg(identifier, 4, 16, QLatin1Char('0'));
    };

    const auto updateButton = [&formatIdentifier](QToolButton *button, const QString &name,
                                                  Uint16 identifier) {
        const QString value = formatIdentifier(identifier);
        button->setText(QStringLiteral("%1: %2").arg(name, value));
        button->setProperty("identifier", identifier == 0 ? QString() : value);
        button->setEnabled(identifier != 0);
        button->show();
    };

    updateButton(m_vendorIdButton, QStringLiteral("VID"), SDL_JoystickGetVendor(m_currentJoystick));
    updateButton(m_productIdButton, QStringLiteral("PID"), SDL_JoystickGetProduct(m_currentJoystick));
}

void MainWindow::copyDeviceIdentifier(QToolButton *button, const QString &name) {
    const QString identifier = button->property("identifier").toString();
    if (identifier.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(identifier);
    ui->statusBar->showMessage(
        QStringLiteral("Copied %1 %2 to clipboard").arg(name, identifier), 2000);
}

void MainWindow::addJoystick(int i) {
    Logger::instance().info(QString("addJoystick %1").arg(i));
    pauseSDLPolling();
    searchJoysticks();
    resumeSDLPolling();
}

void MainWindow::removeJoystick(int i) {
    Logger::instance().info(QString("removeJoystick %1").arg(i));
    pauseSDLPolling();
    searchJoysticks();
    resumeSDLPolling();
}

void MainWindow::pauseSDLPolling() {
    m_worker->abort();
    m_thread->wait();
}

void MainWindow::resumeSDLPolling() {
    setAppMode(m_currentAppMode);
}

void MainWindow::searchJoysticks() {
    closeActiveGamepad();
    int selectedIndex = ui->gamepadComboBox->currentIndex();
    if (selectedIndex < 0) {
        selectedIndex = 0;
    }
    ui->gamepadComboBox->clear();
    Logger::instance().info(QStringLiteral("Searching gamepads..."));

    std::vector<JoystickInfo> joysticks = enumerateJoysticks();

    if (!joysticks.empty()) {
        Logger::instance().info(QString("Found %1 gamepad(s):").arg(joysticks.size()));
        ui->statusBar->showMessage(QString("Found %1 gamepad(s)").arg(joysticks.size()));

        for (const auto &js : joysticks) {
            QString suffix = js.isGameController ? " (mapping available)" : "";
            Logger::instance().info(QString("\"%1\", %2%3").arg(js.name, js.guid, suffix));
            ui->gamepadComboBox->addItem(js.name + suffix);
        }
        ui->newBindings->setEnabled(true);
    } else {
        Logger::instance().info(QStringLiteral("No gamepads found"));
        ui->statusBar->showMessage(QStringLiteral("No gamepads found"));
        ui->gamepadComboBox->addItem("No gamepads found");
        ui->copyButton->setEnabled(false);
        ui->deleteLocalMappingButton->setEnabled(false);
        ui->newBindings->setEnabled(false);
        ui->setEnvVarButton->setEnabled(false);
    }
    if (selectedIndex >= ui->gamepadComboBox->count()) {
        selectedIndex = ui->gamepadComboBox->count() - 1;
    }
    ui->gamepadComboBox->setCurrentIndex(selectedIndex);
}

void MainWindow::on_gamepadComboBox_currentIndexChanged(int index) {
    if (index < 0) {
        return;
    }
    closeActiveGamepad();
    if (SDL_NumJoysticks() > 0) {
        m_currentJoystick = SDL_JoystickOpen(index);
        if (!m_currentJoystick) {
            Logger::instance().error(QString("Failed to open joystick at index %1").arg(index));
            ui->copyButton->setEnabled(false);
            ui->deleteLocalMappingButton->setEnabled(false);
            ui->setEnvVarButton->setEnabled(false);
            return;
        }
        bool isGamepad = SDL_IsGameController(index);
        if (isGamepad) {
            m_currentGamepad = SDL_GameControllerOpen(index);
        }
        ui->copyButton->setEnabled(isGamepad);
        ui->setEnvVarButton->setEnabled(isGamepad);
        updateDeleteLocalMappingState();
        updateDeviceIdentifiers();

        SDL_JoystickGUID guid = SDL_JoystickGetGUID(m_currentJoystick);
        char guidStr[64];
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        QString guidQStr(guidStr);
        if (guidQStr.endsWith("6800")) {
            Logger::instance().warning(
                QString("Device \"%1\" appears to be handled by an SDL built-in HID driver (GUID ends with 6800). "
                        "Mappings for this device can be modified locally but should NOT be submitted to community databases.")
                    .arg(SDL_JoystickName(m_currentJoystick)));
        } else if (guidQStr.mid(28, 2) == "78") {
            Logger::instance().warning(
                QString("Device \"%1\" appears to be handled by an XInput driver (GUID ends with 78XX). "
                        "Mappings for this device can be modified locally but should NOT be submitted to community databases.")
                    .arg(SDL_JoystickName(m_currentJoystick)));
        }
    }
}

void MainWindow::on_cancelButton_clicked() {
    setAppMode(APPMODE_TEST);
    setGamepadBgImage(GAMEPAD_BG_IMAGE_FRONT);
    hideAllImages();
}

void MainWindow::copyGuid() {
    if (!m_currentJoystick) {
        return;
    }
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(m_currentJoystick);
    GamepadGuid::zeroCrc(guid);
    char guidString[33];
    SDL_JoystickGetGUIDString(guid, guidString, sizeof(guidString));

    const char *name = m_currentGamepad ? SDL_GameControllerName(m_currentGamepad) : "";
    QApplication::clipboard()->setText(guidString);
    Logger::instance().info(QString("The GUID for \"%1\" is \"%2\"").arg(name, guidString));

    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText(QString("The GUID for \"%1\" copied to a clipboard buffer").arg(name));
    msg.setIcon(QMessageBox::Information);
    msg.exec();
}

void MainWindow::on_newBindings_clicked() {
    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText("Press the buttons on your gamepad when indicated "
                           "(Your gamepad may look different than the picture). "
                           "If you want to correct a mistake, click 'Back' button. "
                           "To skip a button, click 'Skip' button. "
                           "To exit mapping mode, click 'Cancel' button");
    msg.setIcon(QMessageBox::Information);
    msg.exec();
    setAppMode(APPMODE_MAP);
}

void MainWindow::copyMappingString() {
    if (!m_currentGamepad) {
        return;
    }
    const char *name = SDL_GameControllerName(m_currentGamepad);
    char *bindings = SDL_GameControllerMapping(m_currentGamepad);
    if (!bindings) {
        Logger::instance().error("No mapping available for this controller");
        return;
    }
    QApplication::clipboard()->setText(bindings);
    Logger::instance().info(QString("Mapping string for \"%1\" is \"%2\"").arg(name, bindings));
    SDL_free(bindings);

    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText(QString("Mapping string for \"%1\" copied to a clipboard buffer").arg(name));
    msg.setIcon(QMessageBox::Information);
    msg.exec();
}

void MainWindow::on_deleteLocalMappingButton_clicked() {
    if (!m_currentJoystick) {
        return;
    }
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(m_currentJoystick);
    char guidStr[64];
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));

    const char *name = m_currentGamepad ? SDL_GameControllerName(m_currentGamepad)
                                        : SDL_JoystickName(m_currentJoystick);

    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText(QString("Delete local mapping for \"%1\"?").arg(name));
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::No);
    msg.setIcon(QMessageBox::Question);
    if (msg.exec() != QMessageBox::Yes) {
        return;
    }

    if (m_database.deleteLocalMapping(guidStr)) {
        Logger::instance().info(QString("Deleted local mapping for \"%1\" (%2)").arg(name, guidStr));
        pauseSDLPolling();
        closeActiveGamepad();
        // Re-init game controller subsystem to clear cached mappings
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        m_database.loadMappings();
        searchJoysticks();
        resumeSDLPolling();
    } else {
        Logger::instance().error(QString("No local mapping found for \"%1\" (%2)").arg(name, guidStr));
    }
}

void MainWindow::updateDeleteLocalMappingState() {
    if (!m_currentJoystick) {
        ui->deleteLocalMappingButton->setEnabled(false);
        return;
    }
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(m_currentJoystick);
    char guidStr[64];
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
    ui->deleteLocalMappingButton->setEnabled(m_database.hasLocalMapping(guidStr));
}

void MainWindow::hideAllImages() {
    for (int i = 0; i < (SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX); i++) {
        if (m_images[i] != nullptr) {
            m_images[i]->setVisible(false);
        }
    }
}

void MainWindow::setupImages() {
    for (int i = 0; i < (SDL_CONTROLLER_BUTTON_MAX + SDL_CONTROLLER_BINDING_AXIS_MAX); i++) {
        m_images[i] = new QLabel(ui->mainImage);
        m_images[i]->setPixmap(QPixmap(s_imagesInfo[i].imagePath));
        m_images[i]->move(0, 0);
    }
    m_gamepadFacingIndicator = new QLabel(ui->mainImage);
    m_gamepadFacingIndicator->setPixmap(QPixmap(":/images/PadSetup-Rotate"));
    m_gamepadFacingIndicator->move(0, 0);
    m_gamepadFacingIndicator->setVisible(false);
    hideAllImages();
}

void MainWindow::on_skipButton_clicked() {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = CUSTOM_EVENT_NEXT;
    SDL_PushEvent(&event);
}

void MainWindow::on_previousButton_clicked() {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = CUSTOM_EVENT_PREVIOUS;
    SDL_PushEvent(&event);
}

void MainWindow::newMappingAvailable(QString mapping) {
    setAppMode(APPMODE_TEST);

    const char *name = nullptr;
    if (m_currentGamepad) {
        name = SDL_GameControllerName(m_currentGamepad);
    } else if (m_currentJoystick) {
        name = SDL_JoystickName(m_currentJoystick);
    } else {
        return;
    }
    if (!name) {
        name = "Unknown Gamepad";
    }

    bool ok;
    QString deviceName = QString(name).simplified();
    QString text = QInputDialog::getText(this, APP_NAME, "Enter gamepad name:",
                                         QLineEdit::Normal, deviceName, &ok);
    if (ok) {
        if (text.isEmpty()) {
            text = "Unknown Gamepad";
        } else {
            text.truncate(128);
        }
        text.remove(',');
        mapping.replace("%GAMEPAD_NAME%", text);
        Logger::instance().info(QString("Mapping string: '%1'").arg(mapping));

        m_database.saveLocalMapping(mapping);
        QByteArray mappingBytes = mapping.toUtf8();
        SDL_GameControllerAddMapping(mappingBytes.constData());
        pauseSDLPolling();
        searchJoysticks();
        resumeSDLPolling();
    }
}

void MainWindow::on_setEnvVarButton_clicked() {
    if (!m_currentGamepad) {
        return;
    }
    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText("This will set mapping for current gamepad as environment variable "
                           "\"SDL_GAMECONTROLLERCONFIG\", so all SDL2 games will pick this mapping "
                           "automatically. Continue?");
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::Yes);
    msg.setIcon(QMessageBox::Information);
    if (msg.exec() == QMessageBox::Yes) {
        char *bindings = SDL_GameControllerMapping(m_currentGamepad);
        if (!bindings) {
            Logger::instance().error("No mapping available for this controller");
            return;
        }
        Logger::instance().info(QString("Setting environment variable \"SDL_GAMECONTROLLERCONFIG\" to \"%1\"").arg(bindings));
        m_environment->set("SDL_GAMECONTROLLERCONFIG", bindings);
        SDL_free(bindings);
        ui->resetEnvVarButton->setEnabled(true);
#ifdef _WIN32
        QMessageBox winMsg;
        winMsg.setText(APP_NAME);
        winMsg.setInformativeText("You might need to restart your computer for the changes to take effect");
        winMsg.setIcon(QMessageBox::Information);
        winMsg.exec();
#endif
    }
}

void MainWindow::on_resetEnvVarButton_clicked() {
    m_environment->unset("SDL_GAMECONTROLLERCONFIG");
    Logger::instance().info(QStringLiteral("Resetting environment variable \"SDL_GAMECONTROLLERCONFIG\""));
    QMessageBox msg;
    msg.setText(APP_NAME);
    msg.setInformativeText("You might need to restart your computer for the changes to take effect");
    msg.setIcon(QMessageBox::Information);
    msg.exec();
    ui->resetEnvVarButton->setEnabled(false);
}
