// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/keyboard_hook_win_base.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/win/events_win_utils.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

namespace {

// The Windows KeyboardHook implementation uses a low-level hook in order to
// intercept system key combinations such as Alt + Tab.  A standard hook or
// other window message filtering mechanisms will allow us to observe key events
// but we would be unable to block them or the corresponding system action.
// There are downsides to this approach as described in the following blurbs.

// Low-level hooks are not given a repeat state for each key event.  This is
// because the events are intercepted prior to the OS handling them and tracking
// their states the usual way.  We solve this by tracking the last key seen and
// modifying the event manually to indicate it is a repeat.  This works for
// every key except escape which is used to exit fullscreen and tear down the
// hook.  The quirk is that after the hook is torn down, the first escape key
// event passed to the window will appear like an initial keydown.  This is
// because it *is* an initial keydown from Windows' perspective as it was being
// intercepted before then.

// Intercepting modifier keys (Ctrl, Shift, Win, Etc.) within a low-level
// keyboard hook will result in an incorrect modifier state reported by the OS
// for that key.  This is because the hook intercepts the event before the OS
// has a chance to observe the event and update its internal state.  This
// trade-off is necessary as we also want to intercept and prevent specific key
// combos from taking effect.

// A related side-effect occurs when intercepting printable characters.  Since
// the key event is intercepted before the OS handles it, no char events are
// produced.  So if we intercept scan codes which should generate a printable
// character, the result is that the key up/down events are sent correctly but
// no WM_CHAR message is generated.  This is unacceptable for some usages of the
// hook as the behavior is very different between locked and unlocked states.

// This is a fair number of downsides however the ability to block system key
// combos is a hard requirement so we need to work around them.

// The solution we have adopted is:
// - Only intercept modifiers and allow all other keys to pass through
//   Note: Shift is not included as otherwise it is not applied by the OS and
//         the printable characters generated by the key event will be wrong.
// - Track the repeat state ourselves
// - Update the per-thread key state for the tracked modifiers using
//   SetKeyboardState().

// In practice this works well as intercepting the modifiers allows us to block
// system keyboard combos and all other keys generate the proper events and
// printable chars.

// Using SetKeyboardState() allows us to tell Windows the current state for the
// modifiers we intercept.  This state only works for the current thread,
// meaning that other applications / threads which check the key state for the
// modifiers will not receive an accurate value.  One constraint for the
// KeyboardHook is that it is only engaged when our window is fullscreen and
// focused so we don't need to worry too much about other apps.

// Represents a VK_LCONTROL scancode with the 0x02 flag in the high word.
// The 0x02 flag does not seem to be documented on MSDN or in the public Windows
// headers.  I suspect it is related to the KF_ family of constants which skips
// from 0x0100 to 0x0800 with 0x0200 and 0x0400 undocumented (likely reserved).
constexpr DWORD kSynthesizedControlScanCodeForAltGr = 0x021D;

// {Get|Set}KeyboardState() receives an array with 256 elements.  This is
// described in MSDN but no constant exists for this value.  Function reference:
// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-getkeyboardstate
// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-setkeyboardstate
constexpr int kKeyboardStateArraySize = 256;

// Used for setting and testing the bits in the KeyboardState array.
constexpr BYTE kKeyDown = 0x80;
constexpr BYTE kKeyUp = 0x00;

bool IsAltKey(DWORD vk) {
  return vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
}

bool IsControlKey(DWORD vk) {
  return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL;
}

bool IsWindowsKey(DWORD vk) {
  return vk == VK_LWIN || vk == VK_RWIN;
}

bool IsModifierKey(DWORD vk) {
  // We don't track the state of the shift key as we want to allow the OS to
  // know about its state so it is correctly applied to printable characters.
  return IsAltKey(vk) || IsControlKey(vk) || IsWindowsKey(vk);
}

class ModifierKeyboardHookWinImpl : public KeyboardHookWinBase {
 public:
  ModifierKeyboardHookWinImpl(base::Optional<base::flat_set<DomCode>> dom_codes,
                              KeyEventCallback callback,
                              bool enable_hook_registration);
  ~ModifierKeyboardHookWinImpl() override;

  // KeyboardHookWinBase implementation.
  bool ProcessKeyEventMessage(WPARAM w_param,
                              DWORD vk,
                              DWORD scan_code,
                              DWORD time_stamp) override;

  bool Register();

 private:
  static LRESULT CALLBACK ProcessKeyEvent(int code,
                                          WPARAM w_param,
                                          LPARAM l_param);

  void UpdateModifierState(DWORD vk, bool key_down);

  void ClearModifierStates();

  static ModifierKeyboardHookWinImpl* instance_;

  // Tracks the last non-located key down seen in order to determine if the
  // current key event should be marked as a repeated key press.
  DWORD last_key_down_ = 0;

  // Tracks the number of AltGr key sequences seen since the start of the most
  // recent AltGr key down.  When the AltGr key is pressed, Windows injects a
  // synthesized left control key event followed by the right alt key event.
  // This sequence occurs on the initial keypress and every repeat.
  int altgr_sequence_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ModifierKeyboardHookWinImpl);
};

// static
ModifierKeyboardHookWinImpl* ModifierKeyboardHookWinImpl::instance_ = nullptr;

ModifierKeyboardHookWinImpl::ModifierKeyboardHookWinImpl(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback,
    bool enable_hook_registration)
    : KeyboardHookWinBase(std::move(dom_codes),
                          std::move(callback),
                          enable_hook_registration) {}

ModifierKeyboardHookWinImpl::~ModifierKeyboardHookWinImpl() {
  ClearModifierStates();

  if (!enable_hook_registration())
    return;

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

bool ModifierKeyboardHookWinImpl::Register() {
  // Only one instance of this class can be registered at a time.
  DCHECK(!instance_);
  instance_ = this;

  return KeyboardHookWinBase::Register(reinterpret_cast<HOOKPROC>(
      &ModifierKeyboardHookWinImpl::ProcessKeyEvent));
}

void ModifierKeyboardHookWinImpl::ClearModifierStates() {
  BYTE keyboard_state[kKeyboardStateArraySize] = {0};
  if (!GetKeyboardState(keyboard_state)) {
    DPLOG(ERROR) << "GetKeyboardState() failed: ";
    return;
  }

  // Reset each modifier position.
  keyboard_state[VK_CONTROL] = kKeyUp;
  keyboard_state[VK_LCONTROL] = kKeyUp;
  keyboard_state[VK_RCONTROL] = kKeyUp;
  keyboard_state[VK_MENU] = kKeyUp;
  keyboard_state[VK_LMENU] = kKeyUp;
  keyboard_state[VK_RMENU] = kKeyUp;
  keyboard_state[VK_LWIN] = kKeyUp;
  keyboard_state[VK_RWIN] = kKeyUp;

  if (!SetKeyboardState(keyboard_state))
    DPLOG(ERROR) << "SetKeyboardState() failed: ";
}

bool ModifierKeyboardHookWinImpl::ProcessKeyEventMessage(WPARAM w_param,
                                                         DWORD vk,
                                                         DWORD scan_code,
                                                         DWORD time_stamp) {
  // The |vk| delivered to the low-level hook includes a location which is
  // needed to track individual keystates such as when both left and right
  // control keys are pressed.  Make sure that location information was retained
  // when the vkey was passed into this method.
  DCHECK_NE(vk, static_cast<DWORD>(VK_CONTROL));
  DCHECK_NE(vk, static_cast<DWORD>(VK_MENU));

  if (!IsModifierKey(vk))
    return false;

  // Windows synthesizes an additional key event when AltGr is pressed.  The
  // event has the left control scan code in the low word and a 0x02 flag set in
  // the high word.  The VK_RMENU event will be sent once this key is processed.
  bool is_altgr_sequence = false;
  if (scan_code == kSynthesizedControlScanCodeForAltGr) {
    is_altgr_sequence = true;
    scan_code = KeycodeConverter::DomCodeToNativeKeycode(DomCode::CONTROL_LEFT);
  }

  // If the caller has only requested that Alt be captured, then we don't want
  // to bail early on the injected control event.
  DomCode dom_code = DomCode::NONE;
  if (is_altgr_sequence)
    dom_code = DomCode::ALT_RIGHT;
  else
    dom_code = KeycodeConverter::NativeKeycodeToDomCode(scan_code);

  if (!ShouldCaptureKeyEvent(dom_code))
    return false;

  if (is_altgr_sequence)
    altgr_sequence_count_++;
  else if (vk != VK_RMENU)
    altgr_sequence_count_ = 0;

  // The Windows key is always located, the other modifiers are not.
  DWORD non_located_vk =
      IsWindowsKey(vk)
          ? vk
          : LocatedToNonLocatedKeyboardCode(static_cast<KeyboardCode>(vk));

  bool is_repeat = false;
  MSG msg = {nullptr, w_param, non_located_vk, GetLParamFromScanCode(scan_code),
             time_stamp};
  EventType event_type = EventTypeFromMSG(msg);
  if (event_type == ET_KEY_PRESSED) {
    UpdateModifierState(vk, /*key_down=*/true);
    // We use the non-located vkey to determine whether a key event is a repeat
    // or not.  The exception is for AltGr which has a two key sequence which
    // alternates.
    is_repeat = (last_key_down_ == non_located_vk) || altgr_sequence_count_ > 1;
    last_key_down_ = non_located_vk;
  } else {
    DCHECK_EQ(event_type, ET_KEY_RELEASED);
    UpdateModifierState(vk, /*key_down=*/false);
    altgr_sequence_count_ = 0;
    last_key_down_ = 0;
  }

  std::unique_ptr<KeyEvent> key_event =
      std::make_unique<KeyEvent>(KeyEventFromMSG(msg));
  if (is_repeat)
    key_event->set_flags(key_event->flags() | EF_IS_REPEAT);
  ForwardCapturedKeyEvent(key_event.get());

  return true;
}

void ModifierKeyboardHookWinImpl::UpdateModifierState(DWORD vk,
                                                      bool is_key_down) {
  BYTE keyboard_state[kKeyboardStateArraySize] = {0};
  if (!GetKeyboardState(keyboard_state)) {
    DPLOG(ERROR) << "GetKeyboardState() failed: ";
    return;
  }

  // Update the located virtual key first.
  keyboard_state[vk] = is_key_down ? kKeyDown : kKeyUp;

  // Now update the non-located virtual key.
  keyboard_state[VK_CONTROL] = (keyboard_state[VK_LCONTROL] == kKeyDown ||
                                keyboard_state[VK_RCONTROL] == kKeyDown)
                                   ? kKeyDown
                                   : kKeyUp;
  keyboard_state[VK_MENU] = (keyboard_state[VK_LMENU] == kKeyDown ||
                             keyboard_state[VK_RMENU] == kKeyDown)
                                ? kKeyDown
                                : kKeyUp;

  if (!SetKeyboardState(keyboard_state))
    PLOG(ERROR) << "SetKeyboardState() failed: ";
}

// static
LRESULT CALLBACK ModifierKeyboardHookWinImpl::ProcessKeyEvent(int code,
                                                              WPARAM w_param,
                                                              LPARAM l_param) {
  return KeyboardHookWinBase::ProcessKeyEvent(instance_, code, w_param,
                                              l_param);
}

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::CreateModifierKeyboardHook(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback) {
  std::unique_ptr<ModifierKeyboardHookWinImpl> keyboard_hook =
      std::make_unique<ModifierKeyboardHookWinImpl>(
          std::move(dom_codes), std::move(callback),
          /*enable_hook_registration=*/true);

  if (!keyboard_hook->Register())
    return nullptr;

  return keyboard_hook;
}

std::unique_ptr<KeyboardHookWinBase>
KeyboardHookWinBase::CreateModifierKeyboardHookForTesting(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback) {
  return std::make_unique<ModifierKeyboardHookWinImpl>(
      std::move(dom_codes), std::move(callback),
      /*enable_hook_registration=*/false);
}

}  // namespace ui
