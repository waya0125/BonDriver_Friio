// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#ifndef _TC90522_HPP_
#define _TC90522_HPP_

#include <cstddef>
#include <cstdint>
#include "I2cBus.hpp"

/**
 * 東芝 TC90522 復調器制御クラス (1 インスタンス = 1 復調器アドレス)。
 * emOEM64.sys 0x6a3dc / 0xfbd50 (ISDB-T) / 0xfc350 (ISDB-S) 準拠。
 */
class Tc90522 {
public:
	Tc90522(I2cBus &bus, uint8_t addr);

	void writeReg(uint8_t reg, uint8_t val);
	uint8_t readReg(uint8_t reg);
	void readRegs(uint8_t reg, uint8_t *buf, uint16_t len);
	void updateBits(uint8_t reg, uint8_t mask, uint8_t val);
	void writeTable(const RegVal *table, size_t count);

	void initTer();
	void initSat();

	void setTerActive(bool active);
	void setSatActive(bool active);
	void holdAgcForTune(bool hold);

	void resetTer();
	void resetSat();

	bool isTerLocked();
	bool isSatTmccLocked();
	bool isSatSlotLocked();

	void selectSatTsid(uint16_t reqTsid);

	float getTerCnr();
	float getSatCnr();

	I2cBus &tunerBus();

private:
	class RepeaterBus : public I2cBus {
	public:
		RepeaterBus(I2cBus &parent, uint8_t demodAddr);
		virtual void write(uint8_t addr, const uint8_t *data, uint16_t len, bool stop = true);
		virtual void read(uint8_t addr, uint8_t *data, uint16_t len, bool stop = true);

	private:
		I2cBus &parent;
		uint8_t demodAddr;
	};

	I2cBus &bus;
	uint8_t addr;
	RepeaterBus repeater;
};

#endif
