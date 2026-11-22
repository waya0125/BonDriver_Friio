// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#include <string.h>
#include <chrono>
#include <thread>
#include "Qm1d1c0042.hpp"

namespace {
const uint32_t QM_XTAL_KHZ = 16000;
}

Qm1d1c0042::Qm1d1c0042(I2cBus &bus, uint8_t addr)
	: bus(bus), addr(addr)
{
	memset(regs, 0, sizeof(regs));
}

void Qm1d1c0042::writeReg(uint8_t reg, uint8_t val)
{
	uint8_t msg[2] = { reg, val };
	bus.write(addr, msg, 2, true);
}

void Qm1d1c0042::init()
{
	static const uint8_t regInit[32] = {
		0x68, 0x1c, 0xb0, 0x10, 0xbc, 0xc1, 0x11, 0x35,
		0x09, 0x06, 0x00, 0x00, 0xc3, 0x47, 0xb5, 0x30,
		0x00, 0xff, 0xf3, 0x20, 0x3f, 0x25, 0x5c, 0xd6,
		0x55, 0xcf, 0x95, 0xf6, 0x36, 0xf2, 0x09, 0x00
	};
	memcpy(regs, regInit, sizeof(regs));
	for (uint8_t r = 1; r < 32; r++) {
		writeReg(r, regs[r]);
	}
}

void Qm1d1c0042::tune(uint32_t freqKhz)
{
	/** Linux qm1d1c0042.c / emOEM64.sys 0x61790 準拠 (xtal = 16000 kHz) */
	static const uint32_t convTable[9][3] = {
		{ 2151000, 1, 7 }, { 1950000, 1, 6 }, { 1800000, 1, 5 },
		{ 1600000, 1, 4 }, { 1450000, 1, 3 }, { 1250000, 1, 2 },
		{ 1200000, 0, 7 }, {  975000, 0, 6 }, {  950000, 0, 0 }
	};

	regs[0x08] = static_cast<uint8_t>((regs[0x08] & 0xf0) | 0x02);
	regs[0x13] = static_cast<uint8_t>((regs[0x13] & 0x9f) | 0x20);

	uint8_t val02 = static_cast<uint8_t>(regs[0x02] & 0x0f);
	for (int i = 0; i < 8; i++) {
		if (freqKhz < convTable[i][0] && freqKhz >= convTable[i + 1][0]) {
			val02 |= static_cast<uint8_t>((convTable[i][1] << 7) | (convTable[i][2] << 4));
			break;
		}
	}
	regs[0x02] = val02;

	uint32_t a = (freqKhz + QM_XTAL_KHZ / 2) / QM_XTAL_KHZ;
	regs[0x06] = static_cast<uint8_t>((regs[0x06] & 0x40) | ((a - 12) / 4));
	regs[0x07] = static_cast<uint8_t>((regs[0x07] & 0xf0) | ((a - 4 * ((a - 12) / 4 + 1) - 5) & 0x0f));

	int64_t b = static_cast<int64_t>((static_cast<uint64_t>(freqKhz) << 20) / QM_XTAL_KHZ) -
	            (static_cast<int64_t>(a) << 20);
	uint32_t sd = (b >= 0) ? static_cast<uint32_t>(b) : static_cast<uint32_t>((1 << 22) + b);
	regs[0x09] = static_cast<uint8_t>((regs[0x09] & 0xc0) | ((sd >> 16) & 0x3f));
	regs[0x0a] = static_cast<uint8_t>((sd >> 8) & 0xff);
	regs[0x0b] = static_cast<uint8_t>(sd & 0xff);

	static const uint8_t regsToWrite[] = { 0x02, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x13 };
	for (uint8_t r : regsToWrite) {
		writeReg(r, regs[r]);
	}

	writeReg(0x0c, static_cast<uint8_t>(regs[0x0c] & 0x3f));
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	writeReg(0x0c, static_cast<uint8_t>(regs[0x0c] | 0xc0));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	writeReg(0x08, 0x09);
	writeReg(0x13, regs[0x13]);
}
