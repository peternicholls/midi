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

int main(void) {
    test_record_status_uses_rx_dot();
    test_play_status_uses_total_and_tx_dot();
    return 0;
}
