#include "uci.h"
#include "zobrist.h"

int main() {
    init_zobrist();
    uci_loop();
    return 0;
}

