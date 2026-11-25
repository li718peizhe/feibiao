# 任务：XRobot BSP 迁移到 1.0，ATSRuntime 改成独立 Git 模块

## 目标
1. BSP 改成 XRobot 1.0 格式，让 VS Code 扩展 XRobot 2.0.0 能读取它。
2. `Modules\ATSRuntime` 移出 BSP，做成独立的本地 Git 仓库（1.0 模块格式），登记到本地 index，再由 BSP 用 `xrobot module add` 引用、`xrobot instance add` 建实例。
3. 固件的 Keil 编译不受影响。

## 背景（已确认）
- BSP 根目录：`C:\Users\Lenovo\Desktop\xrobot\四发完整流程 - 副本`（下文 `$BSP`）。固件用 Keil 编译（`MDK-ARM\PROJECT.uvprojx`），入口 `Src\main.c`，第 141 行直接调用 `application\ats_runtime.c` 的 `ATS_Runtime_Init()`。树里没有 LibXR 源码，没有文件 include `User\xrobot_main.hpp`，`User\`、`Modules\` 下的 C++ 都不参与 Keil 编译。
- CLI 已是 xrobot 1.0.0、libxr 6.0.0，`pip check` 无冲突。BSP 还是 0.x 格式，没有改过。
- 上一轮已建好备份 `C:\Users\Lenovo\Desktop\xrobot\.backup\xrobot-1-0`（`Modules\`、`User\`、`XROBOT.md`、`.gitignore`、`tasks.json`、`hash_before.csv`），然后按计划停在「1.0 不支持 BSP 内的本地模块」。本版换了方案。
- 1.0 源码（`site-packages\xrobot`）里的规则：
  - `lock.py`：`modules.yaml` 的 `modules` 是 `owner/Repo[@ref]` 请求；不写 ref 用仓库默认分支；`same`/`same-or-dev` 要求 BSP 在 Git 里，本 BSP 不在，不要用。`setup` 把模块检出到 `Modules\<owner>\<Repo>` 并写 `xrobot.lock`。
  - `module_parser.py`：只认 lock 里的模块，必须是 Git 检出，且根目录有 `<Repo>.hpp`；没列出的目录被忽略。`module_name`、`constructor_args`、`template_args`、`required_hardware` 是 1.0 之前的清单键。
  - `source_manager.py`：包只从 sources.yaml 各源的 index.yaml 里解析，没有 GitHub 兜底。index 条目可以是仓库地址，也可以是 `{id, repo}` 映射；本地 index 里的相对地址按 index 文件所在目录解析。没写 `id` 时，GitHub 地址取 owner/repo，否则用 index 的 `namespace`。
  - `config.py` 会识别 1.0 之前的配置格式；`project.py` 要求 `User\` 下恰好有一个源文件调用 `XROBOT_MAIN()`。
- `$BSP\.vscode\tasks.json` 第 64 到 87 行的两个 XRobot 任务调用了已经不存在的 `xrobot_setup`、`xrobot_gen_main`。

## 新增的本地模块仓库和 index
- 模块仓库：`C:\Users\Lenovo\Desktop\xrobot\xrobot-local\ATSRuntime\`，Git 分支 `main`，只在本地，不加 remote。
- 本地 index：`C:\Users\Lenovo\Desktop\xrobot\xrobot-local\index.yaml`，内容如下。写明 `id`，以后把 `repo` 换成 GitHub 地址时包 id 不变。
```yaml
namespace: local
modules:
  - id: local/ATSRuntime
    repo: ./ATSRuntime
```

## 允许的改动
- 新建：`xrobot-local\`（模块仓库和 index）、`$BSP\User\app_main.cpp`
- 修改：`$BSP\Modules\modules.yaml`、`$BSP\Modules\sources.yaml`、`$BSP\User\xrobot.yaml`、`$BSP\XROBOT.md`、`$BSP\.vscode\tasks.json` 里的两个 XRobot 任务
- 删除：`$BSP\Modules\ATSRuntime\`（备份里有原件）
- xrobot 命令生成或改写的：`$BSP\User\xrobot_main.hpp`、`xrobot.lock`、`$BSP\Modules\local\ATSRuntime\`、`Modules\CMakeLists.txt`、缓存、`.gitignore`/`.gitattributes` 条目等，汇报里逐个列出
- 临时目录 `$BAK\scratch`，结束时删掉

其他一律不动，尤其是 `Src\`、`Inc\`、`application\`、`bsp\`、`components\`、`Drivers\`、`Middlewares\`、`MDK-ARM\PROJECT.uvprojx`、tasks.json 里的 Keil 编译和烧录任务、VS Code 扩展和设置、git 全局配置。不安装或升级任何包，不推送任何仓库，不烧录，不开 GUI。

## 卡住就停
遇到下面任何一种情况就停下，把原始输出写进汇报，不要自己想办法绕过去：
- 版本不对，或者第 1 步的对比显示 BSP 的 XRobot 文件和上次备份时不一样。
- `xrobot new-module` 生成不了参考模板，或者模板要求的写法和下文差别很大（比如构造函数必须带 BSP 外设）。
- `git commit` 因为没有 user.name/user.email 失败（不要改 git 配置）。
- `xrobot source get local/ATSRuntime` 或 `module add` 解析不出这个包。
- `setup`、`instance add`、`gen` 任何一步失败。
- `describe` 报 error，且在允许的改动范围内改不好。
- 文件对比出现允许范围以外的路径。
- Keil 编译出现编译错误。

## 步骤
命令按 PowerShell 7 写。每开一个新 shell 先执行：
```powershell
$WS  = 'C:\Users\Lenovo\Desktop\xrobot'
$BSP = "$WS\四发完整流程 - 副本"
$BAK = "$WS\.backup\xrobot-1-0"
$MOD = "$WS\xrobot-local\ATSRuntime"
```

### 1. 预检（只读；备份已存在，不要重做）
```powershell
xrobot --version; python -m pip show libxr          # 1.0.0；Version: 6.0.0
Get-ChildItem -Recurse -File $BSP | Get-FileHash | Select-Object Hash, Path | Export-Csv "$BAK\hash_now.csv" -NoTypeInformation
$key = '^' + [regex]::Escape("$BSP\") + '(Modules\\|User\\|XROBOT\.md$|\.vscode\\tasks\.json$)'
Compare-Object (Import-Csv "$BAK\hash_before.csv" | Where-Object Path -match $key) (Import-Csv "$BAK\hash_now.csv" | Where-Object Path -match $key) -Property Hash, Path   # 应无输出
```

### 2. 参考模板
先看 `xrobot new-module --help` 和 `xrobot check-module --help`。在 `$BAK\scratch` 里生成一个无依赖、无参数的模板模块（需要的话先在 scratch 里 `xrobot init` 一个临时 BSP）。记下模板的文件结构、清单、构造函数签名和 CMakeLists.txt。

### 3. 建 ATSRuntime 仓库
1. 新建 `$MOD`，按模板结构放入 `ATSRuntime.hpp`（必须在仓库根目录）、`CMakeLists.txt`、`README.md` 和模板里其他必需的文件，内容从 `$BAK\Modules\ATSRuntime\` 迁过来：
   - 清单只留 `module_description`（沿用原描述）和 `depends: []`。
   - 构造函数按模板签名写，函数体只调用 `ATS_Runtime_Init()`；`OnMonitor()` 等其他成员按模板写，保持空实现。
   - 不再 `#include "ats_runtime.h"`，改为直接声明 `extern "C" void ATS_Runtime_Init(void);`；CMakeLists.txt 不再引用 `../../application`。这样模块不依赖 BSP 的头文件。
   - README 写明：`ATS_Runtime_Init()` 由 BSP 实现（本 BSP 在 `application/ats_runtime.c`），模块只负责调用。
2. 用 `xrobot check-module` 检查模块；如果因为缺编译器或网络跑不了，记录原文后继续。
3. 只在本地建仓库并提交（git 不支持 `-b` 时，先 `git init` 再 `git checkout -b main`）：
```powershell
git -C $MOD init -b main
git -C $MOD add -A
git -C $MOD commit -m "ATSRuntime: XRobot 1.0 module that calls ATS_Runtime_Init()"
```
4. 按上文内容写 `$WS\xrobot-local\index.yaml`。

### 4. 迁移 BSP（按顺序）
1. `Modules\modules.yaml`：加 `xrobot: 1.0.0`，`modules` 先保持 `[]`；注释里的命令改成 `xrobot module add owner/Repo[@ref]` 和 `xrobot setup`。
2. `Modules\sources.yaml`：官方源条目保留（1.0 不接受就按报错改），注释改成 `xrobot source ...`。然后登记本地 index（先看 `xrobot source --help`）：`xrobot -C $BSP source add-source "$WS\xrobot-local\index.yaml" --priority 1`；绝对路径不行就用 `file:///C:/Users/Lenovo/Desktop/xrobot/xrobot-local/index.yaml`。用 `xrobot -C $BSP source get local/ATSRuntime` 确认能解析。
3. 删除 `$BSP\Modules\ATSRuntime\`。
4. `xrobot -C $BSP module add local/ATSRuntime@main`
5. 新建 `User\app_main.cpp`，作为唯一的入口源文件：写法以 1.0 源码（`project.py`、`generate_main.py`）为准，不写 `XR_REGISTER`，只调用一次 `XROBOT_MAIN()`。文件开头注释写明：此文件只为满足 XRobot 1.0 的入口规则，不在 Keil 工程里，固件入口仍是 `Src\main.c`。不要加进 `PROJECT.uvprojx`。
6. `User\xrobot.yaml`：改写成空的 1.0 配置（结构看 `config.py`）。`global_settings.monitor_sleep_ms: 1000` 在 1.0 有对应设置就迁过去，没有就删掉，并在汇报里注明。
7. `xrobot -C $BSP setup`：应把模块检出到 `Modules\local\ATSRuntime` 并写出 `xrobot.lock`。
8. `xrobot -C $BSP instance -c User/xrobot.yaml add local/ATSRuntime --id ATSRuntime_0`
9. `xrobot -C $BSP gen -c User/xrobot.yaml`，再 `xrobot -C $BSP format`。
10. 如果 setup 没有重写 `Modules\CMakeLists.txt`、里面仍是 `add_subdirectory(ATSRuntime)`，把路径改成 `local/ATSRuntime`。

### 5. 更新 tasks.json 和 XROBOT.md
- `$BSP\.vscode\tasks.json`：保持现有缩进；两个任务的 `label`、`type`、`options`、`problemMatcher` 不变，只改命令：
  - `🧩 XRobot 配置同步`：`"command": "xrobot_setup"` 改为 `"command": "xrobot"`，加 `"args": ["setup"]`。
  - `🧩 XRobot 重新生成主函数`：`"command": "xrobot_gen_main"` 改为 `"command": "xrobot"`，`args` 改为 `["gen", "-c", "User/xrobot.yaml"]`。
  - 验证时如果不带 `-C` 不能用，两个任务的 `args` 开头加上 `"-C", "${workspaceFolder}"`。
- `$BSP\XROBOT.md`：命令换成 1.0 写法（`xrobot setup`、`xrobot gen -c User/xrobot.yaml`、`xrobot module add owner/Repo`）；环境一节写明 xrobot 1.0.0 和 libxr 6.0.0；说明 `User/app_main.cpp` 的用途；说明 ATSRuntime 现在在 `xrobot-local\ATSRuntime`，改完先在那里提交，再运行 `xrobot setup --update local/ATSRuntime`；说明 VS Code 的 XRobot 视图只读第一个工作区文件夹，要直接打开本文件夹。Keil 编译和烧录部分不动。

## 验证命令
```powershell
xrobot -C $BSP describe                      # 退出码 0，没有 error
xrobot -C $BSP gen -c User/xrobot.yaml       # 再跑一次，应报告 unchanged
xrobot -C $BSP format --check
Push-Location $BSP; xrobot gen -c User/xrobot.yaml; Pop-Location              # 与任务命令一致
Get-Content "$BSP\.vscode\tasks.json" -Raw | ConvertFrom-Json | Out-Null       # 报错说明 JSON 被改坏
git -C $MOD status --short                                                     # 应无输出
Get-ChildItem -Recurse -File $BSP, "$WS\.vscode" -Include *.ps1,*.bat,*.json,*.md,*.yaml,*.yml,*.txt |
  Select-String -Pattern '\bxrobot_(setup|gen_main|add_mod|init_mod|src_man|create_mod|mod_parser)\b|\bxr_(gen_code|parse|cubemx|stm32)\w*'   # 只应剩 AI 笔记/设置里的命中
Get-ChildItem -Recurse -File $BSP | Get-FileHash | Select-Object Hash, Path | Export-Csv "$BAK\hash_after.csv" -NoTypeInformation
Compare-Object (Import-Csv "$BAK\hash_now.csv") (Import-Csv "$BAK\hash_after.csv") -Property Hash, Path
```
文件对比通过后，最后跑一次 Keil 编译（与 VS Code 任务相同），日志在 `$BSP\MDK-ARM\codex_build.log`：
```powershell
Push-Location "$BSP\MDK-ARM"; powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$BSP\.vscode\build.ps1"; Pop-Location
```
Keil 如果因为沙箱或权限失败、而不是编译错误，记录原文后跳过，由用户在 VS Code 里按 Ctrl+Shift+B 自己编译。最后删掉 `$BAK\scratch`。

## 验收标准
- `describe` 退出码 0 且没有 error，列出实例 `ATSRuntime_0` 和模块 `local/ATSRuntime`；第二次 `gen` 报告 unchanged；`format --check` 通过。
- `Modules\local\ATSRuntime` 是 Git 检出，commit 与 `xrobot.lock` 一致；`$MOD` 工作区干净。
- `User\xrobot_main.hpp` 由 xrobot 1.0 生成，其中 `XROBOT_MAIN` 的定义与 `app_main.cpp` 的调用一致。
- 两个 XRobot 任务改用 `xrobot setup` 和 `xrobot gen -c User/xrobot.yaml`，标签不变；Keil 编译和烧录任务原样不变。
- 文件对比只出现允许的路径；旧命令检索只剩 AI 笔记/设置里的命中。
- Keil 编译 0 个错误，或者因沙箱跳过并写明原因。

## 汇报
写清楚：模板模块的结构和构造函数签名；`monitor_sleep_ms` 怎么处理的；每个改动文件与备份相比的 diff（新仓库列出文件和提交号）；`describe` 的输出；Keil 编译的错误数和警告数；跳过或停下的步骤及原因。

## 回滚
```powershell
Remove-Item -Recurse -Force "$BSP\Modules", "$BSP\User"
Copy-Item -Recurse "$BAK\Modules", "$BAK\User" $BSP
Copy-Item "$BAK\XROBOT.md", "$BAK\.gitignore" $BSP
Copy-Item "$BAK\tasks.json" "$BSP\.vscode\tasks.json"
```
再删掉 xrobot 生成的 `xrobot.lock` 和新建的 `.gitattributes`（如果有）。`xrobot-local\` 在 BSP 外，回滚时保留。CLI 降级由用户执行：`python -m pip install "xrobot==0.3.1" "libxr==5.2.4"`。
