UPDATE `creature_template` SET `subname` = 'Journeyman Alchemist'     WHERE `entry` = 1246;  -- Vosur Brakthel <Journeyman Alchemist>
UPDATE `creature_template` SET `subname` = 'Journeyman Leatherworker' WHERE `entry` = 1466;  -- Gretta Finespindle <Journeyman Leatherworker>
UPDATE `creature_template` SET `subname` = 'Journeyman Tailor'        WHERE `entry` = 1703;  -- Uthrar Threx <Journeyman Tailor>
UPDATE `creature_template` SET `subname` = 'Expert Blacksmith'        WHERE `entry` = 10276; -- Rotgath Stonebeard <Expert Blacksmith>
UPDATE `creature_template` SET `subname` = 'Journeyman Blacksmith'    WHERE `entry` = 10277; -- Groum Stonebeard <Journeyman Blacksmith>
UPDATE `creature_template` SET `subname` = 'Journeyman Engineer'      WHERE `entry` = 11028; -- Jemma Quikswitch <Journeyman Engineer>
UPDATE `creature_template` SET `subname` = 'Expert Engineer'          WHERE `entry` = 11029; -- Trixie Quikswitch <Expert Engineer>
UPDATE `creature_template` SET `subname` = 'Journeyman Enchanter'     WHERE `entry` = 11065; -- Thonys Pillarstone <Journeyman Enchanter>

UPDATE `creature_template` SET `npcflag` = 81 WHERE `entry` IN (1246, 1466, 1703, 10276, 10277, 11028, 11029, 11065); -- `trainer_type` = 2
UPDATE `creature_template` SET `type_flags` = 134217728 WHERE `entry` IN (5157, 10276, 11029);

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 15 AND `SourceEntry` = 0 AND `ConditionTypeOrReference` = 7 AND `SourceGroup` IN (593, 2761, 4123, 4147, 4150, 4160, 4205, 4345);
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `ConditionTypeOrReference`, `ConditionValue1`, `ConditionValue2`, `Comment`) VALUES
(15, 593,  0, 7, 164, 125, 'Show menu if blacksmithing is 125 or higher'), -- Bengus Deepforge <Artisan Blacksmith>
(15, 2761, 0, 7, 164, 50,  'Show menu if blacksmithing is 50 or higher'),  -- Rotgath Stonebeard <Expert Blacksmith>
(15, 4123, 0, 7, 171, 50,  'Show menu if alchemy is 50 or higher'),        -- Tally Berryfizz <Expert Alchemist>
(15, 4147, 0, 7, 202, 50,  'Show menu if engineering is 50 or higher'),    -- Trixie Quikswitch <Expert Engineer>
(15, 4150, 0, 7, 202, 125, 'Show menu if engineering is 125 or higher'),   -- Springspindle Fizzlegear <Artisan Engineer>
(15, 4160, 0, 7, 333, 50,  'Show menu if enchanting is 50 or higher'),     -- Gimble Thistlefuzz <Expert Enchanter>
(15, 4205, 0, 7, 165, 50,  'Show menu if leatherworking is 50 or higher'), -- Fimble Finespindle <Expert Leatherworker>
(15, 4345, 0, 7, 197, 50,  'Show menu if tailoring is 50 or higher');      -- Jormund Stonebrow <Expert Tailor>

-- battlemasters
DELETE FROM `creature` WHERE `id` IN (857, 5113, 5115, 6114, 12197, 14982, 19915, 34991, 35007, 35025, 35600) OR `guid` = 86263;
INSERT INTO `creature` (`guid`, `id`, `map`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `curhealth`, `curmana`) VALUES 
(600857, 857,   0, 1, -5039.19, -1266.88, 510.326, 3.92579,  120, 1, 0), -- Donal Osgood <Arathi Basin Battlemaster>
(2019,   5113,  0, 1, -5047.54, -1269.69, 510.408, 6.24828,  540, 1, 0), -- Kelv Sternhammer <Warrior Trainer>
(2020,   5115,  0, 1, -5043.84, -1274.68, 510.324, 1.33007,  490, 1, 0), -- Daera Brightspear <Hunter Trainer>
(2018,   6114,  0, 1, -5046.48, -1273,    510.324, 0.693899, 430, 1, 0), -- Muren Stormpike
(612197, 12197, 0, 1, -5046.1,  -1266.04, 510.325, 5.48483,  120, 1, 0), -- Glordrum Steelbeard <Alterac Valley Battlemaster>
(614982, 14982, 0, 1, -5039.09, -1272.92, 510.324, 2.46114,  120, 1, 0), -- Lylandris <Warsong Gulch Battlemaster>
(86263,  15351, 0, 0, -5042.4,  -1265.32, 510.325, 4.62873,  300, 1, 0), -- Alliance Brigadier General
(81,     19915, 0, 0, -5045.42, -1276.81, 510.325, 1.43228,  300, 1, 0), -- Max Xim <Arena Battlemaster>
(88250,  34991, 0, 1, -5038.09, -1269.94, 510.325, 3.16003,  120, 1, 0), -- Borim Goldhammer <Battlemaster>
(88249,  35007, 0, 1, -5040.18, -1263.17, 510.325, 4.46067,  120, 1, 1), -- Lixa Felflinger <Battlemaster>
(208055, 35025, 0, 1, -5036.95, -1264.96, 510.325, 3.85973,  120, 1, 0), -- Lynette Bracer <Isle of Conquest Battlemaster>
(88248,  35600, 0, 1, -5035.64, -1267.93, 510.324, 3.29044,  300, 1, 1); -- Arcanist Laniria <Wintergrasp Battle-Mage>

-- Myra Tyrngaarde - bread vendor lines
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 5109;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 5109;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(5109, 0, 0, 0, 1, 0, 100, 0, 1000, 15000, 40000, 60000, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Myra Tyrngaarde - Out of Combat - Say Random Line (group 0)');

-- Myra: path 1220 is her own 13-node Commons circuit.
DELETE FROM `creature_addon` WHERE `guid` = 122;
INSERT INTO `creature_addon` (`guid`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `visibilityDistanceType`, `auras`) VALUES
(122, 1220, 0, 0, 1, 0, 0, NULL);

-- Bixi Wobblebonk (13084) - ambient route
UPDATE `creature` SET `MovementType` = 2 WHERE `guid` = 89 AND `id` = 13084;

DELETE FROM `creature_template_addon` WHERE `entry` = 13084;
INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`) VALUES
(13084, 130840, 0, 0, 1, 0);

DELETE FROM `waypoint_data` WHERE `id` = 130840;
INSERT INTO `waypoint_data` (`id`, `point`, `position_x`, `position_y`, `position_z`, `orientation`, `delay`, `move_type`) VALUES
(130840, 1, -5042.06, -1205.56, 508.901, 0.9774, 141000, 0),
(130840, 2, -5041.97, -1204.65, 508.902, NULL, 0, 0),
(130840, 3, -5043.87, -1203.1, 508.901, NULL, 0, 0),
(130840, 4, -5047.12, -1198.18, 505.29, NULL, 0, 0),
(130840, 5, -5046.6, -1195.7, 505.29, NULL, 0, 0),
(130840, 6, -5042.16, -1193.11, 502.235, NULL, 0, 0),
(130840, 7, -5039.25, -1194.53, 502.235, NULL, 0, 0),
(130840, 8, -5038.03, -1197.81, 502.235, NULL, 0, 0),
(130840, 9, -5039.82, -1199.59, 502.235, 3.6, 5000, 0),
(130840, 10, -5038.39, -1203.96, 502.236, 5.2, 7000, 0),
(130840, 11, -5037.77, -1202.85, 502.236, NULL, 0, 0),
(130840, 12, -5038.17, -1199.26, 502.236, NULL, 0, 0),
(130840, 13, -5039.39, -1194.18, 502.236, NULL, 0, 0),
(130840, 14, -5042.25, -1193.52, 502.236, NULL, 0, 0),
(130840, 15, -5046.5, -1195.73, 505.29, NULL, 0, 0),
(130840, 16, -5046.87, -1198.24, 505.29, NULL, 0, 0),
(130840, 17, -5043.92, -1203.21, 508.901, NULL, 0, 0),
(130840, 18, -5041.46, -1207.59, 508.901, NULL, 0, 0),
(130840, 19, -5039.81, -1208.87, 508.901, NULL, 0, 0),
(130840, 20, -5038.46, -1208.77, 508.901, 0.2, 7000, 0),
(130840, 21, -5039.97, -1208.48, 508.901, NULL, 0, 0),
(130840, 22, -5041.19, -1206.04, 508.901, NULL, 0, 0),
(130840, 23, -5040.67, -1203.27, 508.901, NULL, 0, 0),
(130840, 24, -5040.22, -1199.8, 508.901, NULL, 0, 0),
(130840, 25, -5040.42, -1199.26, 508.901, 2.1, 7000, 0),
(130840, 26, -5037.57, -1199.39, 508.901, NULL, 0, 0),
(130840, 27, -5035.08, -1198.91, 508.891, NULL, 0, 0),
(130840, 28, -5032.73, -1197.4, 508.878, 0.5, 7000, 0),
(130840, 29, -5037.16, -1199.98, 508.9, NULL, 0, 0),
(130840, 30, -5039.77, -1202.4, 508.901, NULL, 0, 0);

-- Lines that AzerothCore never stored
DELETE FROM `creature_text` WHERE `CreatureID` IN (2916, 8507);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(2916, 0, 0, 'Ragnaros? Terrible news indeed...', 12, 0, 100, 6, 0, 0, 956, 0, 'Historian Karnik'),
(2916, 1, 0, 'You heard him, traveler. Speak to me again when you\'re ready to speak to Belgrum.', 12, 0, 100, 0, 0, 0, 958, 0, 'Historian Karnik'),
(2916, 2, 0, 'Hammertoe, you''re as smart as you were stubborn in life and even in death. What can you tell me, ole friend?', 12, 0, 100, 0, 0, 0, 917, 0, 'Historian Karnik - quest 724 opening line'),
(2916, 3, 0, 'Oh, $n, wait a minute! There''s one more thing! Come back!', 12, 0, 100, 0, 0, 0, 4451, 0, 'Historian Karnik - quest 3448 accept'),
(8507, 0, 0, 'Oh, $n, wait a minute! There''s one more thing! Come back!', 12, 0, 100, 22, 0, 0, 4451, 0, 'Tymor - quest 3449 accept, carries the shout emote');

-- Sara Balloo (2695) quest 637 - AzerothCore fired both lines at once, with no emotes
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 2695;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 2695;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` = 2695001;

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(2695, 0, 0, 0, 20, 0, 100, 0, 637, 0, 0, 0, 0, 0, 80, 2695001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Sara Balloo - On Quest 637 Rewarded - Run Script'),
--
(2695001, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Sara Balloo - Set Active'),
(2695001, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Sara Balloo - Remove Questgiver Flag'),
(2695001, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Sara Balloo - Say Line 0'),
(2695001, 9, 3, 0, 0, 0, 100, 0, 6000, 6000, 0, 0, 0, 0, 5, 15, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Sara Balloo - Emote Roar'),
(2695001, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Sara Balloo - Say Line 1'),
(2695001, 9, 5, 0, 0, 0, 100, 0, 4000, 4000, 0, 0, 0, 0, 5, 18, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Sara Balloo - Emote Cry'),
(2695001, 9, 6, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 0, 5, 20, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Sara Balloo - Emote Beg'),
(2695001, 9, 7, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 0, 82, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Sara Balloo - Add Questgiver Flag'),
(2695001, 9, 8, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Sara Balloo - Remove Active');

-- Tymor (8507) quest 3449 accept
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 8507;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 8507;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` = 8507001;

-- One row, not two: CreatureTextMgr::SendChat fires the row's Emote immediately before the
-- chat packet, so the shout and the line go out together instead of the line clipping it.
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(8507, 0, 0, 0, 19, 0, 100, 0, 3449, 0, 0, 0, 0, 0, 80, 8507001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                'Tymor - On Quest 3449 Accepted - Run Script'),
--
(8507001, 9, 0, 0, 0, 0, 100, 0, 500, 500, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Tymor - Say Line 0 (emote 22 rides on the text row)');

-- Muren Stormpike (6114) quest 1680 accept
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 6114;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 6114;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` = 6114001;

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(6114, 0, 0, 1, 19, 0, 100, 0, 1680, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,                      'Muren Stormpike - On Quest 1680 Accepted - Face Player'),
(6114, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0,  5, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Muren Stormpike - Link - Emote Wave'),
(6114, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0,  1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Muren Stormpike - Link - Say Line 0'),
(6114, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 80, 6114001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                   'Muren Stormpike - Link - Run Script (turn back)'),
--
(6114001, 9, 0, 0, 0, 0, 100, 0, 4000, 4000, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0.84,              'Muren Stormpike - Turn Back To Post');

-- Tormus Deepforge (6031) quest 1681 - walks to The Great Anvil, works, walks back, reports.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 6031;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 6031;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (6031001, 6031002);

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(6031, 0, 0, 1, 20, 0, 100, 0, 1681, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Tormus Deepforge - On Quest 1681 Rewarded - Walk, do not run'),
(6031, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 64, 1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,                         'Tormus Deepforge - Link - Remember the player'),
(6031, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                          'Tormus Deepforge - Link - Say Line 0 (begins to work)'),
(6031, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 601, 0,0,0,0,0, 8, 0,0,0, -4798.0127, -1105.1676, 499.6052, 0,    'Tormus Deepforge - Link - Walk To The Great Anvil'),
(6031, 0, 4, 0, 34, 0, 100, 0, 8, 601, 0, 0, 0, 0, 80, 6031001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Tormus Deepforge - On Reaching The Anvil - Run Script'),
(6031, 0, 5, 0, 34, 0, 100, 0, 8, 602, 0, 0, 0, 0, 80, 6031002, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Tormus Deepforge - On Returning Home - Run Script'),
--
(6031001, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 4.15396,                 'Tormus Deepforge - Face The Anvil'),
(6031001, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 17, 173, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                     'Tormus Deepforge - Emote State Work'),
(6031001, 9, 2, 0, 0, 0, 100, 0, 4900, 4900, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Tormus Deepforge - Clear Emote State'),
(6031001, 9, 3, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 602, 0, 0, 0, 0, 0, 8, 0, 0, 0, -4793.38, -1098.17, 498.89, 0,  'Tormus Deepforge - Walk Back To His Post'),
(6031002, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 12, 1, 0, 0, 0, 0, 0, 0,                      'Tormus Deepforge - Face The Player'),
(6031002, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 5, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Tormus Deepforge - Emote Talk'),
(6031002, 9, 2, 0, 0, 0, 100, 0, 100, 100, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Tormus Deepforge - Say Line 1 (I am finished)'),
(6031002, 9, 3, 0, 0, 0, 100, 0, 3900, 3900, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 4.60767,           'Tormus Deepforge - Resume His Usual Facing');

-- Historian Karnik (2916) quest 724
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 2916;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (2916001, 2916002);

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(2916, 0, 1, 0, 19, 0, 100, 0, 3448, 0, 0, 0, 0, 0, 80, 2916002, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                'Historian Karnik - On Quest 3448 Accepted - Run Script'),
--
(2916001, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Historian Karnik - Remove Gossip/Questgiver'),
(2916001, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 2.30863,                 'Historian Karnik - Face Hammertoe spot'),
(2916001, 9, 2, 0, 0, 0, 100, 0, 1000, 1000, 0, 0, 0, 0, 11, 4985, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,              'Historian Karnik - Cast Summon Hammertoe''s Spirit'),
(2916001, 9, 3, 0, 0, 0, 100, 0,0,0,0,0,0,0, 12, 2915, 3, 32000, 0,0,0, 8, 0,0,0, -4633.14, -1324.99, 503.383, 5.447,  'Historian Karnik - Summon Hammertoe''s Spirit (32s, outlasts the scene)'),
(2916001, 9, 4, 0, 0, 0, 100, 0, 3300, 3300, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Historian Karnik - Say Line 2 (CMaNGOS opening, was missing)'),
(2916001, 9, 5, 0, 0, 0, 100, 0, 7000, 7000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 19, 2915, 0, 0, 0, 0, 0, 0,              'Hammertoe Spirit - Say Line 0'),
(2916001, 9, 6, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Historian Karnik - Say Line 0'),
(2916001, 9, 7, 0, 0, 0, 100, 0, 5200, 5200, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 19, 2915, 0, 0, 0, 0, 0, 0,              'Hammertoe Spirit - Say Line 1'),
(2916001, 9, 8, 0, 0, 0, 100, 0, 7000, 7000, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Historian Karnik - Say Line 1'),
(2916001, 9, 9, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 1.76278,                 'Historian Karnik - Restore facing'),
(2916001, 9, 10, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 82, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Historian Karnik - Add Gossip/Questgiver'),
(2916002, 9, 0, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Historian Karnik - Say Line 3');

-- Klockmort Spannerspan (6169) quest 1708 - steps to his bench, works, returns and reports.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 6169;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 6169;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (6169001, 6169002);

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(6169, 0, 0, 1, 20, 0, 100, 0, 1708, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Klockmort Spannerspan - On Quest 1708 Rewarded - Walk, do not run'),
(6169, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 64, 1, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,                         'Klockmort Spannerspan - Link - Remember the player'),
(6169, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Klockmort Spannerspan - Link - Set Active'),
(6169, 0, 3, 4, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Klockmort Spannerspan - Link - Remove Questgiver Flag'),
(6169, 0, 4, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 611, 0, 0, 0, 0, 0, 8, 0, 0, 0, -4814.89, -1250.52, 501.926, 0,   'Klockmort Spannerspan - Link - Walk To The Bench'),
(6169, 0, 5, 0, 34, 0, 100, 0, 8, 611, 0, 0, 0, 0, 80, 6169001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Klockmort Spannerspan - On Reaching The Bench - Run Script'),
(6169, 0, 6, 0, 34, 0, 100, 0, 8, 612, 0, 0, 0, 0, 80, 6169002, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Klockmort Spannerspan - On Returning Home - Run Script'),
--
(6169001, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 17, 173, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                     'Klockmort Spannerspan - Emote State Work'),
(6169001, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Klockmort Spannerspan - Say Line 0 (begins to work)'),
(6169001, 9, 2, 0, 0, 0, 100, 0, 6000, 6000, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Klockmort Spannerspan - Clear Emote State'),
(6169001, 9, 3, 0, 0, 0, 100, 0, 1000, 1000, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Klockmort Spannerspan - Say Line 1 (it is done)'),
(6169001, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 5, 4, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Klockmort Spannerspan - Emote Cheer'),
(6169001, 9, 5, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 12, 1, 0, 0, 0, 0, 0, 0,                      'Klockmort Spannerspan - Face The Player'),
(6169001, 9, 6, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 0, 69, 612, 0,0,0,0,0, 8, 0,0,0, -4812.48, -1250.62, 501.947, 0, 'Klockmort Spannerspan - Walk Back To His Post'),
(6169002, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 4.6251,                  'Klockmort Spannerspan - Resume His Usual Facing'),
(6169002, 9, 1, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 0, 82, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Klockmort Spannerspan - Add Questgiver Flag'),
(6169002, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Klockmort Spannerspan - Clear Active');

-- Talvash del Kissel (6826) quest 2204 - fetches a spell focus from his house and repairs the necklace.
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 6826;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (6826002, 6826003);

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(6826, 0, 1, 0, 22, 0, 100, 512, 264, 0, 0, 0, 0, 0, 33, 6826, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,                  'Talvash del Kissel - On train emote - Give kill credit'),
(6826, 0, 2, 3, 20, 0, 100, 0, 2204, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Talvash del Kissel - On Quest 2204 Rewarded - Walk, do not run'),
(6826, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 80, 6826002, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                   'Talvash del Kissel - Link - Run Script'),
(6826, 0, 4, 0, 34, 0, 100, 0, 8, 623, 0, 0, 0, 0, 80, 6826003, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Talvash del Kissel - On Returning Home - Restore Facing'),
--
(6826002, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Talvash del Kissel - Set Active'),
(6826002, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Talvash del Kissel - Remove Gossip/Questgiver'),
(6826002, 9, 2, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Talvash del Kissel - Say Line 1'),
(6826002, 9, 3, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 621, 0, 0, 0, 0, 0, 8, 0,0,0, -4574.79, -998.688, 503.657, 0,   'Talvash del Kissel - Walk To His House'),
(6826002, 9, 4, 0, 0, 0, 100, 0, 10000, 10000, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                'Talvash del Kissel - Say Line 2'),
(6826002, 9, 5, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Talvash del Kissel - Say Line 3'),
(6826002, 9, 6, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 622, 0, 0, 0, 0, 0, 8, 0,0,0, -4580.49, -993.985, 503.657, 0,   'Talvash del Kissel - Walk To The Bed'),
(6826002, 9, 7, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 5, 5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Talvash del Kissel - Emote Exclamation'),
(6826002, 9, 8, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 0, 5, 16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Talvash del Kissel - Emote Kneel'),
(6826002, 9, 9, 0, 0, 0, 100, 0, 3000, 3000, 0, 0, 0, 0, 1, 4, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Talvash del Kissel - Say Line 4'),
(6826002, 9, 10, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 69, 623, 0, 0, 0, 0, 0, 8, 0,0,0, -4588.30, -1001.21, 503.657, 0,  'Talvash del Kissel - Walk Back To His Post'),
(6826002, 9, 11, 0, 0, 0, 100, 0, 5000, 5000, 0, 0, 0, 0, 11, 9795, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,             'Talvash del Kissel - Cast Necklace Repair'),
(6826002, 9, 12, 0, 0, 0, 100, 0, 8000, 8000, 0, 0, 0, 0, 82, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                'Talvash del Kissel - Add Gossip/Questgiver'),
(6826002, 9, 13, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 5, 4, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Talvash del Kissel - Emote Cheer'),
(6826002, 9, 14, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Talvash del Kissel - Clear Active'),
(6826002, 9, 15, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Talvash del Kissel - Say Line 5'),
(6826003, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 3.3161,                  'Talvash del Kissel - Resume His Usual Facing');

-- Laris Geardawdle (9616) quest 4512 - carries the slime sample over and reacts to it.
DELETE FROM `creature_text` WHERE `CreatureID` = 9616;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(9616, 0, 0, 'Incredible! Amazing! I don''t even know what this means!', 12, 0, 100, 0, 0, 0, 5447, 0, 'Laris Geardawdle - quest 4512');

DELETE FROM `waypoints` WHERE `entry` = 96160;
INSERT INTO `waypoints` (`entry`, `pointid`, `position_x`, `position_y`, `position_z`, `orientation`, `delay`, `point_comment`) VALUES
(96160, 1, -4691.35, -1316.52, 503.381, NULL, 1000, 'Laris Geardawdle - react'),
(96160, 2, -4693.75, -1310.95, 503.381, NULL, 0, 'Laris Geardawdle - over'),
(96160, 3, -4692.07, -1315.18, 503.381, NULL, 0, 'Laris Geardawdle - back'),
(96160, 4, -4692.6, -1312.64, 503.381, 2.9321, 1000, 'Laris Geardawdle - home');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 9616;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 9616;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (9616001, 9616002);

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(9616, 0, 0, 1, 20, 0, 100, 0, 4512, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Laris Geardawdle - On Quest 4512 Rewarded - Walk, do not run'),
(9616, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Laris Geardawdle - Link - Set Active'),
(9616, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Laris Geardawdle - Link - Remove Questgiver Flag'),
(9616, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 232, 96160, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Laris Geardawdle - Link - Start Quest Path'),
(9616, 0, 4, 0, 108, 0, 100, 0, 1, 96160, 0, 0, 0, 0, 80, 9616001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,              'Laris Geardawdle - On Waypoint 1 - Run Script'),
(9616, 0, 5, 0, 108, 0, 100, 0, 4, 96160, 0, 0, 0, 0, 80, 9616002, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,              'Laris Geardawdle - On Waypoint 4 - Run Script'),
--
(9616001, 9, 0, 0, 0, 0, 100, 0, 1000, 1000, 0, 0, 0, 0, 5, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Laris Geardawdle - Emote Talk'),
(9616001, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Laris Geardawdle - Say Line 0'),
(9616002, 9, 0, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                 'Laris Geardawdle - Clear Active'),
(9616002, 9, 1, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 82, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Laris Geardawdle - Add Questgiver Flag');

-- Curator Thorius (8256) quest 3182 - carries the horn to its display and mounts it.
DELETE FROM `gameobject` WHERE `guid` = 1471360;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`, 
`rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`) VALUES
(1471360, 147136, 0, 1, 1, -4629.5, -1279.51, 503.381, 2.32129, 0, 0, 0.91706, 0.39875, -60, 100, 1);

DELETE FROM `waypoints` WHERE `entry` = 82560;
INSERT INTO `waypoints` (`entry`, `pointid`, `position_x`, `position_y`, `position_z`, `orientation`, `delay`, `point_comment`) VALUES
(82560, 1, -4652.28, -1278.88, 503.382, 5.3756, 1000, 'Curator Thorius - set off'),
(82560, 2, -4631.19, -1278.02, 503.382, 5.4803, 22000, 'Curator Thorius - the display');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 8256;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 8256;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` = 8256001;

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(8256, 0, 0, 1, 20, 0, 100, 0, 3182, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Curator Thorius - On Quest 3182 Rewarded - Walk, do not run'),
(8256, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Curator Thorius - Link - Set Active'),
(8256, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                          'Curator Thorius - Link - Say Line 1 (just the spot for this horn)'),
(8256, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 232, 82560, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Curator Thorius - Link - Start Quest Path'),
(8256, 0, 4, 0, 108, 0, 100, 0, 2, 82560, 0, 0, 0, 0, 80, 8256001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,              'Curator Thorius - On Reaching The Display - Run Script'),
--
(8256001, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Curator Thorius - Say Line 2 (saving this spot)'),
(8256001, 9, 1, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 0, 17, 173, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,               'Curator Thorius - Emote State Work'),
(8256001, 9, 2, 0, 0, 0, 100, 0, 10000, 10000, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,               'Curator Thorius - Clear Emote State'),
(8256001, 9, 3, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Curator Thorius - Say Line 3 (all done)'),
(8256001, 9, 4, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 5, 4, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Curator Thorius - Emote Cheer'),
(8256001, 9, 5, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 70, 60, 0, 0, 0, 0, 0, 14, 1471360, 147136, 0, 0, 0, 0, 0,          'Curator Thorius - Reveal The Horn For 60s'),
(8256001, 9, 6, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Curator Thorius - Clear Active'),
(8256001, 9, 7, 0, 0, 0, 100, 0, 8000, 8000, 0, 0, 0, 0, 232, 18870, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,            'Curator Thorius - Resume His Ambient Patrol');

-- Grand Mason Marblesten (2790) quest 689 - carves the Memorial to Sully Balloo
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 2790;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 2790;
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (279001, 279002);
DELETE FROM `smart_scripts` WHERE `source_type` = 9 AND `entryorguid` IN (2790001, 2790002); -- 00_cleanup

INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(2790, 0, 0, 1, 20, 0, 100, 0, 689, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                       'Grand Mason Marblesten - On Quest 689 Rewarded - Walk, do not run'),
(2790, 0, 1, 2, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Grand Mason Marblesten - Link - Clear Emote State'),
(2790, 0, 2, 3, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 48, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Grand Mason Marblesten - Link - Set Active'),
(2790, 0, 3, 4, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 83, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Grand Mason Marblesten - Link - Remove Questgiver Flag'),
(2790, 0, 4, 5, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                          'Grand Mason Marblesten - Link - Say Line 0 (get to work)'),
(2790, 0, 5, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 0, 232, 27901, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Grand Mason Marblesten - Link - Start Quest Path'),
(2790, 0, 6, 0, 108, 0, 100, 0, 6, 27901, 0, 0, 0, 0, 80, 279001, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,               'Grand Mason Marblesten - On Reaching The Memorial - Run Script'),
(2790, 0, 7, 0, 108, 0, 100, 0, 11, 27901, 0, 0, 0, 0, 80, 279002, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,              'Grand Mason Marblesten - On Returning To His Post - Run Script'),
(2790, 0, 8, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 0, 232, 27900, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Grand Mason Marblesten - On Respawn - Start Ambient Path (repeating)'),
--
(279001, 9, 0, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 0, 17, 233, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                'Grand Mason Marblesten - Emote State Work Mining'),
(279001, 9, 1, 0, 0, 0, 100, 0, 5950, 5950, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                  'Grand Mason Marblesten - Stop Working'),
(279001, 9, 2, 0, 0, 0, 100, 0, 50, 50, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 14, 821, 139852, 0, 0, 0, 0, 0,               'Grand Mason Marblesten - Reveal The Memorial'),
(279001, 9, 3, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Grand Mason Marblesten - Say Line 1 (there you have it)'),
(279001, 9, 4, 0, 0, 0, 100, 0, 400, 400, 0, 0, 0, 0, 5, 25, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                    'Grand Mason Marblesten - Point At It (after the line, or the talk animation eats it)'),
(279001, 9, 5, 0, 0, 0, 100, 0, 5600, 5600, 0, 0, 0, 0, 66, 1, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 1.26,               'Grand Mason Marblesten - Turn Around (away from the memorial)'),
(279001, 9, 6, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                         'Grand Mason Marblesten - Say Line 2 (guards will be along)'),
(279001, 9, 7, 0, 0, 0, 100, 0, 2000, 2000, 0, 0, 0, 0, 5, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                   'Grand Mason Marblesten - Emote Bow'),
--
(279002, 9, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 17, 233, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Grand Mason Marblesten - Restore His Working Animation (addon emote 233)'),
(279002, 9, 1, 0, 0, 0, 100, 0, 500, 500, 0, 0, 0, 0, 232, 27900, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,               'Grand Mason Marblesten - Resume His Ambient Path (repeating)'),
(279002, 9, 2, 0, 0, 0, 100, 0, 10, 10, 0, 0, 0, 0, 82, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                      'Grand Mason Marblesten - Add Questgiver Flag'),
(279002, 9, 3, 0, 0, 0, 100, 0, 5, 5, 0, 0, 0, 0, 48, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,                        'Grand Mason Marblesten - Clear Active');

-- Grand Mason Marblesten (2790) - ambient route
UPDATE `creature` SET `MovementType` = 0 WHERE `guid` = 100 AND `id` = 2790;

DELETE FROM `creature_template_addon` WHERE `entry` = 2790;
INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`) VALUES
(2790, 0, 0, 0, 1, 233);

DELETE FROM `waypoint_data` WHERE `id` IN (27900, 27901);
INSERT INTO `waypoint_data` (`id`, `point`, `position_x`, `position_y`, `position_z`, `orientation`, `delay`, `move_type`) VALUES
--
(27900, 1, -5033.81, -1022.23, 508.876, 3.8613, 30000, 0),
(27900, 2, -5031.04, -1019.72, 508.876, NULL, 0, 0),
(27900, 3, -5028.21, -1020.5, 508.876, 5.8955, 30000, 0),
(27900, 4, -5028.75, -1022.3, 508.876, 4.3993, 30000, 0),
(27900, 5, -5031.95, -1021.62, 508.876, NULL, 0, 0),
--
(27901, 1, -5032.05, -1014.75, 509.042, NULL, 0, 0),
(27901, 2, -5030.37, -1009.28, 505.265, NULL, 0, 0),
(27901, 3, -5028.67, -1007.62, 505.264, NULL, 0, 0),
(27901, 4, -5023.85, -1009.02, 502.209, NULL, 0, 0),
(27901, 5, -5022.68, -1009.96, 502.209, NULL, 0, 0),
(27901, 6, -5027.29, -1018.77, 502.209, 4.34651, 26000, 0),
(27901, 7, -5022.68, -1009.96, 502.209, NULL, 0, 0),
(27901, 8, -5023.85, -1009.02, 502.209, NULL, 0, 0),
(27901, 9, -5028.67, -1007.62, 505.264, NULL, 0, 0),
(27901, 10, -5030.37, -1009.28, 505.265, NULL, 0, 0),
(27901, 11, -5032.05, -1014.75, 509.042, NULL, 20, 0);

-- King Magni Bronzebeard 2784 (The scenes themselves are C++ in zone_ironforge.cpp)
--   683  Sara Balloo's Plea     - delay 0,    broadcast 860,   text emote (ChatTypeID 2)
--   8484 The Brokering of Peace - delay 1000, broadcast 11308, yell (ChatTypeID 1)
DELETE FROM `creature_text` WHERE `CreatureID` = 2784 AND `GroupID` = 1;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(2784, 1, 0, 'Let it be known that $n - Alliance $C - has earned the undying respect of Ironforge and the Alliance as a whole.  $GHe : She; has engaged in great diplomacy with Timbermaw Hold and performed valiant actions for them on our behalf.   $GHe : She; has gone above and beyond the call of duty.  Three cheers for $n - a true hero of the Alliance!', 14, 0, 100, 5, 0, 0, 11308, 0, 'King Magni Bronzebeard - The Brokering of Peace');

-- John Turner (6175) - ambient route - stops four times a lap and begs one of four charity lines
DELETE FROM `creature_text` WHERE `CreatureID` = 6175;
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
(6175, 0, 0, 'Give to the charities who seek to help the victims of these hard times! Please.', 12, 0, 100, 0, 0, 0, 2403, 0, 'John Turner - Charity Line'),
(6175, 0, 1, 'Don''t forget the orphans of Stormwind!', 12, 0, 100, 0, 0, 0, 2401, 0, 'John Turner - Charity Line'),
(6175, 0, 2, 'Give of your hearts and your purses! Give to the children of Stormwind who have lost their parents.', 12, 0, 100, 0, 0, 0, 2404, 0, 'John Turner - Charity Line'),
(6175, 0, 3, 'Help the children of Stormwind... victims of the war and plague!', 12, 0, 100, 0, 0, 0, 2402, 0, 'John Turner - Charity Line');

UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 6175;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` = 6175;
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(6175, 0, 0, 0, 108, 0, 100, 1, 1, 1080, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'John Turner - On Waypoint 1 Reached - Say Random Charity Line'),
(6175, 0, 1, 0, 108, 0, 100, 1, 3, 1080, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'John Turner - On Waypoint 3 Reached - Say Random Charity Line'),
(6175, 0, 2, 0, 108, 0, 100, 1, 5, 1080, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'John Turner - On Waypoint 5 Reached - Say Random Charity Line'),
(6175, 0, 3, 0, 108, 0, 100, 1, 6, 1080, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'John Turner - On Waypoint 6 Reached - Say Random Charity Line'),
(6175, 0, 4, 0, 108, 0, 100, 1, 7, 1080, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'John Turner - On Waypoint 7 Reached - Say Random Charity Line');

UPDATE `waypoint_data` SET `delay` = 4000 WHERE `id` = 1080 AND `point` IN (1, 3, 5, 7);
UPDATE `waypoint_data` SET `delay` = 10 WHERE `id` = 1080 AND `point` = 6;

-- Gnome Citizen (39623) - Tinker Town - talks and nods on the spot.
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 39623;
DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (-207205, -207207, -207208);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`,
`event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param6`,
`action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`,
`target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
--
(-207208, 0, 0, 0, 1, 0, 100, 0, 3000, 3000, 13000, 13000, 0, 0, 10, 1, 273, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Gnome Citizen - OOC - Random Talk or Yes Emote'),
(-207207, 0, 0, 0, 1, 0, 100, 0, 3000, 3000, 17000, 17000, 0, 0, 10, 1, 273, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Gnome Citizen - OOC - Random Talk or Yes Emote'),
(-207205, 0, 0, 0, 1, 0, 100, 0, 7000, 7000, 17000, 17000, 0, 0, 10, 273, 6, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Gnome Citizen - OOC - Random Yes or Question Emote');


/* "An Earnest Proposition" - the Dungeon Set 2 turn-in ambush, Alliance half.
 * There are EIGHTEEN of these quests, one per class per faction, all with the same title. 
 * Nine end on Deliana 16013 here in Ironforge (8905-8912, 10492) and nine on Mokvar 16012 in Orgrimmar (8913-8920, 10493). 

 * The scene (`dbscripts_on_quest_end`): three Spectral Stalkers 16093 are summoned at fixed points in front of the questgiver for 120s, 
 * one of them says its line 100ms later, and the questgiver answers after 2s. 
 * Spectral Stalker is faction 14 (hostile to everything), so it attacks the questgiver. 
 * Both the scene and that combat AI are C++ (IndividualProgressionAwareness.cpp): the two questgivers carry `creature_template`.`ScriptName` = 'npc_ipp_ds2', 

 * Only the text lives in SQL.
*/

DELETE FROM `creature_text` WHERE `CreatureID` IN (16012, 16013, 16093);
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`) VALUES
--
(16012, 0, 0, 'Begone foul creatures!  Go back to the void your master shaped you from!', 12, 0, 100, 0, 0, 0, 11895, 0, 'Mokvar - Answers The Ambush'),
(16013, 0, 0, 'Begone foul creatures!  Go back to the void your master shaped you from!', 12, 0, 100, 0, 0, 0, 11895, 0, 'Deliana - Answers The Ambush'),
--
(16093, 0, 0, 'Your fate is sealed, Deliana!  You will pay for your trespasses against Lord Valthalak!', 12, 0, 100, 0, 0, 0, 11879, 0, 'Spectral Stalker - Ambush Taunt (Alliance)'),
(16093, 1, 0, 'Your fate is sealed, Mokvar!  You will pay for your trespasses against Lord Valthalak!', 12, 0, 100, 0, 0, 0, 11893, 0, 'Spectral Stalker - Ambush Taunt (Horde)');
