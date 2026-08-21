#include "sync.h"
#include "editor.h" // För att komma åt is_wifi_active
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SYNC_DIR "/home/olov/Dokument/writer"

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

    // Kom ihåg om WiFi var avstängt när funktionen anropades
    bool was_wifi_off = !is_wifi_active;

    if (was_wifi_off) {
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
            "cd %s && sudo -H -u olov git add . && "
            "(sudo -H -u olov git diff --staged --quiet || sudo -H -u olov git commit -m 'Auto-sync') && "
            "sudo -H -u olov git push origin main", SYNC_DIR);
    } else {
        snprintf(command, sizeof(command),
                    "cd %s && sudo -H -u olov git add . && "
                    "(sudo -H -u olov git diff --staged --quiet || sudo -H -u olov git commit -m '%s') && "
                    "sudo -H -u olov git push origin main", SYNC_DIR, commit_msg);
    }

    // 3. Utför push
    int status = system(command);

    // 4. Hantera eventuella konflikter
    if (status != 0) {
        char conflict_cmd[512];
        time_t now = time(NULL);

        snprintf(conflict_cmd, sizeof(conflict_cmd),
                    "cd %s && sudo -H -u olov git checkout -b konflikt-%ld && sudo -H -u olov git push -u origin konflikt-%ld",
                    SYNC_DIR, now, now);
        system(conflict_cmd);

        char checkout_cmd[512];
        snprintf(checkout_cmd, sizeof(checkout_cmd), "cd %s && sudo -H -u olov git checkout main", SYNC_DIR);
        system(checkout_cmd);

        render_status_bar("Synk-konflikt: Sparad som ny branch", target_addr);
    } else {
        render_status_bar("Synkronisering slutförd", target_addr);
    }

    // Stäng enbart av nätverket om vi aktiverade det i denna funktion
    if (was_wifi_off && is_wifi_active) {
        toggle_wifi();
    }
}

void pull_from_git(UDOUBLE target_addr) {
    render_status_bar("Hämtar ändringar från git...", target_addr);

    // Kom ihåg om WiFi var avstängt när funktionen anropades
    bool was_wifi_off = !is_wifi_active;

    if (was_wifi_off) {
        toggle_wifi();
    }

    int retries = 10;
    while (!get_actual_wifi_status() && retries > 0) {
        sleep(1);
        retries--;
    }
    sleep(3);

    char pull_cmd[512];
    snprintf(pull_cmd, sizeof(pull_cmd), "cd %s && sudo -H -u olov git pull origin main", SYNC_DIR);
    system(pull_cmd);

    // Stäng enbart av nätverket om vi aktiverade det i denna funktion
    if (was_wifi_off && is_wifi_active) {
        toggle_wifi();
    }
}
