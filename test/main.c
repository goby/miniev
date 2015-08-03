#include <stdio.h>
#include "../src/mini_event.h"

int main() {
    event_loop loop;
    create_event_loop(&loop, 10);

    return 0;
}
