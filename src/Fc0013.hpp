// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#ifndef _FC0013_HPP_
#define _FC0013_HPP_

#include <cstddef>
#include <cstdint>
#include "I2cBus.hpp"

/**
 * Fitipower FC0013 (ISDB-T / UHF チューナー) 制御クラス。
 * emOEM64.sys 0x32c78 / 0x330e8 / 0x338de / 0x33c00 準拠。
 */
class Fc0013 {
public:
	Fc0013(I2cBus &bus, uint8_t addr = 0xc6);

	void init();
	void tune(uint32_t freqKhz, uint8_t bwMhz = 6);
	void adjustLna();

private:
	struct PllParams {
		uint8_t am;
		uint8_t pm;
		uint16_t k;
	};

	void writeReg(uint8_t reg, uint8_t val);
	uint8_t readReg(uint8_t reg);
	void updateBits(uint8_t reg, uint8_t mask, uint8_t val);
	void writeTable(const RegVal *table, size_t count);
	static PllParams calcPll(uint32_t fVcoKhz, uint32_t refKhz);

	I2cBus &bus;
	uint8_t addr;
};

#endif
