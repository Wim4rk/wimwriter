#include "sync.h"
#include "editor.h" // För att komma åt is_wifi_active
#include <stdlib.h>

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
