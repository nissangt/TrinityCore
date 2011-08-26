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

#include "Cryptography/WardenKeyGeneration.h"
#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Log.h"
#include "Opcodes.h"
#include "ByteBuffer.h"
#include <openssl/md5.h>
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "Player.h"
#include "Util.h"
#include "WardenWin.h"
#include "WardenModuleWin.h"
#include "WardenCheckMgr.h"
#include "AccountMgr.h"

void WardenWin::Init(WorldSession* session, BigNumber* k)
{
    _session = session;
    // Generate Warden Key
    SHA1Randx WK(k->AsByteArray(), k->GetNumBytes());
    WK.generate(_inputKey, 16);
    WK.generate(_outputKey, 16);

    // Seed: 4D808D2C77D905C41A6380EC08586AFE (0x05 packet)
    // Hash: 568C054C781A972A6037A2290C22B52571A06F4E (0x04 packet)
    // Module MD5: 79C0768D657977D697E10BAD956CCED1
    // New Client Key: 7F 96 EE FD A5 B6 3D 20 A4 DF 8E 00 CB F4 83 04
    // New Cerver Key: C2 B7 AD ED FC CC A9 C2 BF B3 F8 56 02 BA 80 9B
    uint8 mod_seed[16] = { 0x4D, 0x80, 0x8D, 0x2C, 0x77, 0xD9, 0x05, 0xC4, 0x1A, 0x63, 0x80, 0xEC, 0x08, 0x58, 0x6A, 0xFE };

    memcpy(_seed, mod_seed, 16);

    _inputCrypto.Init(_inputKey);
    _outputCrypto.Init(_outputKey);

    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, true, "Server side warden for client %u initializing...\n", session->GetAccountId());
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "C->S Key: %s\n", ByteArrayToHexStr(_inputKey, 16).c_str());
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "S->C Key: %s\n", ByteArrayToHexStr(_outputKey, 16).c_str());
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "Seed: %s\n", ByteArrayToHexStr(_seed, 16).c_str());
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "Loading Module...\n");
    
    _module = GetModuleForClient(_session);
    
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "Module Key: %s\n", ByteArrayToHexStr(_module->Key, 16).c_str());
    sLogMgr->Write(WARDEN_LOG, LOGL_FULL, false, "Module ID: %s\n", ByteArrayToHexStr(_module->Id, 16).c_str());

    RequestModule();
}

ClientWardenModule* WardenWin::GetModuleForClient(WorldSession* session)
{
    ClientWardenModule* mod = new ClientWardenModule();

    uint32 length = sizeof(Module_79C0768D657977D697E10BAD956CCED1_Data);

    // Data assign
    mod->CompressedSize = length;
    mod->CompressedData = new uint8[length];
    memcpy(mod->CompressedData, Module_79C0768D657977D697E10BAD956CCED1_Data, length);
    memcpy(mod->Key, Module_79C0768D657977D697E10BAD956CCED1_Key, 16);

    // md5 hash
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, mod->CompressedData, length);
    MD5_Final((uint8*)&mod->Id, &ctx);

    return mod;
}

void WardenWin::InitializeModule()
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Initialize module");

    // Create packet structure
    WardenInitModuleRequest Request;
    Request.Command1 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size1 = 20;
    Request.CheckSumm1 = BuildChecksum(&Request.Unk1, 20);
    Request.Unk1 = 1;
    Request.Unk2 = 0;
    Request.Type = 1;
    Request.String_library1 = 0;
    Request.Function1[0] = 0x00024F80;                      // 0x00400000 + 0x00024F80 SFileOpenFile
    Request.Function1[1] = 0x000218C0;                      // 0x00400000 + 0x000218C0 SFileGetFileSize
    Request.Function1[2] = 0x00022530;                      // 0x00400000 + 0x00022530 SFileReadFile
    Request.Function1[3] = 0x00022910;                      // 0x00400000 + 0x00022910 SFileCloseFile

    Request.Command2 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size2 = 8;
    Request.CheckSumm2 = BuildChecksum(&Request.Unk2, 8);
    Request.Unk3 = 4;
    Request.Unk4 = 0;
    Request.String_library2 = 0;
    Request.Function2 = 0x00419D40;                         // 0x00400000 + 0x00419D40 FrameScript::GetText
    Request.Function2_set = 1;

    Request.Command3 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size3 = 8;
    Request.CheckSumm3 = BuildChecksum(&Request.Unk5, 8);
    Request.Unk5 = 1;
    Request.Unk6 = 1;
    Request.String_library3 = 0;
    Request.Function3 = 0x0046AE20;                         // 0x00400000 + 0x0046AE20 PerformanceCounter
    Request.Function3_set = 1;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenInitModuleRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenInitModuleRequest));
    pkt.append((uint8*)&Request, sizeof(WardenInitModuleRequest));
    _session->SendPacket(&pkt);
}

void WardenWin::RequestHash()
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Request hash");

    // Create packet structure
    WardenHashRequest Request;
    Request.Command = WARDEN_SMSG_HASH_REQUEST;
    memcpy(Request.Seed, _seed, 16);

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenHashRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenHashRequest));
    pkt.append((uint8*)&Request, sizeof(WardenHashRequest));
    _session->SendPacket(&pkt);
}

void WardenWin::HandleHashResult(ByteBuffer& buff)
{
    buff.rpos(buff.wpos());

    const uint8 validHash[20] = { 0x56, 0x8C, 0x05, 0x4C, 0x78, 0x1A, 0x97, 0x2A, 0x60, 0x37, 0xA2, 0x29, 0x0C, 0x22, 0xB5, 0x25, 0x71, 0xA0, 0x6F, 0x4E };
    // Verify hash
    if (memcmp(buff.contents() + 1, validHash, sizeof(validHash)) != 0)
        sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR, "Player %s (guid: %u, account: %u, IP: %s) failed hash reply. Action: %s",
                         _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(), 
                         _session->GetRemoteAddress().c_str(), Punish().c_str());
    else
    {
        sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Request hash reply: succeed");

        // Client 7F96EEFDA5B63D20A4DF8E00CBF48304
        const uint8 client_key[16] = { 0x7F, 0x96, 0xEE, 0xFD, 0xA5, 0xB6, 0x3D, 0x20, 0xA4, 0xDF, 0x8E, 0x00, 0xCB, 0xF4, 0x83, 0x04 };

        // Server C2B7ADEDFCCCA9C2BFB3F85602BA809B
        const uint8 server_key[16] = { 0xC2, 0xB7, 0xAD, 0xED, 0xFC, 0xCC, 0xA9, 0xC2, 0xBF, 0xB3, 0xF8, 0x56, 0x02, 0xBA, 0x80, 0x9B };

        // Change keys here
        memcpy(_inputKey, client_key, 16);
        memcpy(_outputKey, server_key, 16);

        _inputCrypto.Init(_inputKey);
        _outputCrypto.Init(_outputKey);

        _initialized = true;

        _previousTimestamp = getMSTime();
    }
}

void WardenWin::RequestData()
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Request data");

    // If all checks were done, fill the todo list again
    if (_memChecksTodo.empty())
        sWardenCheckMgr->FillChecks(false, _memChecksTodo);

    if (_otherChecksTodo.empty())
        sWardenCheckMgr->FillChecks(true, _otherChecksTodo);

    _serverTicks = getMSTime();

    _currentChecks.clear();

    // Build check request (3 mem checks + 7 other checks)
    for (uint8 i = 0; i < 3; ++i)
    {
        // If todo list is done break loop (will be filled on next Update() run)
        if (_memChecksTodo.empty())
            break;

        // Get check id from the end and remove it from todo
        uint32 id = _memChecksTodo.back();
        _memChecksTodo.pop_back();

        // Add the id to the list sent in this cycle
        _currentChecks.push_back(id);
    }

    ByteBuffer buff;
    buff << uint8(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    for (uint8 i = 0; i < 7; ++i)
    {
        // If todo list is done break loop (will be filled on next Update() run)
        if (_otherChecksTodo.empty())
            break;

        // Get check id from the end and remove it from todo
        uint32 id = _otherChecksTodo.back();
        _otherChecksTodo.pop_back();

        if (WardenCheck* check = sWardenCheckMgr->GetWardenDataById(id))
        {
            // Add the id to the list sent in this cycle
            _currentChecks.push_back(id);

            check->AppendStr(buff);
        }
    }

    uint8 xorByte = _inputKey[0];

    buff << uint8(0x00);
    buff << uint8(TIMING_CHECK ^ xorByte);                  // check TIMING_CHECK

    uint8 index = 1;
    std::stringstream stream;
    stream << "Sent check id's: ";
    for (CheckIds::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
        if (WardenCheck* check = sWardenCheckMgr->GetWardenDataById(*itr))
        {
            check->FillPacket(xorByte, buff, index);
            stream << *itr << " ";
        }

    buff << uint8(xorByte);
    buff.hexlike();

    // Encrypt with warden RC4 key.
    EncryptData(const_cast<uint8*>(buff.contents()), buff.size());

    WorldPacket pkt(SMSG_WARDEN_DATA, buff.size());
    pkt.append(buff);
    _session->SendPacket(&pkt);

    _dataSent = true;

    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Player %s: %s", _session->GetPlayerName(), stream.str().c_str());
}

void WardenWin::HandleData(ByteBuffer& buff)
{
    sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Handle data");

    _dataSent = false;
    _clientResponseTimer = 0;

    uint16 len;
    buff >> len;

    uint32 checkSum;
    buff >> checkSum;

    if (!IsValidCheckSum(checkSum, buff.contents() + buff.rpos(), len))
    {
        buff.rpos(buff.wpos());
        sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR, "Player %s (guid: %u, account: %u, IP: %s) failed checksum. Action: %s",
            _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(), _session->GetRemoteAddress().c_str(), Punish().c_str());
    }
    else
    {
        // TIMING_CHECK
        uint8 res;
        buff >> res;
        // TODO: test it.
        if (res == 0x00)
        {
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR, "Player %s (guid: %u, account: %u, IP: %s) failed timing check. Action: %s",
                _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(), _session->GetRemoteAddress().c_str(), Punish().c_str());
            return;
        }

        uint32 newClientTicks;
        buff >> newClientTicks;

        uint32 ticksNow = getMSTime();
        uint32 ourTicks = newClientTicks + (ticksNow - _serverTicks);

        sLogMgr->WriteLn(WARDEN_LOG, LOGL_FULL, "Player %s: ServerTicks: %u, RequestTicks: %u, Ticks: %u, Ticks diff: %u",
                         _session->GetPlayerName(),
                         ticksNow,          // Now
                         _serverTicks,      // At request
                         newClientTicks,    // At response
                         ourTicks - newClientTicks);

        // Other checks
        bool shouldPunish = false;
        for (CheckIds::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
            if (WardenCheck* check = sWardenCheckMgr->GetWardenDataById(*itr))
                if (!check->Check(_session, buff))
                {
                    sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR,
                                     "Player %s (guid: %u, account: %u, IP: %s) failed %s check (id: %u).",
                                     _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(),
                                     _session->GetRemoteAddress().c_str(), WardenCheckMgr::GetCheckName(check->GetType()), check->GetId());
                    shouldPunish = true;
                }

        if (shouldPunish)
        {
            std::string action = Punish();
            sLogMgr->WriteLn(WARDEN_LOG, LOGL_ERROR, "Player %s (guid: %u, account: %u, IP: %s) is punished due to previous failed checks. Action: %s",
                             _session->GetPlayerName(), _session->GetGuidLow(), _session->GetAccountId(),
                             _session->GetRemoteAddress().c_str(), action.c_str());
        }
    }
}
