#include "status_line.h"

#include <stdio.h>

static void append_timestamp(char *buffer, size_t buffer_size, double seconds) {
    int whole_minutes = (int)(seconds / 60.0);
    double remaining_seconds = seconds - (double)(whole_minutes * 60);
    if (remaining_seconds < 0.0) {
        remaining_seconds = 0.0;
    }

    snprintf(buffer, buffer_size, "%02d:%04.1f", whole_minutes, remaining_seconds);
}

void format_status_line(char *buffer,
                        size_t buffer_size,
                        const char *mode,
                        double elapsed_seconds,
                        bool has_total,
                        double total_seconds,
                        bool rx_active,
                        bool tx_active) {
    char elapsed_buffer[32];
    char total_buffer[32];

    append_timestamp(elapsed_buffer, sizeof(elapsed_buffer), elapsed_seconds);
    total_buffer[0] = '\0';
    if (has_total) {
        append_timestamp(total_buffer, sizeof(total_buffer), total_seconds);
        snprintf(buffer,
                 buffer_size,
                 "%s %s/%s RX%c TX%c",
                 mode,
                 elapsed_buffer,
                 total_buffer,
                 rx_active ? '.' : ' ',
                 tx_active ? '.' : ' ');
        return;
    }

    snprintf(buffer,
             buffer_size,
             "%s %s RX%c TX%c",
             mode,
             elapsed_buffer,
             rx_active ? '.' : ' ',
             tx_active ? '.' : ' ');
}
