# VSCode EIDE / J-Link 外壳接入说明

本文档用于把当前 LED1 工程接入 VSCode EIDE 插件的构建/烧录流程，但不迁移为 EIDE 原生工程。

## 当前工程边界

- 保留现有源码目录：`Hardware/`
- 保留现有生成目录：`Debug/`
- 保留现有 SysConfig 输入：`empty.syscfg`
- 保留现有构建入口：根目录 `makefile`
- 输出文件继续使用：`Debug/LED1.out`、`Debug/LED1.hex`

当前仓库未发现 `CMakeLists.txt`，实际可用构建入口是 TI/CCS 风格 `makefile`。

## EIDE 推荐方式

不推荐完整迁移为 EIDE 原生工程。推荐在 EIDE 中使用外壳工程：

当前仓库已加入最小 EIDE 外壳文件：

- `.eide/eide.yml`
- `.eide/files.options.yml`
- `LED1.code-workspace`

这些文件用于让 EIDE 识别并展示当前项目。源码文件仍保留在原路径；`.c` 文件在 EIDE 内置构建中被排除，避免 EIDE 误用 GCC 重新编译 TI/CCS 工程。

使用方式：

1. 用 VSCode 打开 `LED1.code-workspace`。
2. 打开 EIDE 侧边栏，确认项目名 `LED1` 出现。
3. 日常构建仍优先使用 VSCode Task `MSPM0: Build`，或使用以下命令：

```powershell
d:/TI-MPSM0/ti/ccs2050/ccs/utils/bin/gmake.exe -f makefile all SHELL=cmd.exe
```

4. Clean 命令：

```powershell
d:/TI-MPSM0/ti/ccs2050/ccs/utils/bin/gmake.exe -f makefile clean SHELL=cmd.exe
```

说明：`.eide/eide.yml` 中配置了 `beforeBuildTasks` 调用当前 makefile。当前 EIDE 外壳还补入了 TI SDK 和 CMSIS include 路径，避免 EIDE/GCC 诊断误报 `core_cm0plus.h: No such file or directory`。若 EIDE 一键 Build 行为与预期不一致，直接使用 VSCode 任务 `MSPM0: Build`。

## J-Link 烧录

已在本机发现 J-Link V9.50：

```text
D:\24电赛h题\25E\New Folder\JLink_V950
```

关键文件：

- `JLink.exe`
- `JLinkGDBServerCL.exe`
- `JLinkARM.dll`
- `JLink_x64.dll`
- `USBDriver\InstDrivers.exe`

J-Link 命令文件：

```text
tools/flash_mspm0g3507.jlink
```

内容使用 SEGGER J-Link Commander 命令：

```text
device MSPM0G3507
si SWD
speed 1000
r
h
loadfile Debug/LED1.hex
r
g
exit
```

EIDE 外壳工程已配置 `Custom` flasher，命令为：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/flash_mspm0g3507_jlink.ps1 -Build
```

VSCode 任务中，`MSPM0: Flash` 已设置为默认 J-Link 烧录入口：脚本会先执行当前 TI makefile 生成 `Debug/LED1.hex`，再调用 J-Link Commander 下载。原来的 UART BSL 网页烧录保留为备用任务：`MSPM0: Flash UART BSL Web`。

脚本会按顺序查找：

1. 环境变量 `JLINK_EXE`
2. `C:\Program Files\SEGGER\JLink\JLink.exe`
3. `C:\Program Files (x86)\SEGGER\JLink\JLink.exe`
4. `C:\SEGGER\JLink\JLink.exe`
5. `D:\24电赛h题\25E\New Folder\JLink_V950\JLink.exe`

如果 J-Link 安装在非标准目录，设置：

```powershell
$env:JLINK_EXE="D:\path\to\JLink.exe"
```

或在 EIDE Custom Flasher 中给脚本传入：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/flash_mspm0g3507_jlink.ps1 -JLinkPath "D:\path\to\JLink.exe"
```

只检查 J-Link 路径和 HEX 文件、不执行烧录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/flash_mspm0g3507_jlink.ps1 -CheckOnly
```

构建并检查 J-Link 路径和 HEX 文件、不执行烧录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/flash_mspm0g3507_jlink.ps1 -Build -CheckOnly
```

只检查 J-Link 是否接受 `MSPM0G3507` 设备名、不执行烧录：

```powershell
& "D:\24电赛h题\25E\New Folder\JLink_V950\JLink.exe" -NoGui 1 -CommandFile tools/check_mspm0g3507_device.jlink
```

如果没有连接 J-Link OB，会报 `Cannot connect to the probe/programmer`，这是探针连接问题，不等于烧录脚本执行失败。

## J-Link 参数

- Device：`MSPM0G3507`
- Interface：`SWD`
- Speed：`1000 KHz`
- Program file：`Debug/LED1.hex`

如果 J-Link 版本不支持 `MSPM0G3507` 这个 device 名称，需要升级 SEGGER J-Link Software，或用该版本设备库中对应的 MSPM0G3507 名称。

## 调试

EIDE 主要负责工程管理、构建和下载。断点调试建议使用 Cortex-Debug。

基本参数：

- `servertype`: `jlink`
- `device`: `MSPM0G3507`
- `interface`: `swd`
- `executable`: `${workspaceFolder}/Debug/LED1.out`

如果没有安装 `marus25.cortex-debug` 或 `arm-none-eabi-gdb`，先只使用 EIDE 编译和 J-Link 烧录。

## 回退方式

当前接入没有改变 CCS/SysConfig/makefile 工程结构。回退时只需忽略或删除：

- `.vscode/extensions.json` 中的 EIDE 推荐项
- `.vscode/tasks.json` 中的 `MSPM0: Flash` / `MSPM0: Flash UART BSL Web` 任务改动
- `.eide/eide.yml`
- `.eide/files.options.yml`
- `tools/flash_mspm0g3507_jlink.ps1`
- `docs/eide_jlink_setup.md`

原有构建命令仍然可用：

```powershell
d:/TI-MPSM0/ti/ccs2050/ccs/utils/bin/gmake.exe -f makefile all SHELL=cmd.exe
```
