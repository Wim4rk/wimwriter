#ifndef SYNC_H
#define SYNC_H

#include <stdbool.h>
#include "../firmware/display.h" // Krävs för UDOUBLE

void toggle_wifi(void);
bool get_actual_wifi_status(void);
void sync_to_git(const char* commit_msg, UDOUBLE target_addr);
void pull_from_git(UDOUBLE target_addr);

#endif
