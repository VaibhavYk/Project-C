# Netspeed_dual_ping.c

This C program measures:

- **Download speed** (received bytes → Mbps)  
- **Upload speed** (transmitted bytes → Mbps)  
- **Ping latency** (round-trip time in ms)

It runs on **Linux**, reads data from `/proc/net/dev`, and uses the system `ping` command to measure latency.  
Sampling is done **every 1 second for 10 seconds**, then it prints average values.

---

## 1. File Header & Includes
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
What this part does:
The comments at the top describe:

File name

Purpose of the program

How often samples are taken

How to compile and run the program

The #include lines pull in standard C libraries:

stdio.h → printf, scanf, fopen, fgets, etc.

stdlib.h → exit, popen, system, etc.

string.h → strstr, strspn, strlen for string operations

unistd.h → sleep() for 1-second intervals

These are the basic building blocks for I/O, string handling, and timing.

## 2. Macros (Constants)

    #define TOTAL_TIME 10       // measurement duration in seconds
    #define SAMPLE_INTERVAL 1   // seconds
What this part does:
TOTAL_TIME → for how many seconds the test runs (10 seconds).

SAMPLE_INTERVAL → how often we sample network stats (every 1 second).

Using #define makes it easier to change duration or sampling rate without editing code logic.

## 3. read_bytes – Reading RX/TX from /proc/net/dev
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
What this function does:
Purpose:
Reads the current number of bytes received (rx) and transmitted (tx) for a given network interface (like wlan0 or eth0).

Step-by-step:

fopen("/proc/net/dev", "r");
Opens the special Linux file that contains network stats for all interfaces.

Initialize *rx and *tx to 0 in case interface is not found.

while (fgets(line, sizeof(line), fp))
Reads the file line by line.

strstr(line, iface) and pos == line + strspn(line, " ")

Looks for the interface name in the line.

Ensures it appears at the start of the line (after spaces), so you don’t get partial matches.

Long sscanf(...)
Parses the interface line, which looks like:

wlan0:  bytes packets errs drop fifo frame compressed multicast  bytes packets errs drop fifo colls carrier compressed
It extracts:

rbytes → received bytes

tbytes → transmitted bytes
(The rest are other statistics not used here.)

If successful (n >= 10):

*rx = rbytes;

*tx = tbytes;

Close the file and return 0 (success).

If the function cannot find the interface or parse it, it returns -1.

This function is called repeatedly to track how many bytes have passed through the interface over time.

## 4. measure_ping_ms – Measuring Ping Using ping Command

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
What this function does:
Purpose:
Runs a system ping command and extracts the round-trip time (ms) from its output.

How it works:

Builds a command string:

ping -c 1 -w 1 <host>
-c 1 → send 1 ICMP packet

-w 1 → wait at most 1 second

2>/dev/null → hide error messages

popen(cmd, "r")
Runs the command and opens a pipe for reading its output.

Reads each output line with fgets.

For each line, it searches for "time=", e.g.:

    int
    time=23.5 ms
Uses sscanf(pos, "time=%lf", &ping_ms) to parse the number after time= into ping_ms.

If time= is never found, it returns -1.0 to indicate failure.

This function is used once per second in the main loop, and the ping results are averaged.

## 5. main – Overall Program Flow

    int main() {
    char iface[32];
    printf("Enter network interface (e.g. eth0, wlan0, enp3s0): ");
    if (scanf("%31s", iface) != 1) {
        printf("Invalid input.\n");
        return 1;
    }
What this part does:
Declares a string iface to store the network interface name.

Prompts the user to enter the interface.

Uses scanf to read it (up to 31 characters to avoid buffer overflow).

If input fails, exits with an error.

## 5.1 Initial Setup and First Reading

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
What this part does:
Declares counters to hold previous and current RX/TX values.

Calls read_bytes once to get the initial RX/TX counters.

If that fails, prints an error and exits.

Calculates how many samples will be taken (10 seconds / 1 second = 10 samples).

Initializes totals for download, upload, and ping to compute averages later.

Prints a message so the user knows the test duration and what will be shown.

## 5.2 Main Sampling Loop

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
What this part does:
This is the core logic that runs once per second.

sleep(SAMPLE_INTERVAL);

Waits for 1 second between measurements.

Calls read_bytes to get new RX and TX byte counters.

Calculates the change (difference) in bytes:

    rxdiff = cur_rx - prev_rx;
    txdiff = cur_tx - prev_tx;
That gives the number of bytes received/transmitted during the last 1 second.

Updates prev_rx and prev_tx for the next iteration.

Converts bytes → bits → Mbps:

    download_mbps = (rxdiff * 8) / (seconds * 1,000,000);
    upload_mbps   = (txdiff * 8) / (seconds * 1,000,000);
   Accumulates totals for averaging later:

total_rx_mbps += download_mbps;
total_tx_mbps += upload_mbps;
Calls measure_ping_ms("10.249.66.207");

Pings the given host once.

Returns ping in ms or -1.0 if it failed.

If ping succeeded (ping_ms >= 0.0):

Adds it to total_ping_ms.

Increments ping_count.

Prints download, upload, and ping values.

If ping failed:

Prints Ping: N/A but still shows speeds.

## 5.3 Final Averages and Exit

    printf("\nAverage Download Speed: %.2f Mbps\n", total_rx_mbps / samples);
    printf("Average Upload Speed:   %.2f Mbps\n", total_tx_mbps / samples);

    if (ping_count > 0) {
        printf("Average Ping:           %.2f ms\n", total_ping_ms / ping_count);
    } else {
        printf("Average Ping:           N/A (ping failed)\n");
    }

    return 0;
    }
What this part does:
Computes:

Average download speed = total of all download samples / number of samples

Average upload speed = total of all upload samples / number of samples

Ping average is calculated only if at least one ping succeeded (ping_count > 0).

Prints final summary.

return 0; → program completed successfully.

Summary of Code Sections
Header & Includes → set up environment and libraries

Macros → constants for time configuration

read_bytes → gets raw RX/TX byte counters from /proc/net/dev

measure_ping_ms → runs ping and parses the time= output

main

Reads interface from user

Takes initial snapshot

Loops for TOTAL_TIME seconds

Calculates per-second download/upload speed

Measures ping

Prints values and calculates averages
