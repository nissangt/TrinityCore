-- REPLACE old_db_name WITH THE NAME OF DB, WHERE PREVIOUS VERSION OF CHARACTER EXISTS
-- REPLACE new_db_name WITH THE NAME OF DB, WHERE YOU WANT TO COPY CHARACTER

SET @GUID := <your_guid>;
-- 0. Double-check
SELECT name, race, class, level FROM old_db_name.characters WHERE guid = @GUID;
-- 1. Restore player's data
REPLACE INTO new_db_name.character_account_data         SELECT * FROM old_db_name.character_account_data WHERE guid = @GUID;
REPLACE INTO new_db_name.character_achievement          SELECT * FROM old_db_name.character_achievement WHERE guid = @GUID;
REPLACE INTO new_db_name.character_achievement_progress SELECT * FROM old_db_name.character_achievement_progress WHERE guid = @GUID;
REPLACE INTO new_db_name.character_action               SELECT * FROM old_db_name.character_action WHERE guid = @GUID;
REPLACE INTO new_db_name.character_arena_stats          SELECT * FROM old_db_name.character_arena_stats WHERE guid = @GUID;
REPLACE INTO new_db_name.character_aura                 SELECT * FROM old_db_name.character_aura WHERE guid = @GUID;
REPLACE INTO new_db_name.character_banned               SELECT * FROM old_db_name.character_banned WHERE guid = @GUID;
REPLACE INTO new_db_name.character_battleground_data    SELECT * FROM old_db_name.character_battleground_data WHERE guid = @GUID;
REPLACE INTO new_db_name.character_battleground_random  SELECT * FROM old_db_name.character_battleground_random WHERE guid = @GUID;
REPLACE INTO new_db_name.character_declinedname         SELECT * FROM old_db_name.character_declinedname WHERE guid = @GUID;
REPLACE INTO new_db_name.character_equipmentsets        SELECT * FROM old_db_name.character_equipmentsets WHERE guid = @GUID;
REPLACE INTO new_db_name.character_gifts                SELECT * FROM old_db_name.character_gifts WHERE guid = @GUID;
REPLACE INTO new_db_name.character_glyphs               SELECT * FROM old_db_name.character_glyphs WHERE guid = @GUID;
REPLACE INTO new_db_name.character_homebind             SELECT * FROM old_db_name.character_homebind WHERE guid = @GUID;
REPLACE INTO new_db_name.character_instance             SELECT * FROM old_db_name.character_instance WHERE guid = @GUID;
REPLACE INTO new_db_name.character_pet                  SELECT * FROM old_db_name.character_pet WHERE owner = @GUID;
REPLACE INTO new_db_name.character_queststatus          SELECT * FROM old_db_name.character_queststatus WHERE guid = @GUID;
REPLACE INTO new_db_name.character_queststatus_daily    SELECT * FROM old_db_name.character_queststatus_daily WHERE guid = @GUID;
REPLACE INTO new_db_name.character_queststatus_rewarded SELECT * FROM old_db_name.character_queststatus_rewarded WHERE guid = @GUID;
REPLACE INTO new_db_name.character_queststatus_weekly   SELECT * FROM old_db_name.character_queststatus_weekly WHERE guid = @GUID;
REPLACE INTO new_db_name.character_reputation           SELECT * FROM old_db_name.character_reputation WHERE guid = @GUID;
REPLACE INTO new_db_name.character_skills               SELECT * FROM old_db_name.character_skills WHERE guid = @GUID;
REPLACE INTO new_db_name.character_social               SELECT * FROM old_db_name.character_social WHERE guid = @GUID;
REPLACE INTO new_db_name.character_spell                SELECT * FROM old_db_name.character_spell WHERE guid = @GUID;
REPLACE INTO new_db_name.character_spell_cooldown       SELECT * FROM old_db_name.character_spell_cooldown WHERE guid = @GUID;
-- REPLACE INTO new_db_name.character_stats                SELECT * FROM old_db_name.character_stats WHERE guid = @GUID;
REPLACE INTO new_db_name.character_talent               SELECT * FROM old_db_name.character_talent WHERE guid = @GUID;
REPLACE INTO new_db_name.corpse                         SELECT * FROM old_db_name.corpse WHERE guid = @GUID;
REPLACE INTO new_db_name.item_refund_instance           SELECT * FROM old_db_name.item_refund_instance WHERE player_guid = @GUID;
-- 2. Copy pets' data
REPLACE INTO new_db_name.pet_aura                       SELECT * FROM old_db_name.pet_aura WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);
REPLACE INTO new_db_name.pet_spell                      SELECT * FROM old_db_name.pet_spell WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);
REPLACE INTO new_db_name.pet_spell_cooldown             SELECT * FROM old_db_name.pet_spell_cooldown WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);
-- 3. Delete all the items in new world, which belong to player in old world
DROP TABLE IF EXISTS old_db_name.char_items;
CREATE TABLE old_db_name.char_items (guid int(10) unsigned NOT NULL PRIMARY KEY);
REPLACE INTO old_db_name.char_items                     SELECT itemGuid FROM old_db_name.auctionhouse WHERE itemOwner = @GUID;
REPLACE INTO old_db_name.char_items                     SELECT item FROM old_db_name.character_inventory WHERE guid = @GUID;
REPLACE INTO old_db_name.char_items                     SELECT item_guid FROM old_db_name.mail_items WHERE receiver = @GUID;
DELETE FROM new_db_name.auctionhouse                    WHERE itemGuid IN   (SELECT guid FROM old_db_name.char_items);
DELETE FROM new_db_name.character_inventory             WHERE item IN       (SELECT guid FROM old_db_name.char_items);
DELETE FROM new_db_name.guild_bank_item                 WHERE item_guid IN  (SELECT guid FROM old_db_name.char_items);
DELETE FROM new_db_name.mail_items                      WHERE item_guid IN  (SELECT guid FROM old_db_name.char_items);
DELETE FROM new_db_name.item_instance                   WHERE guid IN       (SELECT guid FROM old_db_name.char_items);
-- 4. Restore player's inventory
REPLACE INTO new_db_name.auctionhouse                   SELECT * FROM old_db_name.auctionhouse WHERE itemowner = @GUID;
REPLACE INTO new_db_name.character_inventory            SELECT * FROM old_db_name.character_inventory WHERE guid = @GUID;
REPLACE INTO new_db_name.mail_items                     SELECT * FROM old_db_name.mail_items WHERE receiver = @GUID;
REPLACE INTO new_db_name.mail                           SELECT * FROM old_db_name.mail WHERE receiver = @GUID;
REPLACE INTO new_db_name.item_instance                  SELECT * FROM old_db_name.item_instance WHERE guid IN (SELECT guid FROM old_db_name.char_items);
-- 5. Restore character
REPLACE INTO new_db_name.characters                     SELECT * FROM old_db_name.characters WHERE guid = @GUID;

/* Checkup scripts
SET @GUID := <your_guid>;

SELECT COUNT(*) FROM old_db_name.character_account_data WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_achievement WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_achievement_progress WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_action WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_arena_stats WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_aura WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_banned WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_battleground_data WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_battleground_random WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_declinedname WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_equipmentsets WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_gifts WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_glyphs WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_homebind WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_instance WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_pet WHERE owner = @GUID;
SELECT COUNT(*) FROM old_db_name.character_queststatus WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_queststatus_daily WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_queststatus_rewarded WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_queststatus_weekly WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_reputation WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_skills WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_social WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_spell WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_spell_cooldown WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_stats WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.character_talent WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.corpse WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.item_refund_instance WHERE player_guid = @GUID;

SELECT COUNT(*) FROM old_db_name.pet_aura WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);
SELECT COUNT(*) FROM old_db_name.pet_spell WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);
SELECT COUNT(*) FROM old_db_name.pet_spell_cooldown WHERE guid IN (SELECT id FROM old_db_name.character_pet WHERE owner = @GUID);

SELECT COUNT(*) FROM old_db_name.auctionhouse WHERE itemOwner = @GUID;
SELECT COUNT(*) FROM old_db_name.character_inventory WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.mail_items WHERE receiver = @GUID;
SELECT COUNT(*) FROM new_db_name.auctionhouse        WHERE itemGuid IN   (SELECT guid FROM old_db_name.char_items);
SELECT COUNT(*) FROM new_db_name.character_inventory WHERE item IN       (SELECT guid FROM old_db_name.char_items);
SELECT COUNT(*) FROM new_db_name.guild_bank_item     WHERE item_guid IN  (SELECT guid FROM old_db_name.char_items);
SELECT COUNT(*) FROM new_db_name.mail_items          WHERE item_guid IN  (SELECT guid FROM old_db_name.char_items);
SELECT COUNT(*) FROM new_db_name.item_instance       WHERE guid IN       (SELECT guid FROM old_db_name.char_items);

SELECT COUNT(*) FROM old_db_name.auctionhouse WHERE itemowner = @GUID;
SELECT COUNT(*) FROM old_db_name.character_inventory WHERE guid = @GUID;
SELECT COUNT(*) FROM old_db_name.mail_items WHERE receiver = @GUID;
SELECT COUNT(*) FROM old_db_name.mail WHERE receiver = @GUID;
SELECT COUNT(*) FROM old_db_name.item_instance WHERE guid IN (SELECT guid FROM old_db_name.char_items);
-- */
