// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#include <chrono>
#include <thread>
#include "Fc0013.hpp"

namespace {
const uint32_t FC_XTAL_KHZ      = 28800;
const uint32_t FC_UHF_HIGH_KHZ  = 862000;
const uint32_t FC_VCO_LIMIT_KHZ = 3560000;
const uint32_t FC_VCO_4_ALT_KHZ = 3800000;
} // namespace

Fc0013::Fc0013(I2cBus &bus, uint8_t addr)
	: bus(bus), addr(addr)
{
}

void Fc0013::writeReg(uint8_t reg, uint8_t val)
{
	uint8_t msg[2] = { reg, val };
	bus.write(addr, msg, 2, true);
}

uint8_t Fc0013::readReg(uint8_t reg)
{
	bus.write(addr, &reg, 1, true);
	uint8_t rd = static_cast<uint8_t>(addr | 0x01);
	bus.write(rd, NULL, 0, false);
	uint8_t val = 0;
	bus.read(addr, &val, 1, true);
	return val;
}

void Fc0013::updateBits(uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t cur = readReg(reg);
	writeReg(reg, static_cast<uint8_t>((cur & ~mask) | (val & mask)));
}

void Fc0013::writeTable(const RegVal *table, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		writeReg(table[i].reg, table[i].val);
	}
}

Fc0013::PllParams Fc0013::calcPll(uint32_t fVcoKhz, uint32_t refKhz)
{
	uint32_t xin = (fVcoKhz + refKhz / 2) / refKhz;
	uint32_t pm = xin >> 3;
	uint32_t am = xin - (pm << 3);
	if (am < 2) {
		am += 8;
		pm -= 1;
	}
	uint32_t rem = fVcoKhz - (fVcoKhz / refKhz) * refKhz;
	uint32_t k = (rem << 15) / refKhz;
	if (k >= 0x4000) {
		k = (k + 0x8000) & 0xffff;
	}
	return { static_cast<uint8_t>(am), static_cast<uint8_t>(pm), static_cast<uint16_t>(k) };
}

void Fc0013::init()
{
	/** emOEM64.sys 0x32c78 準拠 */
	static const RegVal initSeq[] = {
		{0x01, 0x09}, {0x02, 0x16}, {0x03, 0x00}, {0x04, 0x00},
		{0x05, 0x17}, {0x06, 0x02}, {0x07, 0x2a}, {0x08, 0xff},
		{0x09, 0x6f}, {0x0a, 0xb8}, {0x0b, 0x82}, {0x0c, 0xfe},
		{0x0d, 0x01}, {0x0e, 0x00}, {0x0f, 0x00}, {0x10, 0x00},
		{0x11, 0x00}, {0x12, 0x00}, {0x13, 0x00}, {0x14, 0x50},
		{0x15, 0x01}
	};
	writeTable(initSeq, sizeof(initSeq) / sizeof(initSeq[0]));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Fc0013::tune(uint32_t freqKhz, uint8_t bwMhz)
{
	/** emOEM64.sys 0x330e8 / 0x338de 準拠 */
	updateBits(0x1d, 0x1c, 0x1c);
	updateBits(0x07, 0x10, 0x00);
	updateBits(0x14, 0xe0, (freqKhz <= FC_UHF_HIGH_KHZ) ? 0x40 : 0x20);

	struct VcoDiv { uint8_t multi, r05, r06; };
	static const VcoDiv divTable[] = {
		{96, 0x82, 0x00}, {64, 0x02, 0x02}, {48, 0x42, 0x00},
		{32, 0x82, 0x02}, {24, 0x22, 0x00}, {16, 0x42, 0x02},
		{12, 0x12, 0x00}, { 8, 0x22, 0x02}, { 6, 0x0a, 0x00}
	};

	uint32_t f = freqKhz;
	uint8_t multi = 4, r05 = 0x0a, r06 = 0x02;
	bool found = false;
	for (const auto &d : divTable) {
		if (f * d.multi < FC_VCO_LIMIT_KHZ) {
			multi = d.multi;
			r05 = d.r05;
			r06 = d.r06;
			found = true;
			break;
		}
	}
	if (!found) {
		multi = 4;
		if (f * 4 < FC_VCO_4_ALT_KHZ) {
			r05 = 0x12;
		} else {
			f >>= 1;
			r05 = 0x0a;
		}
		r06 = 0x02;
	}

	r06 |= 0x08;
	uint8_t r09 = 0x4f;
	uint32_t fVco = f * multi;
	PllParams pll = calcPll(fVco, FC_XTAL_KHZ);
	if (pll.pm < 11) {
		r09 = 0x6f;
		pll = calcPll(fVco, FC_XTAL_KHZ / 2);
	}

	if (bwMhz == 6) {
		r06 |= 0x80;
	} else if (bwMhz == 7) {
		r06 = static_cast<uint8_t>((r06 & 0x3f) | 0x40);
	} else {
		r06 &= 0x3f;
	}
	r05 |= 0x07;

	const RegVal pllRegs[] = {
		{0x01, pll.am},
		{0x02, pll.pm},
		{0x03, static_cast<uint8_t>((pll.k >> 8) & 0xff)},
		{0x04, static_cast<uint8_t>(pll.k & 0xff)},
		{0x05, r05},
		{0x06, r06},
		{0x09, r09}
	};
	writeTable(pllRegs, sizeof(pllRegs) / sizeof(pllRegs[0]));

	updateBits(0x11, 0x04, (multi == 64) ? 0x04 : 0x00);
	writeReg(0x0e, 0x80);
	writeReg(0x0e, 0x00);
	writeReg(0x0e, 0x00);
	if ((readReg(0x0e) & 0x3f) > 0x3c) {
		r06 &= 0xf7;
		writeReg(0x06, r06);
		writeReg(0x0e, 0x80);
		writeReg(0x0e, 0x00);
	}
	writeReg(0x08, 0x1f);
	writeReg(0x0c, 0xf8);
	writeReg(0x14, 0x50);
}

void Fc0013::adjustLna()
{
	/** emOEM64.sys 0x33c00 準拠: RSSI (0x12/0x13) に応じて LNA (0x14) を調整 */
	uint8_t r14 = readReg(0x14);
	writeReg(0x12, 0x00);
	writeReg(0x13, 0x00);
	uint8_t r12 = readReg(0x12);
	uint8_t r13 = readReg(0x13);

	static const int lnaTable[8] = { 10, 8, 4, 6, 0, 0, 0, 2 };
	static const int pgaTable[4] = { 0, 2, 6, 4 };
	int tot = lnaTable[(r13 >> 5) & 0x07] +
	          pgaTable[(r12 >> 2) & 0x03] +
	          pgaTable[r12 & 0x03] +
	          (r13 & 0x1f) * 2;

	uint8_t mode = r14 & 0x1f;
	uint8_t next = r14;
	if (mode == 0x00 && tot > 0x1e) {
		next = static_cast<uint8_t>((r14 & 0xe0) | 0x08);
	} else if (mode == 0x08) {
		if (tot < 0x06) next = static_cast<uint8_t>(r14 & 0xe0);
		else if (tot > 0x32) next = static_cast<uint8_t>((r14 & 0xf0) | 0x10);
	} else if (mode == 0x10 && tot < 0x1c) {
		next = static_cast<uint8_t>((r14 & 0xe0) | 0x08);
	} else if (mode != 0x00 && mode != 0x08 && mode != 0x10) {
		next = static_cast<uint8_t>((r14 & 0xf0) | 0x10);
	}
	if (next != r14) {
		writeReg(0x14, next);
	}
}
