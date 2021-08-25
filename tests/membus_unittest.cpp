#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "membus.h"

using ::testing::Return;
using namespace msfce::core;

class MockComponent : public msfce::core::MemComponent {
public:
    MockComponent(MemComponentType type) : MemComponent(type)
    {
    }

    MOCK_METHOD(uint8_t, readU8, (uint32_t address), (override));
    MOCK_METHOD(void, writeU8, (uint32_t address, uint8_t value), (override));
};

class MembusTest : public testing::Test {
public:
    void SetUp()
    {
        membus = std::make_unique<Membus>(AddressingType::lowrom, false);

        ram = std::make_shared<BufferMemComponent>(
            MemComponentType::ram, 128 * 1024);
        membus->plugComponent(ram);

        rom = std::make_shared<BufferMemComponent>(
            MemComponentType::rom, 4 * 1024 * 1024);
        membus->plugComponent(rom);

        sram = std::make_shared<BufferMemComponent>(
            MemComponentType::sram, 512 * 1024);
        membus->plugComponent(sram);

        apu = std::make_shared<MockComponent>(MemComponentType::apu);
        membus->plugComponent(apu);
    }

    std::unique_ptr<Membus> membus;
    std::shared_ptr<BufferMemComponent> ram;
    std::shared_ptr<BufferMemComponent> rom;
    std::shared_ptr<BufferMemComponent> sram;
    std::shared_ptr<MockComponent> apu;
};

TEST_F(MembusTest, LowRomRam)
{
    // Bank 0x00 mirrored WRAM
    membus->writeU8(0x1234, 0x43);
    ASSERT_EQ(ram->readU8(0x1234), 0x43);
    ASSERT_EQ(membus->readU8(0x1234), 0x43);
    ASSERT_EQ(membus->readU8(0x7E1234), 0x43);

    // Bank 0x7E
    membus->writeU8(0x7E123A, 0x43);
    ASSERT_EQ(ram->readU8(0x123A), 0x43);
    ASSERT_EQ(membus->readU8(0x123A), 0x43);
    ASSERT_EQ(membus->readU8(0x7E123A), 0x43);

    // Bank 0x7F
    membus->writeU8(0x7F223A, 0x43);
    ASSERT_EQ(ram->readU8(0x1223A), 0x43);
    ASSERT_EQ(membus->readU8(0x7F223A), 0x43);

    // writeU16()
    membus->writeU16(0x1334, 0x4243);
    ASSERT_EQ(ram->readU8(0x1334), 0x43);
    ASSERT_EQ(ram->readU8(0x1335), 0x42);
    ASSERT_EQ(membus->readU16(0x1334), 0x4243);
    ASSERT_EQ(membus->readU16(0x7E1334), 0x4243);
}

TEST_F(MembusTest, LowRomRom)
{
    rom->writeU8(0x0, 0x43);
    ASSERT_EQ(rom->readU8(0x0), 0x43);
    ASSERT_EQ(membus->readU8(0x008000), 0x43);
    ASSERT_EQ(membus->readU8(0x808000), 0x43);

    rom->writeU8(0x80010, 0x53);
    ASSERT_EQ(membus->readU8(0x108010), 0x53);
    ASSERT_EQ(membus->readU8(0x908010), 0x53);

    rom->writeU8(0x7FFF, 0x48);
    rom->writeU8(0x8000, 0x49);
    rom->writeU8(0x8001, 0x4A);
    ASSERT_EQ(membus->readU8(0x00FFFF), 0x48);
    ASSERT_EQ(membus->readU8(0x018000), 0x49);
    ASSERT_EQ(membus->readU16(0x00FFFF), 0x4948);
    ASSERT_EQ(membus->readU24(0x00FFFF), 0x4A4948);

    rom->writeU8(0x3FFFFE, 0x80);
    rom->writeU8(0x3FFFFF, 0x81);
    ASSERT_EQ(membus->readU16(0xFFFFFE), 0x8180);
}

TEST_F(MembusTest, LowRomSram)
{
    // Bank 0x72
    membus->writeU8(0x724343, 0x43);
    ASSERT_EQ(sram->readU8(0x14343), 0x43);
    ASSERT_EQ(membus->readU8(0x724343), 0x43);

    // Bank 0xFF
    membus->writeU8(0xFF6666, 0x43);
    ASSERT_EQ(sram->readU8(0x7E666), 0x43);
    ASSERT_EQ(membus->readU8(0xFF6666), 0x43);
}

TEST_F(MembusTest, LowRomApuReadU8)
{
    EXPECT_CALL(*apu, readU8(0x2140)).Times(1).WillOnce(Return(0x43));

    EXPECT_CALL(*apu, readU8(0x3F2140)).Times(1).WillOnce(Return(0x44));

    ASSERT_EQ(membus->readU8(0x2140), 0x43);
    ASSERT_EQ(membus->readU8(0x3F2140), 0x44);
}

TEST_F(MembusTest, LowRomApuReadU16)
{
    EXPECT_CALL(*apu, readU8(0x2141)).Times(1).WillOnce(Return(0x43));

    EXPECT_CALL(*apu, readU8(0x2140)).Times(1).WillOnce(Return(0x44));

    ASSERT_EQ(membus->readU16(0x2140), 0x4344);
}
