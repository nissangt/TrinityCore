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
    NPCG_NONE = -1,
    NPCG_ALL = 0,           // Teleport all (except GM with GM mode on)
    NPCG_TEAM,              // Teleport players not belonging to given team
    NPCG_SECURITY,          // Teleport players whose security level is below given level
    NPCG_LEVEL,             // Teleport players below given level
    NPCG_GUILD,             // Teleport players not belonging to given guild
    NPCG_GUID               // Teleport all players except player with given guid
};

class GuardInfo
{
public:
    GuardInfo() : _entry(0), _type(NPCG_NONE), _value(0), _distance(0.0f), _teleId(0) { }
    GuardInfo(uint32 entry, GuardType type, uint32 value, float distance, uint32 teleId) :
        _entry(entry), _type(type), _value(value), _distance(distance), _teleId(teleId) { }

    void SaveToDB() const;
    bool LoadFromDB(Field* fields);

    void SendMessage(ChatHandler* handler, bool showHeader) const;

    uint32 GetEntry() const { return _entry; }
    GuardType GetType() const { return _type; }
    uint32 GetValue() const { return _value; }
    float GetDistance() const { return _distance; }
    uint32 GetTeleId() const { return _teleId; }

    void SetType(GuardType type, uint32 value);
    void SetTele(uint32 teleId);
    void SetDistance(float distance);

private:
    uint32 _entry;          // Guard entry from npc_areaguard_template
    GuardType _type;        // Guard type
    uint32 _value;          // Parameter for type
    float _distance;        // Interaction distance
    uint32 _teleId;         // Teleport ID from game_tele table
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

    GuardInfo* GetInfo(uint32 lowGuid);
    GuardInfo* GetInfoByEntry(uint32 entry);

    void Link(GuardInfo const* info, uint32 lowGuid);
    void Unlink(uint32 lowGuid);
    void Delete(GuardInfo const* info);
    void Create(GuardType type, uint32 value, float distance, GameTele const* tele);

    void List(ChatHandler* handler, GuardType type) const;
private:
    GuardInfoMap _guardMap;
    Guards _guards;
    uint32 _maxEntry;
};

#define sGuardMgr ACE_Singleton<GuardMgr, ACE_Null_Mutex>::instance()

#endif
