#include <stdio.h>
#include <iostream>
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

    Byte operator[](uint32_t address) const
    {
        return data[address];
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

    static constexpr Byte INS_LDA_IM = 0xA9;
    static constexpr Byte INS_LDA_ZP = 0xA5;

    void reset(Memory &memory)
    {
        program_counter = 0xFFFC;
        stack_pointer = 0xFF; // TODO maybe we need another value
        decimal_flag = 0;
        A = X = Y = 0;
        carry_flag = zero_flag = interrupt_disable_flag = decimal_flag = break_flag = overflow_flag = negative_flag = 0;
        memory.init();
    }

    Byte fetch(uint32_t &cycles, Memory &memory)
    {
        cycles--;
        return memory[program_counter++];
    }

    Byte read_byte(uint32_t &cycles, Memory &memory, Byte address)
    {
        cycles--;
        return memory.data[address];
    }

    void lda_set_status()
    {
        zero_flag = A == 0;
        negative_flag = (A & 0b1000000) > 0;
    }

    void execute(uint32_t cycles, Memory &memory)
    {
        while (cycles > 0)
        {
            Byte instruction = fetch(cycles, memory);
            switch (instruction)
            {
            case INS_LDA_IM:
            {
                std::cout << "LDA IMD" << std::endl;
                Byte value = fetch(cycles, memory);
                A = value;
                lda_set_status();
                std::cout << "Assigned value " << (int)value << " to reg A" << std::endl;
            }
            break;

            case INS_LDA_ZP:
            {
                std::cout << "LDA ZERO PAGE" << std::endl;
                Byte zero_page_address = fetch(cycles, memory);
                A = read_byte(cycles, memory, zero_page_address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;
            default:
                std::cout << "Unknown instruction: " << instruction << std::endl;
                break;
            }
        }
    }
};

int main()
{
    std::cout << "======== START EMULATING THE CPU ========" << std::endl;
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = 0xA9;
    memory.data[0xFFFD] = 0x9;
    cpu.execute(2, memory);
    return 0;
}