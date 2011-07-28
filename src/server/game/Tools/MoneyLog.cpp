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

#include "MoneyLog.h"
#include "Config.h"
#include "LogMgr.h"

const char* MoneyLogEventToString(MoneyLogEvent e)
{
    switch (e)
    {
        case MLE_MAIL: return "MAIL";
        case MLE_TRADE: return "TRADE";
        case MLE_NPC: return "NPC";
        case MLE_GM: return "GM";
        case MLE_QUEST: return "QUEST";
        case MLE_LOOT: return "LOOT";
        case MLE_GUILDBANK: return "GUILD";
        case MLE_AUCTION: return "AUCTION";
        case MLE_DELETE: return "DELETE";
        case MLE_SPELL: return "SPELL";
        case MLE_OTHER: return "OTHER";
        default: return "<UNK>";
    }
}

MoneyLog::MoneyLog() : _enabled(false), _moneyMinAmountPositive(0), _moneyMinAmountNegative(0), _moneyEventMask(MLE_ALL), _itemsMinLevel(0), _itemsEventMask(MLE_ALL), _itemsQualityMask(0xFF)
{
    _Initialize();
}

MoneyLog::~MoneyLog()
{
    sLogMgr->UnregisterLogFile(MONEY_LOG);
}

void MoneyLog::_Initialize()
{
    sLogMgr->RegisterLogFile(MONEY_LOG);
    _enabled = sLogMgr->IsLogEnabled(MONEY_LOG);
    if (_enabled)
    {
        _moneyMinAmountPositive = sConfig->GetIntDefault("Log.Money.Money.MinAmountPositive", 0);
        _moneyMinAmountNegative = sConfig->GetIntDefault("Log.Money.Money.MinAmountNegative", 0);
        _moneyEventMask = sConfig->GetIntDefault("Log.Money.Money.EventMask", MLE_ALL);
        _itemsMinLevel  = sConfig->GetIntDefault("Log.Money.Items.MinLevel", 0);
        _itemsEventMask = sConfig->GetIntDefault("Log.Money.Items.EventMask", MLE_ALL);
        _itemsQualityMask = sConfig->GetIntDefault("Log.Money.Items.QualityMask", 0xFF);
    }
}

void MoneyLog::LogMoney(Player* player, MoneyLogEvent e, int32 amount, const char* fmt, ...)
{
    player->ModifyMoney(amount);
    if (_enabled && (_moneyEventMask & e))
        if ((amount > 0 && _moneyMinAmountPositive <= amount) || (_moneyMinAmountNegative <= abs(amount)))
        {
            char sz[2048];
            va_list lst;
            va_start(lst, fmt);
            vsnprintf(sz, 2048, fmt, lst);
            va_end(lst);

            bool minus = (amount < 0);
            amount = abs(amount);
            sLogMgr->WriteLn(MONEY_LOG, "%14s (%7u) | %7s | %s%9u.%2u.%2u | %s", player->GetName(), player->GetGUIDLow(), MoneyLogEventToString(e),
                minus ? "-" : "+", amount / 10000, (amount % 10000) / 100, amount % 100, sz);
        }
}
