#include <stdio.h>

int run_matrix(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            for (int k = 0; k < n; ++k) {
                total += (i * k + k * j);
            }
        }
    }
    return total;
}

int main(void) {
    int res = run_matrix(200);
    printf("%d\n", res);
    return 0;
}

