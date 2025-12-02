# netspeed_graph

A small **Linux CLI tool in C** that shows a simple **ASCII bar graph** of your network speed (in Mbps) over 10 seconds.

It:

- Reads network statistics from `/proc/net/dev`
- Monitors a specific network interface (e.g. `wlan0`, `eth0`, `enp3s0`)
- Samples every **1 second** for **10 seconds**
- Prints a text-based bar for each sample (`#` ≈ 1 Mbps)
- Calculates and prints the **average speed** at the end

---

## 🧩 Requirements

- Linux system with `/proc/net/dev` (most distros have this)
- `gcc` or `g++` (any C/C++ compiler)
- A terminal (VS Code built-in terminal works fine)

---

## ⚙️ Building and Running

Save your code as:
netspeed_graph.c
Compile:
g++ netspeed_graph.c -o netspeed_graph
# or
gcc netspeed_graph.c -o netspeed_graph
Run:
./netspeed_graph
You’ll be prompted to enter a network interface name, for example:
wlan0, wlp3s0 (Wi-Fi)
eth0, enp3s0 (Ethernet)

🔍 Code Walkthrough (Line by Line Sections)
Below is an explanation of each important part of the code.

1. File Header and Includes
c
Copy code
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
The comments explain what the program does and how to compile/run it.

#include <stdio.h> – for printf, scanf, FILE, etc.

#include <stdlib.h> – for general utilities (exit, etc.).

#include <string.h> – for string functions like strstr.

#include <unistd.h> – for sleep().

2. Configuration Macros
c
Copy code
#define TOTAL_TIME 10       // total duration in seconds
#define SAMPLE_INTERVAL 1   // sample every 1 second
#define MAX_BAR_WIDTH 50    // characters in graph bar
TOTAL_TIME – total measurement time (10 seconds).

SAMPLE_INTERVAL – how often to sample (1 second).

MAX_BAR_WIDTH – maximum length of the bar graph (50 # characters).

You can change these values to monitor for longer or shorter periods, or adjust how wide the bars are.

3. Reading Network Stats from /proc/net/dev
c
Copy code
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
What this function does:

Opens /proc/net/dev, which contains per-interface network statistics.

Initializes *rx and *tx to 0.

Reads each line of /proc/net/dev:

Uses strstr(line, iface) to find the requested interface name in the line.

The pos == line + strspn(line, " ") check ensures the interface name appears at the start of the line (after leading spaces), to avoid partial matches.

If the line matches:

sscanf parses the line into:

rbytes (received bytes), tbytes (transmitted bytes), and other stats.

Stores rbytes into *rx and tbytes into *tx.

Closes the file and returns 0 (success).

If no matching interface is found, or if opening the file fails, the function returns -1.

4. Function to Draw a Bar
c
Copy code
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
What this does:

Converts a speed value in Mbps into a number of characters.

width ≈ mbps (e.g., 12.5 Mbps → 12 # characters).

Caps the width at MAX_BAR_WIDTH so very high speeds don’t overflow.

Prints that many # characters, then a newline.

This is a simple ASCII “graph” representation of the speed.

You can modify the scaling (e.g., int width = (int)(mbps / 2); for 1 # per 2 Mbps).

5. main() – Program Flow
c
Copy code
int main() {
    char iface[32];
    printf("Enter network interface (e.g. eth0, wlan0, enp3s0): ");
    if (scanf("%31s", iface) != 1) {
        printf("Invalid input.\n");
        return 1;
    }
Declares a buffer iface to store the interface name.

Prompts the user to enter an interface.

Reads the interface name with scanf.

If input fails, exits with an error.

6. Initial Snapshot of Bytes
c
Copy code
    unsigned long long prev_rx, prev_tx, cur_rx, cur_tx;

    if (read_bytes(iface, &prev_rx, &prev_tx) != 0) {
        printf("Interface '%s' not found or error reading /proc/net/dev.\n", iface);
        return 1;
    }
Declares variables for previous and current RX/TX bytes.

Calls read_bytes once to get the starting byte counts (prev_rx and prev_tx).

If that fails, prints an error message and exits.

This “initial snapshot” is used as a baseline to calculate how much data flows each second.

7. Setup for Sampling
c
Copy code
    int samples = TOTAL_TIME / SAMPLE_INTERVAL;
    double speeds[samples];
    double sum_mbps = 0.0;

    printf("\nMeasuring for %d seconds...\n", TOTAL_TIME);
    printf("Each '#' is ~1 Mbps (capped at %d chars)\n\n", MAX_BAR_WIDTH);
samples – how many data points will be collected (10 seconds / 1 second = 10 samples).

speeds[] – array to store each second’s Mbps value.

sum_mbps – used later to calculate the average speed.

Prints a short message explaining what will happen and how the graph is scaled.

8. Sampling Loop
c
Copy code
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
Each iteration does:

sleep(SAMPLE_INTERVAL);
→ Waits 1 second before taking a new sample.

Calls read_bytes again to get cur_rx and cur_tx.

Computes the difference since the last sample:

c
Copy code
rxdiff = cur_rx - prev_rx;
txdiff = cur_tx - prev_tx;
(with checks to avoid negative values if counters wrap around).

Updates prev_rx / prev_tx for the next loop.

Converts bytes to Mbps:

c
Copy code
total_bits = (rxdiff + txdiff) * 8
mbps = total_bits / (SAMPLE_INTERVAL * 1,000,000)
Stores the speed in speeds[i] and adds it to sum_mbps.

Prints a line like:

text
Copy code
t =  1s |   5.23 Mbps | #####
and uses draw_bar(mbps) to draw the ASCII bar.

So for each second you get a timestamp, the measured speed, and a visual bar.

9. Average Speed and Program End
c
Copy code
    double avg_mbps = sum_mbps / samples;
    printf("\nAverage speed over %d seconds: %.2f Mbps\n", TOTAL_TIME, avg_mbps);

    return 0;
}
After the loop, calculates the average Mbps over all samples.

Prints the average.

Returns 0 to indicate success.
