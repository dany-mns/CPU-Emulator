#include <stdio.h>
#include <cassert>
#include <iostream>
#include <cstdint>

#define MAX_MEMORY 64 * 1024 // 64 Kb

using Byte = unsigned char;
using Word = unsigned short;

void decrement_cycles(uint32_t &cycles, uint32_t dec_value)
{
    assert(cycles >= dec_value);
    cycles -= dec_value;
}

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

    void write_word(Word value, uint32_t address, uint32_t &cycles)
    {
        assert(cycles >= 2);
        data[address] = value & 0xFF;
        data[address + 1] = (value >> 8) & 0xFF;
        decrement_cycles(cycles, 2);
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
    static constexpr Byte INS_LDA_ZPX = 0xB5;
    static constexpr Byte INS_JSR = 0x20;

    void reset(Memory &memory)
    {
        program_counter = 0xFFFC;
        stack_pointer = 0xFF; // TODO maybe we need another value
        decimal_flag = 0;
        A = X = Y = 0;
        carry_flag = zero_flag = interrupt_disable_flag = decimal_flag = break_flag = overflow_flag = negative_flag = 0;
        memory.init();
    }

    Byte fetch_byte(uint32_t &cycles, Memory &memory)
    {
        decrement_cycles(cycles, 1);
        return memory[program_counter++];
    }

    Word fetch_word(uint32_t &cycles, Memory &memory)
    {
        Byte first_byte = memory.data[program_counter++];
        Byte second_byte = memory.data[program_counter++];
        decrement_cycles(cycles, 2);

        // little-endian -> second_byte "+" first_byte
        return (second_byte << 8) | first_byte;
    }

    Byte read_byte(uint32_t &cycles, Memory &memory, Byte address)
    {
        assert(address < MAX_MEMORY);
        decrement_cycles(cycles, 1);
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
            Byte instruction = fetch_byte(cycles, memory);
            switch (instruction)
            {
            case INS_LDA_IM:
            {
                std::cout << "LDA IMD" << std::endl;
                Byte value = fetch_byte(cycles, memory);
                A = value;
                lda_set_status();
                std::cout << "Assigned value " << (int)value << " to reg A" << std::endl;
            }
            break;

            case INS_LDA_ZP:
            {
                std::cout << "LDA ZERO PAGE" << std::endl;
                Byte zero_page_address = fetch_byte(cycles, memory);
                A = read_byte(cycles, memory, zero_page_address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;

            case INS_LDA_ZPX:
            {
                std::cout << "LDA ZERO PAGE X" << std::endl;
                Byte zero_page_address = fetch_byte(cycles, memory);
                Byte new_address = zero_page_address + X;
                A = read_byte(cycles, memory, new_address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;
            case INS_JSR:
            {
                std::cout << "JSR: Load new address into PC" << std::endl;
                Word subroutine_addr = fetch_word(cycles, memory);
                memory.write_word(program_counter - 1, stack_pointer, cycles);
                stack_pointer--;
                std::cout << "Override PC old value " << program_counter << " with new value " << subroutine_addr << std::endl;
                program_counter = subroutine_addr;
                decrement_cycles(cycles, 1);
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

    memory.data[0xFFFC] = CPU::INS_JSR;
    memory.data[0xFFFD] = 0x42;
    memory.data[0xFFFE] = 0x42;
    memory.data[0x4242] = CPU::INS_LDA_IM;
    memory.data[0x4243] = 0x69;
    cpu.execute(8, memory);
    return 0;
}