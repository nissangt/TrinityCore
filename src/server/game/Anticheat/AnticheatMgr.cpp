/*
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

#include "AnticheatMgr.h"
#include "AnticheatData.h"
#include "AnticheatScripts.h"
#include "MapManager.h"
#include "LogMgr.h"

#define LOG_NAME "AntiCheat"

#define CLIMB_ANGLE 1.9f

const char* CheatTypeToString(ReportTypes reportType)
{
    switch (reportType)
    {
        case SPEED_HACK_REPORT:             return "speed-hack";
        case FLY_HACK_REPORT:               return "fly-hack";
        case WALK_WATER_HACK_REPORT:        return "waterwalk-hack";
        case JUMP_HACK_REPORT:              return "jump-hack";
        case TELEPORT_PLANE_HACK_REPORT:    return "teleportplane-hack";
        case CLIMB_HACK_REPORT:             return "climb-hack";
        default:
            break;
    }
    return "<unknown-hack>";
}

AnticheatMgr::AnticheatMgr() 
{
    sLogMgr->RegisterLogFile(LOG_NAME);
}

AnticheatMgr::~AnticheatMgr()
{
    _data.clear();
}

void AnticheatMgr::JumpHackDetection(Player* player, MovementInfo movementInfo,uint32 opcode)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & JUMP_HACK_DETECTION) == 0)
        return;

    uint32 key = player->GetGUIDLow();
    if (_data[key].GetLastOpcode() == MSG_MOVE_JUMP && opcode == MSG_MOVE_JUMP)
        BuildReport(player, JUMP_HACK_REPORT);
}

void AnticheatMgr::WalkOnWaterHackDetection(Player* player, MovementInfo movementInfo)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & WALK_WATER_HACK_DETECTION) == 0)
        return;

    uint32 key = player->GetGUIDLow();
    if (!_data[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_WATERWALKING))
        return;

    // if we are a ghost we can walk on water
    if (!player->isAlive())
        return;

    if (player->HasAuraType(SPELL_AURA_FEATHER_FALL) ||
        player->HasAuraType(SPELL_AURA_SAFE_FALL) ||
        player->HasAuraType(SPELL_AURA_WATER_WALK))
        return;

    BuildReport(player, WALK_WATER_HACK_REPORT);
}

void AnticheatMgr::FlyHackDetection(Player* player, MovementInfo movementInfo)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & FLY_HACK_DETECTION) == 0)
        return;

    uint32 key = player->GetGUIDLow();
    if (!_data[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_FLYING))
        return;
    
    if (player->HasAuraType(SPELL_AURA_FLY) ||
        player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) ||
        player->HasAuraType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED))
        return;
    
    BuildReport(player, FLY_HACK_REPORT);
}

void AnticheatMgr::TeleportPlaneHackDetection(Player* player, MovementInfo movementInfo)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & TELEPORT_PLANE_HACK_DETECTION) == 0)
        return;

    uint32 key = player->GetGUIDLow();
    if (_data[key].GetLastMovementInfo().pos.GetPositionZ() != 0 ||
        movementInfo.pos.GetPositionZ() != 0)
        return;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING))
        return;

    if (player->getDeathState() == DEAD_FALLING)
        return;

    float x, y, z;
    player->GetPosition(x, y, z);
    float ground_Z = player->GetMap()->GetHeight(x, y, z);
    float z_diff = fabs(ground_Z - z);   

    // we are not really walking there
    if (z_diff > 1.0f)
        BuildReport(player, TELEPORT_PLANE_HACK_REPORT);
}

// basic detection
void AnticheatMgr::ClimbHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & CLIMB_HACK_DETECTION) == 0)
        return;
    
    uint32 key = player->GetGUIDLow();
    if (opcode != MSG_MOVE_HEARTBEAT ||
        _data[key].GetLastOpcode() != MSG_MOVE_HEARTBEAT)
        return;
    
    // in this case we don't care if they are "legal" flags, they are handled in another parts of the Anticheat Manager.
    if (player->IsInWater() || 
        player->IsFlying() || 
        player->IsFalling())
        return;
    
    Position playerPos;
    player->GetPosition(&playerPos);
    
    float deltaZ = fabs(playerPos.GetPositionZ() - movementInfo.pos.GetPositionZ());
    float deltaXY = movementInfo.pos.GetExactDist2d(&playerPos);
    float angle = MapManager::NormalizeOrientation(tan(deltaZ / deltaXY));
    if (angle > CLIMB_ANGLE)
        BuildReport(player, CLIMB_HACK_REPORT);
}

void AnticheatMgr::SpeedHackDetection(Player* player,MovementInfo movementInfo)
{
    if ((sWorld->getIntConfig(CONFIG_ANTICHEAT_DETECTIONS_ENABLED) & SPEED_HACK_DETECTION) == 0)
        return;
    
    uint32 key = player->GetGUIDLow();
    // We also must check the map because the movementFlag can be modified by the client.
    // If we just check the flag, they could always add that flag and always skip the speed hacking detection.
    // 369 == DEEPRUN TRAM
    if (_data[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT) && player->GetMapId() == 369)
        return;
    
    uint32 distance2D(movementInfo.pos.GetExactDist2d(&_data[key].GetLastMovementInfo().pos));
    uint8 moveType = 0;
    // we need to know HOW is the player moving
    // TO-DO: Should we check the incoming movement flags?
    if (player->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
        moveType = MOVE_SWIM;
    else if (player->IsFlying())
        moveType = MOVE_FLIGHT;
    else if (player->HasUnitMovementFlag(MOVEMENTFLAG_WALKING))
        moveType = MOVE_WALK;
    else
        moveType = MOVE_RUN;
    
    // how many yards the player can do in one sec.
    uint32 speedRate = uint32(player->GetSpeed(UnitMoveType(moveType)) + movementInfo.j_xyspeed);
    
    // how long the player took to move to here.
    uint32 timeDiff = getMSTimeDiff(_data[key].GetLastMovementInfo().time, movementInfo.time);
    if (!timeDiff)
        timeDiff = 1;
    
    // this is the distance doable by the player in 1 sec, using the time done to move to this point.
    uint32 clientSpeedRate = distance2D * 1000 / timeDiff;
    
    // we did the (uint32) cast to accept a margin of tolerance
    if (clientSpeedRate > speedRate)
        BuildReport(player, SPEED_HACK_REPORT);
}

void AnticheatMgr::StartHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (!sWorld->getBoolConfig(CONFIG_ANTICHEAT_ENABLE))
        return;

    if (player->isGameMaster())
        return;

    if (!player->isInFlight() && !player->GetTransport() && !player->GetVehicle())
    {
        SpeedHackDetection(player, movementInfo);
        FlyHackDetection(player, movementInfo);
        WalkOnWaterHackDetection(player, movementInfo);
        JumpHackDetection(player, movementInfo, opcode);
        TeleportPlaneHackDetection(player, movementInfo);
        ClimbHackDetection(player, movementInfo, opcode);
    }
    uint32 key = player->GetGUIDLow();
    _data[key].SetLastMovementInfo(movementInfo);
    _data[key].SetLastOpcode(opcode);
}

void AnticheatMgr::StartScripts()
{
    new AnticheatScripts();
}

void AnticheatMgr::HandlePlayerLogin(Player* player)
{
    uint32 guidLow = player->GetGUIDLow();
    // we must delete this to prevent errors in case of crash
    CharacterDatabase.PExecute("DELETE FROM players_reports_status WHERE guid = %u", guidLow);
    // we initialize the pos of lastMovementPosition var.
    _data[guidLow].SetPosition(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation());
    if (QueryResult result = CharacterDatabase.PQuery("SELECT * FROM daily_players_reports WHERE guid = %u", guidLow))
        _data[guidLow].SetDailyReportState(true);
}

void AnticheatMgr::HandlePlayerLogout(Player* player)
{
    // TO-DO Make a table that stores the cheaters of the day, with more detailed information.
    // We must also delete it at logout to prevent have data of offline players in the db when we query the database (IE: The GM Command)
    CharacterDatabase.PExecute("DELETE FROM players_reports_status WHERE guid = %u", player->GetGUIDLow());
    // Delete not needed data from the memory.
    _data.erase(player->GetGUIDLow());
}

void AnticheatMgr::SavePlayerData(Player* player)
{
    uint32 guidLow = player->GetGUIDLow();
    _data[guidLow].SaveToDB("players_reports_status", guidLow);
}

uint32 AnticheatMgr::GetTotalReports(uint32 guidLow)
{
    return _data[guidLow].GetTotalReports();
}

float AnticheatMgr::GetAverage(uint32 guidLow)
{
    return _data[guidLow].GetAverage();
}

uint32 AnticheatMgr::GetTypeReports(uint32 guidLow, uint8 type)
{
    return _data[guidLow].GetTypeReports(type);
}

bool AnticheatMgr::MustCheckTempReports(uint8 type)
{
    if (type == JUMP_HACK_REPORT)
        return false;

    return true;
}

void AnticheatMgr::BuildReport(Player* player, ReportTypes reportType)
{
    uint32 actualTime = getMSTime();

    uint32 key = player->GetGUIDLow();
    AnticheatData& data = _data[key];
    if (MustCheckTempReports(reportType))
    {
        if (!data.GetTempReportsTimer(reportType))
            data.SetTempReportsTimer(reportType, actualTime);

        if (getMSTimeDiff(data.GetTempReportsTimer(reportType), actualTime) < 3000)
        {
            data.SetTempReports(reportType, data.GetTempReports(reportType) + 1);
            if (data.GetTempReports(reportType) < 3)
                return;
        }
        else
        {
            data.SetTempReportsTimer(reportType, actualTime);
            data.SetTempReports(reportType, 1);
            return;
        }
    }

    // generating creationTime for average calculation
    if (!data.GetTotalReports())
        data.SetCreationTime(actualTime);
    
    // increasing total_reports
    data.SetTotalReports(data.GetTotalReports() + 1);
    // increasing specific cheat report
    data.SetTypeReports(reportType, data.GetTypeReports(reportType) + 1);

    // diff time for average calculation
    uint32 diffTime = getMSTimeDiff(data.GetCreationTime(), getMSTime()) / IN_MILLISECONDS;
    if (diffTime > 0)  
    {
        // Average == Reports per second
        float average = float(data.GetTotalReports()) / float(diffTime);
        data.SetAverage(average);
    }

    if (data.GetTotalReports() > sWorld->getIntConfig(CONFIG_ANTICHEAT_MAX_REPORTS_FOR_DAILY_REPORT))
    {
        if (!data.GetDailyReportState())
        {
            data.SaveToDB("daily_players_reports", key);
            data.SetDailyReportState(true);
        }
    }

    if (data.GetTotalReports() > sWorld->getIntConfig(CONFIG_ANTICHEAT_REPORTS_INGAME_NOTIFICATION))
    {
        std::ostringstream ss;
        ss << "|cFFFFFC00[AC]|cFF00FFFF[|cFF60FF00";
        ss << player->GetName();
        ss << "|cFF00FFFF] Possible cheater: ";
        ss << CheatTypeToString(reportType);

        // Display warning at the center of the screen
        std::string str = ss.str();
        WorldPacket data(SMSG_NOTIFICATION, (str.size() + 1));
        data << str;
        sWorld->SendGlobalGMMessage(&data);
    }
    // Write information to log file
    sLogMgr->Write(LOG_NAME, "[%u] %s (account: %u): %s", player->GetGUIDLow(), player->GetName(), player->GetSession()->GetAccountId(), CheatTypeToString(reportType));
}

void AnticheatMgr::AnticheatGlobalCommand(ChatHandler* handler)
{
    // MySQL will sort all for us, anyway this is not the best way we must only save the anticheat data not whole player's data!.
    sObjectAccessor->SaveAllPlayers();

    QueryResult result = CharacterDatabase.Query("SELECT guid,average,total_reports FROM players_reports_status WHERE total_reports != 0 ORDER BY average ASC LIMIT 3;");
    if (!result)
        handler->PSendSysMessage("No players found.");
    else
    {
        handler->SendSysMessage("=================================");
        handler->SendSysMessage("Players with the lowest averages:");
        do
        {
            Field* fields = result->Fetch();
     
            uint32 guid = fields[0].GetUInt32();
            float average = fields[1].GetFloat();
            uint32 total_reports = fields[2].GetUInt32();

            if (Player* player = sObjectMgr->GetPlayerByLowGUID(guid))
                handler->PSendSysMessage("Player: %s Average: %f Total Reports: %u", player->GetName(), average, total_reports);

        } while (result->NextRow());

        result = CharacterDatabase.Query("SELECT guid,average,total_reports FROM players_reports_status WHERE total_reports != 0 ORDER BY total_reports DESC LIMIT 3;");
        if (!result)
            handler->SendSysMessage("No players found.");
        else
        {
            handler->SendSysMessage("==============================");
            handler->SendSysMessage("Players with the most reports:");
            do
            {
                Field *fields = result->Fetch();
         
                uint32 guid = fields[0].GetUInt32();
                float average = fields[1].GetFloat();
                uint32 total_reports = fields[2].GetUInt32();

                if (Player* player = sObjectMgr->GetPlayerByLowGUID(guid))
                    handler->PSendSysMessage("Player: %s Total Reports: %u Average: %f", player->GetName(), total_reports, average);

            } while (result->NextRow());
        }
    }
}

void AnticheatMgr::AnticheatDeleteCommand(uint32 guid)
{
    if (!guid)
    {
        for (AnticheatPlayersDataMap::iterator itr = _data.begin(); itr != _data.end(); ++itr)
            itr->second.Reset();
        CharacterDatabase.Execute("DELETE FROM players_reports_status");
    }
    else
    {
        _data[guid].Reset();
        CharacterDatabase.PExecute("DELETE FROM players_reports_status WHERE guid = %u", guid);
    }
}

void AnticheatMgr::ResetDailyReportStates()
{
     for (AnticheatPlayersDataMap::iterator itr = _data.begin(); itr != _data.end(); ++itr)
         _data[itr->first].SetDailyReportState(false);
}
