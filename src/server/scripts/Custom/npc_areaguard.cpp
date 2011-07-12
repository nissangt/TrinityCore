/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
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

#include "ScriptPCH.h"
#include "npc_areaguard.h"
#include "LogMgr.h"
#include "GuildMgr.h"

#define MAX_RADIUS 100.0f

void GuardInfo::SaveToDB() const
{
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_ADD_NPCGUARD_TEMPLATE);
    stmt->setUInt32(0, _entry);
    stmt->setUInt8 (1, uint8(_type));
    stmt->setUInt32(2, _value);
    stmt->setFloat (3, _distance);
    stmt->setUInt32(4, _teleId);
    stmt->setString(5, _comment);
    WorldDatabase.Execute(stmt);
}

bool GuardInfo::LoadFromDB(Field* fields)
{
    _entry      = fields[0].GetUInt32();
    _type       = GuardType(fields[1].GetUInt8());
    _value      = fields[2].GetUInt32();
    _distance   = fields[3].GetFloat();
    _teleId     = fields[4].GetUInt32();
    _comment    = fields[5].GetString();
    
    if (!sObjectMgr->GetGameTele(_teleId))
    {
        sLog->outError("GUARD: area guard (entry: %u) has invalid teleport id (%u)", _entry, _teleId);
        return false;
    }
    if (_distance < 0.0f || _distance > MAX_RADIUS)
    {
        sLog->outError("GUARD: area guard (entry: %u) has invalid interaction radius (%.1f), it will be set to %.1f",
                       _entry, _distance, MAX_RADIUS);
        _distance = MAX_RADIUS;
    }
    return true;
}

const char* divider = "|----------|-----|------------------|--------------------------|----------------------------------|";
// |---------------|--------------|---------------|----------------|---------------|
// | entry (max 8) | dist (max 3) | type (max 16) | value (max 24) | tele (max 32) |
// |---------------|--------------|---------------|----------------|---------------|
void GuardInfo::SendMessage(ChatHandler* handler, bool showHeader) const
{
    std::string sType;
    std::string sValue;
    switch (_type)
    {
        case NPCG_ALL:
            sType = handler->GetTrinityString(LANG_GUARD_INFO_ALL);
            break;
        case NPCG_TEAM:
            sType = handler->GetTrinityString(_value == TEAM_ALLIANCE ? LANG_GUARD_INFO_TEAM_ALLIANCE : LANG_GUARD_INFO_TEAM_HORDE);
            break;
        case NPCG_SECURITY:
            sType = handler->GetTrinityString(LANG_GUARD_INFO_SECURITY);
            sValue = _value;
            break;
        case NPCG_LEVEL:
            sType = handler->GetTrinityString(LANG_GUARD_INFO_LEVEL);
            sValue = _value;
            break;
        case NPCG_GUILD:
        {
            sType = handler->GetTrinityString(LANG_GUARD_INFO_GUILD);
            Guild const* guild = sGuildMgr->GetGuildById(_value);
            sValue = (guild ? guild->GetName() : "<unknown>");
            break;
        }
        case NPCG_GUID:
        {
            if (!sObjectMgr->GetPlayerNameByGUID(_value, sValue))
                sValue = "<unknown>";
            sValue += " (";
            sValue += _value;
            sValue += ")";
            sType = handler->GetTrinityString(LANG_GUARD_INFO_GUID);
            break;
        }
        case NPCG_NONE:
            break;
    }
    GameTele const* tele = sObjectMgr->GetGameTele(_teleId);
    char str[255];
    sprintf(str, "| %8u | %3.0f | %16s | %24s | %32s |",
            _entry, _distance, sType.c_str(), sValue.c_str(), (tele ? tele->name.c_str() : "<unknown>"));

    if (showHeader)
    {
        handler->SendSysMessage(divider);
        handler->SendSysMessage(LANG_GUARD_HEADER);
        handler->SendSysMessage(divider);
    }
    handler->SendSysMessage(str);
    if (showHeader)
        handler->SendSysMessage(divider);
}

void GuardInfo::SetType(GuardType type, uint32 value)
{
    _type = type;
    _value = value;
    SaveToDB();
}

void GuardInfo::SetTele(uint32 teleId)
{
    _teleId = teleId;
    SaveToDB();
}

void GuardInfo::SetDistance(float distance)
{
    _distance = distance;
    SaveToDB();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Guard Manager

GuardMgr::GuardMgr() { }

GuardMgr::~GuardMgr() { }

void GuardMgr::LoadGuardTemplates()
{
    uint32 oldMSTime = getMSTime();

    _maxEntry = 0;
    _guardMap.clear();

    PreparedQueryResult result = WorldDatabase.Query(WorldDatabase.GetPreparedStatement(WORLD_LOAD_NPCGUARD_TEMPLATES));
    if (!result)
    {
        sLog->outErrorDb(">> Loaded 0 area guard templates. DB table `npc_areaguard_template` is empty!");
        sLog->outString();
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        GuardInfo gi;
        if (!gi.LoadFromDB(fields))
            continue;

        uint32 entry = gi.GetEntry();
        if (_maxEntry < entry)
            _maxEntry = entry;

        _guardMap[entry] = gi;

        ++count;
    }
    while (result->NextRow());

    sLog->outString(">> Loaded %u area guard templates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    sLog->outString();
}

void GuardMgr::LoadGuards()
{
    uint32 oldMSTime = getMSTime();

    _guards.clear();

    PreparedQueryResult result = WorldDatabase.Query(WorldDatabase.GetPreparedStatement(WORLD_LOAD_NPCGUARDS));
    if (!result)
    {
        sLog->outErrorDb(">> Loaded 0 area guards. DB table `npc_areaguard` is empty!");
        sLog->outString();
        return;
    }

    uint32 count = 0;
    do
    {
        uint32 guid = (*result)[0].GetUInt32();
        uint32 guardEntry = (*result)[1].GetUInt32();
        _guards[guid] = guardEntry;

        ++count;
    }
    while (result->NextRow());
    
    sLog->outString(">> Loaded %u area guards in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
    sLog->outString();
}

GuardInfo* GuardMgr::GetInfo(uint32 lowGuid)
{
    Guards::iterator itr = _guards.find(lowGuid);
    if (itr != _guards.end())
        return GetInfoByEntry(itr->second);
    return NULL;
}

GuardInfo* GuardMgr::GetInfoByEntry(uint32 entry)
{
    GuardInfoMap::iterator itr = _guardMap.find(entry);
    if (itr != _guardMap.end())
        return &itr->second;
    return NULL;    
}

void GuardMgr::Link(GuardInfo const* info, uint32 lowGuid)
{
    _guards[lowGuid] = info->GetEntry();
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_ADD_NPCGUARD_TEMPLATE);
    stmt->setUInt32(0, lowGuid);
    stmt->setUInt32(1, info->GetEntry());
    WorldDatabase.Execute(stmt);
}

void GuardMgr::Unlink(uint32 lowGuid)
{
    Guards::iterator itr = _guards.find(lowGuid);
    if (itr != _guards.end())
    {
        _guards.erase(lowGuid);
        PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_NPCGUARD_BY_GUID);
        stmt->setUInt32(0, lowGuid);
        WorldDatabase.Execute(stmt);
    }
}

void GuardMgr::Delete(GuardInfo const* info)
{
    uint32 entry = info->GetEntry();
    // Clean collections
    // TODO: Check removal
    for (Guards::iterator itr = _guards.begin(); itr != _guards.end(); ++itr)
        if (itr->second == entry)
            _guards.erase(itr);
    _guardMap.erase(entry);
    // Clean DB
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_NPCGUARD);
    stmt->setUInt32(0, entry);
    WorldDatabase.Execute(stmt);
    stmt = WorldDatabase.GetPreparedStatement(WORLD_DEL_NPCGUARD_TEMPLATE);
    stmt->setUInt32(0, entry);
    WorldDatabase.Execute(stmt);
}

uint32 GuardMgr::Create(GuardType type, uint32 value, float distance, GameTele const* tele, const std::string& comment)
{
    GuardInfo info(++_maxEntry, type, value, distance, tele->id, comment);
    info.SaveToDB();
    _guardMap[info.GetEntry()] = info;
    return info.GetEntry();
}

void GuardMgr::List(ChatHandler* handler, GuardType type) const
{
    handler->SendSysMessage(divider);
    handler->SendSysMessage(LANG_GUARD_HEADER);
    handler->SendSysMessage(divider);
    for (GuardInfoMap::const_iterator itr = _guardMap.begin(); itr != _guardMap.end(); ++itr)
        if (type == NPCG_NONE || itr->second.GetType() == type)
            itr->second.SendMessage(handler, false);
    handler->SendSysMessage(divider);
}

class npc_areaguard : public CreatureScript
{
public:
    npc_areaguard() : CreatureScript("npc_areaguard") { }

    struct npc_areaguardAI : public Scripted_NoMovementAI
    {
        npc_areaguardAI(Creature* creature) : Scripted_NoMovementAI(creature)
        {
            _info = sGuardMgr->GetInfo(me->GetGUIDLow());
            if (!_info)
            {
                sLog->outError("GUARD: creature (GUID: %u, entry: %u) has 'npc_areaguard' script, but no template is assigned to it",
                               me->GetGUIDLow(), me->GetEntry());
                return;
            }

            _tele = sObjectMgr->GetGameTele(_info->GetTeleId());
            if (!_tele)
            {
                sLog->outError("GUARD: given game teleport (ID: %u) not found!", _info->GetTeleId());
                return;
            }

            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_NORMAL, true);
            me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_MAGIC, true);
        }

        void Reset() { }

        void Aggro(Unit* /*who*/) { }

        void AttackStart(Unit* /*who*/) { }

        void MoveInLineOfSight(Unit* who)
        {
            if (!_info || !_tele)
                return;

            if (!who || !who->IsInWorld())
                return;

            // Return if distance is greater than guard distance
            if (!me->IsWithinDist(who, _info->GetDistance(), false))
                return;

            Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();
            // Return if player has GM flag on or is in process of teleport
            if (!player || player->isGameMaster() || player->IsBeingTeleported())
                return;

            bool teleport = false;
            uint32 value = _info->GetValue();
            switch (_info->GetType())
            {
                // Action on all players without GM flag on
                case NPCG_ALL:
                    teleport = true;
                    break;
                case NPCG_TEAM:
                    // Action based on Team
                    // 0 - Alliance
                    // 1 - Horde
                    // 2 - Neutral
                    teleport = (player->GetTeamId() != TeamId(value));
                    break;
                case NPCG_SECURITY:
                    // Action based on GM Level
                    teleport = (player->GetSession()->GetSecurity() < AccountTypes(value));
                    break;
                case NPCG_LEVEL:
                    // Action based on Player Level
                    teleport = (player->getLevel() < value);
                    break;
                case NPCG_GUILD:
                    // Action based on Guild ID
                    teleport = (player->GetGuildId() != value);
                    break;
                case NPCG_GUID:
                    // Action based on Player GUID
                    teleport = (player->GetGUID() != value);
                    break;
                case NPCG_NONE:
                    break;
            }

            if (teleport)
                player->TeleportTo(_tele->mapId, _tele->position_x, _tele->position_y, _tele->position_z, _tele->orientation);

            float x, y, z, o;
            me->GetHomePosition(x, y, z, o);
            me->SetOrientation(o);
        }

        void UpdateAI(const uint32 /*diff*/) { }

        private:
            GuardInfo const* _info;
            GameTele const* _tele;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_areaguardAI(creature);
    }
};

class GuardCommandScript : public CommandScript
{
public:
    GuardCommandScript() : CommandScript("GuardCommandScript") { }
    
    ChatCommand* GetCommands() const
    {
        static ChatCommand modCommandTable[] =
        {
            { "distance",       SEC_GAMEMASTER,     true,  &HandleGuardSetDistanceCommand,  "", NULL },
            { "tele",           SEC_GAMEMASTER,     true,  &HandleGuardSetTeleCommand,      "", NULL },
            { "type",           SEC_GAMEMASTER,     true,  &HandleGuardSetTypeCommand,      "", NULL },
            { NULL,             0,                  false, NULL,                            "", NULL }
        };
        static ChatCommand guardCommandTable[] =
        {
            { "info",           SEC_GAMEMASTER,     false, &HandleGuardInfoCommand,         "", NULL },
            { "link",           SEC_GAMEMASTER,     false, &HandleGuardLinkCommand,         "", NULL },
            { "unlink",         SEC_GAMEMASTER,     false, &HandleGuardUnlinkCommand,       "", NULL },
            { "list",           SEC_GAMEMASTER,     true,  &HandleGuardListCommand,         "", NULL },
            { "add",            SEC_GAMEMASTER,     true,  &HandleGuardAddCommand,          "", NULL },
            { "del",            SEC_GAMEMASTER,     true,  &HandleGuardDelCommand,          "", NULL },
            { "set",            SEC_GAMEMASTER,     true,  NULL,                            "", modCommandTable },
            { NULL,             0,                  false, NULL,                            "", NULL }
        };
        static ChatCommand commandTable[] =
        {
            { "guard",          SEC_GAMEMASTER,     false, NULL,               "", guardCommandTable },
            { NULL,             0,                  false, NULL,               "", NULL              }
        };
        return commandTable;
    }
    
    // .guard info <guid>/<selected creature>
    static bool HandleGuardInfoCommand(ChatHandler* handler, const char* args)
    {
        uint32 lowGuid = _ExtractCreatureGuid(handler, (char*)args);
        if (!lowGuid)
            return false;

        if (GuardInfo const* gi = sGuardMgr->GetInfo(lowGuid))
            gi->SendMessage(handler, true);
        else
            handler->PSendSysMessage(LANG_GUARD_NO_INFO, lowGuid);

        return true;
    }

    // .guard link <guard entry> <guid>/<selected creature>
    static bool HandleGuardLinkCommand(ChatHandler* handler, const char* args)
    {
        if (!args)
            return false;

        // Extract entry (1 param)
        char* sEntry = strtok((char*)args, " ");
        uint32 entry(atoi(sEntry));
        GuardInfo const* gi = sGuardMgr->GetInfoByEntry(entry);
        if (!gi)
        {
            handler->PSendSysMessage(LANG_GUARD_NO_TEMPLATE, entry);
            handler->SetSentErrorMessage(true);
            return false;
        }
        
        // Extract creature entry (2 param or selected creature)
        char* sGuid = strtok(NULL, " ");
        uint32 lowGuid = _ExtractCreatureGuid(handler, sGuid);
        if (!lowGuid)
            return false;

        // TODO: Check if creature can be npc_areaguard
        // Link
        sGuardMgr->Link(gi, lowGuid);
        return true;
    }

    // .guard unlink <guid>/<selected creature>
    static bool HandleGuardUnlinkCommand(ChatHandler* handler, const char* args)
    {
        // Extract guid (or take it from selected creature)
        uint32 lowGuid = _ExtractCreatureGuid(handler, (char*)args);
        if (!lowGuid)
            return false;

        // Unlink
        if (sGuardMgr->GetInfo(lowGuid))
            sGuardMgr->Unlink(lowGuid);
        else
            handler->PSendSysMessage(LANG_GUARD_NO_INFO, lowGuid);
        return true;
    }

    // Show list of available guard templates
    // .guard list
    static bool HandleGuardListCommand(ChatHandler* handler, const char* args)
    {
        char* sType = strtok((char*)args, " ");
        sGuardMgr->List(handler, sType ? GuardType(atoi(sType)) : NPCG_NONE);
        return true;
    }

    // Create new guard template
    // .guard add <type> <value> <distance> <tele> [<comment>]
    static bool HandleGuardAddCommand(ChatHandler* handler, const char* args)
    {
        // 1. Type
        char* sType = strtok((char*)args, " ");
        // 2. Value
        char* sValue = strtok(NULL, " ");
        if (!sType || !sValue)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;            
        }
        GuardType type = GuardType(atoi(sType));
        uint32 value(atoi(sValue));
        if (!_ValidateTypeValue(handler, type, value))
            return false;

        // 3. Distance
        float distance = 0.0f;
        char* sDistance = strtok(NULL, " ");
        if (sDistance)
            distance = float(atoi(sDistance));
        if (!distance || distance > MAX_RADIUS)
        {
            handler->PSendSysMessage(LANG_GUARD_WRONG_DISTANCE, sDistance);
            handler->SetSentErrorMessage(true);
            return false;            
        }
        // 4. Tele
        char* sTele = strtok(NULL, " ");
        // 5. Comment
        char* comment = strtok(NULL, "");

        GameTele const* tele = handler->extractGameTeleFromLink(sTele);
        if (!tele)
        {
            handler->PSendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;            
        }
        // Create new info
        uint32 entry = sGuardMgr->Create(type, value, distance, tele, comment ? comment : "");
        if (GuardInfo* info = sGuardMgr->GetInfoByEntry(entry))
            info->SendMessage(handler, true);
        return true;
    }

    // Delete existing guard template
    // .guard del <entry>/<selected creature>
    static bool HandleGuardDelCommand(ChatHandler* handler, const char* args)
    {
        if (GuardInfo* gi = _ExtractGuardInfo(handler, (char*)args))
            sGuardMgr->Delete(gi);
        return true;
    }

    // .guard set distance <distance> <entry>/<selected creature>
    static bool HandleGuardSetDistanceCommand(ChatHandler* handler, const char* args)
    {
        if (!args)
            return false;

        // 1 param: distance
        char* sDistance = strtok((char*)args, " ");
        float distance(atoi(sDistance));
        if (!distance || distance > MAX_RADIUS)
        {
            handler->PSendSysMessage(LANG_GUARD_WRONG_DISTANCE, sDistance);
            handler->SetSentErrorMessage(true);
            return false;            
        }
        
        // 2 param (optional): guard template entry
        char* sEntry = strtok(NULL, " ");
        if (GuardInfo* gi = _ExtractGuardInfo(handler, sEntry))
            gi->SetDistance(distance);

        return true;
    }

    // .guard set tele <tele id>/<tele name> <entry>/<selected creature>
    static bool HandleGuardSetTeleCommand(ChatHandler* handler, const char* args)
    {
        if (!args)
            return false;

        // 1 param: tele name or tele id
        char* sTele = strtok((char*)args, " ");
        GameTele const* tele = handler->extractGameTeleFromLink(sTele);
        if (!tele)
        {
            handler->PSendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
            handler->SetSentErrorMessage(true);
            return false;            
        }        
        // 2 param (optional): guard template entry
        char* sEntry = strtok(NULL, " ");
        if (GuardInfo* gi = _ExtractGuardInfo(handler, sEntry))
            gi->SetTele(tele->id);

        return true;
    }

    // .guard set type <type> <value> <entry>/<selected creature>
    static bool HandleGuardSetTypeCommand(ChatHandler* handler, const char* args)
    {
        if (!args)
            return false;

        // 1 param: guard type
        char* sType  = strtok((char*)args, " ");
        // 2 param: value to use with type
        char* sValue = strtok(NULL, " ");
        if (!sType || !sValue)
        {
            handler->SendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;            
        }
        GuardType type = GuardType(atoi(sType));
        uint32 value(atoi(sValue));
        if (!_ValidateTypeValue(handler, type, value))
            return false;

        // 3 param (optional): guard template entry
        char* sEntry = strtok(NULL, " ");
        if (GuardInfo* gi = _ExtractGuardInfo(handler, sEntry))
            gi->SetType(type, value);

        return true;
    }

private:
    // Extract guard info either by entry (if given), or from selected creature
    static GuardInfo* _ExtractGuardInfo(ChatHandler* handler, char* args)
    {
        GuardInfo* gi = NULL;
        if (args && strlen(args))
        {
            uint32 entry(atoi(args));
            gi = sGuardMgr->GetInfoByEntry(entry);
            if (!gi)
                handler->PSendSysMessage(LANG_GUARD_NO_TEMPLATE, entry);
        }
        else if (Creature* creature = handler->getSelectedCreature())
        {
            uint32 lowGuid = creature->GetGUIDLow();
            gi = sGuardMgr->GetInfo(lowGuid);
            if (!gi)
                handler->PSendSysMessage(LANG_GUARD_NO_INFO, lowGuid);
        }
        return gi;
    }

    // Extract guard info either by given lowGuid or from selected creature
    static uint32 _ExtractCreatureGuid(ChatHandler* handler, char* args)
    {
        uint32 lowGuid = 0;
        if (args && strlen(args))
        {
            lowGuid = uint32(atoi(args));
            if (!lowGuid)
            {
                handler->PSendSysMessage(LANG_GUARD_WRONG_GUID, args);
                handler->SetSentErrorMessage(true);
            }
        }
        else
        {
            if (Creature* creature = handler->getSelectedCreature())
                lowGuid = creature->GetGUIDLow();
            else
            {
                handler->PSendSysMessage(LANG_SELECT_CREATURE);
                handler->SetSentErrorMessage(true);
            }
        }
        return lowGuid;
    }

    static bool _ValidateTypeValue(ChatHandler* handler, GuardType type, uint32 value)
    {
        bool res = true;
        switch (type)
        {
            case NPCG_TEAM:
                if (value > TEAM_HORDE)
                {
                    handler->PSendSysMessage(LANG_GUARD_WRONG_TEAM, value, TEAM_HORDE);
                    res = false;
                }
                break;
            case NPCG_SECURITY:
                if (value > SEC_ADMINISTRATOR)
                {
                    handler->PSendSysMessage(LANG_GUARD_WRONG_SECURITY, value, SEC_ADMINISTRATOR);
                    res = false;
                }
                break;
            case NPCG_LEVEL:
                if (value > DEFAULT_MAX_LEVEL)
                {
                    handler->PSendSysMessage(LANG_GUARD_WRONG_LEVEL, value, DEFAULT_MAX_LEVEL);
                    res = false;
                }
                break;
            case NPCG_GUILD:
                if (!sGuildMgr->GetGuildById(value))
                {
                    handler->PSendSysMessage(LANG_GUARD_WRONG_GUILD, value);
                    res = false;
                }
                break;
            case NPCG_GUID:
                if (!sObjectMgr->GetPlayerAccountIdByGUID(MAKE_NEW_GUID(value, 0, HIGHGUID_PLAYER)))
                {
                    handler->PSendSysMessage(LANG_GUARD_WRONG_GUID, value);
                    res = false;
                }
                break;
            default:
                handler->PSendSysMessage(LANG_GUARD_WRONG_TYPE, type, NPCG_GUID);
                res = false;
                break;
        }
        if (!res)
            handler->SetSentErrorMessage(true);
        return res;
    }
};

void AddSC_npc_areaguard()
{
    sGuardMgr->LoadGuardTemplates();
    sGuardMgr->LoadGuards();
    new GuardCommandScript();
    new npc_areaguard();
}
