#include "ScriptPCH.h"
#include <cstring>

extern WorldDatabaseWorkerPool WorldDatabase;

#define MSG_GOSSIP_TELE          "Teleport to GuildHouse"
#define MSG_GOSSIP_BUY           "Buy GuildHouse (1000 gold)"
#define MSG_GOSSIP_SELL          "Sell GuildHouse (500 gold)"
#define MSG_GOSSIP_NEXTPAGE      "Next -->"
#define MSG_INCOMBAT             "You are in combat and cannot be teleported to your GuildHouse."
#define MSG_NOGUILDHOUSE         "Your guild currently does not own a GuildHouse."
#define MSG_NOFREEGH             "Unfortunately, all GuildHouses are in use."
#define MSG_ALREADYHAVEGH        "Sorry, but you already own a GuildHouse (%s)."
#define MSG_NOTENOUGHMONEY       "You do not have the %u gold required to purchase a GuildHouse."
#define MSG_GHOCCUPIED           "This GuildHouse is unavailable for purchase as it is currently in use."
#define MSG_CONGRATULATIONS      "Congratulations! You have successfully purchased a GuildHouse."
#define MSG_SOLD                 "You have sold your GuildHouse and have received %u gold."
#define MSG_NOTINGUILD           "You need to be in a guild before you can use a GuildHouse."

#define CODE_SELL "SELL"
#define MSG_CODEBOX_SELL "Type \"" CODE_SELL "\" into the field to confirm that you want to sell your GuildHouse."

enum eGuildHouse
{
    GH_ACTION_TELEPORT = 1001,
    GH_ACTION_LIST = 1002,
    GH_ACTION_SELL = 1003,

    GH_OFFSET_ACTION_ID = 1500,
    GH_OFFSET_NEXT = 10000,
    
    GH_COST_BUY  = 700000000, // 70000 g.
    GH_COST_SELL = 350000000, // 35000 g.
    
    GH_MAX_GOSSIP_COUNT = 10,
};

bool isPlayerGuildLeader(Player *player)
{
    return (player->GetGuildId() != 0) && (player->GetRank() == 0);
}

bool getGuildHouseCoords(uint32 guildId, WorldLocation& loc)
{
    //if player has no guild
    if (guildId == 0)
        return false;
    
    if (QueryResult result = WorldDatabase.PQuery("SELECT `x`, `y`, `z`, `map` FROM `guildhouses` WHERE `guildId` = %u", guildId))
    {
        Field *fields = result->Fetch();
        float x = fields[0].GetFloat();
        float y = fields[1].GetFloat();
        float z = fields[2].GetFloat();
        uint32 mapId = fields[3].GetUInt32();
        loc.WorldRelocate(WorldLocation(mapId, x, y, z));
        return true;
    }
    return false;
}

void teleportPlayerToGuildHouse(Player* player, Creature* creature)
{
    //if player has no guild
    if (player->GetGuildId() == 0)
    {
        creature->MonsterWhisper(MSG_NOTINGUILD, player->GetGUID());
        return;
    } 

    //if player in combat
    if (player->isInCombat())
    {
        creature->MonsterSay(MSG_INCOMBAT, LANG_UNIVERSAL, player->GetGUID());
        return;
    }

    WorldLocation loc;
    // teleport player to the specified location
    if (getGuildHouseCoords(player->GetGuildId(), loc))
        player->TeleportTo(loc);
    else
        creature->MonsterWhisper(MSG_NOGUILDHOUSE, player->GetGUID());
}

bool showBuyList(Player* player, Creature* creature, uint32 showFromId = 0)
{
    //show not occupied guildhouses
    QueryResult result = WorldDatabase.PQuery("SELECT `id`, `comment` FROM `guildhouses` WHERE `guildId` = 0 AND `id` > %u ORDER BY `id` ASC LIMIT %u",
                                              showFromId, GH_MAX_GOSSIP_COUNT);
    if (result)
    {
        uint32 guildHouseId = 0;
        do
        {
            Field* fields = result->Fetch();
            guildHouseId = fields[0].GetInt32();
            std::string comment = fields[1].GetString();

            // Send comment as a gossip item
            // Translate guildhouseId into Action variable
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TABARD, comment, GOSSIP_SENDER_MAIN, guildHouseId + GH_OFFSET_ACTION_ID);
        }
        while (result->NextRow());

        // Assume that we have additional page
        // Add link to next GH_MAX_GOSSIP_COUNT items
        if (result->GetRowCount() == GH_MAX_GOSSIP_COUNT)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_TALK, MSG_GOSSIP_NEXTPAGE, GOSSIP_SENDER_MAIN, guildHouseId + GH_OFFSET_NEXT);

        player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }
    else
    {
        if (showFromId == 0)
        {
            // All guildhouses are occupied
            creature->MonsterWhisper(MSG_NOFREEGH, player->GetGUID());
            player->CLOSE_GOSSIP_MENU();
        }
        else
            // This condition occurs when COUNT(guildhouses) % GOSSIP_COUNT_MAX == 0
            // Just show GHs from beginning
            showBuyList(player, creature);
    }
    return false;
}

bool isPlayerHasGuildhouse(Player* player, Creature* creature, bool whisper = false)
{
    if (QueryResult result = WorldDatabase.PQuery("SELECT `comment` FROM `guildhouses` WHERE `guildId` = %u", player->GetGuildId()))
    {
        if (whisper)
        {
            // whisper to player "already have etc..."
            char msg[100];
            sprintf(msg, MSG_ALREADYHAVEGH, (*result)[0].GetString().c_str());
            creature->MonsterWhisper(msg, player->GetGUID());
        }
        return true;
    }
    return false;
}

void buyGuildhouse(Player* player, Creature* creature, uint32 guildhouseId)
{
    if (player->GetMoney() < GH_COST_BUY)
    {
        // Show how much money player need to buy GH (in gold)
        char msg[100];
        sprintf(msg, MSG_NOTENOUGHMONEY, GH_COST_BUY / 10000);
        creature->MonsterWhisper(msg, player->GetGUID());
        return;
    }

    // Player already have GH
    if (isPlayerHasGuildhouse(player, creature, true))
        return;

    // Check if somebody already occupied this GH
    if (QueryResult result = WorldDatabase.PQuery("SELECT `id` FROM `guildhouses` WHERE `id` = %u AND `guildId` <> 0", guildhouseId))
    {
        creature->MonsterWhisper(MSG_GHOCCUPIED, player->GetGUID());
        return;
    }

    // Update DB
    WorldDatabase.DirectPExecute("UPDATE `guildhouses` SET `guildId` = %u WHERE `id` = %u", player->GetGuildId(), guildhouseId);
    player->ModifyMoney(-GH_COST_BUY);
    creature->MonsterSay(MSG_CONGRATULATIONS, LANG_UNIVERSAL, player->GetGUID());
}

void sellGuildhouse(Player* player, Creature* creature)
{
    if (isPlayerHasGuildhouse(player, creature))
    {
        WorldDatabase.DirectPExecute("UPDATE `guildhouses` SET `guildId` = 0 WHERE `guildId` = %u", player->GetGuildId());
        player->ModifyMoney(GH_COST_SELL);

        // display message e.g. "here your money etc."
        char msg[100];
        sprintf(msg, MSG_SOLD, GH_COST_SELL / 10000);
        creature->MonsterWhisper(msg, player->GetGUID());
    }
}

class guildmaster : public CreatureScript
{
public:
    guildmaster() : CreatureScript("guildmaster") { }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code)
    {
        if (sender == GOSSIP_SENDER_MAIN)
        {
            if (action == GH_ACTION_SELL)
            {
                // Right code
                if (std::strcmp(code, CODE_SELL) == 0)
                    sellGuildhouse(player, creature);

                player->CLOSE_GOSSIP_MENU();
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
                // teleport player to GH
                player->CLOSE_GOSSIP_MENU();
                teleportPlayerToGuildHouse(player, creature);
                break;
            case GH_ACTION_LIST:
                // Show list of GHs which currently not occupied
                showBuyList(player, creature);
                break;
            default:
                if (action > GH_OFFSET_NEXT)
                    showBuyList(player, creature, action - GH_OFFSET_NEXT);
                else if (action > GH_OFFSET_ACTION_ID)
                {
                    // Player clicked on buy list
                    player->CLOSE_GOSSIP_MENU();
                    
                    // Get guildhouseId from action
                    // guildhouseId = action - OFFSET_GH_ID_TO_ACTION
                    buyGuildhouse(player, creature, action - GH_OFFSET_ACTION_ID);
                }
        }        
        return true;
    }
    
    
    bool OnGossipHello(Player* player, Creature* creature)
    {
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, MSG_GOSSIP_TELE, GOSSIP_SENDER_MAIN, GH_ACTION_TELEPORT);
        if (isPlayerGuildLeader(player))
        {
            if (isPlayerHasGuildhouse(player, creature))
                player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_MONEY_BAG, MSG_GOSSIP_SELL, GOSSIP_SENDER_MAIN, GH_ACTION_SELL, MSG_CODEBOX_SELL, 0, true);
            else
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_MONEY_BAG, MSG_GOSSIP_BUY, GOSSIP_SENDER_MAIN, GH_ACTION_LIST);
        }
        player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }
};

void AddSC_guildmaster()
{
    new guildmaster();
}
