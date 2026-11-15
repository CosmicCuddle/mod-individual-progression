-- CosmicCuddle Individual Progression: paid expansion receipt audit (main).
-- READ-ONLY. A single SELECT; no DDL/DML. Run against a BACKUP or replica of
-- the characters database, never as a step of any migration, e.g.:
--   mysql -B --skip-column-names <characters_db_copy> < ip_paid_receipt_audit.sql > receipts.tsv
-- One output row PER matching settings row (no GROUP BY, so duplicate or
-- collation-variant rows can never collapse), plus one row with
-- row_present = 0 for a character with none.
-- Columns: guid, account, row_present (0/1), HEX(source), HEX(data) (exact
-- bytes, so case and trailing spaces survive export), highest rewarded IP
-- stage marker quest (66001-66018) or 0.
SELECT c.guid,
       c.account,
       IF(s.guid IS NULL, 0, 1) AS row_present,
       IFNULL(HEX(s.source), '') AS source_hex,
       IFNULL(HEX(s.data), '') AS data_hex,
       IFNULL((SELECT MAX(q.quest) FROM character_queststatus_rewarded q
               WHERE q.guid = c.guid AND q.quest BETWEEN 66001 AND 66018), 0) AS max_marker
FROM characters c
LEFT JOIN character_settings s
       ON s.guid = c.guid AND s.source = 'ip_paid_expansion'
ORDER BY c.guid;
