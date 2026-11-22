// SPDX-License-Identifier: MIT
// Copyright (c) 2026 waya0125

#ifndef _I2CBUS_HPP_
#define _I2CBUS_HPP_

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <thread>

/** レジスタ番号と値の組 */
struct RegVal {
	uint8_t reg;
	uint8_t val;
};

/**
 * 最小 I2C バスインターフェース。
 * EM28178 本体 I2C と TC90522 リピータ (0xfe) 経由の双方を統一的に扱う。
 */
class I2cBus {
public:
	virtual ~I2cBus() {}
	virtual void write(uint8_t addr, const uint8_t *data, uint16_t len, bool stop = true) = 0;
	virtual void read(uint8_t addr, uint8_t *data, uint16_t len, bool stop = true) = 0;
};

/** 述語が真になるまで intervalMs 間隔で最大 maxTries 回待つ */
template <typename Pred>
inline bool waitUntil(Pred pred, int maxTries, int intervalMs)
{
	for (int i = 0; i < maxTries; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
		if (pred()) {
			return true;
		}
	}
	return false;
}

#endif
