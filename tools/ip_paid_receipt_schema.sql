-- CosmicCuddle Individual Progression: character_settings key verification.
-- READ-ONLY. SELECT statements on information_schema only. Run against the
-- SAME backup or replica (as its current database):
--   mysql -B --skip-column-names <characters_db_copy> < ip_paid_receipt_schema.sql > schema.tsv
-- Columns: kind, name, value. The release audit requires the primary key to
-- be exactly (guid, source), as in Core's base schema.
SELECT 'primary_key', 'character_settings',
       IFNULL(GROUP_CONCAT(k.COLUMN_NAME ORDER BY k.ORDINAL_POSITION SEPARATOR ','), '')
FROM information_schema.KEY_COLUMN_USAGE k
WHERE k.TABLE_SCHEMA = DATABASE() AND k.TABLE_NAME = 'character_settings'
  AND k.CONSTRAINT_NAME = 'PRIMARY';

SELECT 'collation', c.COLUMN_NAME, IFNULL(c.COLLATION_NAME, '')
FROM information_schema.COLUMNS c
WHERE c.TABLE_SCHEMA = DATABASE() AND c.TABLE_NAME = 'character_settings'
  AND c.COLUMN_NAME IN ('source', 'data')
ORDER BY c.COLUMN_NAME;
