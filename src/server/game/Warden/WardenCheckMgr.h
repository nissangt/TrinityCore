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

#ifndef _WARDENCHECKMGR_H
#define _WARDENCHECKMGR_H

#include <map>
#include "Cryptography/BigNumber.h"

enum WardenCheckType
{
    MEM_CHECK               = 0xF3, // 243: byte moduleNameIndex + uint Offset + byte Len (check to ensure memory isn't modified)
    PAGE_CHECK_A            = 0xB2, // 178: uint Seed + byte[20] SHA1 + uint Addr + byte Len (scans all pages for specified hash)
    PAGE_CHECK_B            = 0xBF, // 191: uint Seed + byte[20] SHA1 + uint Addr + byte Len (scans only pages starts with MZ+PE headers for specified hash)
    MPQ_CHECK               = 0x98, // 152: byte fileNameIndex (check to ensure MPQ file isn't modified)
    LUA_STR_CHECK           = 0x8B, // 139: byte luaNameIndex (check to ensure LUA string isn't used)
    DRIVER_CHECK            = 0x71, // 113: uint Seed + byte[20] SHA1 + byte driverNameIndex (check to ensure driver isn't loaded)
    TIMING_CHECK            = 0x57, //  87: empty (check to ensure GetTickCount() isn't detoured)
    PROC_CHECK              = 0x7E, // 126: uint Seed + byte[20] SHA1 + byte moluleNameIndex + byte procNameIndex + uint Offset + byte Len (check to ensure proc isn't detoured)
    MODULE_CHECK            = 0xD9, // 217: uint Seed + byte[20] SHA1 (check to ensure module isn't injected)
};

class WardenCheck
{
public:
    bool LoadFromDB(Field* fields);

    uint32 GetId() const { return _id; }
    WardenCheckType GetType() const { return _type; }

    void AppendStr(ByteBuffer& buff) const;
    void FillPacket(uint8 xorByte, ByteBuffer& buff, uint8& index);
    bool Check(WorldSession* session, ByteBuffer& buff);

private:
    uint32 _id;
    WardenCheckType _type;
    BigNumber _data;
    uint32 _address;                                        // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    uint8 _length;                                          // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    std::string _str;                                       // LUA, MPQ, DRIVER
    BigNumber _result;                                      // MEM_CHECK
};

// We have a linear key without any gaps, so we use vector for fast access
typedef std::vector<WardenCheck*> Checks;
typedef std::vector<uint32> CheckIds;

class WardenCheckMgr
{
    friend class ACE_Singleton<WardenCheckMgr, ACE_Null_Mutex>;
    WardenCheckMgr();
    ~WardenCheckMgr();

public:
    static const char* GetCheckName(WardenCheckType type);

    WardenCheck* GetWardenDataById(uint32 id);
    void LoadWardenChecks();
    void FillChecks(bool other, CheckIds& ids) const;

private:
    Checks _checks;
    CheckIds _memChecksPool;
    CheckIds _otherChecksPool;
};

#define sWardenCheckMgr ACE_Singleton<WardenCheckMgr, ACE_Null_Mutex>::instance()

#endif
