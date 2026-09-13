#include "MainWindow.h"

#include "../Shared/Theme.h"
#include "../Shared/IconExtract.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QIcon>

using namespace esp;

// =====================================================================
// InstallTask
// =====================================================================

InstallTask::InstallTask(const InstallOptions &opts, QObject *parent)
    : QThread(parent), opts_(opts)
{
}

void InstallTask::run()
{
    QStringList warnings;
    const int failures = runInstall(opts_,
        [this](const QString &line) { emit logLine(line); },
        [this](int p) { emit progress(p); },
        &warnings);
    emit done(failures, warnings);
}

// =====================================================================
// MainWindow
// =====================================================================

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    win_ = new ModernWindow(tr("安装注册程序"), QSize(880, 740), nullptr, this);
    connect(win_, &ModernWindow::closeClicked, this, [this] {
        if (working_) {
            QMessageBox::information(win_, tr("安装注册程序"),
                tr("安装进行中，请等待完成后再退出。"));
            return;
        }
        win_->close();
    });
    win_->setTitleBarIcon(QIcon(QStringLiteral(":/icons/ProgramReg.ico")).pixmap(44, 44));
    win_->setWindowIcon(QIcon(QStringLiteral(":/icons/ProgramReg.ico")));

    auto *content = new QWidget(win_);
    auto *clay = new QVBoxLayout(content);
    clay->setContentsMargins(30, 20, 30, 16);
    clay->setSpacing(8);

    // ---- 头部 ----
    auto *head = new QHBoxLayout;
    auto *headText = new QVBoxLayout;
    headText->setSpacing(2);
    auto *title = new QLabel(tr("安装注册程序"), content);
    title->setProperty("role", "pageTitle");
    headText->addWidget(title);
    auto *sub = new QLabel(tr("将应用程序注册到控制面板「卸载程序」界面"), content);
    sub->setProperty("role", "pageSub");
    headText->addWidget(sub);
    head->addLayout(headText);
    head->addStretch();
    iconLabel_ = new QLabel(content);
    iconLabel_->setPixmap(QIcon(QStringLiteral(":/icons/ProgramReg.ico")).pixmap(52, 52));
    iconLabel_->setFixedSize(58, 58);
    iconLabel_->setAlignment(Qt::AlignCenter);
    head->addWidget(iconLabel_);
    clay->addLayout(head);
    clay->addSpacing(8);

    // ---- 表单 ----
    auto *form = new QGridLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    int row = 0;

    auto addFieldRow = [&](const QString &label, QWidget *field, QWidget *b1 = nullptr, QWidget *b2 = nullptr) {
        auto *l = new QLabel(label, content);
        l->setProperty("role", "fieldLabel");
        l->setFixedWidth(150);
        form->addWidget(l, row, 0);
        form->addWidget(field, row, 1);
        if (b1) form->addWidget(b1, row, 2);
        if (b2) form->addWidget(b2, row, 3);
        ++row;
    };

    name_ = new QLineEdit(content);
    name_->setPlaceholderText(tr("显示在卸载列表中的名称"));
    addFieldRow(tr("程序名称 *"), name_);

    version_ = new QLineEdit(QStringLiteral("1.0.0"), content);
    version_->setFixedWidth(130);
    publisher_ = new QLineEdit(content);
    publisher_->setPlaceholderText(tr("可选"));
    {
        auto *l = new QLabel(tr("版本 / 发布者"), content);
        l->setProperty("role", "fieldLabel");
        l->setFixedWidth(150);
        form->addWidget(l, row, 0);
        auto *hb = new QHBoxLayout;
        hb->setSpacing(10);
        hb->addWidget(version_);
        hb->addWidget(publisher_, 1);
        form->addLayout(hb, row, 1);
        ++row;
    }

    auto makeField = [&](QLineEdit **target) {
        *target = new QLineEdit(content);
        (*target)->setReadOnly(true);
        (*target)->setPlaceholderText(tr("未选择"));
    };
    auto makePickBtn = [&](const QString &text, void (MainWindow::*slot)()) {
        auto *b = new QPushButton(text, content);
        b->setProperty("role", "secondary");
        b->setFixedSize(76, 34);
        connect(b, &QPushButton::clicked, this, slot);
        return b;
    };
    auto makeClearBtn = [&](QLineEdit *field) {
        auto *b = new QPushButton(tr("清除"), content);
        b->setProperty("role", "secondary");
        b->setFixedSize(52, 34);
        connect(b, &QPushButton::clicked, this, [field] { field->clear(); });
        return b;
    };

    makeField(&dir_);
    addFieldRow(tr("安装目录 *"), dir_, makePickBtn(tr("浏览..."), &MainWindow::pickDir));

    makeField(&mainExe_);
    addFieldRow(tr("主程序 exe *"), mainExe_, makePickBtn(tr("选择..."), &MainWindow::pickMainExe));

    makeField(&uninstallExe_);
    addFieldRow(tr("卸载 exe (可选)"), uninstallExe_,
                makePickBtn(tr("选择..."), &MainWindow::pickUninstallExe),
                makeClearBtn(uninstallExe_));

    makeField(&installScript_);
    addFieldRow(tr("额外安装命令 (可选)"), installScript_,
                makePickBtn(tr("选择..."), &MainWindow::pickInstallScript),
                makeClearBtn(installScript_));

    makeField(&uninstallScript_);
    addFieldRow(tr("额外卸载命令 (可选)"), uninstallScript_,
                makePickBtn(tr("选择..."), &MainWindow::pickUninstallScript),
                makeClearBtn(uninstallScript_));

    clay->addLayout(form);
    clay->addSpacing(4);

    // ---- 快捷方式选项 ----
    auto *chkRow = new QHBoxLayout;
    desktop_ = new QCheckBox(tr("创建桌面快捷方式"), content);
    desktop_->setChecked(true);
    startMenu_ = new QCheckBox(tr("创建开始菜单快捷方式"), content);
    startMenu_->setChecked(true);
    taskbar_ = new QCheckBox(tr("创建任务栏快捷方式"), content);
    chkRow->addWidget(desktop_);
    chkRow->addSpacing(20);
    chkRow->addWidget(startMenu_);
    chkRow->addSpacing(20);
    chkRow->addWidget(taskbar_);
    chkRow->addStretch();
    clay->addLayout(chkRow);
    clay->addSpacing(4);

    // ---- 安装按钮 ----
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    installBtn_ = new QPushButton(tr("开始安装"), content);
    installBtn_->setProperty("role", "primary");
    installBtn_->setFixedSize(200, 42);
    connect(installBtn_, &QPushButton::clicked, this, &MainWindow::startInstall);
    btnRow->addWidget(installBtn_);
    btnRow->addStretch();
    clay->addLayout(btnRow);
    clay->addSpacing(4);

    // ---- 进度 + 日志 ----
    progress_ = new QProgressBar(content);
    progress_->setRange(0, 100);
    progress_->setFixedHeight(10);
    progress_->setTextVisible(false);
    progress_->setVisible(false);
    clay->addWidget(progress_);

    logView_ = new QPlainTextEdit(content);
    logView_->setProperty("role", "log");
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(2000);
    clay->addWidget(logView_, 1);

    win_->setContent(content);
    win_->setFixedClientSize(880, 700);
}

void MainWindow::show()
{
    win_->show();
}

// =====================================================================
// 选择对话框
// =====================================================================

void MainWindow::pickDir()
{
    const QString d = QFileDialog::getExistingDirectory(win_, tr("选择安装目录（应用将注册到该目录）"), dir_->text());
    if (!d.isEmpty())
        dir_->setText(d);
}

void MainWindow::pickMainExe()
{
    const QString f = QFileDialog::getOpenFileName(win_, tr("选择应用主体 exe"),
        mainExe_->text().isEmpty() ? QString() : QFileInfo(mainExe_->text()).absolutePath(),
        tr("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (f.isEmpty())
        return;
    mainExe_->setText(f);
    if (name_->text().trimmed().isEmpty())
        name_->setText(QFileInfo(f).completeBaseName());
    if (dir_->text().trimmed().isEmpty())
        dir_->setText(QFileInfo(f).absolutePath());
    const QPixmap icon = extractExeIcon(f, 64);
    iconLabel_->setPixmap(icon.isNull()
        ? QIcon(QStringLiteral(":/icons/ProgramReg.ico")).pixmap(52, 52)
        : icon.scaled(52, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::pickUninstallExe()
{
    const QString f = QFileDialog::getOpenFileName(win_, tr("选择卸载程序 exe（可留空，使用内置卸载程序）"),
        uninstallExe_->text().isEmpty() ? QString() : QFileInfo(uninstallExe_->text()).absolutePath(),
        tr("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!f.isEmpty())
        uninstallExe_->setText(f);
}

void MainWindow::pickInstallScript()
{
    const QString f = QFileDialog::getOpenFileName(win_, tr("选择额外安装命令（cmd/bat，安装最后阶段执行）"),
        installScript_->text().isEmpty() ? QString() : QFileInfo(installScript_->text()).absolutePath(),
        tr("命令脚本 (*.cmd *.bat);;所有文件 (*.*)"));
    if (!f.isEmpty())
        installScript_->setText(f);
}

void MainWindow::pickUninstallScript()
{
    const QString f = QFileDialog::getOpenFileName(win_, tr("选择额外卸载命令（cmd/bat，卸载最后阶段执行）"),
        uninstallScript_->text().isEmpty() ? QString() : QFileInfo(uninstallScript_->text()).absolutePath(),
        tr("命令脚本 (*.cmd *.bat);;所有文件 (*.*)"));
    if (!f.isEmpty())
        uninstallScript_->setText(f);
}

// =====================================================================
// 安装
// =====================================================================

bool MainWindow::validate(QString *msg)
{
    if (name_->text().trimmed().isEmpty()) { *msg = tr("请填写程序名称（显示在卸载列表中的名称）。"); return false; }
    if (dir_->text().trimmed().isEmpty()) { *msg = tr("请选择安装目录。"); return false; }
    if (mainExe_->text().trimmed().isEmpty() || !QFile::exists(mainExe_->text())) { *msg = tr("请选择有效的应用主体 exe 文件。"); return false; }
    if (!uninstallExe_->text().trimmed().isEmpty() && !QFile::exists(uninstallExe_->text())) { *msg = tr("所选卸载程序 exe 不存在。"); return false; }
    if (!installScript_->text().trimmed().isEmpty() && !QFile::exists(installScript_->text())) { *msg = tr("所选额外安装命令文件不存在。"); return false; }
    if (!uninstallScript_->text().trimmed().isEmpty() && !QFile::exists(uninstallScript_->text())) { *msg = tr("所选额外卸载命令文件不存在。"); return false; }
    return true;
}

void MainWindow::startInstall()
{
    if (working_)
        return;
    QString msg;
    if (!validate(&msg)) {
        QMessageBox::warning(win_, tr("提示"), msg);
        return;
    }

    working_ = true;
    win_->setClosable(false);
    installBtn_->setEnabled(false);
    logView_->clear();
    progress_->setVisible(true);
    progress_->setValue(0);

    InstallOptions o;
    o.appName = name_->text().trimmed();
    o.version = version_->text().trimmed();
    o.publisher = publisher_->text().trimmed();
    o.installDir = dir_->text().trimmed();
    o.mainExePath = mainExe_->text().trimmed();
    o.uninstallExePath = uninstallExe_->text().trimmed();
    o.installScriptPath = installScript_->text().trimmed();
    o.uninstallScriptPath = uninstallScript_->text().trimmed();
    o.startMenuShortcut = startMenu_->isChecked();
    o.desktopShortcut = desktop_->isChecked();
    o.taskbarShortcut = taskbar_->isChecked();

    task_ = new InstallTask(o, this);
    connect(task_, &InstallTask::progress, this, [this](int p) { progress_->setValue(p); });
    connect(task_, &InstallTask::logLine, this, [this](const QString &l) { logView_->appendPlainText(l); });
    connect(task_, &InstallTask::done, this, &MainWindow::onDone);
    connect(task_, &InstallTask::failed, this, &MainWindow::onFailed);
    task_->start();
}

void MainWindow::onDone(int failures, const QStringList &warnings)
{
    working_ = false;
    win_->setClosable(true);
    installBtn_->setEnabled(true);
    progress_->setVisible(false);
    if (task_) {
        task_->deleteLater();
        task_ = nullptr;
    }

    QStringList lines;
    lines << tr("安装注册完成！") << QString()
          << tr("程序名称：%1").arg(name_->text().trimmed())
          << tr("安装目录：%1").arg(dir_->text().trimmed())
          << (uninstallExe_->text().trimmed().isEmpty()
                 ? tr("卸载程序：内置默认卸载程序 uninstall.exe")
                 : tr("卸载程序：%1").arg(uninstallExe_->text().trimmed()));
    if (!warnings.isEmpty()) {
        lines << QString() << tr("警告：");
        for (const QString &w : warnings)
            lines << QStringLiteral("· %1").arg(w);
    }
    if (failures == 0) {
        lines << QString() << tr("可在 控制面板 → 程序和功能 中查看并卸载。");
        QMessageBox::information(win_, tr("安装注册程序"), lines.join(QStringLiteral("\n")));
    } else {
        QMessageBox::warning(win_, tr("安装注册程序"),
            tr("安装流程结束，但有 %1 个步骤未成功，详见日志。").arg(failures));
    }
}

void MainWindow::onFailed(const QString &error)
{
    working_ = false;
    win_->setClosable(true);
    installBtn_->setEnabled(true);
    progress_->setVisible(false);
    if (task_) {
        task_->deleteLater();
        task_ = nullptr;
    }
    logView_->appendPlainText(QStringLiteral("[错误] ") + error);
    QMessageBox::critical(win_, tr("安装注册程序"), tr("安装失败：") + error);
}
