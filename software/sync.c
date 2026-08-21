#include "sync.h"
#include "editor.h" // För att komma åt is_wifi_active
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

void sync_to_git(const char* commit_msg, UDOUBLE target_addr) {
    render_status_bar("Synkroniserar... Ansluter till nätverk.", target_addr);

    // 1. Slå på WiFi om det är avstängt
    if (!is_wifi_active) {
        toggle_wifi();
    }

    // 2. Vänta på att WiFi-gränssnittet ska vakna
    int retries = 10;
    while (!get_actual_wifi_status() && retries > 0) {
        sleep(1);
        retries--;
    }

    // Ge Tailscale ytterligare ett par sekunder att upprätta tunneln
    sleep(3);

    char command[512];

    // Vi navigerar till rätt mapp innan vi kör git-kommandona
    if (commit_msg == NULL || strlen(commit_msg) == 0) {
        snprintf(command, sizeof(command),
                    "cd ~/Dokument/writer && git add . && "
                    "(git diff --staged --quiet || git commit -m 'Auto-sync') && "
                    "git push origin main");
    } else {
        snprintf(command, sizeof(command),
                    "cd ~/Dokument/writer && git add . && "
                    "(git diff --staged --quiet || git commit -m '%s') && "
                    "git push origin main", commit_msg);
    }

    // 3. Utför push
    int status = system(command);

    // 4. Hantera eventuella konflikter
    if (status != 0) {
        char conflict_cmd[512];
        time_t now = time(NULL);
        snprintf(conflict_cmd, sizeof(conflict_cmd),
                 "cd ~/Dokument/writer && git checkout -b konflikt-%ld && git push -u origin konflikt-%ld",
                 now, now);
        system(conflict_cmd);

        // Återgå till main
        system("cd ~/Dokument/writer && git checkout main");

        render_status_bar("Synk-konflikt: Sparad som ny branch", target_addr);
    } else {
        render_status_bar("Synkronisering slutförd", target_addr);
    }

    // 5. Stäng av WiFi igen för att spara ström
    if (is_wifi_active) {
        toggle_wifi();
    }
}

void pull_from_git(UDOUBLE target_addr) {
    render_status_bar("Hämtar ändringar från git...", target_addr);

    // 1. Slå på WiFi om det är avstängt
    if (!is_wifi_active) {
        toggle_wifi();
    }

    // 2. Vänta in nätverk och Tailscale
    int retries = 10;
    while (!get_actual_wifi_status() && retries > 0) {
        sleep(1);
        retries--;
    }
    sleep(3);

    // 3. Hämta ändringar
    // Vi förutsätter att den lokala arbetskatalogen är ren (inga pågående konflikter vid uppstart).
    system("cd ~/Dokument/writer && git pull origin main");

    // 4. Stäng av WiFi
    if (is_wifi_active) {
        toggle_wifi();
    }
}
