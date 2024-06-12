#include <stdio.h>
#include <cstdint>

#define MAX_MEMORY 64 * 1024 // 64 Kb

using Byte = unsigned char;
using Word = unsigned short;

struct Memory
{
    Byte data[MAX_MEMORY];

    void init()
    {
        for (uint32_t i = 0; i < MAX_MEMORY; i++)
        {
            data[i] = 0;
        }
    }
};

struct CPU
{
    Word program_counter;
    Byte stack_pointer;

    Byte A, X, Y;

    // status flags
    Byte carry_flag : 1;
    Byte zero_flag : 1;
    Byte interrupt_disable_flag : 1;
    Byte decimal_flag : 1;
    Byte break_flag : 1;
    Byte overflow_flag : 1;
    Byte negative_flag : 1;

    void reset(Memory &memory)
    {
        program_counter = 0xFFFC;
        stack_pointer = 0x0100;
        decimal_flag = 0;
        A = X = Y = 0;
        carry_flag = zero_flag = interrupt_disable_flag = decimal_flag = break_flag = overflow_flag = negative_flag = 0;
        memory.init();
    }
};

int main()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);
    printf("hello\n");
    return 0;
}