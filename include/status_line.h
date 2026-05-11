#ifndef STATUS_LINE_H
#define STATUS_LINE_H

#include <stdbool.h>
#include <stddef.h>

void format_status_line(char *buffer,
                        size_t buffer_size,
                        const char *mode,
                        double elapsed_seconds,
                        bool has_total,
                        double total_seconds,
                        bool rx_active,
                        bool tx_active);

#endif
