/*
 * Copyright (C) 2008-2011 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TRINITYCORE_MONEYLOG_H
#define TRINITYCORE_MONEYLOG_H

#include "ScriptPCH.h"

enum MoneyLogEvent
{
    MLE_MAIL        = 0x001,
    MLE_TRADE       = 0x002,
    MLE_NPC         = 0x004,
    MLE_GM          = 0x008,
    MLE_QUEST       = 0x010,
    MLE_LOOT        = 0x020,
    MLE_GUILDBANK   = 0x040,
    MLE_AUCTION     = 0x080,
    MLE_DELETE      = 0x100,
    MLE_SPELL       = 0x200,
    MLE_OTHER       = 0x400,

    MLE_ALL         = 0x7FF,
};

#define MONEY_LOG "Money"

class MoneyLog
{
    friend class ACE_Singleton<MoneyLog, ACE_Thread_Mutex>;
    MoneyLog();
    ~MoneyLog();
public:
    void LogMoney(Player* player, MoneyLogEvent e, int32 amount, const char* fmt, ...);

private:
    void _Initialize();

    bool _enabled;

    int32 _moneyMinAmountPositive;
    int32 _moneyMinAmountNegative;
    uint32 _moneyEventMask;

    uint32 _itemsMinLevel;
    uint32 _itemsEventMask;
    uint32 _itemsQualityMask;
};

#define sMoneyLog (ACE_Singleton<MoneyLog, ACE_Thread_Mutex>::instance())

#endif
