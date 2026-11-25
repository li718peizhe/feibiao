# EventBinder

事件绑定模块：按构造参数把一个模块的事件映射到另一个模块的事件 / Event binding Module that maps the events of one Module to the events of another according to its constructor parameters

## 1. 模块作用 / Purpose

EventBinder 把模块之间的联动关系（例如遥控器拨杆切换 CMD 控制模式、切换云台与底盘模式）写在配置中，业务代码中不出现这些映射。

`modules` 声明可参与绑定的模块：每项是一个名字和一个提供 `LibXR::Event& GetEvent()` 的模块实例（`ModuleInfo(name, module)` 取 `&module.GetEvent()`，也可直接给出 `LibXR::Event*`）。`event_binding_groups` 是若干组绑定，每组的 `bindings` 是若干条 `EventBinding`，格式为 `{源模块名, 源事件 ID, 目标模块名, 目标事件 ID}`；事件 ID 可以直接写枚举值，构造时统一转换为 `uint32_t`。

构造函数遍历全部绑定，按名字在 `modules` 中查找源和目标，对源和目标都能找到的绑定执行 `target_event->Bind(*source_event, source_event_id, target_event_id)`：此后源模块激活源事件时，目标模块的目标事件随之被激活。绑定只在构造时完成一次。

EventBinder puts the linkage between Modules, for example remote controller switches changing the CMD control mode or the gimbal and chassis modes, into the configuration, so that the business code contains none of these mappings.

`modules` declares the Modules that can take part in binding: each entry is a name and a Module instance that provides `LibXR::Event& GetEvent()` (`ModuleInfo(name, module)` takes `&module.GetEvent()`; a `LibXR::Event*` can also be given directly). `event_binding_groups` is a list of binding groups, and the `bindings` of each group is a list of `EventBinding` in the form `{source Module name, source event ID, target Module name, target event ID}`; event IDs can be written as enumerators and are converted to `uint32_t` on construction.

The constructor walks all bindings, looks up the source and the target by name in `modules`, and for each binding whose source and target are both found executes `target_event->Bind(*source_event, source_event_id, target_event_id)`: from then on, when the source Module activates the source event, the target event of the target Module is activated as well. Binding is done once at construction.

## 2. 构造接口 / Constructor

```cpp
EventBinder(std::initializer_list<ModuleInfo> modules,
            std::initializer_list<BindingGroup> event_binding_groups = {});
```

配置参数：

- `modules`：`std::initializer_list<EventBinder::ModuleInfo>`，无默认值。每项为 `{"名字", 实例}`，名字是 C++ 字符串字面量，实例是在本实例之前列出的某个模块实例的 id。
- `event_binding_groups`：`std::initializer_list<EventBinder::BindingGroup>`，默认 `{}`（不绑定）。每个 `BindingGroup` 只有一个成员 `bindings`，即 `EventBinding` 列表；每条 `EventBinding` 为 `{"源模块名", 源事件, "目标模块名", 目标事件}`，名字与 `modules` 中的名字一致。

Configuration parameters:

- `modules`: `std::initializer_list<EventBinder::ModuleInfo>`, no default. Each entry is `{"name", instance}`, where the name is a C++ string literal and the instance is the id of a Module instance listed before this one.
- `event_binding_groups`: `std::initializer_list<EventBinder::BindingGroup>`, default `{}` (no binding). Each `BindingGroup` has the single member `bindings`, a list of `EventBinding`; each `EventBinding` is `{"source Module name", source event, "target Module name", target event}`, with names equal to the names in `modules`.

## 3. Topic

无 / None

## 4. 配置示例 / Configuration Example

`xrobot instance add QDU-Robomaster/EventBinder` 写入的实例，`modules` 与 `event_binding_groups` 按 C++ 初始化列表文本填写。下例把 DR16 的拨杆映射到 CMD 的控制模式和云台模式：

An instance written by `xrobot instance add QDU-Robomaster/EventBinder`, with `modules` and `event_binding_groups` filled in as C++ initializer list text. The example maps the DR16 switches to the CMD control mode and the gimbal mode:

```yaml
modules:
  - module: QDU-Robomaster/EventBinder
    id: event_binder
    args:
      - modules: '{{"dr16", dr16}, {"cmd", cmd}, {"gimbal", gimbal}}'
      - event_binding_groups: '{{{{"dr16", DR16::SwitchPos::DR16_SW_L_POS_MID, "cmd", CMD::Mode::CMD_OP_CTRL}, {"dr16", DR16::SwitchPos::DR16_SW_L_POS_BOT, "cmd", CMD::Mode::CMD_AUTO_CTRL}, {"dr16", DR16::SwitchPos::DR16_SW_R_POS_TOP, "gimbal", GimbalEvent::SET_MODE_RELAX}, {"dr16", DR16::SwitchPos::DR16_SW_R_POS_MID, "gimbal", GimbalEvent::SET_MODE_COMMON}}}}'
```

`dr16`、`cmd`、`gimbal` 是其他模块实例的 id，分别由 `QDU-Robomaster/DR16`、`QDU-Robomaster/CMD`、`QDU-Robomaster/Gimbal` 提供，须在本实例之前列出。`event_binding_groups` 的四层花括号依次为组列表、一组、绑定列表和一条绑定。

`dr16`, `cmd` and `gimbal` are the ids of other Module instances provided by `QDU-Robomaster/DR16`, `QDU-Robomaster/CMD` and `QDU-Robomaster/Gimbal`; they are listed before this instance. The four levels of braces in `event_binding_groups` are the group list, one group, the binding list and one binding.

## 5. 依赖与硬件 / Dependencies and Hardware

依赖：

- `QDU-Robomaster/DR16`：遥控器模块，常用的事件源（`DR16::SwitchPos`、`DR16::Key`）。
- `QDU-Robomaster/CMD`：常用的事件目标（`CMD::Mode`）。
- LibXR。

绑定对象可以是任何提供 `GetEvent()` 的模块。硬件：无。

Dependencies:

- `QDU-Robomaster/DR16`: the remote controller Module, a common event source (`DR16::SwitchPos`, `DR16::Key`).
- `QDU-Robomaster/CMD`: a common event target (`CMD::Mode`).
- LibXR.

Any Module that provides `GetEvent()` can be bound. Hardware: none.
