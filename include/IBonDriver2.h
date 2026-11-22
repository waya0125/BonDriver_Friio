// SPDX-License-Identifier: MIT
#pragma once

#include "IBonDriver.h"

class IBonDriver2 : public IBonDriver {
public:
    virtual LPCTSTR GetTunerName(void) = 0;

    virtual const BOOL IsTunerOpening(void) = 0;

    virtual LPCTSTR EnumTuningSpace(const DWORD dwSpace) = 0;
    virtual LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) = 0;

    virtual const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel) = 0;

    virtual const DWORD GetCurSpace(void) = 0;
    virtual const DWORD GetCurChannel(void) = 0;

    virtual void Release(void) = 0;
};
