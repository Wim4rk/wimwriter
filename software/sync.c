#include "sync.h"
#include "editor.h" // För att komma åt is_wifi_active
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

void toggle_wifi(void) {
    // Växla flaggan
    is_wifi_active = !is_wifi_active;

    // Kör systemanrop för att styra nätverkskretsen
    if (is_wifi_active) {
        system("rfkill unblock wifi");
    } else {
        system("rfkill block wifi");
    }
}

bool get_actual_wifi_status(void) {
    // Sökvägen kan variera något beroende på OS,
    // men rfkill0 brukar motsvara wlan0.
    int fd = open("/sys/class/rfkill/rfkill0/state", O_RDONLY);
    if (fd < 0) return false;

    char state_char;
    bool is_active = false;
    if (read(fd, &state_char, 1) == 1) {
        // '1' betyder unblocked (aktiv), '0' och '2' betyder blocked
        is_active = (state_char == '1');
    }
    close(fd);
    return is_active;
}
