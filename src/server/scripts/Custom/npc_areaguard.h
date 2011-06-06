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

#ifndef NPC_AREAGUARD_H
#define NPC_AREAGUARD_H

enum GuardType {
    NPCG_ALL = 0,           // Teleport all (except GM with GM mode on)
    NPCG_TEAM,              // Teleport players not belonging to given team
    NPCG_SECURITY,          // Teleport players whose security level is below given level
    NPCG_LEVEL,             // Teleport players below given level
    NPCG_GUILD,             // Teleport players not belonging to given guild
    NPCG_GUID               // Teleport all players except player with given guid
};

struct GuardInfo
{
    uint32 entry;       // Guard entry from npc_areaguard_template
    GuardType type;     // Guard type
    uint32 value;       // Parameter for type
    float distance;     // Interaction distance
    uint32 teleId;      // Teleport ID from game_tele table
    
    GuardInfo() : entry(0), type(NPCG_ALL), value(0), distance(0.0f), teleId(0) { }
    GuardInfo(uint32 _entry, GuardType _type, uint32 _value, float _distance, uint32 _teleId) :
        entry(_entry), type(_type), value(_value), distance(_distance), teleId(_teleId) { } 
};

typedef UNORDERED_MAP <uint32, GuardInfo> GuardInfoMap;
typedef UNORDERED_MAP <uint32, uint32> Guards;

class GuardMgr
{
    friend class ACE_Singleton<GuardMgr, ACE_Null_Mutex>;
    GuardMgr();
    ~GuardMgr();
    
public:
    void LoadGuardTemplates();
    void LoadGuards();
    
    GuardInfo const* GetInfo(uint32 guidLow) const;
    
private:    
    GuardInfoMap _guardMap;
    Guards _guards;    
};

#define sGuardMgr ACE_Singleton<GuardMgr, ACE_Null_Mutex>::instance()

#endif
