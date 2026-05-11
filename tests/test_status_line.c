#include "status_line.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void test_record_status_uses_rx_dot(void) {
    char line[128];
    memset(line, 0, sizeof(line));

    format_status_line(line, sizeof(line), "REC", 12.4, false, 0.0, true, false);

    assert(strcmp(line, "REC 00:12.4 RX. TX ") == 0);
}

static void test_play_status_uses_total_and_tx_dot(void) {
    char line[128];
    memset(line, 0, sizeof(line));

    format_status_line(line, sizeof(line), "PLAY", 3.1, true, 10.0, false, true);

    assert(strcmp(line, "PLAY 00:03.1/00:10.0 RX  TX.") == 0);
}

static void test_zero_size_buffer_is_accepted(void) {
    format_status_line(NULL, 0, "REC", 1.0, false, 0.0, false, false);
}

static void test_small_buffer_truncates_without_crashing(void) {
    char line[5];
    memset(line, 'x', sizeof(line));

    format_status_line(line, sizeof(line), "PLAY", 3.1, true, 10.0, false, true);

    assert(line[sizeof(line) - 1] == '\0');
}

int main(void) {
    test_record_status_uses_rx_dot();
    test_play_status_uses_total_and_tx_dot();
    test_zero_size_buffer_is_accepted();
    test_small_buffer_truncates_without_crashing();
    return 0;
}
