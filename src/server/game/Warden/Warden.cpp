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

#include <openssl/md5.h>
#include <openssl/sha.h>

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "LogMgr.h"
#include "Opcodes.h"
#include "ByteBuffer.h"
#include "World.h"
#include "Player.h"
#include "Util.h"
#include "Warden.h"
#include "AccountMgr.h"
#include "WardenWin.h"
#include "WardenMac.h"

// static
Warden* Warden::GetWarden(const std::string& os, WorldSession* session, BigNumber* k)
{
    Warden* res = NULL;
    if (os == "Win")
        res = new WardenWin();
    // Disabled as it is causing the client to crash
    // else if (os == "OSX")
    //     res = new WardenMac();
    if (res)
        res->Init(session, k);
    return res;
}

// static
bool Warden::IsValidCheckSum(uint32 checksum, const uint8* data, const uint16 length)
{
    uint32 newChecksum = BuildChecksum(data, length);
    bool isValid = (checksum == newChecksum);
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, isValid ? "CHECKSUM IS VALID" : "CHECKSUM IS NOT VALID");
    return isValid;
}

// static
uint32 Warden::BuildChecksum(const uint8* data, uint32 length)
{
    uint8 hash[20];
    SHA1(data, length, hash);
    uint32 checkSum = 0;
    for (uint8 i = 0; i < 5; ++i)
        checkSum ^= *(uint32*)(&hash[0] + i * 4);
    
    return checkSum;
}

Warden::Warden() : _session(NULL), _inputCrypto(16), _outputCrypto(16), _checkTimer(10000), 
_dataSent(false), _clientResponseTimer(0), _previousTimestamp(0), _module(NULL), _initialized(false),
_maxClientResponse(sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_RESPONSE_DELAY))
{
}

Warden::~Warden()
{
    if (_module)
    {
        delete _module;
        _module = NULL;
    }
    _initialized = false;
}

void Warden::SendModuleToClient()
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Send module to client");

    // Create packet structure
    WardenModuleTransfer packet;

    uint32 sizeLeft = _module->CompressedSize;
    uint32 pos = 0;
    while (sizeLeft > 0)
    {
        uint16 burstSize = std::min(sizeLeft, 500U);
        packet.Command = WARDEN_SMSG_MODULE_CACHE;
        packet.DataSize = burstSize;
        memcpy(packet.Data, &_module->CompressedData[pos], burstSize);
        sizeLeft -= burstSize;
        pos += burstSize;

        EncryptData((uint8*)&packet, burstSize + 3);
        WorldPacket pkt1(SMSG_WARDEN_DATA, burstSize + 3);
        pkt1.append((uint8*)&packet, burstSize + 3);
        _session->SendPacket(&pkt1);
    }
}

void Warden::RequestModule()
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Request module");

    // Create packet structure
    WardenModuleUse request;
    request.Command = WARDEN_SMSG_MODULE_USE;

    memcpy(request.ModuleId, _module->Id, 16);
    memcpy(request.ModuleKey, _module->Key, 16);
    request.Size = _module->CompressedSize;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&request, sizeof(WardenModuleUse));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenModuleUse));
    pkt.append((uint8*)&request, sizeof(WardenModuleUse));
    _session->SendPacket(&pkt);
}

void Warden::Update()
{
    if (_initialized)
    {
        uint32 ticks = getMSTime();
        uint32 diff = abs(int32(ticks) - int32(_previousTimestamp));
        _previousTimestamp = ticks;

        if (_dataSent)
        {
            if (_maxClientResponse)
            {
                // Punish player if client response delays more than set in config
                if (_clientResponseTimer > _maxClientResponse * IN_MILLISECONDS)
                {
                    sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR,
                                     "Player %s (guid: %u, account: %u, IP: %s) exceeded module response delay (%u ms, latency: %u). Action: %s",
                                     _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(), _session->GetRemoteAddress().c_str(),
                                     _clientResponseTimer, _session->GetLatency(), Punish().c_str());
                    _clientResponseTimer = 0;
                }
                else
                    _clientResponseTimer += diff;
            }
        }
        else if (_checkTimer > 0)
        {
            if (diff >= _checkTimer)
            {
                RequestData();

                uint32 period = sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_CHECK_PERIOD);
                _checkTimer = irand(period - 5, period + 5) * IN_MILLISECONDS;
            }
            else
                _checkTimer -= diff;
        }
    }
}

void Warden::DecryptData(uint8* buffer, uint32 length)
{
    _inputCrypto.UpdateData(length, buffer);
}

void Warden::EncryptData(uint8* buffer, uint32 length)
{
    _outputCrypto.UpdateData(length, buffer);
}

std::string Warden::Punish() const
{
    uint32 action = sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_FAIL_ACTION);
    switch (action)
    {
        case 0:
            return "None";
        case 1:
            _session->KickPlayer();
            return "Kick";
        case 2:
            {
                std::stringstream duration;
                duration << sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_BAN_DURATION) << "s";
                std::string accountName;
                sAccountMgr->GetName(_session->GetAccountId(), accountName);
                sWorld->BanAccount(BAN_ACCOUNT, accountName, duration.str(), "Warden Anticheat violation", "Server");
            }
            return "Ban";
        default:
            return "Undefined";
    }
}

void WorldSession::HandleWardenDataOpcode(WorldPacket& recvData)
{
    _warden->DecryptData(const_cast<uint8*>(recvData.contents()), recvData.size());

    uint8 opcode;
    recvData >> opcode;
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Got packet, opcode %02X, size %u", opcode, uint32(recvData.size()));
    recvData.hexlike();

    switch (opcode)
    {
        case WARDEN_CMSG_MODULE_MISSING:
            _warden->SendModuleToClient();
            break;
        case WARDEN_CMSG_MODULE_OK:
            _warden->RequestHash();
            break;
        case WARDEN_CMSG_CHEAT_CHECKS_RESULT:
            _warden->HandleData(recvData);
            break;
        case WARDEN_CMSG_MEM_CHECKS_RESULT:
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "NYI WARDEN_CMSG_MEM_CHECKS_RESULT received!");
            break;
        case WARDEN_CMSG_HASH_RESULT:
            _warden->HandleHashResult(recvData);
            _warden->InitializeModule();
            break;
        case WARDEN_CMSG_MODULE_FAILED:
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "NYI WARDEN_CMSG_MODULE_FAILED received!");
            break;
        default:
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_WARNING, "Got unknown warden opcode %02X of size %u.", opcode, uint32(recvData.size() - 1));
            break;
    }
}
