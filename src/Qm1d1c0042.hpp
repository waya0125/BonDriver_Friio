// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#ifndef _QM1D1C0042_HPP_
#define _QM1D1C0042_HPP_

#include <cstdint>
#include "I2cBus.hpp"

/**
 * シャープ QM1D1C0042 (ISDB-S チューナー) 制御クラス。
 */
class Qm1d1c0042 {
public:
	Qm1d1c0042(I2cBus &bus, uint8_t addr = 0xc0);

	void init();
	void tune(uint32_t freqKhz);

private:
	void writeReg(uint8_t reg, uint8_t val);

	I2cBus &bus;
	uint8_t addr;
	uint8_t regs[32];
};

#endif
