-- THROWAWAY harness database ONLY (created by run_harness.sh, after Core's
-- base db_characters schema). Never run against an owner/live/backup DB.
CREATE TABLE harness_ctl (fail_stage INT NOT NULL);
INSERT INTO harness_ctl VALUES (0);
CREATE TABLE harness_tail (id INT AUTO_INCREMENT PRIMARY KEY, guid INT UNSIGNED NOT NULL);
DELIMITER //
CREATE TRIGGER harness_fail_characters BEFORE UPDATE ON characters FOR EACH ROW
BEGIN
  IF (SELECT fail_stage FROM harness_ctl LIMIT 1) = 1 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'harness injected failure: characters';
  END IF;
  IF (SELECT fail_stage FROM harness_ctl LIMIT 1) = 3 THEN
    SET @harness_slow = SLEEP(2);
  END IF;
END//
CREATE TRIGGER harness_fail_tail BEFORE INSERT ON harness_tail FOR EACH ROW
BEGIN
  IF (SELECT fail_stage FROM harness_ctl LIMIT 1) = 2 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'harness injected failure: after receipt';
  END IF;
END//
DELIMITER ;
SET SESSION sql_mode = '';
INSERT INTO characters (guid, account, name, race, class, gender, level, money)
VALUES (1001, 1, 'Harnessa', 1, 1, 0, 60, 1000), (1002, 1, 'Harnessb', 1, 1, 0, 60, 1000),
       (1003, 1, 'Harnessc', 1, 1, 0, 60, 1000), (1004, 1, 'Harnessd', 1, 1, 0, 60, 1000),
       (1005, 1, 'Harnesse', 1, 1, 0, 60, 1000), (1006, 1, 'Harnessf', 1, 1, 0, 60, 1000), (1007, 1, 'Harnessg', 1, 1, 0, 60, 1000);
INSERT INTO character_queststatus_rewarded (guid, quest, active)
VALUES (1001, 66007, 1), (1002, 66007, 1), (1003, 66007, 1), (1004, 66007, 1), (1005, 66007, 1), (1006, 66007, 1), (1007, 66007, 1);

-- Keyed save-ordering prototype rev2 (keyed_ordering_harness.cpp):
--   harness_write_seq: per-character marker (seq, fence, owner). It changes only by
--     compare-and-set (trigger): a WRITE keeps fence/owner and sets seq + 1; a CLAIM
--     keeps seq and sets fence + 1 with a new owner. An older-fence write fails.
--   harness_write_log: one row per committed keyed write (exact op identity).
--   harness_purchase_journal: unique purchase id bound to guid + seq + fence + op.
--   harness_fail: an always-failing table for definite statement failures.
CREATE TABLE harness_write_seq (guid INT UNSIGNED NOT NULL PRIMARY KEY, seq BIGINT UNSIGNED NOT NULL,
  fence BIGINT UNSIGNED NOT NULL, owner VARCHAR(64) NOT NULL) ENGINE=InnoDB;
CREATE TABLE harness_write_log (guid INT UNSIGNED NOT NULL, seq BIGINT UNSIGNED NOT NULL,
  fence BIGINT UNSIGNED NOT NULL, op VARCHAR(64) NOT NULL, PRIMARY KEY (guid, seq)) ENGINE=InnoDB;
CREATE TABLE harness_purchase_journal (purchase_id VARCHAR(64) NOT NULL PRIMARY KEY, guid INT UNSIGNED NOT NULL,
  seq BIGINT UNSIGNED NOT NULL, fence BIGINT UNSIGNED NOT NULL, op VARCHAR(64) NOT NULL, KEY idx_guid (guid)) ENGINE=InnoDB;
CREATE TABLE harness_fail (guid INT UNSIGNED NOT NULL) ENGINE=InnoDB;
DELIMITER //
CREATE TRIGGER harness_marker_cas BEFORE UPDATE ON harness_write_seq FOR EACH ROW
BEGIN
  IF NOT ((NEW.fence = OLD.fence AND NEW.owner = OLD.owner AND NEW.seq = OLD.seq + 1)
       OR (NEW.fence = OLD.fence + 1 AND NEW.seq = OLD.seq)) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'harness marker compare-and-set refused';
  END IF;
END//
CREATE TRIGGER harness_fail_always BEFORE INSERT ON harness_fail FOR EACH ROW
BEGIN
  SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'harness injected failure: keyed definite';
END//
DELIMITER ;
INSERT INTO characters (guid, account, name, race, class, gender, level, money)
VALUES (1101, 1, 'Keyed1101', 1, 1, 0, 60, 1000),
       (1102, 1, 'Keyed1102', 1, 1, 0, 60, 1000),
       (1103, 1, 'Keyed1103', 1, 1, 0, 60, 1000),
       (1104, 1, 'Keyed1104', 1, 1, 0, 60, 1000),
       (1105, 1, 'Keyed1105', 1, 1, 0, 60, 1000),
       (1106, 1, 'Keyed1106', 1, 1, 0, 60, 1000),
       (1107, 1, 'Keyed1107', 1, 1, 0, 60, 1000),
       (1108, 1, 'Keyed1108', 1, 1, 0, 60, 1000),
       (1109, 1, 'Keyed1109', 1, 1, 0, 60, 1000),
       (1111, 1, 'Keyed1111', 1, 1, 0, 60, 1000),
       (1112, 1, 'Keyed1112', 1, 1, 0, 60, 1000),
       (1113, 1, 'Keyed1113', 1, 1, 0, 60, 1000),
       (1114, 1, 'Keyed1114', 1, 1, 0, 60, 1000),
       (1121, 1, 'Keyed1121', 1, 1, 0, 60, 1000),
       (1122, 1, 'Keyed1122', 1, 1, 0, 60, 1000),
       (1123, 1, 'Keyed1123', 1, 1, 0, 60, 1000),
       (1124, 1, 'Keyed1124', 1, 1, 0, 60, 1000),
       (1131, 1, 'Keyed1131', 1, 1, 0, 60, 1000),
       (1132, 1, 'Keyed1132', 1, 1, 0, 60, 1000),
       (1133, 1, 'Keyed1133', 1, 1, 0, 60, 1000),
       (1134, 1, 'Keyed1134', 1, 1, 0, 60, 1000),
       (1141, 1, 'Keyed1141', 1, 1, 0, 60, 1000),
       (1142, 1, 'Keyed1142', 1, 1, 0, 60, 1000),
       (1151, 1, 'Keyed1151', 1, 1, 0, 60, 1000),
       (1161, 1, 'Keyed1161', 1, 1, 0, 60, 1000),
       (1162, 1, 'Keyed1162', 1, 1, 0, 60, 1000),
       (1163, 1, 'Keyed1163', 1, 1, 0, 60, 1000),
       (1164, 1, 'Keyed1164', 1, 1, 0, 60, 1000),
       (1165, 1, 'Keyed1165', 1, 1, 0, 60, 1000),
       (1166, 1, 'Keyed1166', 1, 1, 0, 60, 1000),
       (1167, 1, 'Keyed1167', 1, 1, 0, 60, 1000),
       (1168, 1, 'Keyed1168', 1, 1, 0, 60, 1000),
       (1169, 1, 'Keyed1169', 1, 1, 0, 60, 1000),
       (1170, 1, 'Keyed1170', 1, 1, 0, 60, 1000),
       (1171, 1, 'Keyed1171', 1, 1, 0, 60, 1000),
       (1172, 1, 'Keyed1172', 1, 1, 0, 60, 1000),
       (1173, 1, 'Keyed1173', 1, 1, 0, 60, 1000),
       (1174, 1, 'Keyed1174', 1, 1, 0, 60, 1000),
       (1175, 1, 'Keyed1175', 1, 1, 0, 60, 1000),
       (1176, 1, 'Keyed1176', 1, 1, 0, 60, 1000),
       (1177, 1, 'Keyed1177', 1, 1, 0, 60, 1000);
