# ATSRuntime

## 1. 模块作用

This XRobot 1.0 module invokes the ATS board-service initialization sequence
when its instance is constructed.

`ATS_Runtime_Init()` is implemented by the BSP (in this project,
`application/ats_runtime.c`). This module only calls that function.

## 2. 构造接口

```cpp
ATSRuntime();
```

## 3. Topic

无 / None

## 4. 配置示例

```yaml
modules:
  - module: local/ATSRuntime
    id: ATSRuntime_0
```

## 5. 依赖与硬件

模块依赖 / Module dependencies:

无 / None

硬件 / Hardware:

无 / None
