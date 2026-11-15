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
