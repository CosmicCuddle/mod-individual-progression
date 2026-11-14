-- CosmicCuddle Individual Progression: paid receipt anomaly checks.
-- READ-ONLY. SELECT statements only; no DDL/DML. Run against the SAME backup
-- or replica as ip_paid_receipt_audit.sql:
--   mysql -B --skip-column-names <characters_db_copy> < ip_paid_receipt_anomalies.sql > anomalies.tsv
-- Every row is an anomaly that FAILS the release audit. Columns:
-- kind, guid, row count, HEX(source) values ('|'-separated).
-- `s.source = 'ip_paid_expansion'` uses the column collation (the same match
-- IP's strict reader uses), so case/trailing-space variants are included;
-- grouping is by the integer guid only, so nothing collapses by collation.

-- 1. More than one matching row for one character.
SELECT 'duplicate', s.guid, COUNT(*), GROUP_CONCAT(HEX(s.source) ORDER BY HEX(s.source) SEPARATOR '|')
FROM character_settings s
WHERE s.source = 'ip_paid_expansion'
GROUP BY s.guid
HAVING COUNT(*) > 1;

-- 2. A receipt row whose character does not exist.
SELECT 'orphan', s.guid, 1, HEX(s.source)
FROM character_settings s
LEFT JOIN characters c ON c.guid = s.guid
WHERE s.source = 'ip_paid_expansion' AND c.guid IS NULL;

-- 3. A row matched only through the collation (different case/trailing space).
SELECT 'source_variant', s.guid, 1, HEX(s.source)
FROM character_settings s
WHERE s.source = 'ip_paid_expansion' AND BINARY s.source <> BINARY 'ip_paid_expansion';
