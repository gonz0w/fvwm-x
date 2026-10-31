#ifndef BUTTONS_KEYBOARD_H
#define BUTTONS_KEYBOARD_H

void buttons_set_keyboard_navigation(Bool enabled);
void buttons_keyboard_init(void);
void buttons_keyboard_cleanup(void);
Window buttons_keyboard_focus_window(void);
Bool buttons_keyboard_event(XEvent *event);
void buttons_keyboard_status(void);
Bool buttons_is_focused(const button_info *b);
Bool buttons_is_key_pressed(const button_info *b);
void buttons_state_changed(button_info *b);
void buttons_keyboard_return_tray(button_info *b, int direction);
button_info *buttons_group_state_changed(void);

#endif
