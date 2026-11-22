// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#include <math.h>
#include <string.h>
#include "Tc90522.hpp"
#include "error.hpp"

namespace {
// TC90522 共通・I2Cリピータ
const uint8_t TC_REG_SLAVE_REPEATER = 0xfe;

// TC90522 ISDB-T (0x20) レジスタ・定数
const uint8_t TC_T_REG_RESET     = 0x01;
const uint8_t TC_T_RESET_SOFT    = 0x40; // emOEM64.sys 0xfbd50 準拠リセット
const uint8_t TC_T_REG_CLK_0E    = 0x0e;
const uint8_t TC_T_REG_CLK_0F    = 0x0f;
const uint8_t TC_T_CLK_ACTIVE    = 0x01; // ISDB-T 起動
const uint8_t TC_T_CLK_SLEEP     = 0x77; // ISDB-T スリープ
const uint8_t TC_T_REG_TS_PIN_1D = 0x1d;
const uint8_t TC_T_REG_TS_PIN_1F = 0x1f;
const uint8_t TC_T_TS_ENABLE     = 0x00;
const uint8_t TC_T_TS_TRISTATE   = 0xa8;
const uint8_t TC_T_REG_AGC_23    = 0x23;
const uint8_t TC_T_REG_AGC_25    = 0x25;
const uint8_t TC_T_REG_STATUS    = 0x80;
const uint8_t TC_T_LOCK_MASK     = 0x28; // bit5 & bit3 が 0 でロック
const uint8_t TC_T_REG_CN        = 0x8b;

// TC90522 ISDB-S (0x22) レジスタ・定数
const uint8_t TC_S_REG_RESET     = 0x03;
const uint8_t TC_S_RESET_SOFT    = 0x01;
const uint8_t TC_S_REG_TS_PIN_1C = 0x1c;
const uint8_t TC_S_REG_TS_PIN_1F = 0x1f;
const uint8_t TC_S_TS_ENABLE_1C  = 0x05;
const uint8_t TC_S_TS_ENABLE_1F  = 0x00;
const uint8_t TC_S_TS_TRISTATE_1C = 0x80;
const uint8_t TC_S_TS_TRISTATE_1F = 0x22;
const uint8_t TC_S_REG_TSID_HI   = 0x8f;
const uint8_t TC_S_REG_TSID_LO   = 0x90;
const uint8_t TC_S_REG_CN        = 0xbc;
const uint8_t TC_S_REG_TMCC_STAT = 0xc3;
const uint8_t TC_S_TMCC_UNLOCK   = 0x10;
const uint8_t TC_S_REG_SLOT_STAT = 0xc5;
const uint8_t TC_S_SLOT_LOCK_MASK = 0x03;
const uint8_t TC_S_REG_TMCC_TSID = 0xce;
} // namespace

Tc90522::RepeaterBus::RepeaterBus(I2cBus &parent, uint8_t demodAddr)
	: parent(parent), demodAddr(demodAddr)
{
}

void Tc90522::RepeaterBus::write(uint8_t tunerAddr, const uint8_t *data, uint16_t len, bool stop)
{
	if (len > 253) {
		throw traceable_error("Tc90522 repeater write length overflow.");
	}
	uint8_t buf[256];
	buf[0] = TC_REG_SLAVE_REPEATER;
	buf[1] = tunerAddr;
	if (len > 0) {
		memcpy(&buf[2], data, len);
	}
	parent.write(demodAddr, buf, static_cast<uint16_t>(len + 2), stop);
}

void Tc90522::RepeaterBus::read(uint8_t /*tunerAddr*/, uint8_t *data, uint16_t len, bool stop)
{
	parent.read(demodAddr, data, len, stop);
}

Tc90522::Tc90522(I2cBus &bus, uint8_t addr)
	: bus(bus), addr(addr), repeater(bus, addr)
{
}

void Tc90522::writeReg(uint8_t reg, uint8_t val)
{
	uint8_t buf[2] = { reg, val };
	bus.write(addr, buf, 2, true);
}

uint8_t Tc90522::readReg(uint8_t reg)
{
	uint8_t val = 0;
	readRegs(reg, &val, 1);
	return val;
}

void Tc90522::readRegs(uint8_t reg, uint8_t *buf, uint16_t len)
{
	bus.write(addr, &reg, 1, false);
	bus.read(addr, buf, len, true);
}

void Tc90522::updateBits(uint8_t reg, uint8_t mask, uint8_t val)
{
	uint8_t cur = readReg(reg);
	writeReg(reg, static_cast<uint8_t>((cur & ~mask) | (val & mask)));
}

void Tc90522::writeTable(const RegVal *table, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		writeReg(table[i].reg, table[i].val);
	}
}

void Tc90522::initTer()
{
	/** emOEM64.sys 0x6a3dc / 0xfbd50 準拠 (TSピンは一旦トライステート) */
	static const RegVal terInit[] = {
		{TC_T_REG_CLK_0E, TC_T_CLK_ACTIVE}, {TC_T_REG_CLK_0F, 0x77},
		{TC_T_REG_RESET, TC_T_RESET_SOFT},  {0x20, 0x00},
		{0x21, 0xff},                       {0x22, 0x8f},
		{TC_T_REG_AGC_23, 0xfc},            {0x24, 0x4c},
		{TC_T_REG_AGC_25, 0x80},            {0xf4, 0x80},
		{TC_T_REG_TS_PIN_1D, TC_T_TS_TRISTATE}
	};
	writeTable(terInit, sizeof(terInit) / sizeof(terInit[0]));
}

void Tc90522::initSat()
{
	/** emOEM64.sys 0x6a3dc / 0xfc350 準拠 (TSピンは一旦トライステート) */
	static const RegVal satInit[] = {
		{0x07, 0x01}, {0x08, 0x10}, {0x15, 0x00}, {0x1d, 0x00},
		{0x0a, 0xff}, {0x10, 0xb1}, {0x11, 0x40}, {0x85, 0x7a},
		{0x87, 0x04}, {TC_S_REG_TS_PIN_1C, TC_S_TS_TRISTATE_1C},
		{TC_S_REG_TS_PIN_1F, TC_S_TS_TRISTATE_1F},
		{TC_S_REG_RESET, TC_S_RESET_SOFT}
	};
	writeTable(satInit, sizeof(satInit) / sizeof(satInit[0]));
}

void Tc90522::setTerActive(bool active)
{
	if (active) {
		writeReg(TC_T_REG_CLK_0E, TC_T_CLK_ACTIVE);
		writeReg(TC_T_REG_CLK_0F, 0x77);
		writeReg(TC_T_REG_TS_PIN_1D, TC_T_TS_ENABLE);
		writeReg(TC_T_REG_TS_PIN_1F, TC_T_TS_ENABLE);
	} else {
		writeReg(TC_T_REG_CLK_0E, TC_T_CLK_SLEEP);
		writeReg(TC_T_REG_TS_PIN_1D, TC_T_TS_TRISTATE);
	}
}

void Tc90522::setSatActive(bool active)
{
	if (active) {
		writeReg(TC_S_REG_TS_PIN_1F, TC_S_TS_ENABLE_1F);
		writeReg(TC_S_REG_TS_PIN_1C, TC_S_TS_ENABLE_1C);
	} else {
		writeReg(TC_S_REG_TS_PIN_1C, TC_S_TS_TRISTATE_1C);
		writeReg(TC_S_REG_TS_PIN_1F, TC_S_TS_TRISTATE_1F);
	}
}

void Tc90522::holdAgcForTune(bool hold)
{
	// Windows 公式ドライバ準拠: 選局前は 0x25=0x00, 0x23|=0x01、選局後は戻す
	if (hold) {
		writeReg(TC_T_REG_AGC_25, 0x00);
		updateBits(TC_T_REG_AGC_23, 0x01, 0x01);
	} else {
		updateBits(TC_T_REG_AGC_23, 0x01, 0x00);
		writeReg(TC_T_REG_AGC_25, 0x80);
	}
}

void Tc90522::resetTer()
{
	writeReg(TC_T_REG_RESET, TC_T_RESET_SOFT);
}

void Tc90522::resetSat()
{
	writeReg(TC_S_REG_RESET, TC_S_RESET_SOFT);
}

bool Tc90522::isTerLocked()
{
	return (readReg(TC_T_REG_STATUS) & TC_T_LOCK_MASK) == 0;
}

bool Tc90522::isSatTmccLocked()
{
	return (readReg(TC_S_REG_TMCC_STAT) & TC_S_TMCC_UNLOCK) == 0;
}

bool Tc90522::isSatSlotLocked()
{
	return (readReg(TC_S_REG_SLOT_STAT) & TC_S_SLOT_LOCK_MASK) != 0;
}

void Tc90522::selectSatTsid(uint16_t reqTsid)
{
	uint8_t tmcc[24];
	readRegs(TC_S_REG_TMCC_TSID, tmcc, sizeof(tmcc));
	uint16_t tsid = reqTsid;
	if (tsid == 0) {
		for (int i = 0; i < 12; i++) {
			uint16_t cand = static_cast<uint16_t>((tmcc[i * 2] << 8) | tmcc[i * 2 + 1]);
			if (cand != 0xffff && cand != 0x0000) {
				tsid = cand;
				break;
			}
		}
	}
	if (tsid != 0) {
		writeReg(TC_S_REG_TSID_HI, static_cast<uint8_t>(tsid >> 8));
		writeReg(TC_S_REG_TSID_LO, static_cast<uint8_t>(tsid & 0xff));
		resetSat();
	}
}

float Tc90522::getTerCnr()
{
	uint8_t b[3] = {};
	readRegs(TC_T_REG_CN, b, 3);
	uint32_t cndat = (static_cast<uint32_t>(b[0]) << 16) | (static_cast<uint32_t>(b[1]) << 8) | b[2];
	if (cndat == 0) return 0.0f;
	return static_cast<float>(10.0 * log10(static_cast<double>(1 << 24) / cndat));
}

float Tc90522::getSatCnr()
{
	uint8_t b[2] = {};
	readRegs(TC_S_REG_CN, b, 2);
	uint16_t cndat = static_cast<uint16_t>((b[0] << 8) | b[1]);
	if (cndat < 3000) return 0.0f;
	double x = static_cast<double>(cndat - 3000);
	double p = sqrt(x) / 64.0;
	double p2 = p * p, p3 = p2 * p, p4 = p2 * p2, p5 = p4 * p;
	double cnr = (-1634.6 * p5 + 14341.0 * p4 - 50259.0 * p3 + 88977.0 * p2 - 89565.0 * p + 58857.0) / 1000.0;
	return cnr > 0.0 ? static_cast<float>(cnr) : 0.0f;
}

I2cBus &Tc90522::tunerBus()
{
	return repeater;
}
