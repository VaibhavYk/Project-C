// netspeed_graph.c
// Live ASCII graph of network speed on Linux using /proc/net/dev
// Samples every 1 second for 10 seconds and prints a bar graph.
//
// Compile: g++ netspeed_graph.c -o netspeed_graph
// Run:     ./netspeed_graph

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL_TIME 10       // total duration in seconds
#define SAMPLE_INTERVAL 1   // sample every 1 second
#define MAX_BAR_WIDTH 50    // characters in graph bar

// Read network bytes for a given interface
int read_bytes(const char *iface, unsigned long long *rx, unsigned long long *tx) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        perror("Error opening /proc/net/dev");
        return -1;
    }

    char line[256];
    *rx = *tx = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Look for the interface name followed by ":" to avoid partial matches
        char *pos = strstr(line, iface);
        if (pos && pos == line + strspn(line, " ")) { // interface at start (ignoring spaces)
            // Format in /proc/net/dev:
            // Inter-|   Receive                                                |  Transmit
            //  face |bytes packets errs drop fifo frame compressed multicast|bytes packets errs drop fifo colls carrier compressed
            char iface_name[32];
            unsigned long long rbytes, rpackets, rerrs, rdrop, rfifo, rframe, rcompressed, rmulticast;
            unsigned long long tbytes, tpackets, terrs, tdrop, tfifo, tcolls, tcarrier, tcompressed;

            int n = sscanf(line,
                           " %[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           iface_name,
                           &rbytes, &rpackets, &rerrs, &rdrop, &rfifo, &rframe, &rcompressed, &rmulticast,
                           &tbytes, &tpackets, &terrs, &tdrop, &tfifo, &tcolls, &tcarrier, &tcompressed);

            if (n >= 10) { // we at least got rbytes and tbytes
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

// Draw a horizontal bar for a given Mbps value
void draw_bar(double mbps) {
    // Simple scaling: 1 Mbps -> 1 char (you can tweak)
    int width = (int)mbps;
    if (width > MAX_BAR_WIDTH) width = MAX_BAR_WIDTH;
    if (width < 0) width = 0;

    for (int i = 0; i < width; i++) {
        putchar('#');
    }
    putchar('\n');
}

int main() {
    char iface[32];
    printf("Enter network interface (e.g. eth0, wlan0, enp3s0): ");
    if (scanf("%31s", iface) != 1) {
        printf("Invalid input.\n");
        return 1;
    }

    unsigned long long prev_rx, prev_tx, cur_rx, cur_tx;

    if (read_bytes(iface, &prev_rx, &prev_tx) != 0) {
        printf("Interface '%s' not found or error reading /proc/net/dev.\n", iface);
        return 1;
    }

    int samples = TOTAL_TIME / SAMPLE_INTERVAL;
    double speeds[samples];
    double sum_mbps = 0.0;

    printf("\nMeasuring for %d seconds...\n", TOTAL_TIME);
    printf("Each '#' is ~1 Mbps (capped at %d chars)\n\n", MAX_BAR_WIDTH);

    for (int i = 0; i < samples; i++) {
        sleep(SAMPLE_INTERVAL);

        if (read_bytes(iface, &cur_rx, &cur_tx) != 0) {
            printf("Error: failed to read /proc/net/dev during sampling.\n");
            return 1;
        }

        unsigned long long rxdiff = (cur_rx >= prev_rx) ? (cur_rx - prev_rx) : 0;
        unsigned long long txdiff = (cur_tx >= prev_tx) ? (cur_tx - prev_tx) : 0;

        prev_rx = cur_rx;
        prev_tx = cur_tx;

        double interval = (double)SAMPLE_INTERVAL;
        double total_bits = (double)(rxdiff + txdiff) * 8.0;
        double mbps = total_bits / (interval * 1000000.0);

        speeds[i] = mbps;
        sum_mbps += mbps;

        printf("t = %2ds | %7.2f Mbps | ", (i + 1) * SAMPLE_INTERVAL, mbps);
        draw_bar(mbps);
    }

    double avg_mbps = sum_mbps / samples;
    printf("\nAverage speed over %d seconds: %.2f Mbps\n", TOTAL_TIME, avg_mbps);

    return 0;
}
