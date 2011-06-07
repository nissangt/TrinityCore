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

#define MAX_RADIUS 100.0f

GuardMgr::GuardMgr() { }

GuardMgr::~GuardMgr() { }

void GuardMgr::LoadGuardTemplates()
{
    uint32 oldMSTime = getMSTime();

    _guardMap.clear();                                  // for reload case

    QueryResult result = WorldDatabase.Query("SELECT `entry`,`type`,`value`,`distance`,`teleId` FROM `npc_areaguard_template`");
    if (!result)
    {
        sLog->outErrorDb(">> Loaded 0 area guard templates. DB table `npc_areaguard_template` is empty!");
        sLog->outString();
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields   = result->Fetch();

        GuardInfo gi;
        gi.entry        = fields[0].GetUInt32();
        gi.type         = GuardType(fields[1].GetUInt8());
        gi.value        = fields[2].GetUInt32();
        gi.distance     = fields[3].GetFloat();
        gi.teleId       = fields[4].GetUInt32();

        if (!sObjectMgr->GetGameTele(gi.teleId))
        {
            sLog->outError("GUARD: area guard (entry: %u) has invalid teleport id (%u)", gi.entry, gi.teleId);
            continue;
        }
        if (gi.distance < 0.0f || gi.distance > MAX_RADIUS)
        {
            sLog->outError("GUARD: area guard (entry: %u) has invalid interaction radius (%.1f), will set it to %.1f",
                           gi.entry, gi.distance, MAX_RADIUS);
            gi.distance = MAX_RADIUS;
        }

        _guardMap[gi.entry] = gi;

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

    QueryResult result = WorldDatabase.Query("SELECT `guid`,`guardEntry` FROM `npc_areaguard`");
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

GuardInfo const* GuardMgr::GetInfo(uint32 guidLow) const
{
    Guards::const_iterator itr = _guards.find(guidLow);
    if (itr != _guards.end())
    {
        GuardInfoMap::const_iterator infoItr = _guardMap.find(itr->second);
        if (infoItr != _guardMap.end())
            return &infoItr->second;
    }
    return NULL;
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

            _tele = sObjectMgr->GetGameTele(_info->teleId);
            if (!_tele)
            {
                sLog->outError("GUARD: given game teleport (ID: %u) not found!", _info->teleId);
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
                return

            // Return if distance is greater than guard distance
            if (!me->IsWithinDist(who, _info->distance, false))
                return;

            Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();
            // Return if player has GM flag on or is in process of teleport
            if (!player || player->isGameMaster() || player->IsBeingTeleported())
                return;

            bool teleport = false;
            switch (_info->type)
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
                    teleport = (player->GetTeamId() != TeamId(_info->value));
                    break;
                case NPCG_SECURITY:
                    // Action based on GM Level
                    teleport = (player->GetSession()->GetSecurity() < AccountTypes(_info->value));
                    break;
                case NPCG_LEVEL:
                    // Action based on Player Level
                    teleport = (player->getLevel() < _info->value);
                    break;
                case NPCG_GUILD:
                    // Action based on Guild ID
                    teleport = (player->GetGuildId() != _info->value);
                    break;
                case NPCG_GUID:
                    // Action based on Player GUID
                    teleport = (player->GetGUID() != _info->value);
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

void AddSC_npc_areaguard()
{
    sGuardMgr->LoadGuardTemplates();
    sGuardMgr->LoadGuards();
    new npc_areaguard();
}
