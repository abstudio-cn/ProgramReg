#pragma once
// MainWindow — 安装注册程序主界面（表单式，对应 ProgramReg 的 MainForm.cs）

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QThread>

#include "../Shared/InstallCore.h"

class QLineEdit;
class QCheckBox;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QLabel;

namespace esp { class ModernWindow; }

// 安装任务（后台线程）
class InstallTask : public QThread
{
    Q_OBJECT
public:
    explicit InstallTask(const esp::InstallOptions &opts, QObject *parent = nullptr);

signals:
    void progress(int percent);
    void logLine(const QString &line);
    void done(int failures, const QStringList &warnings);
    void failed(const QString &error);

protected:
    void run() override;

private:
    esp::InstallOptions opts_;
};

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // 显示内部 ModernWindow
    void show();

private slots:
    void startInstall();
    void onDone(int failures, const QStringList &warnings);
    void onFailed(const QString &error);

private:
    void pickDir();
    void pickMainExe();
    void pickUninstallExe();
    void pickInstallScript();
    void pickUninstallScript();
    bool validate(QString *msg);

    esp::ModernWindow *win_ = nullptr;
    QLineEdit *name_ = nullptr;
    QLineEdit *version_ = nullptr;
    QLineEdit *publisher_ = nullptr;
    QLineEdit *dir_ = nullptr;
    QLineEdit *mainExe_ = nullptr;
    QLineEdit *uninstallExe_ = nullptr;
    QLineEdit *installScript_ = nullptr;
    QLineEdit *uninstallScript_ = nullptr;
    QCheckBox *desktop_ = nullptr;
    QCheckBox *startMenu_ = nullptr;
    QCheckBox *taskbar_ = nullptr;
    QLabel *iconLabel_ = nullptr;
    QPushButton *installBtn_ = nullptr;
    QPlainTextEdit *logView_ = nullptr;
    QProgressBar *progress_ = nullptr;
    InstallTask *task_ = nullptr;
    bool working_ = false;
};
