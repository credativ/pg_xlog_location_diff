CREATE FUNCTION pg_xlog_location_diff(IN pos1 text, IN pos2 text)
       RETURNS numeric
       STRICT
       IMMUTABLE
       AS 'MODULE_PATHNAME', 'pg_xlog_location_diff'
       LANGUAGE C;
