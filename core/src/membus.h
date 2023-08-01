#pragma once

#include <stdint.h>
#include <memory>
#include <vector>
#include <functional>

#include "memcomponent.h"

namespace msfce::core {

enum class AddressingType {
    lowrom,
    highrom,
};

class Membus {
public:
    Membus(AddressingType addrType, bool fastRom);
    ~Membus() = default;

    int plugComponent(const std::shared_ptr<MemComponent>& component);

    uint8_t readU8(uint32_t addr, int* cycles = nullptr);
    uint16_t readU16(uint32_t addr, int* cycles = nullptr);
    uint32_t readU24(uint32_t addr, int* cycles = nullptr);

    void writeU8(uint32_t addr, uint8_t value, int* cycles = nullptr);
    void writeU16(uint32_t addr, uint16_t value, int* cycles = nullptr);

private:
    enum class BankType {
        invalid,
        direct,
        mirrored,
    };

    struct MemoryRange {
        uint8_t bankStart;
        uint8_t bankEnd;

        uint16_t offsetStart;
        uint16_t offsetEnd;

        MemComponentType type;
        uint32_t access;

        int cycles;
    };

    struct MemoryMirror {
        uint8_t srcBankStart;
        uint8_t srcBankEnd;

        uint8_t targetBankStart;
        uint8_t targetBankEnd;
    };

    struct MemoryMap {
        std::vector<MemoryRange> components;
        std::vector<MemoryMirror> mirrors;
    };

    struct Bank {
        BankType type = BankType::invalid;

        // type == direct
        std::vector<const MemoryRange*> ranges;

        // type == mirrored
        uint8_t targetBank;
    };

    struct ComponentHandler {
        std::shared_ptr<MemComponent> ptr;
        std::function<uint32_t(uint8_t bank, uint16_t offset)> addrConverter;
    };

private:
    void initLowRom();
    void initHighRom();

    ComponentHandler* getComponentFromAddr(
        uint32_t addr,
        MemComponentType* type,
        uint8_t* bankId,
        uint16_t* offset,
        int* cycles,
        uint32_t access);

    int getRomTiming(uint8_t bank);

    uint8_t internalReadU8(uint32_t addr);
    void internalWriteU8(uint32_t addr, uint8_t value);

private:
    bool m_FastRom = false;

    Bank m_Banks[0x100];
    ComponentHandler m_Components[kComponentTypeCount];

    // LUT for system area (often accessed)
    const MemoryRange* m_SystemArea[0x8000];

    static const MemoryMap s_LowRomMap;
    static const MemoryMap s_HighRomMap;
};

} // namespace msfce::core
