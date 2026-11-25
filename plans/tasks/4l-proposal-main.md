# 任务 4l A 版方案：上传 `xrobot` 代码、日常计划与问题总结至 GitHub (`li718peizhe/feibiao`)

- **日期**：2026-11-25
- **作者**：主对话（A 版）
- **对应事项**：Orca item 7
- **目标**：将 `C:\Users\Lenovo\Desktop\xrobot` 中的工程代码、日常计划（`plans/`）、项目总结（`handoff.md` 等）及问题触发/排查/解决记录安全、完整地上传至 `https://github.com/li718peizhe/feibiao`（`main` 分支）。

---

## 1. 现状核对
1. **GitHub 登录与目标仓库**：
   - 本机 `gh auth status` 已登录 `li718peizhe`（HTTPS，`repo` / `workflow` 权限就绪）。
   - 目标仓库 `https://github.com/li718peizhe/feibiao` 为**公开仓库**（`isPrivate: false`），`main` 分支非空，当前已有 4 项内容：`.gitignore`、`simulation`、`下位机开发协议文档.md`、`遥控器控制左中右下控制皮筋（未进行高强度测试）`。
2. **本地 `C:\Users\Lenovo\Desktop\xrobot` 结构与体量问题**：
   - **非 Git 根目录 + 约 30 个嵌套 `.git`**：`dart-fw/`、`dart-fw/Middlewares/Third_Party/LibXR/`、`dart-fw/Modules/**`、`xrobot-local/DartLauncher/`、`xrobot-local/ATSRuntime/`、`四发完整流程 - 副本/Modules/local/ATSRuntime/` 均含独立 `.git`。若直接在根目录 `git init && git add .`，Git 会把嵌套仓库记为“空 gitlink”，导致源码文件根本传不到 GitHub。
   - **超大产物与无关/敏感目录**：
     - `dart-fw/build/`：652.98 MB（编译产物，含大体积 `.elf`/`.o`，单文件超 GitHub 100 MB 限制风险高）；
     - `graphify-out/`：287.59 MB（知识图谱缓存）；
     - `.venv/`：142.28 MB（Python 虚拟环境）；
     - `四发完整流程 - 副本/`：285.02 MB（旧 Keil 副本，含 `MDK-ARM` 编译产物）；
     - `%SystemDrive%`（0.94 MB 误生成目录）、`.tmp_gpt.json`（27 KB 临时文件）、`.agent/`、`.claude/`、`.codex/`、`.workbuddy/`（本地工具状态，可能含路径或会话信息）。
   - **未提交实车改动**：`dart-fw/Modules/local/DartLauncher`（含 4j 实车调参 + 4k 联动回零）与 `dart-fw/Core/Src/main.c`、`dart-fw/User/RobotConfig/artisans_dart.yaml` 当前有本地未提交改动，必须完整保留并上传。

---

## 2. A 版设计方案

### 2.1 隔离暂存克隆策略（零破坏本地工作区）
- **不在 `C:\Users\Lenovo\Desktop\xrobot` 原地 `git init`**：
  - 在临时目录（如 `C:\Users\Lenovo\AppData\Local\Temp\feibiao-stage`）克隆 `https://github.com/li718peizhe/feibiao.git`（保留远端已有 `main` 提交历史，不覆盖原有 4 项内容）。
  - 将筛选后的目录/文件复制到克隆工作区中的对应子目录（如直接放仓库根目录，或整理为清晰目录结构），**过滤掉所有嵌套 `.git` 目录**，使全部源码以真实文件形式纳入单仓版本控制，同时完全不触碰本地 `dart-fw` / `xrobot-local` 的 `.git` 状态。

### 2.2 纳入与排除清单
- **纳入（代码 + 日常计划 + 总结 + 问题触发/解决）**：
  1. **固件与模块代码**：`dart-fw/`（含 `Core/`、`User/`、`Modules/`、`Middlewares/`、`Drivers/`、`cmake/`、`tools/`、`CMakeLists.txt`、`CMakePresets.json`、`xrobot.lock` 等，排除 `build/` 与所有 `.git`）、`xrobot-local/`（排除 `.git`、`build-host/`）、`simulation/`（与远端既有 `simulation` 合并对齐）。
  2. **日常计划与协议文档**：`plans/` 全量（含 `plans/tasks/*.md`、`plans/rules-2027-dart.md`、`plans/vision-protocol-v1.0-20261031.md`、`plans/dart-fw-libxr.md`、`plans/tasks/diagnostics/` 等；检查单文件均 < 50 MB）。
  3. **总结与问题记录**：`handoff.md`、`.backup/handoff.md.*.bak`、`campus-50-launch.md`、`DartLauncher_*.md` / `DartLauncher_arm_filtered.txt`、`飞镖日志汇总_21-05至21-29.md`、`workbuddy-case.md`、`codex-report.md`、`GEMINI.md`、`.vscode/tasks.json`。
  4. **新增/更新根目录 `README.md` 与 `.gitignore`**：在 `README.md` 索引工程结构、交接总结（`handoff.md`）、日常计划（`plans/tasks/`）与问题排查记录；`.gitignore` 屏蔽 `build/`、`build-host/`、`.venv/`、`__pycache__/`、`*.elf`、`*.bin`、`*.hex`、`*.o`、`*.tmp`。
- **排除（明确不传）**：
  - `dart-fw/build/`、`.venv/`、`graphify-out/`、`%SystemDrive%/`、`四发完整流程 - 副本/`（如确需保留旧 Keil 源码可另议，默认不传 285 MB 副本）、`.tmp_gpt.json`、`.agent/`、`.claude/`、`.codex/`、`.workbuddy/`、`rules/`（41 MB 公开规则 PDF 如需要可后续单独决定，默认先排除大二进制或仅保留 `rules/md/`）。

### 2.3 安全与大文件检查
- 推送前对暂存仓执行：
  1. 单文件大小检查（确认无 >50 MB 文件，杜绝 GitHub 100 MB 拒收）；
  2. 敏感信息扫描（检查无 token / key / 密码 / 私人凭据）；
  3. 确认远端既有文件（`下位机开发协议文档.md`、`遥控器控制左中右下控制皮筋（未进行高强度测试）`）未被误删。

---

## 3. 验证与验收标准
1. 本地 `C:\Users\Lenovo\Desktop\xrobot` 原有嵌套 `.git` 仓库、`dart-fw` 未提交改动及文件无任何损坏或丢失。
2. `https://github.com/li718peizhe/feibiao` `main` 分支成功推送，包含代码（含 4j/4k 最新源码）、`plans/` 日常计划、`handoff.md` 总结及问题排查记录，且保留远端原有 4 项内容。
3. 仓库内无 `.venv/`、`build/`、`graphify-out/`、嵌套空 gitlink、敏感信息或 >50 MB 大文件。
