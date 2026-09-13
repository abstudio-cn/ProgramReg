#include "InstallCore.h"

#include "PeInfo.h"
#include "RegOps.h"
#include "Shortcut.h"
#include "ShellPin.h"
#include "IniFile.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>

namespace esp {

namespace {
// 静态库无 QObject 上下文，统一用 translate
QString tr_(const char *s)
{
    return QCoreApplication::translate("InstallCore", s);
}

QString commonProgramsDir()
{
    wchar_t buf[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, buf) != S_OK)
        return {};
    return QString::fromWCharArray(buf);
}

QString commonDesktopDir()
{
    wchar_t buf[MAX_PATH] = {0};
    if (SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, buf) != S_OK)
        return {};
    return QString::fromWCharArray(buf);
}

// 统计目录大小（字节）
quint64 estimateDirSizeBytes(const QString &dir)
{
    quint64 total = 0;
    QDir d(dir);
    if (!d.exists())
        return 0;
    for (const QFileInfo &fi : d.entryInfoList(QDir::Files | QDir::Hidden | QDir::System,
                                               QDir::DirsFirst | QDir::Name)) {
        if (fi.isDir())
            total += estimateDirSizeBytes(fi.absoluteFilePath());
        else
            total += quint64(fi.size());
    }
    return total;
}

QString samePathTrim(const QString &p)
{
    return QDir::cleanPath(p).toLower().replace('/', '\\');
}

bool samePath(const QString &a, const QString &b)
{
    return samePathTrim(a) == samePathTrim(b);
}

// 复制文件（覆盖已存在文件）。返回 false 表示失败。
bool copyFileOver(const QString &src, const QString &dst, QString *error)
{
    if (QFile::exists(dst) && !QFile::remove(dst)) {
        if (error)
            *error = tr_("无法覆盖已存在文件：%1").arg(dst);
        return false;
    }
    if (!QFile::copy(src, dst)) {
        if (error)
            *error = tr_("复制失败：%1").arg(dst);
        return false;
    }
    return true;
}

// 执行 cmd/bat 脚本并等待结束。返回退出码（-1 表示启动/执行失败）。
int runScriptAndWait(const QString &scriptPath, const QString &workingDir, QString *error)
{
    QProcess p;
    p.setWorkingDirectory(workingDir);
    p.start(scriptPath, {}, QIODevice::ReadOnly);
    if (!p.waitForStarted(10000)) {
        if (error)
            *error = tr_("无法启动脚本");
        return -1;
    }
    if (!p.waitForFinished(-1)) {
        if (error)
            *error = tr_("脚本执行失败");
        return -1;
    }
    return p.exitCode();
}
} // namespace

int runInstall(const InstallOptions &o,
               const std::function<void(const QString &)> &log,
               const std::function<void(int percent)> &progress,
               QStringList *warningsOut)
{
    int failures = 0;
    auto setP = [&](int p) { if (progress) progress(p); };
    auto warn = [&](const QString &w) {
        if (warningsOut) warningsOut->append(w);
        log(tr_("[警告] ") + w);
    };
    const QString installDir = QDir::cleanPath(o.installDir);

    log(tr_("开始安装 %1 ...").arg(o.appName));
    setP(3);

    // 1. 创建安装目录
    if (!QDir().mkpath(installDir)) {
        log(tr_("[错误] 创建安装目录失败：%1").arg(installDir));
        return 1;
    }

    // 2. 主程序就位（若所选 exe 不在安装目录内则复制）
    const QString mainExeName = QFileInfo(o.mainExePath).fileName();
    const QString mainExeInDir = QDir(installDir).filePath(mainExeName);
    if (!samePath(o.mainExePath, mainExeInDir)) {
        QString err;
        if (!copyFileOver(o.mainExePath, mainExeInDir, &err)) {
            log(tr_("[错误] 复制主程序失败：") + err);
            failures++;
        } else {
            log(tr_("已复制主程序：%1").arg(mainExeInDir));
        }
    }
    setP(20);

    // 3. 卸载程序就位：未选择则释放内置默认卸载程序（静态 Qt 单文件）
    QString uninstallExe;
    if (o.uninstallExePath.trimmed().isEmpty()) {
        const QString src = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("UninstallHelper.exe"));
        uninstallExe = QDir(installDir).filePath(QStringLiteral("uninstall.exe"));
        if (!QFile::exists(src)) {
            log(tr_("[错误] 未找到内置卸载程序 UninstallHelper.exe（需与 ProgramReg.exe 位于同一目录）。"));
            failures++;
        } else {
            QString err;
            if (!copyFileOver(src, uninstallExe, &err)) {
                log(tr_("[错误] 释放内置卸载程序失败：") + err);
                failures++;
            } else {
                log(tr_("已释放内置卸载程序 uninstall.exe。"));
            }
        }
    } else {
        uninstallExe = QDir(installDir).filePath(QFileInfo(o.uninstallExePath).fileName());
        if (!samePath(o.uninstallExePath, uninstallExe)) {
            QString err;
            if (!copyFileOver(o.uninstallExePath, uninstallExe, &err)) {
                log(tr_("[错误] 复制卸载程序失败：") + err);
                failures++;
            }
        }
    }
    setP(30);

    // 4. 额外卸载命令就位（持久保存到安装目录；安装命令仅本次执行不复制）
    const QString installScriptName = QFileInfo(o.installScriptPath).fileName();
    const QString uninstallScriptName = QFileInfo(o.uninstallScriptPath).fileName();
    if (!o.uninstallScriptPath.trimmed().isEmpty()) {
        const QString dst = QDir(installDir).filePath(uninstallScriptName);
        if (!samePath(o.uninstallScriptPath, dst)) {
            QString err;
            if (!copyFileOver(o.uninstallScriptPath, dst, &err)) {
                log(tr_("[错误] 复制卸载命令失败：") + err);
                failures++;
            }
        }
    }
    if (failures > 0)
        return failures;
    setP(40);

    // 5. 架构 → 注册表视图
    QString arch = peArchitecture(mainExeInDir);
    if (arch == QLatin1String("unknown")) {
        arch = QStringLiteral("x64");
        warn(tr_("无法识别主程序架构，默认按 64 位注册表视图处理。"));
    }
    const QString view = (arch == QLatin1String("x86")) ? QStringLiteral("32") : QStringLiteral("64");
    setP(50);

    // 6. 生成 uninstall.ini（卸载程序读取它确定待删除内容）
    const QString iniPath = QDir(installDir).filePath(QStringLiteral("uninstall.ini"));
    IniFile ini(iniPath);
    ini.set(QStringLiteral("AppInfo"), QStringLiteral("AppName"), o.appName);
    ini.set(QStringLiteral("AppInfo"), QStringLiteral("MainExe"), mainExeName);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("InstallDir"), installDir);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("StartMenuFolder"),
            o.startMenuShortcut ? o.appName : QString());
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("RegistryKeyName"), o.appName);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("RegistryView"), view);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("PinTaskbar"),
            o.taskbarShortcut ? QStringLiteral("1") : QStringLiteral("0"));
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("DesktopShortcut"),
            o.desktopShortcut ? QStringLiteral("1") : QStringLiteral("0"));
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("InstallScript"), installScriptName);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("UninstallScript"), uninstallScriptName);
    ini.set(QStringLiteral("Cleanup"), QStringLiteral("DeleteInstallDir"), QStringLiteral("1"));
    QString err;
    if (!ini.save(&err)) {
        log(tr_("[错误] 写入卸载配置失败：") + err);
        failures++;
    } else {
        log(tr_("已写入卸载配置 uninstall.ini。"));
    }
    setP(60);

    // 7. 注册表卸载项（控制面板「卸载程序」界面）
    const QString uninstallKey =
        QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + o.appName;
    if (regSetValue(uninstallKey, QStringLiteral("DisplayName"), o.appName, view, &err)
        && regSetValue(uninstallKey, QStringLiteral("DisplayVersion"),
                       o.version.isEmpty() ? QStringLiteral("1.0.0") : o.version, view, &err)
        && regSetValue(uninstallKey, QStringLiteral("Publisher"), o.publisher, view, &err)
        && regSetValue(uninstallKey, QStringLiteral("DisplayIcon"), mainExeInDir, view, &err)
        && regSetValue(uninstallKey, QStringLiteral("InstallLocation"), installDir, view, &err)
        && regSetValue(uninstallKey, QStringLiteral("UninstallString"),
                       QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(uninstallExe)), view, &err)
        && regSetValue(uninstallKey, QStringLiteral("QuietUninstallString"),
                       QStringLiteral("\"%1\" /quiet").arg(QDir::toNativeSeparators(uninstallExe)), view, &err)
        && regSetDword(uninstallKey, QStringLiteral("NoModify"), 1, view, &err)
        && regSetDword(uninstallKey, QStringLiteral("NoRepair"), 1, view, &err)
        && regSetDword(uninstallKey, QStringLiteral("EstimatedSize"),
                       quint32(qMin<quint64>(estimateDirSizeBytes(installDir) / 1024, 0x7FFFFFFF)), view, &err)) {
        log(tr_("已写入注册表卸载项（%1 视图）。").arg(view));
    } else {
        log(tr_("[错误] 写入注册表卸载项失败：") + err);
        failures++;
    }
    setP(75);

    // 8. 开始菜单快捷方式（所有用户）
    QString startMenuLnk;
    if (o.startMenuShortcut) {
        startMenuLnk = QDir(commonProgramsDir()).filePath(o.appName + QStringLiteral("/") + o.appName + QStringLiteral(".lnk"));
        if (createShortcut(mainExeInDir, startMenuLnk, installDir, o.appName, &err))
            log(tr_("已创建开始菜单快捷方式。"));
        else {
            log(tr_("[错误] 创建开始菜单快捷方式失败：") + err);
            failures++;
        }
    }

    // 9. 桌面快捷方式（所有用户桌面）
    if (o.desktopShortcut) {
        const QString desktopLnk = QDir(commonDesktopDir()).filePath(o.appName + QStringLiteral(".lnk"));
        if (createShortcut(mainExeInDir, desktopLnk, installDir, o.appName, &err))
            log(tr_("已创建桌面快捷方式。"));
        else {
            log(tr_("[错误] 创建桌面快捷方式失败：") + err);
            failures++;
        }
    }
    setP(88);

    // 10. 任务栏固定（优先固定 .lnk，兼容性更好）
    if (o.taskbarShortcut) {
        QString pinTarget = QFile::exists(startMenuLnk) ? startMenuLnk : mainExeInDir;
        bool pinned = toggleTaskbarPin(pinTarget, true);
        if (!pinned && startMenuLnk.isEmpty()) {
            const QString lnk = QDir(installDir).filePath(o.appName + QStringLiteral(".lnk"));
            if (createShortcut(mainExeInDir, lnk, installDir, o.appName, nullptr))
                pinned = toggleTaskbarPin(lnk, true);
        }
        if (!pinned)
            warn(tr_("未能自动固定到任务栏，请手动右键程序选择「固定到任务栏」。"));
        else
            log(tr_("已固定到任务栏。"));
    }
    setP(94);

    // 11. 额外安装命令（最后阶段执行）
    if (!o.installScriptPath.trimmed().isEmpty()) {
        log(tr_("执行额外安装命令 ..."));
        QString serr;
        const int code = runScriptAndWait(o.installScriptPath, installDir, &serr);
        if (code == -1)
            warn(tr_("额外安装命令执行失败：%1").arg(serr));
        else if (code != 0)
            warn(tr_("额外安装命令执行完成，但退出码为 %1（非 0）。").arg(code));
        else
            log(tr_("额外安装命令执行完成。"));
    }

    log(failures == 0 ? tr_("安装完成。")
                      : tr_("安装流程结束，有 %1 个步骤未成功。").arg(failures));
    setP(100);
    return failures;
}

} // namespace esp
