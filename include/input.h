// Joystick and button input (spec section 10).
// Call inputUpdate() once per loop(); the event getters then report what
// happened during that update. Events are cleared at the start of the next
// inputUpdate(), so read them in the same loop pass.
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

enum Dir : uint8_t { DIR_NONE, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

// Sets up pins and samples the joystick's resting position as its center,
// so leave the stick untouched at power-on.
void inputInit();

void inputUpdate();

// True on the update where the button's debounced press edge happened.
bool inputRevealPressed();  // joystick push
bool inputFlagPressed();    // flag button

// One cursor step: fires at once when the stick leaves the dead zone, then
// auto-repeats (JOY_REPEAT_DELAY_MS, then every JOY_REPEAT_RATE_MS) while
// held. Only the dominant axis counts, so there are no diagonal steps.
Dir inputDir();

// How long both buttons have been held together (0 if not both down).
uint32_t inputBothHeldMs();

// Raw axis readings after swap/invert, and the calibrated center (for tests).
int16_t inputRawX();
int16_t inputRawY();
int16_t inputCenterX();
int16_t inputCenterY();

#endif
