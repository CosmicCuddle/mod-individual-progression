-- CosmicCuddle Individual Progression: paid expansion receipt audit.
-- READ-ONLY. A single SELECT; no DDL/DML. Run against a BACKUP or replica of
-- the characters database, never as a step of any migration, e.g.:
--   mysql -B --skip-column-names <characters_db> < ip_paid_receipt_audit.sql > receipts.tsv
--   python3 ip_paid_receipt_audit.py receipts.tsv
-- Columns: guid, account, row_present (0/1), HEX(data) (exact bytes, so
-- trailing spaces survive export), highest rewarded IP stage marker quest
-- (66001-66018) or 0.
SELECT c.guid,
       c.account,
       IF(s.guid IS NULL, 0, 1) AS row_present,
       IFNULL(HEX(s.data), '') AS data_hex,
       IFNULL(MAX(q.quest), 0) AS max_marker
FROM characters c
LEFT JOIN character_settings s
       ON s.guid = c.guid AND s.source = 'ip_paid_expansion'
LEFT JOIN character_queststatus_rewarded q
       ON q.guid = c.guid AND q.quest BETWEEN 66001 AND 66018
GROUP BY c.guid, c.account, s.guid, s.data
ORDER BY c.guid;
