# ProgramReg — 安装注册程序（C++/Qt 版）

将任意 Windows 应用程序注册到控制面板「程序和功能 / 卸载程序」界面的小工具。
由 C# WinForms 版本（ProgramReg.old）重构而来，界面风格对齐 ExeSimplePackager。

## 组成

| 项目 | 说明 |
|------|------|
| `ProgramReg` | 安装注册程序（主程序，requireAdministrator 清单提权） |
| `UninstallHelper` | 默认卸载程序（asInvoker + 运行时自提权，避免控制面板「没有足够的权限」） |
| `Shared` | 共享静态库（InstallCore / UninstallRunner / Theme / RegOps / Shortcut / ShellPin / PeInfo / IniFile / IconExtract） |

## 功能

1. 选择安装目录、应用主体 exe、卸载 exe（可选）
2. 可选创建「桌面」「开始菜单」快捷方式和「任务栏」快捷方式
3. 可选额外安装/卸载命令（cmd/bat，分别在安装、卸载最后阶段执行）
4. 点击「开始安装」后自动完成：
   - 主程序复制到安装目录（若已在目录内则原位注册）
   - 未选择卸载 exe 时，自动释放内置默认卸载程序 `uninstall.exe`（静态 Qt 单文件自包含）
   - 额外卸载命令复制到安装目录持久保存
   - 生成 `uninstall.ini`（记录关联的待删除内容）
   - 写入注册表卸载项（自动识别 32/64 位应用，写入对应注册表视图）
   - 按需创建桌面/开始菜单快捷方式、尝试固定到任务栏
   - 若有额外安装命令，在最后阶段执行
5. 默认卸载程序 `uninstall.exe` 读取同目录 `uninstall.ini`，依次执行：
   结束主程序进程 → 取消任务栏固定 → 删除开始菜单/桌面快捷方式 → 删除注册表卸载项
   → 删除额外路径 → 删除安装目录内容 → 执行额外卸载命令 → 安排自身延迟清理
6. 卸载完成后若有无法自动删除的文件（排除卸载程序自身文件），弹提示框：
   「打开文件夹」打开安装目录手动清理，或「取消」

## 构建

需要：VS2026 Community（MSVC）+ 静态 Qt 6.9.3（自编，位于 `C:\Qt\6.9.3-static-msvc2022_64`，
参考 ExeSimplePackager 的 `scripts\build-qt-static.ps1`）。

```
powershell -ExecutionPolicy Bypass -File scripts\build.ps1          # Ninja 静态构建
powershell -ExecutionPolicy Bypass -File scripts\gen-vs.ps1         # 生成 VS 工程（build-vs\ProgramReg.slnx）
powershell -ExecutionPolicy Bypass -File scripts\verify.ps1         # 产物验证
```

产物（**两个 exe 同目录分发**）：
- `build\bin\ProgramReg.exe`（约 21MB，静态自包含）
- `build\bin\UninstallHelper.exe`（内置默认卸载程序）

## 使用

1. 双击 `ProgramReg.exe`（UAC 提权提示请点「是」）
2. 填写程序名称；选择安装目录与主程序 exe；卸载 exe 可留空（使用内置默认卸载程序）
3. 勾选需要的快捷方式选项，点击「开始安装」
4. 打开 控制面板 → 程序和功能，即可看到该程序，可正常卸载

## uninstall.ini 格式（由安装程序自动生成）

```ini
[AppInfo]
AppName=程序名
MainExe=主程序.exe

[Cleanup]
InstallDir=D:\AppDir        ; 待删除的安装目录
StartMenuFolder=程序名      ; 开始菜单文件夹名（空=无）
RegistryKeyName=程序名      ; 注册表卸载项名称
RegistryView=64             ; 32=32位视图(WOW6432Node)，64=64位视图
PinTaskbar=1                ; 1=尝试取消任务栏固定
DesktopShortcut=1           ; 1=删除所有用户桌面快捷方式
InstallScript=setup.bat     ; 额外安装命令（空=无，仅记录）
UninstallScript=clean.bat   ; 额外卸载命令（卸载最后阶段执行，已存于安装目录）
DeleteInstallDir=1          ; 1=删除整个安装目录
```

## 注意

- 安装器 `ProgramReg.exe` 为 requireAdministrator 清单提权；卸载器为 asInvoker + 运行时自提权（用户取消 UAC 时给出友好提示）
- 任务栏固定通过系统「固定到任务栏」菜单动作实现，部分 Windows 11 版本可能不暴露该动作，届时请手动固定
- 32 位应用注册表项位于 `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\`
- 同名旧注册表项在安装时会被覆盖更新
