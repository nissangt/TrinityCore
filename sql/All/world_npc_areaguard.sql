DROP TABLE IF EXISTS `npc_areaguard_template`;
CREATE TABLE `npc_areaguard_template` (
  `entry` mediumint(8) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL default '0' COMMENT '0=All,1=Team,2=SecLvl,3=PlrLvl,4=Guild,5=PlrGuid',
  `value` tinyint(3) unsigned NOT NULL default '0',
  `distance` float NOT NULL default '65',
  `teleId` float NOT NULL,
  `comment` varchar(255) NULL,
  PRIMARY KEY  (`entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='NPC Area Guard';

DROP TABLE IF EXISTS `npc_areaguard`;
CREATE TABLE `npc_areaguard` (
  `guid` int(10) unsigned NOT NULL,
  `guardEntry` mediumint(8) unsigned NOT NULL
  PRIMARY KEY  (`creature_entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='NPC Area Guard';

INSERT INTO `npc_areaguard_template` (`entry`, `type`, `distance`, `value`, `teleId`, `comment`) VALUES
(1, 0, 65, 0, 732, 'Teleport all non-GM players to Ratchet');

DELETE FROM `creature_template` WHERE `entry` = 92015;
INSERT INTO `creature_template` (`entry`, `heroic_entry`, `modelid_A`, `modelid_A2`, `modelid_H`, `modelid_H2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `minhealth`, `maxhealth`, `minmana`, `maxmana`, `armor`, `faction_A`, `faction_H`, `npcflag`, `speed`, `scale`, `rank`, `mindmg`, `maxdmg`, `dmgschool`, `attackpower`, `baseattacktime`, `rangeattacktime`, `unit_flags`, `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `class`, `race`, `minrangedmg`, `maxrangedmg`, `rangedattackpower`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `resistance1`, `resistance2`, `resistance3`, `resistance4`, `resistance5`, `resistance6`, `spell1`, `spell2`, `spell3`, `spell4`, `PetSpellDataId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `InhabitType`, `RacialLeader`, `RegenHealth`, `equipment_id`, `mechanic_immune_mask`, `flags_extra`, `ScriptName`) VALUES
(92015, 0, 10458, 0, 0, 0, 'Terminator', 'Area Guard', NULL, 0, 70, 70, 12000, 12000, 4000, 4000, 12000, 35, 35, 129, 8, 1, 0, 2000, 50000, 0, 0, 2000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 20, 20, 20, 20, 20, 20, 0, 0, 0, 0, 0, 521411, 521411, '', 0, 3, 0, 1, 0, 0, 0, 'npc_areaguard');
