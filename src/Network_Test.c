#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL_TIME 10       // measurement duration in seconds
#define SAMPLE_INTERVAL 1   // seconds

// ---------- Read RX/TX bytes for a given interface ----------

int read_bytes(const char *iface, unsigned long long *rx, unsigned long long *tx) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        perror("Error opening /proc/net/dev");
        return -1;
    }

    char line[256];
    *rx = *tx = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *pos = strstr(line, iface);
        if (pos && pos == line + strspn(line, " ")) {

            char iface_name[32];
            unsigned long long rbytes, rpackets, rerrs, rdrop, rfifo, rframe, rcompressed, rmulticast;
            unsigned long long tbytes, tpackets, terrs, tdrop, tfifo, tcolls, tcarrier, tcompressed;

            int n = sscanf(line,
                           " %[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           iface_name,
                           &rbytes, &rpackets, &rerrs, &rdrop, &rfifo, &rframe, &rcompressed, &rmulticast,
                           &tbytes, &tpackets, &terrs, &tdrop, &tfifo, &tcolls, &tcarrier, &tcompressed);

            if (n >= 10) {
                *rx = rbytes;
                *tx = tbytes;
                fclose(fp);
                return 0;
            }
        }
    }

    fclose(fp);
    return -1;
}

// ---------- Measure ping using "ping -c 1" and parse time=...ms ----------

double measure_ping_ms(const char *host) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -w 1 %s 2>/dev/null", host);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        // Could not run ping
        return -1.0;
    }

    char line[256];
    double ping_ms = -1.0;

    while (fgets(line, sizeof(line), fp)) {
        // Look for "time=" substring
        char *pos = strstr(line, "time=");
        if (pos) {
            // Example: "time=23.5 ms"
            if (sscanf(pos, "time=%lf", &ping_ms) == 1) {
                break;
            }
        }
    }

    pclose(fp);
    return ping_ms; // -1.0 if not found
}

// ---------- main ----------

int main() {
    char iface[32];
    printf("Enter network interface (e.g. eth0, wlan0, enp3s0): ");
    if (scanf("%31s", iface) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    unsigned long long prev_rx, prev_tx, cur_rx, cur_tx;

    if (read_bytes(iface, &prev_rx, &prev_tx) != 0) {
        printf("Error: Interface '%s' not found or cannot read /proc/net/dev.\n", iface);
        return 1;
    }

    int samples = TOTAL_TIME / SAMPLE_INTERVAL;
    double total_rx_mbps = 0.0;
    double total_tx_mbps = 0.0;
    double total_ping_ms = 0.0;
    int ping_count = 0;

    printf("\nMeasuring for %d seconds...\n", TOTAL_TIME);
    printf("Showing Download (Mbps), Upload (Mbps), Ping (ms)\n\n");

    for (int i = 0; i < samples; i++) {
        sleep(SAMPLE_INTERVAL);

        if (read_bytes(iface, &cur_rx, &cur_tx) != 0) {
            printf("Error reading /proc/net/dev.\n");
            return 1;
        }

        unsigned long long rxdiff = (cur_rx >= prev_rx) ? (cur_rx - prev_rx) : 0;
        unsigned long long txdiff = (cur_tx >= prev_tx) ? (cur_tx - prev_tx) : 0;

        prev_rx = cur_rx;
        prev_tx = cur_tx;

        double download_mbps = ((double)rxdiff * 8.0) / (SAMPLE_INTERVAL * 1000000.0);
        double upload_mbps   = ((double)txdiff * 8.0) / (SAMPLE_INTERVAL * 1000000.0);

        total_rx_mbps += download_mbps;
        total_tx_mbps += upload_mbps;

        // Measure ping to Google DNS (or any host you like)
        double ping_ms = measure_ping_ms("10.249.66.207");
        if (ping_ms >= 0.0) {
            total_ping_ms += ping_ms;
            ping_count++;
            printf("Second %2d | Down: %7.2f Mbps | Up: %7.2f Mbps | Ping: %6.2f ms\n",
                   i + 1, download_mbps, upload_mbps, ping_ms);
        } else {
            printf("Second %2d | Down: %7.2f Mbps | Up: %7.2f Mbps | Ping:   N/A\n",
                   i + 1, download_mbps, upload_mbps);
        }
    }

    printf("\nAverage Download Speed: %.2f Mbps\n", total_rx_mbps / samples);
    printf("Average Upload Speed:   %.2f Mbps\n", total_tx_mbps / samples);

    if (ping_count > 0) {
        printf("Average Ping:           %.2f ms\n", total_ping_ms / ping_count);
    } else {
        printf("Average Ping:           N/A (ping failed)\n");
    }

    return 0;
}
