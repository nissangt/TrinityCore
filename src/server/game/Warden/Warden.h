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

#ifndef _WARDEN_BASE_H
#define _WARDEN_BASE_H

#include <map>
#include "Cryptography/ARC4.h"
#include "Cryptography/BigNumber.h"
#include "ByteBuffer.h"

#define WARDEN_LOG "Warden"

enum WardenOpcodes
{
    // Client->Server
    WARDEN_CMSG_MODULE_MISSING                  = 0,
    WARDEN_CMSG_MODULE_OK                       = 1,
    WARDEN_CMSG_CHEAT_CHECKS_RESULT             = 2,
    WARDEN_CMSG_MEM_CHECKS_RESULT               = 3,        // only sent if MEM_CHECK bytes doesn't match
    WARDEN_CMSG_HASH_RESULT                     = 4,
    WARDEN_CMSG_MODULE_FAILED                   = 5,        // this is sent when client failed to load uploaded module due to cache fail

    // Server->Client
    WARDEN_SMSG_MODULE_USE                      = 0,
    WARDEN_SMSG_MODULE_CACHE                    = 1,
    WARDEN_SMSG_CHEAT_CHECKS_REQUEST            = 2,
    WARDEN_SMSG_MODULE_INITIALIZE               = 3,
    WARDEN_SMSG_MEM_CHECKS_REQUEST              = 4,        // byte len; while(!EOF) { byte unk(1); byte index(++); string module(can be 0); int offset; byte len; byte[] bytes_to_compare[len]; }
    WARDEN_SMSG_HASH_REQUEST                    = 5
};

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct WardenModuleUse
{
    uint8 Command;
    uint8 ModuleId[16];
    uint8 ModuleKey[16];
    uint32 Size;
};

struct WardenModuleTransfer
{
    uint8 Command;
    uint16 DataSize;
    uint8 Data[500];
};

struct WardenHashRequest
{
    uint8 Command;
    uint8 Seed[16];
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

struct ClientWardenModule
{
    uint8 Id[16];
    uint8 Key[16];
    uint32 CompressedSize;
    uint8 *CompressedData;

    ~ClientWardenModule()
    {
        if (CompressedData)
            delete[] CompressedData;
    }
};

class WorldSession;

class Warden
{
public:
    Warden();
    virtual ~Warden();

    virtual void Init(WorldSession* session, BigNumber* k) = 0;
    virtual ClientWardenModule* GetModuleForClient(WorldSession* session) = 0;
    virtual void InitializeModule() = 0;
    virtual void RequestHash() = 0;
    virtual void HandleHashResult(ByteBuffer &buff) = 0;
    virtual void RequestData() = 0;
    virtual void HandleData(ByteBuffer &buff) = 0;

    void SendModuleToClient();
    void RequestModule();
    void Update();
    void DecryptData(uint8* buffer, uint32 length);
    void EncryptData(uint8* buffer, uint32 length);

    static bool IsValidCheckSum(uint32 checksum, const uint8 *data, const uint16 length);
    static uint32 BuildChecksum(const uint8 *data, uint32 length);
    static Warden* GetWarden(const std::string& os, WorldSession* session, BigNumber* k);

    std::string Punish() const;

protected:
    WorldSession* _session;
    uint8 _inputKey[16];
    uint8 _outputKey[16];
    uint8 _seed[16];
    ARC4 _inputCrypto;
    ARC4 _outputCrypto;
    uint32 _checkTimer;                          // Timer for sending check requests
    bool _dataSent;
    uint32 _clientResponseTimer;                 // Timer for client response delay
    uint32 _previousTimestamp;
    ClientWardenModule* _module;
    bool _initialized;
    uint32 _maxClientResponse;
};

#endif
