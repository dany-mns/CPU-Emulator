#include <stdio.h>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <string>
#include <bitset>

#define MAX_MEMORY 64 * 1024 // 64 Kb

using Byte = uint8_t;
using Word = uint16_t;

void decrement_cycles(uint32_t &cycles, uint32_t dec_value)
{
    assert(cycles >= dec_value);
    cycles -= dec_value;
}

std::string to_binary(unsigned short a)
{
    std::stringstream stream;
    stream << "0b" << std::bitset<16>(a);
    return stream.str();
}

std::string to_hex(unsigned short a)
{
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
    Byte unused_flag : 1;
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
    static constexpr Byte INS_AND_IM = 0x29;
    static constexpr Byte INS_BIT_ZP = 0x24;
    static constexpr Byte INS_TXA = 0x8A;
    static constexpr Byte INS_INC_ZP_X = 0xF6;
    static constexpr Byte INS_INC_ABS_X = 0xFE;
    static constexpr Byte INS_NOP = 0xEA;
    static constexpr Byte INS_RTI = 0x40;
    static constexpr Byte INS_BRK = 0x00;
    static constexpr Byte INS_BEQ = 0xF0;

    Byte all_flags()
    {
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

    void set_flags(Byte flags)
    {
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

    Word SP_address() const
    {
        return 0x0100 | SP;
    }

    void push_word_to_stack(uint32_t &cycles, Memory &memory, Word value)
    {
        std::cout << "Saving word value " << to_hex(value) << " on stack at address " << to_hex(SP_address()) << std::endl;
        memory.write_byte((value) >> 8, SP_address(), cycles);
        SP--;
        memory.write_byte((value) & 0xFF, SP_address(), cycles);
        SP--;
    }

    void push_byte_to_stack(uint32_t &cycles, Memory &memory, Byte value)
    {
        std::cout << "Saving byte value " << to_hex(value) << " on stack at address " << to_hex(SP_address()) << std::endl;
        memory.write_byte(value, SP_address(), cycles);
        SP--;
    }

    void write_byte_to_memory(uint32_t &cycles, Memory &memory, Byte value, Word address)
    {
        assert(address < MAX_MEMORY);
        std::cout << "Writing byte value " << to_hex(value) << " at address " << to_hex(address) << std::endl;
        memory.data[address] = value;
        decrement_cycles(cycles, 1);
    }

    void write_word_to_memory(uint32_t &cycles, Memory &memory, Word value, Word address)
    {
        assert(address < MAX_MEMORY);
        std::cout << "Writing word value " << to_hex(value) << " at address " << to_hex(address) << std::endl;
        Byte f_byte = (value >> 8) & 0xFF;
        Byte s_byte = value & 0xFF;
        memory.data[address] = s_byte;
        memory.data[address + 1] = f_byte;
        decrement_cycles(cycles, 2);
    }

    Byte fetch_byte(uint32_t &cycles, Memory &memory)
    {
        decrement_cycles(cycles, 1);
        return memory[PC++];
    }

    Word fetch_word(uint32_t &cycles, Memory &memory)
    {
        Byte first_byte = fetch_byte(cycles, memory);
        Byte second_byte = fetch_byte(cycles, memory);

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

    Byte read_byte_from_stack(uint32_t &cycles, Memory &memory)
    {
        Byte byte_value = read_byte_from_memory(cycles, memory, SP_address() + 1);
        std::cout << "Reading 1 byte with value " << to_hex(byte_value) << " from stack starting from address " << to_hex(SP_address() + 1) << std::endl;
        // TODO should I decrease 1 cycle for SP++?
        SP++;
        return byte_value;
    }

    Word read_word_from_stack(uint32_t &cycles, Memory &memory)
    {
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
                std::cout << "STA ABSOLUTE" << std::endl;
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

            case INS_AND_IM:
            {
                Byte v = fetch_byte(cycles, memory);
                std::cout << "Performing AND operation with INSTRUCTION AND IMEMEDIATE between " << to_hex(A) << " & " << to_hex(v) << std::endl;
                A &= v;
                std::cout << "Result between A reg and imd value: " << to_hex(A) << std::endl;
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

            case INS_BIT_ZP:
            {
                Byte memory_value = fetch_byte(cycles, memory);
                Byte result = A & memory_value;
                zero_flag = result == 0;
                std::cout << "Check what bytes are set based on mask from register A = " << to_binary(A) << " and value " << to_binary(memory_value) << ", result = " << to_binary(result) << std::endl;
                negative_flag = (memory_value & 0b10000000) > 0;
                overflow_flag = (memory_value & 0b1000000) > 0;
                std::cout << "N flag = " << to_binary(negative_flag) << ", V flag = " << to_binary(overflow_flag) << std::endl;
                decrement_cycles(cycles, 1);
            }
            break;

            case INS_TXA:
            {
                std::cout << "Transfer value " << to_hex(X) << " from X reg to A reg with previous value " << to_hex(A) << std::endl;
                A = X;
                decrement_cycles(cycles, 1);
                A_reg_status();
            }
            break;

            case INS_INC_ZP_X:
            {
                Byte zp_address = fetch_byte(cycles, memory);
                // TODO this can be overflow of byte and we will lose some part. We take into consideration a word instead of byte ?!
                Byte new_address = zp_address + X;
                std::cout << "Increment value from address ZeroPage " << to_hex(zp_address) << " + X reg " << to_hex(X) << " = " << to_hex(new_address) << std::endl;
                Byte value = read_byte_from_memory(cycles, memory, new_address);
                std::cout << "Value from address " << to_hex(new_address) << " is " << to_hex(value) << std::endl;
                Byte inc_value = value + 1;
                std::cout << "Value " << to_hex(value) << " incremented is " << to_hex(inc_value) << std::endl;
                write_byte_to_memory(cycles, memory, inc_value, new_address);
            }
            break;

            case INS_INC_ABS_X:
            {
                Word im_address = fetch_word(cycles, memory);
                Word new_address = im_address + X;
                std::cout << "IM Address: " << to_hex(im_address) << " + " << " X: " << to_hex(X) << " = " << to_hex(new_address);
                Word value = read_word_from_memory(cycles, memory, new_address);
                Word inc_value = value + 1;
                std::cout << "Value from address " << to_hex(new_address) << " is " << to_hex(value) << " and inc by 1 will be " << to_hex(inc_value) << std::endl;
                write_word_to_memory(cycles, memory, inc_value, new_address);
            }
            break;

            case INS_NOP:
            {
                std::cout << "NOP -> No instruction" << std::endl;
                decrement_cycles(cycles, 1);
            }
            break;

            case INS_BEQ:
            {
                if (zero_flag == 1)
                {
                    Byte relative_addr = fetch_byte(cycles, memory);
                    Word old_pc = PC;
                    PC += relative_addr;

                    std::cout << "Zero flag is set -> jump to a new instruction using relative address " << to_hex(relative_addr) << " FROM " << to_hex(old_pc) << " TO " << to_hex(PC) << std::endl;
                    const bool page_changed = (PC >> 8) != (old_pc >> 8);
                    if (page_changed)
                    {
                        decrement_cycles(cycles, 1);
                    }
                }
                else
                {
                    std::cout << "Zero flag is not set -> NO jump" << std::endl;
                }
            }
            break;

            case INS_RTI:
            {
                std::cout << "Pulls the processor flags from the stack followed by the program counter" << std::endl;
                Byte flags = read_byte_from_stack(cycles, memory);
                PC = read_word_from_memory(cycles, memory) + 1;
                set_flags(flags);
            }
            break;

            case INS_BRK:
            {
                std::cout << "The program counter and processor status are pushed on the stack then the IRQ interrupt vector at $FFFE/F is loaded into the PC and the break flag in the status set to one" << std::endl;
                push_word_to_stack(cycles, memory, PC - 1);
                push_byte_to_stack(cycles, memory, all_flags());
                Word interrupt_vect_addr = 0xFFFE;
                PC = read_word_from_memory(cycles, memory, interrupt_vect_addr);
                break_flag = 1;
                interrupt_disable_flag = 1;
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

void test_BEQ()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_BEQ;
    memory.data[0xFFFD] = 0x2;
    cpu.zero_flag = 1;

    cpu.execute(2, memory);

    assert(cpu.PC == 0xFFFF);
}

void test_INC_ZP_X()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_INC_ZP_X;
    memory.data[0xFFFD] = 0x01;
    cpu.X = 0x2;

    memory.data[0x03] = 0x03;
    cpu.execute(6, memory);

    assert(memory[0x03] == 0x4);
}

void test_INS_ABS_X()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_INC_ABS_X;
    memory.data[0xFFFD] = 0x21;
    memory.data[0xFFFE] = 0x20;
    cpu.X = 0x1;
    memory.data[0x2022] = 0x27;

    cpu.execute(7, memory);

    assert(memory[0x2022] == 0x28);
}

void test_TXA()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_TXA;
    cpu.A = 0x27;
    cpu.X = 0x26;
    cpu.execute(2, memory);

    assert(cpu.A == 0x26);
}

void test_bit_zp()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_BIT_ZP;
    cpu.A = 0b11000000;
    memory.data[0xFFFD] = 0b01000000;

    cpu.execute(3, memory);
    assert(cpu.overflow_flag == 0b1);
    assert(cpu.negative_flag == 0b0);
    assert(cpu.zero_flag == 0b0);
}

void test_and_imd()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_AND_IM;
    cpu.A = 0b111;
    memory.data[0xFFFD] = 0b010;

    cpu.execute(2, memory);
    assert(cpu.A == 0b010);
}

void test_pla()
{
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

void test_pha()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_STACK_PHA;
    cpu.A = 0x69;
    cpu.execute(3, memory);

    assert(memory[cpu.SP_address() + 1] == 0x69);
}

void test_jmp_indirect()
{
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

void test_jmp_absolute()
{
    Memory memory;
    CPU cpu;
    cpu.reset(memory);

    memory.data[0xFFFC] = CPU::INS_JMP_ABS;
    memory.data[0xFFFD] = 0x01;
    memory.data[0xFFFE] = 0x02;

    cpu.execute(3, memory);

    assert(cpu.PC == 0x0201);
}

void test_sta_absolute()
{
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
    // test_pla();
    // test_and_imd();
    // test_bit_zp();
    // test_TXA();
    // test_INC_ZP_X();
    // test_INS_ABS_X();
    test_BEQ();
    return 0;
}