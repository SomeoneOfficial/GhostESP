#ifndef CLICK_HIGH_SPEED_H
#define CLICK_HIGH_SPEED_H

#include <stdbool.h>
#include <stdint.h>

bool click_high_speed_start(volatile bool *stop_requested);
bool click_high_speed_wait(void);
void click_high_speed_stop(void);

#endif
