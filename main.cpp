#include <stdio.h>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <string>

#define MAX_MEMORY 64 * 1024 // 64 Kb

using Byte = unsigned char;
using Word = unsigned short;

void decrement_cycles(uint32_t &cycles, uint32_t dec_value)
{
    assert(cycles >= dec_value);
    cycles -= dec_value;
}

std::string to_hex(unsigned short a) {
    std::stringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << a;
    return stream.str();
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
    Byte unused_flag: 1;
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
    static constexpr Byte INS_JMP_ABS = 0x4C;
    static constexpr Byte INS_JMP_INDIRECT = 0x6C;
    static constexpr Byte INS_STACK_TSX = 0xBA;
    static constexpr Byte INS_STACK_TXS = 0x9A;
    static constexpr Byte INS_STACK_PHA = 0x48;
    static constexpr Byte INS_STACK_PHP = 0x08;
    static constexpr Byte INS_STACK_PLA = 0x68;
    static constexpr Byte INS_STACK_PLP = 0x28;

    Byte all_flags() {
        Byte flags = 0;
        flags |= (carry_flag & 0x01) << 0;
        flags |= (zero_flag & 0x01) << 1;
        flags |= (interrupt_disable_flag & 0x01) << 2;
        flags |= (decimal_flag & 0x01) << 3;
        flags |= (break_flag & 0x01) << 4;
        flags |= (unused_flag & 0x01) << 5;
        flags |= (overflow_flag & 0x01) << 6;
        flags |= (negative_flag & 0x01) << 7;
        return flags;        
    }

    void set_flags(Byte flags) {
        carry_flag = (flags >> 0) & 0x01;
        zero_flag = (flags >> 1) & 0x01;
        interrupt_disable_flag = (flags >> 2) & 0x01;
        decimal_flag = (flags >> 3) & 0x01;
        break_flag = (flags >> 4) & 0x01;
        unused_flag = (flags >> 5) & 0x01;
        overflow_flag = (flags >> 6) & 0x01;
        negative_flag = (flags >> 7) & 0x01;
    }

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

    void push_word_to_stack(uint32_t& cycles, Memory& memory, Word value) {
        std::cout << "Saving word value " << to_hex(value) << " on stack at address " << to_hex(SP_address()) << std::endl;
        memory.write_byte((value) >> 8, SP_address(), cycles);
        SP--;
        memory.write_byte((value) & 0xFF, SP_address(), cycles);
        SP--;
    }

    void push_byte_to_stack(uint32_t& cycles, Memory& memory, Byte value) {
        std::cout << "Saving byte value " << to_hex(value) << " on stack at address " << to_hex(SP_address()) << std::endl;
        memory.write_byte(value, SP_address(), cycles);
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
        std::cout << "Read BYTE value " << to_hex(byte_value) << " from memory address: " << to_hex(address) << std::endl;
        return byte_value;
    }

    Word read_word_from_memory(uint32_t &cycles, Memory &memory, Word address)
    {
        assert(address < MAX_MEMORY);
        Byte first_byte = read_byte_from_memory(cycles, memory, address);
        Byte second_byte = read_byte_from_memory(cycles, memory, address + 1);
        Word word_value = (second_byte << 8) | first_byte;
        std::cout << "WORD value " << to_hex(word_value) << " from memory address: " << to_hex(address) << std::endl;
        return word_value;
    }

    Byte read_byte_from_stack(uint32_t& cycles, Memory& memory) {
        Byte byte_value = read_byte_from_memory(cycles, memory, SP_address() + 1);
        std::cout << "Reading 1 byte with value " << to_hex(byte_value) << " from stack starting from address " << to_hex(SP_address() + 1) << std::endl;
        // TODO should I decrease 1 cycle for SP++?
        SP++;
        return byte_value;
    }

    Word read_word_from_stack(uint32_t& cycles, Memory& memory) {
        Byte s_byte = read_byte_from_stack(cycles, memory);
        Byte f_byte = read_byte_from_stack(cycles, memory);
        std::cout << "First byte from stack is " << to_hex(f_byte) << " second byte from stack is " << to_hex(s_byte) << std::endl;
        return (f_byte << 8) | s_byte;
    }

    void A_reg_status()
    {
        zero_flag = A == 0;
        negative_flag = (A & 0b1000000) > 0;
    }

    void execute(uint32_t cycles, Memory &memory)
    {
        bool stop_execution = false;
        while (cycles > 0 && !stop_execution)
        {
            Byte instruction = fetch_byte(cycles, memory);
            switch (instruction)
            {
            case INS_LDA_IM:
            {
                std::cout << "LDA IMD" << std::endl;
                Byte value = fetch_byte(cycles, memory);
                A = value;
                A_reg_status();
                std::cout << "Assigned value " << to_hex(value) << " to reg A" << std::endl;
            }
            break;

            case INS_LDA_ZP:
            {
                std::cout << "LDA ZERO PAGE" << std::endl;
                Byte zero_page_address = fetch_byte(cycles, memory);
                A = read_byte_from_memory(cycles, memory, zero_page_address);
                A_reg_status();
                std::cout << "Assigned value " << std::hex << (int)A << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;

            case INS_LDA_ZPX:
            {
                std::cout << "LDA ZERO PAGE X" << std::endl;
                Byte zero_page_address = fetch_byte(cycles, memory);
                Byte new_address = zero_page_address + X;
                A = read_byte_from_memory(cycles, memory, new_address);
                A_reg_status();
                std::cout << "Assigned value " << to_hex(A) << " to reg A based on Zero Page instruction" << std::endl;
            }
            break;

            case INS_JSR:
            {
                std::cout << "JSR: Load new address into PC" << std::endl;
                Word subroutine_addr = fetch_word(cycles, memory);
                // TODO why we push PC - 1
                push_word_to_stack(cycles, memory, PC - 1);
                std::cout << "Override PC old value " << to_hex(PC) << " with new value " << to_hex(subroutine_addr) << std::endl;
                PC = subroutine_addr;
                decrement_cycles(cycles, 1);
            }
            break;

            case INS_RTS:
            {
                std::cout << "Returning from a subroutine using RTS instruction" << std::endl;
                std::cout << "Reading program counter register from stack" << std::endl;
                // We increment 1 because PC was added with value PC - 1 by JSR instruction
                Word return_address = read_word_from_stack(cycles, memory) + 1;
                std::cout << "Override old value " << to_hex(PC) << " of PC register with new value " << to_hex(return_address) << std::endl;
                PC = return_address;
            } 
            break;

            case INS_LDA_ABS:
            {
                std::cout << "LDA Absolute" << std::endl;
                Word address = fetch_word(cycles, memory);
                A = read_byte_from_memory(cycles, memory, address);
                A_reg_status();
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

            case INS_JMP_ABS:
            {
                Word address = fetch_word(cycles, memory);
                std::cout << "Jumping using JMP Absolute from " << to_hex(PC) << " to address " << to_hex(address) << std::endl;
                PC = address;
            }
            break;

            case INS_JMP_INDIRECT:
            {
                Word address = fetch_word(cycles, memory);
                Word new_PC = read_word_from_memory(cycles, memory, address);
                std::cout << "In the JMP instruction found address " << to_hex(address) << ", take PC address from that memory location" << std::endl; 
                std::cout << "Jumping using JMP INDIRECT from " << to_hex(PC) << " to address " << to_hex(new_PC) << std::endl;
                PC = new_PC;
            }
            break;

            case INS_STACK_TSX:
            {
                // TODO should be 2 cycles. X = SP should consume 1 cycle?
                std::cout << "Copies the current contents of the stack register " << SP << " into the X register" << std::endl;
                std::cout << "Setting CPU flags" << std::endl;
                X = SP;
                zero_flag = X == 0;
                negative_flag = (X & 0b1000000) == 1;
            }
            break;

            case INS_STACK_TXS:
            {
                std::cout << "Copy X with value " << to_hex(X) << " in SP" << std::endl;
                SP = X;
                cycles--;
            }
            break;

            case INS_STACK_PHA:
            {
                std::cout << "Push val reg A " << to_hex(A) << " to stack" << std::endl;
                // TODO why 3 cycles?
                push_byte_to_stack(cycles, memory, A);                
            }
            break;

            case INS_STACK_PHP:
            {
                std::cout << "Push val CPU flags  " << to_hex(all_flags()) << " to stack" << std::endl;
                push_byte_to_stack(cycles, memory, all_flags());
            }
            break;

            case INS_STACK_PLA:
            {
                // TODO why 4 cycles? and not 2, 1 for fetch instruction and 1 for writting into memory
                Byte v = read_byte_from_stack(cycles, memory);
                std::cout << "Pull accumulator from stack from address " << to_hex(SP_address()) << " with value " << to_hex(v) << std::endl;
                A = v;
                A_reg_status();
            }
            break;

            case INS_STACK_PLP:
            {
                // TODO why 4 cycles? and not 2, 1 for fetch instruction and 1 for writting into memory
                Byte v = read_byte_from_stack(cycles, memory);
                std::cout << "Pull accumulator from stack from address " << to_hex(SP_address()) << " with value " << to_hex(v) << std::endl;
                set_flags(v);
            }
            break;

            default:
                std::cout << "Unknown instruction: " << to_hex(instruction) << " -> STOP execution" << std::endl;
                stop_execution = true;
                break;
            }
        }
    }
};

void test_pla() {
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_STACK_PLA;
    cpu.A = 0x69;

    memory.data[cpu.SP_address()] = 0x27;
    cpu.SP--;

    cpu.execute(4, memory);
    assert(cpu.A == 0x27);
}

void test_pha() {
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_STACK_PHA;
    cpu.A = 0x69;
    cpu.execute(3, memory);

    assert(memory[cpu.SP_address() + 1] == 0x69);
}

void test_jmp_indirect() {
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_JMP_INDIRECT;
    memory.data[0xFFFD] = 0x20;
    memory.data[0xFFFE] = 0x01;
    memory.data[0x0120] = 0xFC;
    memory.data[0x0121] = 0xBA;

    cpu.execute(5, memory);
    assert(cpu.PC == 0xBAFC);
}

void test_jmp_absolute() {
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_JMP_ABS;
    memory.data[0xFFFD] = 0x01;
    memory.data[0xFFFE] = 0x02;

    cpu.execute(3, memory);

    assert(cpu.PC == 0x0201);
}

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

void test_ins_rts()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_JSR;
    memory.data[0xFFFD] = 0x42;
    memory.data[0xFFFE] = 0x42;
    memory.data[0x4242] = CPU::INS_LDA_IM;
    memory.data[0x4243] = 0x69;
    memory.data[0x4244] = CPU::INS_RTS;
    cpu.execute(11, memory); // TODO how many cycles we really need (14? do I miss to decrement somewhere?)

    assert(cpu.PC == 0xFFFF);
    assert(cpu.A == 0x69);
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
    std::cout << "======== START EMULATING THE 6502 CPU ========" << std::endl;
    // test_ins_jsr();
    // test_ins_lda_abs();
    // test_sta_zero_page();
    // test_sta_absolute();
    // test_ins_rts();
    // test_jmp_absolute();
    // test_jmp_indirect();
    // test_pha();
    test_pla();
    return 0;
}