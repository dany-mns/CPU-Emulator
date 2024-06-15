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

    void write_byte(Byte value, uint32_t address, uint32_t &cycles)
    {
        assert(cycles >= 1);
        data[address] = value;
        decrement_cycles(cycles, 1);
    }
};

struct CPU
{
    Word PC;
    Byte SP;

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
    static constexpr Byte INS_RTS = 0x60;
    static constexpr Byte INS_LDA_ABS = 0xAD;
    static constexpr Byte INS_STA_ZERO_PAGE = 0x85;
    static constexpr Byte INS_STA_ABS = 0x8D;

    void reset(Memory &memory)
    {
        PC = 0xFFFC;
        SP = 0xFF;
        decimal_flag = 0;
        A = X = Y = 0;
        carry_flag = zero_flag = interrupt_disable_flag = decimal_flag = break_flag = overflow_flag = negative_flag = 0;
        memory.init();
    }

    Word SP_address() const {
        return 0x0100 | SP;
    }

    void push_pc_to_stack(uint32_t& cycles, Memory& memory) {
        std::cout << "Saving PC register with value " << std::hex << PC - 1 << " on stack at address " << std::hex << SP_address();
        memory.write_byte((PC -1) >> 8, SP_address(), cycles);
        SP--;
        memory.write_byte((PC - 1) & 0xFF, SP, cycles);
        SP--;
    }

    Byte fetch_byte(uint32_t &cycles, Memory &memory)
    {
        decrement_cycles(cycles, 1);
        return memory[PC++];
    }

    Word fetch_word(uint32_t &cycles, Memory &memory)
    {
        Byte first_byte = memory.data[PC++];
        Byte second_byte = memory.data[PC++];
        decrement_cycles(cycles, 2);

        // little-endian -> second_byte "+" first_byte
        return (second_byte << 8) | first_byte;
    }

    Byte read_byte_from_memory(uint32_t &cycles, Memory &memory, Word address)
    {
        assert(address < MAX_MEMORY);
        decrement_cycles(cycles, 1);
        Byte byte_value = memory.data[address];
        std::cout << "Read BYTE value " << std::hex << byte_value << " from memory address: " << std::hex << address << std::endl;
        return byte_value;
    }

    Byte read_word_from_memory(uint32_t &cycles, Memory &memory, Word address)
    {
        assert(address < MAX_MEMORY);
        Byte first_byte = read_byte_from_memory(cycles, memory, address);
        Byte second_byte = read_byte_from_memory(cycles, memory, address - 1);
        Word word_value = (first_byte << 8) | second_byte;
        std::cout << "Read WORD value " << std::hex << word_value << " from memory address: " << std::hex << address << std::endl;
        return word_value;
    }

    Byte read_byte_from_stack(uint32_t& cycles, Memory& memory) {
        std::cout << "Reading 1 byte from stack starting from address " << std::hex << SP_address() + 1 << std::endl;
        SP++;
        return read_byte_from_memory(cycles, memory, SP_address() + 1);
    }

    Word read_word_from_stack(uint32_t cycles, Memory& memory) {
        Byte s_byte = read_byte_from_stack(cycles, memory);
        Byte f_byte = read_byte_from_stack(cycles, memory);
        std::cout << "First byte from stack is " << std::hex << f_byte << " second byte from stack is " << std::hex << s_byte;
        return (f_byte << 8) | s_byte;
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
                A = read_byte_from_memory(cycles, memory, zero_page_address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;

            case INS_LDA_ZPX:
            {
                std::cout << "LDA ZERO PAGE X" << std::endl;
                Byte zero_page_address = fetch_byte(cycles, memory);
                Byte new_address = zero_page_address + X;
                A = read_byte_from_memory(cycles, memory, new_address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;

            case INS_JSR:
            {
                std::cout << "JSR: Load new address into PC" << std::endl;
                Word subroutine_addr = fetch_word(cycles, memory);
                push_pc_to_stack(cycles, memory);
                std::cout << "Override PC old value " << std::hex << PC << " with new value " << subroutine_addr << std::endl;
                PC = subroutine_addr;
                decrement_cycles(cycles, 1);
            }
            break;

            case INS_RTS:
            {
                std::cout << "Returning from a subroutine using RTS instruction" << std::endl;
                std::cout << "Reading program counter register from stack" << std::endl;
                Word PC_from_stack = read_word_from_stack(cycles, memory);
                Word return_address = read_word_from_memory(cycles, memory, PC_from_stack);
                std::cout << "Override old value " << std::hex << PC << " of PC register with new value " << std::hex << return_address << std::endl;
                PC = return_address;
            } 
            break;

            case INS_LDA_ABS:
            {
                std::cout << "LDA Absolute" << std::endl;
                Word address = fetch_word(cycles, memory);
                A = read_byte_from_memory(cycles, memory, address);
                lda_set_status();
                std::cout << "Assigned value " << (int)A << " to register A" << std::endl;
            }
            break;

            case INS_STA_ZERO_PAGE:
            {
                std::cout << "STA Zero Page" << std::endl;
                Byte address = fetch_byte(cycles, memory);
                memory.write_byte(A, address, cycles);
                std::cout << "Value " << (int)A << " was written at memory location: " << std::hex << address << std::endl;
            }
            break;

            case INS_STA_ABS:
            {
                std::cout <<"STA ABSOLUTE" << std::endl;
                Word address = fetch_word(cycles, memory);
                memory.write_byte(A, address, cycles);
                std::cout << "Value " << (int)A << " was written at memory location: " << std::hex << address << std::endl;
            }
            break;

            default:
                std::cout << "Unknown instruction: " << instruction << std::endl;
                break;
            }
        }
    }
};

void test_sta_absolute() {
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_STA_ABS;
    memory.data[0xFFFD] = 0x01;
    memory.data[0xFFFE] = 0x02;
    memory.data[0x0201] = 0x68;

    cpu.A = 0x69;

    cpu.execute(4, memory);
    assert(memory.data[0x0201] == 0x69);
}

void test_ins_jsr()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_JSR;
    memory.data[0xFFFD] = 0x42;
    memory.data[0xFFFE] = 0x42;
    memory.data[0x4242] = CPU::INS_LDA_IM;
    memory.data[0x4243] = 0x69;
    cpu.execute(8, memory);
    assert(cpu.A == 0x69);
}

void test_sta_zero_page()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_STA_ZERO_PAGE;
    memory.data[0xFFFD] = 0x01;

    cpu.A = 0x69;
    cpu.execute(3, memory);

    assert(memory.data[0x1] == 0x69);
}

void test_ins_lda_abs()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_LDA_ABS;
    memory.data[0xFFFD] = 0x80;
    memory.data[0xFFFE] = 0x42;

    memory.data[0x4280] = 0x69;

    cpu.execute(4, memory);
    assert(cpu.A == 0x69);
}

int main()
{
    std::cout << "======== START EMULATING THE CPU ========" << std::endl;
    // test_ins_jsr();
    // test_ins_lda_abs();
    // test_sta_zero_page();
    test_sta_absolute();
    return 0;
}