# 任务 4l 实施方案（B 版）：上传 `C:\Users\Lenovo\Desktop\xrobot` 代码与文档至 GitHub (`li718peizhe/feibiao`)

- 日期：2026-11-25
- 任务号：4l
- 状态：B 版草稿（独立方案，待对比合稿）
- 远端基线：`https://github.com/li718peizhe/feibiao`（公开仓库 `isPrivate=false`，`diskUsage=19481 KB`，默认分支 `main` 最新提交 `4ad0d38`（2026-10-08），另有旧分支 `master@907b31e`（2026-10-04）；`gh auth status` 已登录 `li718peizhe`，https 协议，含 `repo` 权限）
- 本地基线：`C:\Users\Lenovo\Desktop\xrobot` 根目录非 Git 仓库，内部包含 30 个嵌套 `.git`（含 `dart-fw@fa0838b`、`dart-fw/Modules/local/DartLauncher@e17b490+11项未提交修改`、`dart-fw/Modules/QDU-Robomaster/Referee+3项未提交本地补丁`、`xrobot-local/DartLauncher@e17b490`、`xrobot-local/ATSRuntime@a339f30` 等）

---

## 1. 现状盘点与核心约束

### 1.1 远端 `li718peizhe/feibiao` 现状
- **可见性**：**公开仓库（Public）**，任何人都可浏览全部提交历史与文件。
- **分支**：`main`（默认分支，领先 `master` 1 个提交 `4ad0d38`）和 `master`。
- **已有顶层内容**：
  - `.gitignore`（146 B，已忽略 `**/.git/`、`**/.claude/`、`**/settings.local.json`、`**/*/tmp/`、`__pycache__/` 等）
  - `simulation/`（含 `dart.py`、`dart_sim/`、`test_dart_console.py`、`test_dart_sim.py`）
  - `下位机开发协议文档.md`（6.6 KB）
  - `遥控器控制左中右下控制皮筋（未进行高强度测试）/`

### 1.2 本地 `C:\Users\Lenovo\Desktop\xrobot` 现状（总计约 1.65 GB）
| 顶层项 | 体量 | 说明 |
| --- | ---: | --- |
| `dart-fw/` | 771.34 MB | 主固件工程，其中 `build/` 占 **652.98 MB**、`.git` 占 26.70 MB；剔除二者后约 **91.6 MB**（`Drivers/` 54.19 MB、`Middlewares/` 27.08 MB、`Modules/` 10.03 MB、`Core/`+`User/`+`cmake/` < 1 MB） |
| `graphify-out/` | 287.59 MB | 可再生知识图谱缓存，含 3 个 56~64 MB 的 `graph.json`（超过 GitHub 50 MB 警告线） |
| `四发完整流程 - 副本/` | 285.02 MB | 历史旧工程副本（含 12.68 MB `.lib`、8.17 MB PDF、嵌套 `ATSRuntime/.git`） |
| `.venv/` | 142.28 MB | 本地 Python 虚拟环境（8858 文件） |
| `xrobot-local/` | 83.38 MB | 本地模块源仓库，其中 `DartLauncher/build-host/` 占 **82.04 MB**；剔除后仅 **1.34 MB** |
| `rules/` | 41.16 MB | RoboMaster 2027 官方 PDF 手册（21.42 MB + 17.40 MB） |
| `simulation/` | 29.49 MB | 仿真代码与网格模型（含 7.52 MB `frame.stl`），与远端 `simulation/` 同名 |
| `plans/` | 5.84 MB | 日常计划、任务方案、总结与现场诊断数据（含 `plans/tasks/` 86 项、`diagnostics/` 3.87 MB） |
| `.workbuddy/` / `%SystemDrive%/` / `build/` / `.backup/` / `.claude/` / `.codex/` / `.vscode/` / `.agent/` | ~2.9 MB | 工具缓存、误生成的 Windows 缓存目录、本地配置与临时构建产物 |
| 根目录文档/日志（`handoff.md`、`飞镖日志汇总_*.md`、`DartLauncher_*.md/.txt` 等） | ~0.15 MB | 调试日志、交接记录、以及少量与飞镖无关或含本机路径的笔记 |

---

## 2. 仓库合并策略（保留远端已有内容）

### 2.1 方案选择：**外部暂存克隆（Staging Clone）+ 增量同步**（推荐）
- **为什么不直接在 `C:\Users\Lenovo\Desktop\xrobot` 根目录执行 `git init`**：
  1. 原目录已存在 30 个嵌套 `.git`，直接 `git init` 后极易把子目录误加为 160000 gitlink 空指针，或因删除 `.git` 而破坏 `dart-fw` / `xrobot setup` / `LibXR` submodule 的日常开发工作流；
  2. 原目录存在大量本地构建产物（`dart-fw/build/`、`.venv/`、`graphify-out/`），在原目录建仓容易误把远端旧目录（`遥控器控制左中右下控制皮筋（未进行高强度测试）/`、`下位机开发协议文档.md`）混入本地嵌入式开发树。
- **推荐做法（非破坏性暂存同步）**：
  1. 在 `C:\Users\Lenovo\AppData\Local\Temp\feibiao-sync`（或工作区外独立目录）执行 `git clone https://github.com/li718peizhe/feibiao.git`；
  2. 基于远端 `origin/main` 检出集成工作分支（如 `chore/sync-xrobot-20261125`），**严禁 `--force` 强推或覆盖历史**；
  3. **完整保留远端已有内容**：
     - 保留 `下位机开发协议文档.md`；
     - 保留 `遥控器控制左中右下控制皮筋（未进行高强度测试）/`；
     - 保留原 `.gitignore` 规则并做增量追加；
     - `master` 分支不做删除或强改，仅按正常快进/合并策略更新默认分支 `main`（或先推 PR/分支审阅后再合入 `main`）。
  4. **同名目录 `simulation/` 的合并处理**：
     - 在同步前先对暂存仓的 `simulation/` 与本地 `C:\Users\Lenovo\Desktop\xrobot\simulation/` 执行差异比对（`diff -rq` / PowerShell 哈希对比）；
     - 若本地 `simulation/` 是远端 `4ad0d38` 的直接演进（仅新增/更新且无误删），则以本地最新文件更新 `simulation/`（剔除 `__pycache__/`）；若远端存在本地没有的独有文件，则保留远端独有文件，不做静默删除。
  5. **新增目录布局**（平铺在仓库根下，清晰可读，与远端旧文件零冲突）：
     - `dart-fw/`（主固件快照，含 `DartLauncher` 与 `Referee` 最新工作区源码）
     - `xrobot-local/`（本地模块源码与 `index.yaml`，不含 `build-host/`）
     - `plans/`（日常计划、任务方案、总结与诊断数据）
     - `docs/logs/`（或根目录按原名保留：经人工确认的飞镖调试日志汇总、`handoff.md` 等问题触发与解决记录）
     - `SYNC_MANIFEST.md`（记录本次同步的各嵌套仓库基线 commit、dirty 文件清单与复现说明）

---

## 3. 嵌套 `.git` 处理方式（30 个嵌套仓库）

本地共有 **30 个嵌套 `.git`**，分为四类：
1. **有未提交实车改动的仓库（核心资产）**：
   - `dart-fw/.git`（`Core/Src/main.c`、`User/RobotConfig/artisans_dart.yaml` 有未提交修改）
   - `dart-fw/Modules/local/DartLauncher/.git`（含 4j/4k 最新 11 个文件修改，领先 `xrobot-local/DartLauncher`）
   - `dart-fw/Modules/QDU-Robomaster/Referee/.git`（含 3 个裁判系统本地补丁）
2. **本地自研独立仓库**：
   - `xrobot-local/DartLauncher/.git`（停在 `e17b490`）
   - `xrobot-local/ATSRuntime/.git`（已有独立远端 `li718peizhe/ATSRuntime.git`）
3. **第三方依赖仓库与 Submodule**：
   - `dart-fw/Middlewares/Third_Party/LibXR/.git`（`.git` 文件指针）
   - `dart-fw/Modules/QDU-Robomaster/*`（18 个）与 `dart-fw/Modules/xrobot-org/*`（6 个）
4. **旧副本内嵌套仓库**：
   - `四发完整流程 - 副本/Modules/local/ATSRuntime/.git`（建议整目录排除）

### 处理原则（杜绝 gitlink，零破坏原目录）
1. **原目录只读不改**：绝不删除 `C:\Users\Lenovo\Desktop\xrobot` 下的任何 `.git` 目录或文件。
2. **暂存区“去 `.git` 镜像拷贝”（Vendored Snapshot）**：
   - 使用 `robocopy /E /XD .git ...`（并额外清理名为 `.git` 的文件，如 `LibXR/.git` 文件指针，因为 `/XD` 只排除目录不排除 `.git` 文件！）将源码同步到暂存仓。
   - 这样暂存仓内**不存在任何 `.git` 目录或 `.git` 文件**，所有源码（包括 `DartLauncher` 和 `Referee` 的最新未提交修改）都会以普通 blob（mode `100644`）完整提交到 GitHub，不会出现灰色不可点击的 160000 gitlink 空目录。
3. **溯源记录（`SYNC_MANIFEST.md`）**：
   - 同步时自动导出各关键嵌套仓库的 `HEAD` commit、branch、remote URL 及 `git status --short` 列表写入 `SYNC_MANIFEST.md`，既保证代码可一键浏览/编译，又保留上游版本溯源信息。
4. **消除双份 `DartLauncher` 误解**：
   - 在 `SYNC_MANIFEST.md` 明确标注：`dart-fw/Modules/local/DartLauncher` 为当前实车最新源码（含未提交的 4j/4k 修改），`xrobot-local/DartLauncher` 为 `xrobot.lock` 锁定基线（`e17b490`）。

---

## 4. 纳入 / 排除清单与 `.gitignore` 规划

### 4.1 纳入清单（预计总增量 ~120 MB 以内，单文件均 < 8 MB）
| 路径 | 纳入范围 | 预估体量 |
| --- | --- | ---: |
| `dart-fw/` | 除 `build/`、`.git`、`build-*.log`、`.settings/`、`.mxproject` 外的全部工程源码、配置、CubeMX `DevC.ioc`、`Drivers/`、`Middlewares/`（去除 `LibXR/.git` 文件）、`Modules/`（去除各子 `.git`） | ~91 MB |
| `xrobot-local/` | `index.yaml`、`ATSRuntime/`、`DartLauncher/`（剔除 `build-host/` 与 `.git`） | ~1.3 MB |
| `simulation/` | 仿真脚本与模型（与远端比对合并，剔除 `__pycache__/`） | ~29 MB（远端已有大部分，实际增量极小） |
| `plans/` | 全部计划、任务方案、总结、问题排查与 `diagnostics/`（剔除 `__pycache__/`，且需经第 5 节隐私复核） | ~5.8 MB |
| 飞镖调试日志与复盘 | `handoff.md`、`飞镖日志汇总_21-05至21-29.md`、`DartLauncher_*.md`、`DartLauncher_arm_filtered.txt`（脱敏后纳入，可放 `docs/logs/` 或根目录） | ~0.12 MB |
| `GEMINI.md`（可选） | 项目协作约定文档（若确认无个人敏感路径） | ~2 KB |

### 4.2 明确排除清单（不上传）
| 路径 | 体量 | 排除原因 |
| --- | ---: | --- |
| `dart-fw/build/`、`build/`、`xrobot-local/DartLauncher/build-host/` | ~736 MB | 编译中间产物与历史 `.elf`/`.a` 二进制 |
| `graphify-out/` | 287.59 MB | 可再生知识图谱缓存，含 3 个 >50 MB 的 `graph.json` |
| `.venv/` | 142.28 MB | 本地 Python 虚拟环境 |
| `四发完整流程 - 副本/` | 285.02 MB | 历史备份副本（含 12.68 MB `.lib` 与第三方 PDF），避免与现有工程混淆（见第 8 节确认） |
| `rules/` | 41.16 MB | RoboMaster 官方版权 PDF（21.4 MB + 17.4 MB），已有自编摘要 `plans/rules-2027-dart.md`（见第 8 节确认） |
| `%SystemDrive%/` | 0.94 MB | 误生成的 Windows 缓存目录 |
| `.backup/`、`.claude/`、`.codex/`、`.agent/`、`.vscode/`、`.workbuddy/` | ~2.0 MB | 本机 IDE/Agent 配置、会话状态与备份 |
| `.tmp_gpt.json`、`codex-report.md`、`dart-fw/build-4g-final.log`、`cad2urdf/`（空）、`test_search.py` | ~0.03 MB | 临时脚本、工具中间输出与空目录 |
| `plans/codexplusplus-relay-gpt61-gemini-compatibility.md`、`campus-50-launch.md`、`workbuddy-case.md`（默认排除，待确认） | ~0.03 MB | 疑似非飞镖项目文档/工具中转配置笔记，公开仓库默认不纳入 |

### 4.3 根 `.gitignore` 增量规则
在保留远端原 `.gitignore` 基础上补充：
```gitignore
# Build & firmware artifacts
build/
**/build/
**/build-host/
*.elf
*.bin
*.hex
*.map
*.o
*.d
*.a
build-*.log

# Local environments & caches
.venv/
graphify-out/
.backup/
.workbuddy/
.codex/
.agent/
.vscode/
%SystemDrive%/
.tmp_*.json
codex-report.md
```

---

## 5. 隐私、合规与大文件检查（门禁）

由于 `li718peizhe/feibiao` 为 **公开仓库（Public）**，提交前必须通过以下四项强制门禁：

1. **密钥与凭据扫描（必须 0 命中）**：
   - 扫描暂存仓全部待提交文本文件，匹配：`ghp_`、`gho_`、`github_pat_`、`sk-[A-Za-z0-9]{16,}`、`AIza[0-9A-Za-z_-]{20,}`、`AKIA[0-9A-Z]{16}`、`BEGIN .*PRIVATE KEY`、`Bearer `、`api_key`、`password`、`token`、`cookie`、`Authorization:` 等。
   - 初查结果：`plans/` 与 `dart-fw/User/` 与根目录 `.md`/`.txt` 中高危密钥正则均为 0 命中；但仍需对完整暂存树执行一次全量扫描。
2. **个人隐私与本机路径检查**：
   - `handoff.md`、`plans/` 部分文件含 `C:\Users\Lenovo\...`（仅本机通用用户名 `Lenovo`，无真实姓名/手机号，仍建议抽查 `plans/` 中是否含微信号、手机号、内网地址、或第三方中转站域名/配置）。
   - 重点人工复核 `plans/codexplusplus-relay-gpt61-gemini-compatibility.md`、`campus-50-launch.md`、`workbuddy-case.md` 是否与飞镖项目无关，避免误公开无关个人记录。
3. **大文件与二进制检查**：
   - 门禁阈值：**禁止任何单文件 >= 50 MB**（GitHub 50 MB 警告、100 MB 硬拒绝）；对 **5~50 MB** 的文件逐一列出确认。
   - 预期纳入范围内 >5 MB 的文件仅 2 个：
     - `simulation/dart_sim/model/meshes/frame.stl`（7.52 MB，远端原有）
     - `dart-fw/Middlewares/Third_Party/LibXR/lib/doxygen-awesome-css/docs/img/fancy_scrollbars_webkit.gif`（5.35 MB，第三方文档配图，可在确认后顺带排除 `LibXR/lib/doxygen-awesome-css/docs/` 进一步瘦身）。
4. **Gitlink 与嵌套 `.git` 零残留检查**：
   - 门禁：`git ls-files -s | Select-String '^160000'` 输出必须为空；
   - 暂存仓工作树（排除根 `.git`）中 `Get-ChildItem -Force -Recurse -Filter '.git'` 输出必须为空。

---

## 6. 分阶段执行步骤（供后续执行者按序操作）

> 原则：全程在暂存克隆目录操作，**不修改原目录 `C:\Users\Lenovo\Desktop\xrobot` 任何文件与 `.git`**；卡住就停下汇报，不要自己乱改。

### 阶段 0：人工确认边界项
确认第 8 节中的待确认项（公开仓库可见性、排除/纳入边界、`simulation/` 合并方式、直接推 `main` 还是先推分支）。

### 阶段 1：克隆远端仓库到暂存目录
```powershell
$stage = "C:\Users\Lenovo\AppData\Local\Temp\feibiao-sync"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
git clone https://github.com/li718peizhe/feibiao.git $stage
git -C $stage checkout -b chore/sync-xrobot-20261125
```

### 阶段 2：比对 `simulation/` 并镜像同步目标目录（去 `.git`）
1. 比对 `$stage\simulation` 与 `C:\Users\Lenovo\Desktop\xrobot\simulation` 的文件差异，确认无远端独有文件被误删后再同步（排除 `__pycache__`）。
2. 使用 `robocopy` 将 `dart-fw/`、`xrobot-local/`、`plans/` 及确认纳入的调试日志同步到 `$stage`，显式排除：
   - 目录：`.git`、`build`、`build-host`、`__pycache__`、`.settings`、`.vscode`
   - 文件：`.git`（含 submodule 文件指针如 `LibXR/.git`）、`build-*.log`、`.mxproject`、`*.elf`、`*.bin`、`*.hex`、`*.map`、`*.a`、`*.o`
3. 增量更新 `$stage\.gitignore`，生成 `$stage\SYNC_MANIFEST.md`（写入各嵌套仓库 commit 与 dirty 清单）。

### 阶段 3：执行离线门禁检查（无 push）
运行第 7.1 节全部检查命令，确认：
- 无 160000 gitlink、无嵌套 `.git`；
- 无 >= 50 MB 文件；
- 密钥与敏感信息扫描 0 命中；
- 远端原有 `下位机开发协议文档.md` 与 `遥控器控制左中右下控制皮筋（未进行高强度测试）/` 均 `status` 无删除（`git diff --name-status` 中无 `D` 指向远端保留文件）。

### 阶段 4：提交并经用户确认后推送
1. 按主题拆分为清晰提交（例如：1. `chore: 更新 .gitignore 与同步清单`；2. `feat(dart-fw): 同步飞镖下位机工程与本地模块源码`；3. `docs(plans): 同步日常计划、任务方案、总结与现场日志`；4. `feat(simulation): 同步仿真更新`——视差异有无）。
2. 向用户展示 `git log --stat -n 5` 与总变更体量，获得确认后再执行 `git push -u origin ...`（合并到 `main` 时仅使用快进或常规合并，禁止 `--force`）。

---

## 7. 验证命令与验收标准

### 7.1 推送前离线验证命令（在 `$stage` 中执行）
```powershell
# 1. 确认远端保留目录与文件未被删除
Test-Path "$stage\下位机开发协议文档.md"
Test-Path "$stage\遥控器控制左中右下控制皮筋（未进行高强度测试）"

# 2. 确认暂存工作树无任何嵌套 .git（目录或文件）
Get-ChildItem -LiteralPath $stage -Force -Recurse -Filter ".git" | Where-Object { $_.FullName -ne "$stage\.git" }

# 3. 暂存全部改动后检查不存在 160000 gitlink
git -C $stage add -A
git -C $stage ls-files -s | Select-String '^160000'

# 4. 检查大文件（>= 10 MB 列出审阅，>= 50 MB 必须为 0）
Get-ChildItem -LiteralPath $stage -Recurse -File -Force | Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.Length -ge 10MB } | Sort-Object Length -Descending | Select-Object @{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}}, FullName

# 5. 检查密钥正则命中（必须为 0）
git -C $stage grep -n -I -E '(ghp_|gho_|github_pat_|sk-[A-Za-z0-9]{16,}|AIza[0-9A-Za-z_-]{20,}|AKIA[0-9A-Z]{16}|BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY)'

# 6. 检查总待提交文件数与体量
git -C $stage diff --cached --shortstat
```

### 7.2 推送后验收标准
1. **远端完整性**：在 GitHub 网页或 `gh api repos/li718peizhe/feibiao/contents` 下可见原有 `下位机开发协议文档.md`、`遥控器控制左中右下控制皮筋（未进行高强度测试）/`、`simulation/` 均完好保留；新增 `dart-fw/`、`xrobot-local/`、`plans/` 及日志文档均可正常点开浏览，无任何灰色不可打开的子模块图标。
2. **源码一致性**：`dart-fw/Modules/local/DartLauncher/` 与 `dart-fw/Modules/QDU-Robomaster/Referee/` 的最新修改已完整上传；`plans/tasks/` 日常计划、总结、问题排查记录完整。
3. **无垃圾与无泄密**：远端不含 `build/`、`build-host/`、`.venv/`、`graphify-out/`、`%SystemDrive%/`、`.claude/`、`.tmp_gpt.json`、任何 >50 MB 文件或凭据/私钥。
4. **本地零破坏**：原目录 `C:\Users\Lenovo\Desktop\xrobot` 下的 `dart-fw`、`xrobot-local` 及其 30 个 `.git` 状态与同步前完全一致。

---

## 8. 风险点与需人工确认事项

1. **仓库公开可见性（高优先级）**：`li718peizhe/feibiao` 当前为 **Public（公开仓库）**。确认是否维持公开并接受 `plans/` 中的现场调试过程与策略细节公开，还是先将仓库改为 Private 再推送？
2. **第三方目录体积权衡**：`dart-fw/Drivers/`（54 MB）与 `dart-fw/Middlewares/Third_Party/LibXR/`（27 MB）与 `dart-fw/Modules/QDU-Robomaster/`（10 MB）合计约 91 MB。若希望拉取即编译（含 `Referee` 本地补丁），建议全量快照；若希望仓库轻量化，可仅保留自研源码 + `Referee` 补丁文件（其余由 `xrobot setup` / CubeMX 复现）。需确认倾向哪种。
3. **待决目录/文件确认**：
   - `rules/`（41 MB RoboMaster 官方 PDF）：默认排除（已有 `plans/rules-2027-dart.md`），是否同意？
   - `四发完整流程 - 副本/`（285 MB 旧副本）：默认排除，是否同意？
   - 疑似非飞镖或工具笔记文件（`plans/codexplusplus-relay-gpt61-gemini-compatibility.md`、`campus-50-launch.md`、`workbuddy-case.md`）：默认排除，是否同意？
4. **推送分支方式**：确认是直接快进合入 `main` 推送，还是先推送独立分支（如 `chore/sync-xrobot-20261125`）在 GitHub 预览后再合并？

---

> 卡住就停下汇报，不要自己乱改。
