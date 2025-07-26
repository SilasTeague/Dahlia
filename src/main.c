#include "uci.h"
#include "zobrist.h"
#include <stdlib.h>
#include <time.h>

int main() {
    srand(time(NULL));
    init_zobrist();
    uci_loop();
    return 0;
}

