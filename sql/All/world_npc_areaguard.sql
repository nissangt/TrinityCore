DROP TABLE IF EXISTS `npc_areaguard_template`;
CREATE TABLE `npc_areaguard_template` (
  `entry` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL default '0' COMMENT '0=All,1=Team,2=SecLvl,3=PlrLvl,4=Guild,5=PlrGuid',
  `value` int(10) unsigned NOT NULL default '0',
  `distance` float NOT NULL default '65',
  `teleId` mediumint(8) NOT NULL,
  `comment` varchar(255) NULL,
  PRIMARY KEY  (`entry`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='NPC Area Guard';

DROP TABLE IF EXISTS `npc_areaguard`;
CREATE TABLE `npc_areaguard` (
  `guid` int(10) unsigned NOT NULL,
  `guardEntry` int(10) unsigned NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='NPC Area Guard';

INSERT INTO `npc_areaguard_template` (`entry`, `type`, `distance`, `value`, `teleId`, `comment`) VALUES
(1, 0, 65, 0, 732, 'Teleport all non-GM players to Ratchet');

DELETE FROM `creature_template` WHERE `entry` = 92015;
INSERT INTO `creature_template` (`entry`,`difficulty_entry_1`,`difficulty_entry_2`,`difficulty_entry_3`,`KillCredit1`,`KillCredit2`,`modelid1`,`modelid2`,`modelid3`,`modelid4`,`name`,`subname`,`IconName`,`gossip_menu_id`,`minlevel`,`maxlevel`,`exp`,
`faction_A`,`faction_H`,`npcflag`,`speed_walk`,`scale`,`rank`,`mindmg`,`maxdmg`,`dmgschool`,`attackpower`,`dmg_multiplier`,`baseattacktime`,`rangeattacktime`,`unit_class`,`unit_flags`,`dynamicflags`,`family`,`trainer_type`,`trainer_spell`,`trainer_class`,`trainer_race`,`minrangedmg`,`maxrangedmg`,`rangedattackpower`,`type`,`type_flags`,`lootid`,`pickpocketloot`,`skinloot`,`resistance1`,`resistance2`,`resistance3`,`resistance4`,`resistance5`,`resistance6`,`spell1`,`spell2`,`spell3`,`spell4`,`spell5`,`spell6`,`spell7`,`spell8`,`PetSpellDataId`,`VehicleId`,`mingold`,`maxgold`,`AIName`,`MovementType`,`InhabitType`,`Health_mod`,`Mana_mod`,`Armor_mod`,`RacialLeader`,`questItem1`,`questItem2`,`questItem3`,`questItem4`,`questItem5`,`questItem6`,`movementId`,`RegenHealth`,`equipment_id`,`mechanic_immune_mask`,`flags_extra`,`ScriptName`) VALUES
(92015,0,0,0,0,0,10458,0,0,0,'Terminator','Area Guard',NULL,0,80,80,0,35,35,0,0.91,1,0,7,7,0,3,1,2000,2200,1,390,0,0,0,0,0,0,1.76,2.42,100,7,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'',0,7,1,1,1,0,0,0,0,0,0,0,0,1,0,0,32770,'npc_areaguard');

DELETE FROM `trinity_string` WHERE `entry` BETWEEN 6700 AND 6716;
INSERT INTO `trinity_string`(`entry`, `content_default`) VALUES
(6700, 'All'),
(6701, 'Alliance'),
(6702, 'Horde'),
(6703, 'GM Level'),
(6704, 'Level'),
(6705, 'Guild'),
(6706, 'Guid'),
(6707, 'Guard info for creature entry %u not found.'),
(6708, 'There is no guard with entry %u.'),
(6709, 'Given distance "%s" is invalid.'),
(6710, 'There is no guard with guid "%s" or guid is invalid.'),
(6711, 'Team value %u cannot be more than %u.'),
(6712, 'GM level %u cannot be more than %u'),
(6713, 'Level %u cannot be more than %u'),
(6714, 'There is no guild with id %u.'),
(6715, 'Type %u cannot be more than %u.'),
(6716, '|  Entry   |  R  |       Type       |          Value           |        Teleport Location         |');
