#include "SDL_config.h"

#include "SDL_nacl.h"

extern "C" {
#include "SDL.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
}

#include "SDL_naclevents_c.h"
#include "eventqueue.h"
#include <ppapi/cpp/input_event.h>
#include <ppapi/cpp/point.h>
#include <ppapi/cpp/var.h>

static EventQueue event_queue;

struct SDL_NACL_Event {
  PP_InputEvent_Type type;
  Uint8 button;
  SDL_keysym keysym;
  int32_t x, y;
};

static Uint8 translateButton(int32_t button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return SDL_BUTTON_LEFT;
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return SDL_BUTTON_MIDDLE;
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return SDL_BUTTON_RIGHT;
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
    default:
      return 0;
  }
}

// Translate ASCII code to browser keycode
static uint8_t translateAscii(uint8_t ascii) {
  if ('A' <= ascii && ascii <= 'Z') {
    return ascii;
  } else if ('a' <= ascii && ascii <= 'z') {
    return toupper(ascii);
  } else if ('0' <= ascii && ascii <= '9') {
    return ascii;
  } else if (' ' == ascii || '\r' == ascii || '\t' == ascii ||
             '\x1b' == ascii || '\b' == ascii) {
    return ascii;
  } else {
    switch (ascii) {
      case '!': return '1';
      case '@': return '2';
      case '#': return '3';
      case '$': return '4';
      case '%': return '5';
      case '^': return '6';
      case '&': return '7';
      case '*': return '8';
      case '(': return '9';
      case ')': return '0';
      case ';': case ':': return 186;
      case '=': case '+': return 187;
      case ',': case '<': return 188;
      case '-': case '_': return 189;
      case '.': case '>': return 190;
      case '/': case '?': return 191;
      case '`': case '~': return 192;
      case '[': case '{': return 219;
      case '\\': case '|': return 220;
      case ']': case '}': return 221;
      case '\'': case '"': return 222;
      default:
        break;
    }
  }
  return 0;
}

// Translate browser keycode to SDLKey
static SDLKey translateKey(uint32_t code) {
  if (code >= 'A' && code <= 'Z')
    return (SDLKey)(code - 'A' + SDLK_a);
  if (code >= SDLK_0 && code <= SDLK_9)
    return (SDLKey)code;
  const uint32_t f1_code = 112;
  if (code >= f1_code && code < f1_code + 12)
    return (SDLKey)(code - f1_code + SDLK_F1);
  const uint32_t kp0_code = 96;
  if (code >= kp0_code && code < kp0_code + 10)
    return (SDLKey)(code - kp0_code + SDLK_KP0);
  switch (code) {
    case SDLK_BACKSPACE:
      return SDLK_BACKSPACE;
    case SDLK_TAB:
      return SDLK_TAB;
    case SDLK_RETURN:
      return SDLK_RETURN;
    case SDLK_PAUSE:
      return SDLK_PAUSE;
    case SDLK_ESCAPE:
      return SDLK_ESCAPE;
    case 16:
      return SDLK_LSHIFT;
    case 17:
      return SDLK_LCTRL;
    case 18:
      return SDLK_LALT;
    case 32:
      return SDLK_SPACE;
    case 37:
      return SDLK_LEFT;
    case 38:
      return SDLK_UP;
    case 39:
      return SDLK_RIGHT;
    case 40:
      return SDLK_DOWN;
    case 106:
      return SDLK_KP_MULTIPLY;
    case 107:
      return SDLK_KP_PLUS;
    case 109:
      return SDLK_KP_MINUS;
    case 110:
      return SDLK_KP_PERIOD;
    case 111:
      return SDLK_KP_DIVIDE;
    case 45:
      return SDLK_INSERT;
    case 46:
      return SDLK_DELETE;
    case 36:
      return SDLK_HOME;
    case 35:
      return SDLK_END;
    case 33:
      return SDLK_PAGEUP;
    case 34:
      return SDLK_PAGEDOWN;
    case 189:
      return SDLK_MINUS;
    case 187:
      return SDLK_EQUALS;
    case 219:
      return SDLK_LEFTBRACKET;
    case 221:
      return SDLK_RIGHTBRACKET;
    case 186:
      return SDLK_SEMICOLON;
    case 222:
      return SDLK_QUOTE;
    case 220:
      return SDLK_BACKSLASH;
    case 188:
      return SDLK_COMMA;
    case 190:
      return SDLK_PERIOD;
    case 191:
      return SDLK_SLASH;
    case 192:
      return SDLK_BACKQUOTE;
    default:
      return SDLK_UNKNOWN;
  }
}

void SDL_NACL_PushEvent(const pp::InputEvent& ppevent) {
  static Uint8 last_scancode = 0;
  SDL_keysym keysym;
  struct SDL_NACL_Event* event =
    (struct SDL_NACL_Event*)malloc(sizeof(struct SDL_NACL_Event));
  event->type = ppevent.GetType();

  pp::InputEvent *input_event = const_cast<pp::InputEvent*>(&ppevent);

  if (event->type == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
      event->type == PP_INPUTEVENT_TYPE_MOUSEUP) {
    pp::MouseInputEvent *mouse_event =
      reinterpret_cast<pp::MouseInputEvent*>(input_event);
    event->button = translateButton(mouse_event->GetButton());
  } else if (event->type == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
    pp::MouseInputEvent *mouse_event =
      reinterpret_cast<pp::MouseInputEvent*>(input_event);
    event->x = mouse_event->GetPosition().x();
    event->y = mouse_event->GetPosition().y();
  } else if (event->type == PP_INPUTEVENT_TYPE_KEYDOWN ||
             event->type == PP_INPUTEVENT_TYPE_KEYUP ||
             event->type == PP_INPUTEVENT_TYPE_CHAR) {
    // PPAPI sends us separate events for KEYDOWN and CHAR; the first one
    // contains only the keycode, the second one - only the unicode text.
    // SDL wants both in SDL_PRESSED event :(
    // For now, ignore the keydown event for printable ascii (32-126) as we
    // know we'll get a char event and can set sym directly. For everything
    // else, risk sending an extra SDL_PRESSED with unicode text and zero
    // keycode for scancode / sym.
    // It seems that SDL 1.3 is better in this regard.
    pp::KeyboardInputEvent *keyboard_event =
      reinterpret_cast<pp::KeyboardInputEvent*>(input_event);
    keysym.scancode = keyboard_event->GetKeyCode();
    keysym.unicode = keyboard_event->GetCharacterText().AsString()[0];
    keysym.sym = translateKey(keysym.scancode);
    if (event->type == PP_INPUTEVENT_TYPE_KEYDOWN) {
      last_scancode = keysym.scancode;
      if (keysym.sym >= ' ' &&  keysym.sym <= 126) {
        return;
      }
    } else if (event->type == PP_INPUTEVENT_TYPE_CHAR) {
      if (keysym.sym >= ' ' &&  keysym.sym <= 126) {
        keysym.scancode = translateAscii(keysym.unicode);
        keysym.sym = translateKey(keysym.scancode);
      } else if (last_scancode) {
        keysym.scancode = last_scancode;
        keysym.sym = translateKey(keysym.scancode);
      }
    } else {  // event->type == PP_INPUTEVENT_TYPE_KEYUP
      last_scancode = 0;
    }
    keysym.mod = KMOD_NONE;
    event->keysym = keysym;
  } else {
    return;
  }
  event_queue.PushEvent(event);
}

void NACL_PumpEvents(_THIS) {
  SDL_NACL_Event* event;
  while (event = event_queue.PopEvent()) {
    if (event->type == PP_INPUTEVENT_TYPE_MOUSEDOWN) {
      SDL_PrivateMouseButton(SDL_PRESSED, event->button, 0, 0);
    } else if (event->type == PP_INPUTEVENT_TYPE_MOUSEUP) {
      SDL_PrivateMouseButton(SDL_RELEASED, event->button, 0, 0);
    } else if (event->type == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
      SDL_PrivateMouseMotion(0, 0, event->x, event->y);
    } else if (event->type == PP_INPUTEVENT_TYPE_KEYDOWN || event->type == PP_INPUTEVENT_TYPE_CHAR) {
      SDL_PrivateKeyboard(SDL_PRESSED, &event->keysym);
    } else if (event->type == PP_INPUTEVENT_TYPE_KEYUP) {
      SDL_PrivateKeyboard(SDL_RELEASED, &event->keysym);
    }
    free(event);
  }
}

void NACL_InitOSKeymap(_THIS) {
  /* do nothing. */
}
