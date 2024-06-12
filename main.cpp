#include <stdio.h>

struct CPU {
    using Byte = unsigned char;
    using Word = unsigned short;

    Word program_counter;
    Byte stack_pointer;

    Byte A, X, Y;

    // status flags
    Byte C: 1;
    Byte Z: 1;
    Byte I: 1;
    Byte D: 1;
    Byte B: 1;
    Byte V: 1;
    Byte N: 1;
    Byte C: 1;
};

int main() {
    printf("hello\n");
    return 0;
}