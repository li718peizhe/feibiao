#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: 事件绑定模块：按构造参数把一个模块的事件映射到另一个模块的事件 / Event binding Module that maps the events of one Module to the events of another according to its constructor parameters
depends:
- id: QDU-Robomaster/DR16
  ref: same-or-dev
- id: QDU-Robomaster/CMD
  ref: same-or-dev
=== END MANIFEST === */
// clang-format on

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "CMD.hpp"
#include "DR16.hpp"

/**
 * @brief 事件绑定模块：按构造参数把一个模块的事件映射到另一个模块的事件。
 *        Event binding Module that maps the events of one Module to the events of
 *        another according to its constructor parameters.
 */
class EventBinder
{
 public:
  /**
   * @brief 单条事件绑定：源模块的源事件激活时，激活目标模块的目标事件。
   *        One event binding: when the source event of the source Module is activated,
   *        the target event of the target Module is activated.
   */
  struct EventBinding
  {
    const char* source_module;  ///< 源模块名称，与 `ModuleInfo::name` 一致
                                ///< Source Module name, equal to `ModuleInfo::name`
    uint32_t source_event;      ///< 源事件 ID
                                ///< Source event ID
    const char* target_module;  ///< 目标模块名称，与 `ModuleInfo::name` 一致
                                ///< Target Module name, equal to `ModuleInfo::name`
    uint32_t target_event;      ///< 目标事件 ID
                                ///< Target event ID

    /**
     * @brief 构造一条绑定，事件 ID 可以是枚举值，统一转换为 `uint32_t`。
     *        Construct a binding; event IDs can be enumerators and are converted to
     *        `uint32_t`.
     *
     * @tparam T1 源事件 ID 的类型。
     *            Type of the source event ID.
     * @tparam T2 目标事件 ID 的类型。
     *            Type of the target event ID.
     * @param src_mod 源模块名称。
     *                Source Module name.
     * @param src_evt 源事件 ID。
     *                Source event ID.
     * @param target_mod 目标模块名称。
     *                   Target Module name.
     * @param target_evt 目标事件 ID。
     *                   Target event ID.
     */
    template <typename T1, typename T2>
    constexpr EventBinding(const char* src_mod, T1 src_evt, const char* target_mod,
                           T2 target_evt)
        : source_module(src_mod),
          source_event(static_cast<uint32_t>(src_evt)),
          target_module(target_mod),
          target_event(static_cast<uint32_t>(target_evt))
    {
    }
  };

  /**
   * @brief 一组事件绑定。
   *        A group of event bindings.
   */
  struct BindingGroup
  {
    std::initializer_list<EventBinding> bindings;  ///< 该组的绑定列表
                                                   ///< Binding list of the group
  };

  /**
   * @brief 可参与绑定的模块：名称与它的事件对象。
   *        A Module that can take part in binding: its name and its event object.
   */
  struct ModuleInfo
  {
    const char* name;         ///< 模块名称 Module name
    LibXR::Event* event_ptr;  ///< 模块的事件对象 Event object of the Module

    /**
     * @brief 由提供 `GetEvent()` 的模块实例构造。
     *        Construct from a Module instance that provides `GetEvent()`.
     *
     * @tparam T 模块类型。
     *           Module type.
     * @param n 模块名称。
     *          Module name.
     * @param module 模块实例。
     *               Module instance.
     */
    template <typename T>
    constexpr ModuleInfo(const char* n, T& module)
        : name(n), event_ptr(&module.GetEvent())
    {
    }
    /**
     * @brief 由事件对象指针构造。
     *        Construct from an event object pointer.
     *
     * @param n 模块名称。
     *          Module name.
     * @param e 模块的事件对象。
     *          Event object of the Module.
     */
    constexpr ModuleInfo(const char* n, LibXR::Event* e) : name(n), event_ptr(e) {}
  };

  /**
   * @brief 构造 EventBinder，对源和目标名称都能在 `modules` 中找到的绑定执行
   *        `target_event->Bind(*source_event, source_event_id, target_event_id)`。
   *        Construct EventBinder and, for every binding whose source and target names
   *        are both found in `modules`, execute
   *        `target_event->Bind(*source_event, source_event_id, target_event_id)`.
   *
   * @param modules 可参与绑定的模块列表。
   *                Modules that can take part in binding.
   * @param event_binding_groups 事件绑定分组。
   *                             Groups of event bindings.
   */
  EventBinder(std::initializer_list<ModuleInfo> modules,
              std::initializer_list<BindingGroup> event_binding_groups = {})
  {
    auto find_event = [&](const char* name) -> LibXR::Event*
    {
      for (const auto& mod : modules)
      {
        if (strcmp(mod.name, name) == 0)
        {
          return mod.event_ptr;
        }
      }
      return nullptr;
    };

    for (const auto& group : event_binding_groups)
    {
      for (const auto& binding : group.bindings)
      {
        LibXR::Event* source_event = find_event(binding.source_module);
        LibXR::Event* target_event = find_event(binding.target_module);

        if (source_event && target_event)
        {
          target_event->Bind(*source_event, static_cast<uint32_t>(binding.source_event),
                             static_cast<uint32_t>(binding.target_event));
        }
      }
    }
  }

 private:
};
