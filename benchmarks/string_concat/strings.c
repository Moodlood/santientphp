#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char* s = (char*)malloc(16);
    strcpy(s, "start");
    size_t cap = 16;
    size_t len = 5;

    for (int i = 0; i < 50000; ++i) {
        if (len + 2 > cap) {
            cap *= 2;
            s = (char*)realloc(s, cap);
        }
        s[len] = 'a';
        s[len + 1] = '\0';
        len++;
    }
    printf("%d\n", 50000);
    free(s);
    return 0;
}

