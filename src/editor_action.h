#pragma once

#include <variant>

#include "debug.h"
#include "position.h"
#include "signal.h"
#include "util.h"

class Selection;

namespace VME
{

  enum MouseButtons
  {
    NoButton = 0,
    LeftButton = 1 << 0,
    RightButton = 1 << 1,
    MiddleButton = 1 << 2,
    BackButton = 1 << 3,
    ExtraButton1 = 1 << 4,
    ExtraButton2 = 1 << 5,
    ExtraButton3 = 1 << 6,
    ExtraButton4 = 1 << 7
  };
  using MouseButton = MouseButtons;
  VME_ENUM_OPERATORS(MouseButtons)

  enum ModifierKeys
  {
    None = 0,
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
  };
  VME_ENUM_OPERATORS(ModifierKeys)

  struct MouseEvent
  {
    MouseEvent(ScreenPosition pos, MouseButtons buttons, ModifierKeys modifiers) : _pos(pos), _buttons(buttons), _modifiers(modifiers) {}
    inline MouseButtons buttons() const noexcept
    {
      return _buttons;
    }

    inline ModifierKeys modifiers() const noexcept
    {
      return _modifiers;
    }

    inline ScreenPosition pos() const noexcept
    {
      return _pos;
    }

  private:
    ScreenPosition _pos;

    MouseButtons _buttons;
    ModifierKeys _modifiers;
  };
} // namespace VME

struct MouseAction
{
  struct RawItem
  {
    uint32_t serverId = 100;
    /*
      If true, the raw item is currently being drag in an area. Once released,
      each position of the area has an item of serverId added.
    */
    bool area = false;

    /**
     * If true, this action erases rather than adds items
     */
    bool erase = false;
  };

  struct Select
  {
    std::optional<Position> moveOrigin = {};
    std::optional<Position> moveDelta = {};
    bool area = false;

    bool isMoving() const;

    void updateMoveDelta(const Selection &selection, const Position &currentPosition);
    void setMoveOrigin(const Position &origin);
    void reset();
  };

  struct Pan
  {
    std::optional<WorldPosition> cameraOrigin;
    std::optional<ScreenPosition> mouseOrigin;

    inline bool active() const
    {
      return cameraOrigin && mouseOrigin;
    }

    void stop()
    {
      cameraOrigin.reset();
      mouseOrigin.reset();
    }
  };

  struct None
  {
  };
};

using MouseAction_t = std::variant<MouseAction::None, MouseAction::RawItem, MouseAction::Select, MouseAction::Pan>;

/**
 * Utility class for sending UI information to a MapView.
 * 
 * This is a necessary effect of separating normal code and UI code
 *
 * */
class UIUtils
{
public:
  virtual ~UIUtils() = default;

  virtual ScreenPosition mouseScreenPosInView() = 0;
  virtual VME::ModifierKeys modifiers() const = 0;
  virtual void waitForDraw(std::function<void()> f) = 0;
};

/*
  Contains mouse actions that can occur on a MapView.
*/
class EditorAction
{
public:
  inline MouseAction_t &action() noexcept
  {
    return _action;
  };

  inline void setPrevious()
  {
    DEBUG_ASSERT(!_locked, "The action is locked.");

    _action = _previousAction;
    _previousAction = MouseAction::None{};

    actionChanged.fire(_action);
  }

  inline MouseAction_t previous() const noexcept
  {
    return _previousAction;
  }
  /**
   * @return true if the set was successful.
   */
  bool setIfUnlocked(const MouseAction_t action)
  {
    if (_locked)
      return false;

    set(action);
    return true;
  }

  void set(const MouseAction_t action)
  {
    DEBUG_ASSERT(!_locked, "The action is locked.");
    _previousAction = _action;
    _action = action;

    actionChanged.fire(_action);
  }

  void setRawItem(uint32_t serverId) noexcept
  {
    set(MouseAction::RawItem{serverId});
  }

  void reset() noexcept
  {
    unlock();
    set(MouseAction::Select{});
  }

  template <typename T>
  T *as()
  {
    return std::get_if<T>(&_action);
  }

  template <typename T>
  bool is()
  {
    return std::holds_alternative<T>(_action);
  }

  template <auto MemberFunction, typename T>
  void onActionChanged(T *instance)
  {
    actionChanged.connect<MemberFunction>(instance);
  }

  inline void lock() noexcept;
  inline void unlock() noexcept;
  inline bool locked() const noexcept;

private:
  Nano::Signal<void(const MouseAction_t &)> actionChanged;

  MouseAction_t _previousAction = MouseAction::Select{};
  MouseAction_t _action = MouseAction::Select{};
  bool _locked = false;
};

inline void EditorAction::lock() noexcept
{
  _locked = true;
}

inline void EditorAction::unlock() noexcept
{
  _locked = false;
}

inline bool EditorAction::locked() const noexcept
{
  return _locked;
}