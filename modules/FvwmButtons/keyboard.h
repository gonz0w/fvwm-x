#ifndef BUTTONS_KEYBOARD_H
#define BUTTONS_KEYBOARD_H

void buttons_set_keyboard_navigation(Bool enabled);
Bool buttons_keyboard_event(XEvent *event);
Bool buttons_is_focused(const button_info *b);
Bool buttons_is_key_pressed(const button_info *b);
void buttons_state_changed(button_info *b);
button_info *buttons_group_state_changed(void);

#endif
