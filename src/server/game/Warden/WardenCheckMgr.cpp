/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "LogMgr.h"
#include "Cryptography/HMACSHA1.h"
#include "Database/DatabaseEnv.h"
#include "Util.h"
#include "WardenCheckMgr.h"
#include "WardenWin.h"

inline void SetBigNumber(BigNumber& k, const std::string& s)
{
    k.SetHexStr(s.c_str());
    int len = s.size() / 2;
    if (k.GetNumBytes() < len)
    {
        uint8* temp = new uint8[len];
        memset(temp, 0, len);
        memcpy(temp, k.AsByteArray(), k.GetNumBytes());
        std::reverse(temp, temp + len);
        k.SetBinary((uint8*)temp, len);
        delete[] temp;
    }
}

bool WardenCheck::LoadFromDB(Field* fields)
{
    _id     = fields[0].GetUInt32();
    _type   = WardenCheckType(fields[1].GetUInt8());
    switch (_type)
    {
        case PAGE_CHECK_A:  // data, address, length
            SetBigNumber(_data, fields[2].GetString());
            _address = fields[4].GetUInt32();
            _length  = fields[5].GetUInt8();
            break;
        case PAGE_CHECK_B:  // data, address, length
            SetBigNumber(_data, fields[2].GetString());
            _address = fields[4].GetUInt32();
            _length  = fields[5].GetUInt8();
            break;
        case DRIVER_CHECK:  // data, str
            SetBigNumber(_data, fields[2].GetString());
            _str = fields[6].GetString();
            break;
        case MEM_CHECK:     // result, address, length
            SetBigNumber(_result, fields[3].GetString());
            _address = fields[4].GetUInt32();
            _length  = fields[5].GetUInt8();
            break;
        case PROC_CHECK:    // address, length
            _address = fields[4].GetUInt32();
            _length  = fields[5].GetUInt8();
            // _str = fields[6].GetString(); // support is missing
            break;
        case MPQ_CHECK:     // result, str
            SetBigNumber(_result, fields[3].GetString());
            _str = fields[6].GetString();
            break;
        case LUA_STR_CHECK: // str
            _str = fields[6].GetString();
            break;
        case MODULE_CHECK:  // str
            _str = fields[6].GetString();
            break;
        default:
            break;
    }
    return true;
}

void WardenCheck::AppendStr(ByteBuffer& buff) const
{
    switch (_type)
    {
        case MPQ_CHECK:
        case LUA_STR_CHECK:
        case DRIVER_CHECK:
            buff << uint8(_str.size());
            buff.append(_str.c_str(), _str.size());
            break;
        default:
            break;
    }
}

void WardenCheck::FillPacket(uint8 xorByte, ByteBuffer& buff, uint8& index)
{
    buff << uint8(_type ^ xorByte);
    switch (_type)
    {
        case MEM_CHECK:
            buff << uint8 (0x00);
            buff << uint32(_address);
            buff << uint8 (_length);
            break;
        case PAGE_CHECK_A:
        case PAGE_CHECK_B:
            buff.append(_data.AsByteArray(0, false), _data.GetNumBytes());
            buff << uint32(_address);
            buff << uint8 (_length);
            break;
        case MPQ_CHECK:
        case LUA_STR_CHECK:
            buff << uint8(index++);
            break;
        case DRIVER_CHECK:
            buff.append(_data.AsByteArray(0, false), _data.GetNumBytes());
            buff << uint8(index++);
            break;
        case MODULE_CHECK:
        {
            uint32 seed = static_cast<uint32>(rand32());
            buff << seed;

            HmacHash hmac(4, (uint8*)&seed);
            hmac.UpdateData(_str);
            hmac.Finalize();

            buff.append(hmac.GetDigest(), hmac.GetLength());
            break;
        }
        /*
        case PROC_CHECK:
            buff.append(_data.AsByteArray(0, false), _data.GetNumBytes());
            buff << uint8 (index++);
            buff << uint8 (index++);
            buff << uint32(_address);
            buff << uint8 (_length);
            break;
        //*/
        default:
            break;                                      // Should never happen
    }
}

bool WardenCheck::Check(WorldSession* session, ByteBuffer& buff)
{
    switch (_type)
    {
        case MEM_CHECK:
        {
            uint8 res;
            buff >> res;
            if (res != 0)
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: MEM_CHECK is not 0x00 (check: %u, account: %u)", _id, session->GetAccountId());
                return false;
            }
            if (memcmp(buff.contents() + buff.rpos(), _result.AsByteArray(0, false), _length) != 0)
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: MEM_CHECK failed (check: %u, account: %u)", _id, session->GetAccountId());
                buff.rpos(buff.rpos() + _length);
                return false;
            }
            buff.rpos(buff.rpos() + _length);
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "RESULT: MEM_CHECK passed (check: %u, account: %u)", _id, session->GetAccountId());
            break;
        }
        case PAGE_CHECK_A:
        case PAGE_CHECK_B:
        case DRIVER_CHECK:
        case MODULE_CHECK:
        {
            const uint8 byte = 0xE9;
            if (memcmp(buff.contents() + buff.rpos(), &byte, sizeof(uint8)) != 0)
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: %s failed (check: %u, account: %u)",
                                 WardenCheckMgr::GetCheckName(_type), _id, session->GetAccountId());
                buff.rpos(buff.rpos() + 1);
                return false;
            }
            buff.rpos(buff.rpos() + 1);
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "RESULT: %s passed (check: %u, account: %u)",
                             WardenCheckMgr::GetCheckName(_type), _id, session->GetAccountId());
            break;
        }
        case LUA_STR_CHECK:
        {
            uint8 res;
            buff >> res;
            if (res != 0)
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: LUA_STR_CHECK failed (check: %u, account: %u)", _id, session->GetAccountId());
                return false;
            }
            std::string s;
            uint8 luaStrLen;
            buff >> luaStrLen;
            if (luaStrLen != 0)
            {
                char *str = new char[luaStrLen + 1];
                memset(str, 0, luaStrLen + 1);
                memcpy(str, buff.contents() + buff.rpos(), luaStrLen);
                s = str;
                delete[] str;
            }
            buff.rpos(buff.rpos() + luaStrLen);         // Skip string
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "RESULT: LUA_STR_CHECK '%s' passed (check: %u, account: %u)", s.c_str(), _id, session->GetAccountId());
            break;
        }
        case MPQ_CHECK:
        {
            uint8 res;
            buff >> res;
            if (res != 0)
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: MPQ_CHECK is not 0x00 (check: %u, account: %u)", _id, session->GetAccountId());
                return false;
            }
            if (memcmp(buff.contents() + buff.rpos(), _result.AsByteArray(0, false), 20) != 0) // SHA1
            {
                sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "RESULT: MPQ_CHECK fail (check: %u, account: %u)", _id, session->GetAccountId());
                buff.rpos(buff.rpos() + 20);            // 20 bytes SHA1
                return false;
            }
            buff.rpos(buff.rpos() + 20);                // 20 bytes SHA1
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "RESULT: MPQ_CHECK passed (check: %u, account: %u)", _id, session->GetAccountId());
            break;
        }
        default:                                        // Should never happen
            break;
    }
    return true;
}

// static
const char* WardenCheckMgr::GetCheckName(WardenCheckType type)
{
    switch (type)
    {
        case MEM_CHECK:     return "MEM_CHECK";
        case PAGE_CHECK_A:  return "PAGE_CHECK_A";
        case PAGE_CHECK_B:  return "PAGE_CHECK_B";
        case MPQ_CHECK:     return "MPQ_CHECK";
        case LUA_STR_CHECK: return "LUA_STR_CHECK";
        case DRIVER_CHECK:  return "DRIVER_CHECK";
        case TIMING_CHECK:  return "TIMING_CHECK";
        case PROC_CHECK:    return "PROC_CHECK";
        case MODULE_CHECK:  return "MODULE_CHECK";
        default:            return "<Unknown>";
    }
}

WardenCheckMgr::WardenCheckMgr()
{
    sLogMgr->RegisterLogFile(WARDEN_LOG);
}

WardenCheckMgr::~WardenCheckMgr()
{
    for (uint32 i = 0; i < _checks.size(); ++i)
        delete _checks[i];
}

void WardenCheckMgr::LoadWardenChecks()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld->getBoolConfig(CONFIG_BOOL_WARDEN_ENABLED))
    {
        sLog->outString(">> Warden disabled, loading checks skipped.");
        sLog->outString();
        return;
    }

    QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM warden_checks");
    if (!result)
    {
        sLog->outString(">> Loaded 0 Warden checks. DB table `warden_checks` is empty!");
        sLog->outString();
        return;
    }

    _checks.resize((*result)[0].GetUInt32() + 1);

    //                                   0   1     2     3       4        5       6
    result = WorldDatabase.Query("SELECT id, type, data, result, address, length, str FROM warden_checks ORDER BY id ASC");
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        WardenCheck* check = new WardenCheck();
        if (!check->LoadFromDB(fields))
        {
            delete check;
            continue;
        }

        _checks[check->GetId()] = check;

        switch (check->GetType()) {
            case MEM_CHECK:
            case MODULE_CHECK:
                _memChecksPool.push_back(check->GetId());
                break;
            default:
                _otherChecksPool.push_back(check->GetId());
                break;
        }

        ++count;
    }
    while (result->NextRow());

    sLog->outString(">> Loaded %u warden checks.", count);
    sLog->outString();
}

void WardenCheckMgr::FillChecks(bool other, CheckIds& ids) const
{
    if (other)
        ids.assign(_otherChecksPool.begin(), _otherChecksPool.end());
    else
        ids.assign(_memChecksPool.begin(), _memChecksPool.end());
}

WardenCheck* WardenCheckMgr::GetWardenDataById(uint32 id)
{
    if (id < _checks.size())
        return _checks[id];
    return NULL;
}
