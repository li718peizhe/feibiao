# 任务 4l 定稿方案：整理并上传 `xrobot` 代码、日常计划与问题总结至 GitHub (`li718peizhe/feibiao`)

- **日期**：2026-11-25
- **状态**：已规划
- **对应事项**：Orca item 7
- **依据方案**：
  - A 版（主对话）：`plans/tasks/4l-proposal-main.md`
  - B 版（`planner`）：`plans/tasks/4l-proposal-planner.md`
- **目标**：将 `C:\Users\Lenovo\Desktop\xrobot` 中的飞镖工程代码、日常计划（`plans/`）、项目总结（`handoff.md` 等）及问题触发/排查/解决记录安全、完整地上传至 `https://github.com/li718peizhe/feibiao`，保留远端已有内容且零破坏本地嵌套 Git 工作区。

---

## 1. 方案对比与采纳

| 分歧点 | A 版（主对话） | B 版（`planner`） | 采纳结论与理由 |
|---|---|---|---|
| **仓库合并与推送分支** | 在外部暂存目录克隆远端 `main`，直接合入 `main` 并保留原有 4 项 | 在外部暂存目录基于远端 `origin/main` 检出独立分支（如 `chore/sync-xrobot-20261125`），保留 `main`/`master` 及原有 4 项，待确认后再合入 `main` | **采纳 B 版（先推独立分支或经人工确认后快进合入 `main`，严禁 `--force`）**。原因：远端 `main` 已有既有提交 `4ad0d38` 和 4 项内容，先经离线门禁与差异比对再推送更稳妥 |
| **嵌套 `.git` 处理** | 复制时过滤嵌套 `.git` 目录 | 用 `robocopy` 显式排除 `.git` 目录及 `.git` 文件指针（如 `LibXR/.git`），并新增 `SYNC_MANIFEST.md` 记录各嵌套仓库 commit 与 dirty 清单 | **采纳 B 版**。原因：`LibXR/.git` 是 submodule 文件指针而非目录，只排目录会漏掉它；加 `SYNC_MANIFEST.md` 可完整保留原有版本溯源信息 |
| **非飞镖/临时/隐私文件边界** | 排除 `build/`、`.venv/`、`graphify-out/`、`%SystemDrive%/`、`四发完整流程 - 副本/`、`rules/`、`.tmp_gpt.json`、`.agent/.claude/.codex/.workbuddy`；保留 `.backup/handoff*`、`workbuddy-case.md`、`campus-50-launch.md`、`codex-report.md`、`GEMINI.md`、`.vscode/tasks.json` | 同样排除大体积/环境/IDE 目录，同时进一步识别出 `plans/codexplusplus-relay-gpt61-gemini-compatibility.md`、`campus-50-launch.md`、`workbuddy-case.md`、`rules/` 等疑似非飞镖或工具笔记建议默认排除 | **采纳 B 版作为默认安全基线（公开仓库最小暴露原则）**：默认排除 `plans/codexplusplus-relay-gpt61-gemini-compatibility.md`、`campus-50-launch.md`、`workbuddy-case.md`、`codex-report.md`、`.backup/`、`.vscode/`、`rules/`、`四发完整流程 - 副本/`、`dart-fw/build/`、`xrobot-local/DartLauncher/build-host/`、`.venv/`、`graphify-out/`、`%SystemDrive%/`、`LibXR/lib/doxygen-awesome-css/docs/`；若用户确认需要保留其中某项再放开 |
| **大文件与公开仓库门禁** | 检查 >50 MB 文件与敏感信息 | 给出完整 6 条离线 PowerShell 门禁命令（保留检查、零 `.git`、零 `160000` gitlink、`>=10 MB` 审阅 & `>=50 MB` 归零、密钥正则 0 命中、`--shortstat`） | **采纳 B 版**。原因：门禁命令可直接执行复现，能 100% 避免 GitHub 公开仓库泄密或被拒收 |

---

## 2. 阶段划分与改动范围

> **硬约束**：全程在外部暂存目录 `$stage = "C:\Users\Lenovo\AppData\Local\Temp\feibiao-sync"` 操作，**严禁修改原目录 `C:\Users\Lenovo\Desktop\xrobot` 任何文件或 `.git`**。

### 阶段 1：克隆远端基线并对齐 `simulation/`
1. `git clone https://github.com/li718peizhe/feibiao.git $stage`
2. 检出工作分支 `chore/sync-xrobot-20261125`（或经人工确认后直接合入 `main`）。
3. 比对 `$stage\simulation` 与本地 `xrobot\simulation` 差异，确认不误删远端独有文件后同步（排除 `__pycache__`）。
4. 完整保留远端原有 `下位机开发协议文档.md`、`遥控器控制左中右下控制皮筋（未进行高强度测试）/` 及原 `.gitignore` 规则。

### 阶段 2：镜像同步代码、计划、总结与问题记录
1. **`dart-fw/`**：全量源码（含 `Core/`、`User/`、`Modules/local/DartLauncher` 最新 4j+4k 未提交改动、`Modules/QDU-Robomaster/Referee` 3 处本地补丁、`Drivers/`、`Middlewares/`、`cmake/`、`tools/`），排除 `build/`、`.git`（目录与文件指针）、`.settings/`、`.vscode/`、`build-*.log`、`*.elf/*.bin/*.hex/*.map/*.a/*.o` 及 `LibXR/lib/doxygen-awesome-css/docs/`。
2. **`xrobot-local/`**：`DartLauncher/`、`ATSRuntime/` 源码，排除 `build-host/` 与 `.git`。
3. **`plans/`**：日常计划、任务方案与 `diagnostics/`（默认排除 `plans/codexplusplus-relay-gpt61-gemini-compatibility.md`）。
4. **总结与问题记录**：`handoff.md`、`飞镖日志汇总_21-05至21-29.md`、`DartLauncher_*.md`、`DartLauncher_arm_filtered.txt`。
5. **补充仓库元文件**：增量更新 `$stage\.gitignore`，生成 `$stage\SYNC_MANIFEST.md`（记录本地嵌套仓库 commit 与 dirty 摘要）和根目录 `README.md` 索引。

### 阶段 3：离线门禁检查（推送前必过）
```powershell
Test-Path "$stage\下位机开发协议文档.md"
Test-Path "$stage\遥控器控制左中右下控制皮筋（未进行高强度测试）"
Get-ChildItem -LiteralPath $stage -Force -Recurse -Filter ".git" | Where-Object { $_.FullName -ne "$stage\.git" }
git -C $stage add -A
git -C $stage ls-files -s | Select-String '^160000'
Get-ChildItem -LiteralPath $stage -Recurse -File -Force | Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.Length -ge 10MB } | Sort-Object Length -Descending | Select-Object @{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}}, FullName
git -C $stage grep -n -I -E '(ghp_|gho_|github_pat_|sk-[A-Za-z0-9]{16,}|AIza[0-9A-Za-z_-]{20,}|AKIA[0-9A-Z]{16}|BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY)'
git -C $stage diff --cached --shortstat
```

### 阶段 4：提交与推送
- 按主题拆分为清晰提交（`.gitignore`+清单、`dart-fw`+`xrobot-local` 源码、`plans/`+总结与日志、`simulation/`），确认无误后推送至 `origin`（禁止 `--force`）。

---

## 3. 验收标准
1. **远端完整**：`https://github.com/li718peizhe/feibiao` 保留原有 4 项，新增 `dart-fw/`（含 4j/4k 最新源码）、`xrobot-local/`、`plans/`、`handoff.md` 及日志文档均可正常点开浏览，无灰色不可打开的 gitlink 子模块。
2. **门禁全绿**：无 `160000` gitlink、无嵌套 `.git`、无 `>= 50 MB` 文件、密钥扫描 0 命中。
3. **本地零破坏**：`C:\Users\Lenovo\Desktop\xrobot` 原有 30 个嵌套 `.git` 与工作区文件无任何改动或损坏。

---

## 4. 风险点与需人工确认处
1. **公开仓库可见性（高优先）**：`li718peizhe/feibiao` 当前为 **Public（公开）**，所有人可见。请确认维持公开，还是先改为 Private 再上传？
2. **默认排除项确认**：`dart-fw/build/`（653 MB）、`graphify-out/`（288 MB）、`.venv/`（142 MB）、`四发完整流程 - 副本/`（285 MB）、`rules/`（41 MB 官方 PDF）、非飞镖笔记（`campus-50-launch.md`、`workbuddy-case.md`、`plans/codexplusplus-relay-gpt61-gemini-compatibility.md`）默认排除，是否同意？
3. **推送分支方式**：确认直接合入 `main` 推送，还是先推送新分支（如 `chore/sync-xrobot-20261125`）在 GitHub 预览后再合并？
