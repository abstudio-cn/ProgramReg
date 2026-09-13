#pragma once
// InstallCore — 安装注册核心（对应 ProgramReg 的 InstallHelper.cs）：
// 文件就位 → 卸载配置 uninstall.ini → 注册表卸载项 → 快捷方式 → 任务栏固定 → 额外安装命令

#include <QString>
#include <QStringList>
#include <functional>

namespace esp {

struct InstallOptions
{
    QString appName;
    QString version = QStringLiteral("1.0.0");
    QString publisher;
    QString installDir;
    QString mainExePath;         // 用户选择的主程序 exe 完整路径
    QString uninstallExePath;    // 可为空 → 使用内置默认卸载程序
    QString installScriptPath;   // 额外安装命令（cmd/bat，可为空）
    QString uninstallScriptPath; // 额外卸载命令（cmd/bat，可为空）
    bool startMenuShortcut = true;
    bool desktopShortcut = true;
    bool taskbarShortcut = false;
};

// 执行安装注册。log 逐条报告；progress 报百分比（0-100）；返回失败数。
int runInstall(const InstallOptions &o,
               const std::function<void(const QString &)> &log,
               const std::function<void(int percent)> &progress = nullptr,
               QStringList *warningsOut = nullptr);

} // namespace esp
