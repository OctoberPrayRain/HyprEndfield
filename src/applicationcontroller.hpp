#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QLocalServer;
class QQuickWindow;
class SettingsController;

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hudVisible READ hudVisible NOTIFY hudVisibleChanged)
    Q_PROPERTY(bool hudOpen READ hudOpen NOTIFY hudOpenChanged)
    Q_PROPERTY(QString localUserName READ localUserName CONSTANT)
    Q_PROPERTY(QString layerScope READ layerScope NOTIFY layerScopeChanged)
    // What the profile badge prints under its number: the session's compositor,
    // taken from the same environment variables the integrations use, falling
    // back to XDG_CURRENT_DESKTOP and then to "Wayland".
    Q_PROPERTY(QString compositorName READ compositorName CONSTANT)

public:
    explicit ApplicationController(QObject *parent = nullptr);
    ~ApplicationController() override;

    [[nodiscard]] bool hudVisible() const;
    [[nodiscard]] bool hudOpen() const;
    [[nodiscard]] QString localUserName() const { return qEnvironmentVariable("USER"); }
    [[nodiscard]] QString compositorName() const;
    void setWindow(QQuickWindow *window);
    void setSettings(SettingsController *settings);
    bool listen(const QString &serverName);
    void executeCommand(const QString &command);
    QString layerScope() const;
    Q_INVOKABLE void setProjectionActive(bool active);

public slots:
    void showHud();
    void hideHud();
    void toggleHud();
    void completeHide();
    void quitApplication();
    void beginFileDrag();
    void endFileDrag(bool delivered);

signals:
    void hudVisibleChanged();
    void hudOpenChanged();
    void layerScopeChanged();

private slots:
    void acceptConnection();

private:
    void activateCompositorMode();
    void resetCompositorMode();
    void setHudVisible(bool visible);
    void setHudOpen(bool open);
    void finishFileDrag();
    void selectScreen();
    void checkFocus();
    void recreateSurface();
    void showAndFocusWindow();
    void finishFocusRequest();

    QPointer<QQuickWindow> m_window;
    QLocalServer *m_server = nullptr;
    SettingsController *m_settings = nullptr;
    QTimer m_focusRequestTimeout;
    bool m_receivedFocus = false;
    bool m_projectionActive = false;
    bool m_hudVisible = false;
    bool m_hudOpen = false;
    bool m_compositorModeActive = false;
    bool m_fileDragActive = false;
    bool m_fileDragFinished = false;
    bool m_fileDragDelivered = false;
    bool m_fileDragRetracted = false;
};
