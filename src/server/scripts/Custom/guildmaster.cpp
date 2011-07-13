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
#include "Config.h"
#include "GuildMgr.h"

enum eGuildHouse
{
    GH_ACTION_TELEPORT      = 1001,
    GH_ACTION_LIST          = 1002,
    GH_ACTION_SELL          = 1003,

    GH_OFFSET_ACTION_ID     = 1500,
    GH_OFFSET_NEXT          = 10000,

    GH_BUY_PRICE_DEFAULT    = 500000000, // 50000 g.
    GH_SELL_PRICE_DEFAULT   = 250000000, // 25000 g.

    GH_MAX_GOSSIP_COUNT     = 10,
};

class guildmaster : public CreatureScript
{
    // Guild ID -> Guild House ID mapping
    typedef UNORDERED_MAP <uint32, uint32> GuildHouseGuilds;
    GuildHouseGuilds _guildHouseGuilds;

    struct GuildHouseEntry
    {
        uint32 _id;
        uint32 _guildId;
        std::string _comment;
        WorldLocation _loc;

        explicit GuildHouseEntry(Field* fields)
        {
            _id             = fields[0].GetUInt32();
            _guildId        = fields[1].GetUInt32();
            _comment        = fields[2].GetString();
            uint32 mapId    = fields[3].GetUInt32();
            float x         = fields[4].GetFloat();
            float y         = fields[5].GetFloat();
            float z         = fields[6].GetFloat();
            _loc.WorldRelocate(WorldLocation(mapId, x, y, z));
        }

        void SaveToDB() const
        {
            WorldDatabase.DirectPExecute("UPDATE `guildhouses` SET `guildId` = %u WHERE `id` = %u", _guildId, _id);
        }

        uint32 AddToBuyList(uint32 startId, Player* player, uint32 price, const char* buyWord) const
        {
            if (!_guildId && _id > startId)
            {
                char buyMsg[255];
                sprintf(buyMsg, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_BUY_PROMPT), buyWord, _comment.c_str(), uint32(price / 10000));

                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_TABARD, _comment, GOSSIP_SENDER_MAIN, _id + GH_OFFSET_ACTION_ID, buyMsg, 0, true);
                return _id;
            }
            return 0;
        }

        void Teleport(Player* player) const
        {
            player->TeleportTo(_loc);
        }

        bool Buy(Player* player)
        {
            if (!_guildId)
            {
                _guildId = player->GetGuildId();
                SaveToDB();
                return true;
            }
            return false;
        }

        void Sell()
        {
            _guildId = 0;
            SaveToDB();
        }
    };

    typedef std::vector <GuildHouseEntry*> GuildHouses;
    GuildHouses _guildHouses;
    // Cache values
    uint32 _buyPrice;
    uint32 _sellPrice;
    const char* _sellWord;
    const char* _buyWord;

public:
    guildmaster() : CreatureScript("guildmaster") { _Load(); }

    ~guildmaster() { _Clear(); }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code)
    {
        if (sender == GOSSIP_SENDER_MAIN)
        {
            if (action == GH_ACTION_SELL)
            {
                // Check if code was entered correctly
                if (stricmp(code, _sellWord) == 0)
                    _SellGuildHouse(player, creature);

                player->PlayerTalkClass->SendCloseGossip();
                return true;
            }
            else if (action > GH_OFFSET_ACTION_ID)
            {
                // Check if code was entered correctly
                if (stricmp(code, _buyWord) == 0)
                    _BuyGuildHouse(player, creature, action - GH_OFFSET_ACTION_ID);

                player->PlayerTalkClass->SendCloseGossip();
                return true;
            }
        }
        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        if (sender != GOSSIP_SENDER_MAIN)
            return false;

        switch (action)
        {
            case GH_ACTION_TELEPORT:
                player->PlayerTalkClass->SendCloseGossip();
                _TeleportPlayerToGuildHouse(player, creature);
                break;
            case GH_ACTION_LIST:
                // Show list of GHs which currently not occupied
                _ShowBuyList(player, creature);
                break;
            default:
                if (action > GH_OFFSET_NEXT)
                    _ShowBuyList(player, creature, action - GH_OFFSET_NEXT);
                else if (action > GH_OFFSET_ACTION_ID)
                {
                    // Player clicked on buy list
                    player->PlayerTalkClass->SendCloseGossip();

                    // Get guildhouseId from action
                    // guildhouseId = action - GH_OFFSET_ACTION_ID
                    _BuyGuildHouse(player, creature, action - GH_OFFSET_ACTION_ID);
                }
        }
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        bool hasGuildHouse = _GetPlayerGuildHouse(player);
        // Add teleport option only if player has guildhouse
        if (hasGuildHouse)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_GOSSIP_TELEPORT), GOSSIP_SENDER_MAIN, GH_ACTION_TELEPORT);

        // Only guild master can buy or sell GH
        if (_IsPlayerGuildLeader(player))
        {
            if (hasGuildHouse)
            {
                // If there is already a GH assigned to GM, then add sell option
                char msg[255];
                sprintf(msg, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_GOSSIP_SELL), uint32(_sellPrice / 10000));

                char sellMsg[255];
                sprintf(sellMsg, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_SELL_PROMPT), _sellWord);

                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_MONEY_BAG, msg, GOSSIP_SENDER_MAIN, GH_ACTION_SELL, sellMsg, 0, true);
            }
            else
            {
                // Otherwise, add buy option
                char msg[255];
                sprintf(msg, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_GOSSIP_BUY), uint32(_buyPrice / 10000));
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_MONEY_BAG, msg, GOSSIP_SENDER_MAIN, GH_ACTION_LIST);
            }
        }
        player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

private:
    inline bool _IsPlayerGuildLeader(Player* player) const
    {
        return (player->GetGuildId() != 0) && (player->GetRank() == 0);
    }

    inline void _AddGuildHouse(GuildHouseEntry* gh)
    {
        // Make sure vector is large enough to hold new guild house
        if (_guildHouses.size() <= gh->_id)
            _guildHouses.resize(gh->_id + 1, NULL);
        _guildHouses[gh->_id] = gh;
        // Save Guild -> Guild House connection
        if (gh->_guildId)
            _guildHouseGuilds[gh->_guildId] = gh->_id;
    }

    inline GuildHouseEntry* _GetGuildHouse(uint32 guildHouseId)
    {
        return _guildHouses[guildHouseId];
    }

    inline GuildHouseEntry const* _GetGuildHouse(uint32 guildHouseId) const
    {
        return _guildHouses[guildHouseId];
    }

    inline GuildHouseEntry* _GetPlayerGuildHouse(Player* player)
    {
        GuildHouseGuilds::iterator itr = _guildHouseGuilds.find(player->GetGuildId());
        if (itr != _guildHouseGuilds.end())
            return _GetGuildHouse(itr->second);
        return NULL;
    }

    inline GuildHouseEntry const* _GetPlayerGuildHouse(Player* player) const
    {
        GuildHouseGuilds::const_iterator itr = _guildHouseGuilds.find(player->GetGuildId());
        if (itr != _guildHouseGuilds.end())
            return _GetGuildHouse(itr->second);
        return NULL;
    }

    void _TeleportPlayerToGuildHouse(Player* player, Creature* creature) const
    {
        // If player has no guild
        if (!player->GetGuildId())
        {
            creature->MonsterWhisper(LANG_GH_NO_GUILD, player->GetGUID());
            return;
        }

        // If player is in combat
        if (player->isInCombat())
        {
            creature->MonsterSay(LANG_GH_IN_COMBAT, LANG_UNIVERSAL, player->GetGUID());
            return;
        }

        if (GuildHouseEntry const* gh = _GetPlayerGuildHouse(player))
            gh->Teleport(player);
        else
            creature->MonsterWhisper(LANG_GH_NO_GUILDHOUSE, player->GetGUID());
    }

    bool _ShowBuyList(Player* player, Creature* creature, uint32 showFromId = 0) const
    {
        uint32 lastId = 0;
        uint32 count = 0;
        for (GuildHouses::const_iterator itr = _guildHouses.begin(); itr != _guildHouses.end(); ++itr)
            if (GuildHouseEntry const* gh = (*itr))
                if (lastId = gh->AddToBuyList(showFromId, player, _buyPrice, _buyWord))
                    if (++count >= GH_MAX_GOSSIP_COUNT)
                        break;

        if (count)
        {
            // Add link to next page (from last page it will lead to the first)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_GOSSIP_NEXT), GOSSIP_SENDER_MAIN, lastId + GH_OFFSET_NEXT + 1);
            player->PlayerTalkClass->SendGossipMenu(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            return true;
        }
        else
        {
            if (!showFromId)
            {
                creature->MonsterWhisper(LANG_GH_NO_FREE_GUILDHOUSE, player->GetGUID());
                player->PlayerTalkClass->SendCloseGossip();
            }
            else
                // No more GHs with ID more thaen showFromId. List from beginning.
                _ShowBuyList(player, creature);
        }
        return false;
    }

    void _BuyGuildHouse(Player* player, Creature* creature, uint32 guildHouseId)
    {
        // Check money
        if (player->GetMoney() < _buyPrice)
        {
            creature->MonsterWhisper(LANG_GH_NOT_ENOUGH_GOLD, player->GetGUID());
            return;
        }
        // Check if player already has guildhouse
        if (_GetPlayerGuildHouse(player))
        {
            creature->MonsterWhisper(LANG_GH_ALREADY_HAVE_GUILDHOUSE, player->GetGUID());
            return;
        }
        if (GuildHouseEntry* gh = _GetGuildHouse(guildHouseId))
        {
            if (gh->Buy(player))
            {
                _guildHouseGuilds[gh->_guildId] = gh->_id;
                player->ModifyMoney(-int32(_buyPrice));
                creature->MonsterSay(LANG_GH_BUY_SUCCESS, LANG_UNIVERSAL, player->GetGUID());
            }
            else
                creature->MonsterWhisper(LANG_GH_GUILDHOUSE_IN_USE, player->GetGUID());
        }
    }

    void _SellGuildHouse(Player* player, Creature* creature)
    {
        if (GuildHouseEntry* gh = _GetPlayerGuildHouse(player))
        {
            _guildHouseGuilds[gh->_guildId] = 0;
            gh->Sell();
            player->ModifyMoney(_sellPrice);
            creature->MonsterWhisper(LANG_GH_SELL_SUCCESS, player->GetGUID());
        }
    }

    void _Clear()
    {
        for (GuildHouses::iterator itr = _guildHouses.begin(); itr != _guildHouses.end(); ++itr)
            if (*itr)
                delete (*itr);
        _guildHouses.clear();
        _guildHouseGuilds.clear();
    }

    void _Load()
    {
        _buyPrice = sConfig->GetIntDefault("GuildHouse.BuyPrice", GH_BUY_PRICE_DEFAULT);
        _sellPrice = sConfig->GetIntDefault("GuildHouse.SellPrice", GH_SELL_PRICE_DEFAULT);
        _sellWord = sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_SELL_WORD);
        _buyWord = sObjectMgr->GetTrinityStringForDBCLocale(LANG_GH_BUY_WORD);

        uint32 oldMSTime = getMSTime();

        _Clear();

        QueryResult result = WorldDatabase.Query("SELECT `id`,`guildId`,`comment`,`map`,`x`,`y`,`z` FROM `guildhouses` ORDER BY `id`");
        if (!result)
        {
            sLog->outErrorDb(">> Loaded 0 guild houses. DB table `guildhouses` is empty!");
            sLog->outString();
            return;
        }

        _guildHouses.resize(result->GetRowCount(), NULL);
        uint32 count = 0;
        do
        {
            GuildHouseEntry* gh = new GuildHouseEntry(result->Fetch());
            // Validation
            if (gh->_guildId && !sGuildMgr->GetGuildById(gh->_guildId))
            {
                sLog->outError("GUILDHOUSE: guild house (id: %u) has non-existent guild (id: %u) assigned to it", gh->_id, gh->_guildId);
                delete gh;
                continue;
            }
            _AddGuildHouse(gh);
            ++count;
        }
        while (result->NextRow());

        sLog->outString(">> Loaded %u guild houses in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        sLog->outString();
    }
};

void AddSC_guildmaster()
{
    new guildmaster();
}
