#include <stdio.h>

int main() {

    printf("  a b c d e f g h\n");
    for (int row = 8; row > 0; row--) {
        printf("%d", row);
        for (int col = 8; col > 0; col--) {
            printf(" .");
        }
        printf("\n");
    }

    return 0;
}