#include "postgres.h"
#include "funcapi.h"
#include "access/xlog_internal.h"
#include "utils/builtins.h"
#include "utils/numeric.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#if PG_VERSION_NUM >= 90200
#error "Versions starting with 9.2 already have pg_xlog_location_diff() included"
#endif

/******************************************************************************
 * Private declarations
 ******************************************************************************/
static void
validate_xlog_location(char *str);

/******************************************************************************
 * Public declarations
 ******************************************************************************/
PG_FUNCTION_INFO_V1(pg_xlog_location_diff);
Datum
pg_xlog_location_diff(PG_FUNCTION_ARGS);

/*
 * Validate the text form of a transaction log location.
 * (Just using sscanf() input allows incorrect values such as
 * negatives, so we have to be a bit more careful about that).
 */
static void
validate_xlog_location(char *str)
{
#define MAXLSNCOMPONENT		8

	int			len1,
				len2;

	len1 = strspn(str, "0123456789abcdefABCDEF");
	if (len1 < 1 || len1 > MAXLSNCOMPONENT || str[len1] != '/')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for transaction log location: \"%s\"", str)));

	len2 = strspn(str + len1 + 1, "0123456789abcdefABCDEF");
	if (len2 < 1 || len2 > MAXLSNCOMPONENT || str[len1 + 1 + len2] != '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for transaction log location: \"%s\"", str)));
}

/*
 * Compute the difference in bytes between two WAL locations.
 */
Datum
pg_xlog_location_diff(PG_FUNCTION_ARGS)
{
	text	   *location1 = PG_GETARG_TEXT_P(0);
	text	   *location2 = PG_GETARG_TEXT_P(1);
	char	   *str1,
			   *str2;
	XLogRecPtr	loc1,
				loc2;
	Numeric		result;

	/*
	 * Read and parse input
	 */
	str1 = text_to_cstring(location1);
	str2 = text_to_cstring(location2);

	validate_xlog_location(str1);
	validate_xlog_location(str2);

	if (sscanf(str1, "%X/%X", &loc1.xlogid, &loc1.xrecoff) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("could not parse transaction log location \"%s\"", str1)));
	if (sscanf(str2, "%X/%X", &loc2.xlogid, &loc2.xrecoff) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		   errmsg("could not parse transaction log location \"%s\"", str2)));

	/*
	 * Sanity check
	 */
	if (loc1.xrecoff > XLogFileSize)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("xrecoff \"%X\" is out of valid range, 0..%X", loc1.xrecoff, XLogFileSize)));
	if (loc2.xrecoff > XLogFileSize)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("xrecoff \"%X\" is out of valid range, 0..%X", loc2.xrecoff, XLogFileSize)));

	/*
	 * result = XLogFileSize * (xlogid1 - xlogid2) + xrecoff1 - xrecoff2
	 */
	result = DatumGetNumeric(DirectFunctionCall2(numeric_sub,
	   DirectFunctionCall1(int8_numeric, Int64GetDatum((int64) loc1.xlogid)),
	 DirectFunctionCall1(int8_numeric, Int64GetDatum((int64) loc2.xlogid))));
	result = DatumGetNumeric(DirectFunctionCall2(numeric_mul,
	  DirectFunctionCall1(int8_numeric, Int64GetDatum((int64) XLogFileSize)),
												 NumericGetDatum(result)));
	result = DatumGetNumeric(DirectFunctionCall2(numeric_add,
												 NumericGetDatum(result),
	DirectFunctionCall1(int8_numeric, Int64GetDatum((int64) loc1.xrecoff))));
	result = DatumGetNumeric(DirectFunctionCall2(numeric_sub,
												 NumericGetDatum(result),
	DirectFunctionCall1(int8_numeric, Int64GetDatum((int64) loc2.xrecoff))));

	PG_RETURN_NUMERIC(result);
}

