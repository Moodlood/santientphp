#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int value;
} Node;

int main(void) {
    int sum = 0;
    for (int i = 0; i < 1000000; ++i) {
        Node* b = (Node*)malloc(sizeof(Node));
        b->value = i;
        sum += b->value;
        free(b);
    }
    printf("%d\n", sum);
    return 0;
}

