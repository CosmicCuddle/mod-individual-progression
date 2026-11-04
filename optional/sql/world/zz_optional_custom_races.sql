/*
    IMPORTANT! You only need to run this file if you use custom races. (although, no harm was done if you ran this by mistake)
    This SQL will let these custom races pick up certain quests that would otherwise only be available to the standard races.
*/

-- 1101 (Alliance), 690 (Horde)

-- AQ war effort
UPDATE `quest_template` SET `AllowableRaces` = 690 WHERE `ID` IN (8792, 8793, 8794, 10500) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 1101);
UPDATE `quest_template` SET `AllowableRaces` = 1101 WHERE `ID` IN (8795, 8796, 8797, 10501) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 690);
UPDATE `quest_template` q JOIN `quest_template` p ON p.`ID` = q.`ID` - 100000 SET q.`AllowableRaces` = p.`AllowableRaces` WHERE q.`ID` IN (108850, 108855);

-- AV: Wanted Orcs!
UPDATE `quest_template` SET `AllowableRaces` = 1101 WHERE `ID` = 7402 AND (`AllowableRaces` = 0 OR `AllowableRaces` & 690); -- alliance only

-- SI: Lieutenants, Investigate the Scourge
UPDATE `quest_template` SET `AllowableRaces` = 1101 WHERE `ID` IN (9260, 9261, 9262) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 690); -- Stormwind, Ironforge, Darnassus, Exodar(12817)
UPDATE `quest_template` SET `AllowableRaces` = 690 WHERE `ID` IN (9263, 9264, 9265) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 1101);  -- Orgrimmar, Thunder Bluff, Undercity, Silvermoon(12816)

UPDATE `quest_template` SET `AllowableRaces` = 1101 WHERE `ID` IN (9094, 9317, 9318, 9321, 9337, 9341) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 690);
UPDATE `quest_template` SET `AllowableRaces` = 690 WHERE `ID` IN (9333, 9334, 9335, 9336, 9320, 9343) AND (`AllowableRaces` = 0 OR `AllowableRaces` & 1101);

-- Hinterlands: Troll Necklace Bounty quest should be for Alliance only
UPDATE `quest_template` SET `AllowableRaces` = 1101 WHERE `ID` = 2880 AND (`AllowableRaces` = 0 OR `AllowableRaces` & 690);

-- Westfall: 'A Swift Message' should be for humans only. 1791 = the ten stock playable races; bits other modules added stay.
UPDATE `quest_template` SET `AllowableRaces` = 1 | (`AllowableRaces` & ~1791) WHERE `ID` IN (6181, 6261, 6281, 6285);

-- Blood Knight quests: the pre-2.4 copies take the races of the originals
UPDATE `quest_template` q JOIN `quest_template` p ON p.`ID` = q.`ID` - 100000 SET q.`AllowableRaces` = p.`AllowableRaces` WHERE q.`ID` IN (109681, 109684);
