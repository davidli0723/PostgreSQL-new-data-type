/*
 * src/tutorial/postadd.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include "fmgr.h"
#include "regex.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "access/hash.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

typedef struct postaddress
{
	int32	length;
	char 	data[FLEXIBLE_ARRAY_MEMBER];
}	postaddress;

int regexMatch(char *str, char *pattern);
void invalid_postadd_msg(char *str);

int regexMatch(char *str, char *pattern) {
	regex_t regex;
	int ret;
	ret = regcomp(&regex, pattern, REG_EXTENDED);
	if (ret) {
		return -1;
	}
	ret = regexec(&regex, str, 0, NULL, 0);

	regfree(&regex);
	if (ret == REG_NOMATCH) {
		return 0;
	}
	return 1;
}

void invalid_postadd_msg(char *str) {
	ereport(ERROR,(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type PostAddress: \"%s\"", str)));
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(postadd_in);

Datum
postadd_in(PG_FUNCTION_ARGS)
{
	char		*str = PG_GETARG_CSTRING(0);
	char		*address = pstrdup(str);
	char		*unit = NULL;
	char		*street = NULL;
	char		*suburb = NULL;
	char		*state_postcode = NULL;
	char		*address_ptr;

	int		total_size;
	postaddress	*result;
	// find if there is unit in address and check format
   	address_ptr = strchr(address, '/');
    	if (address_ptr != NULL) {
        	*address_ptr = '\0';
        	unit = address;
        	address = ++address_ptr;
        	if (!regexMatch(unit, "^[a-zA-Z]{1}[0-9]+$")) {
			invalid_postadd_msg(str);
		}
    	}

	//find if there is street in address and check format
	address_ptr = strchr(address, ',');
	if (address_ptr == NULL) {
		invalid_postadd_msg(str);	
	}
	*address_ptr = '\0'; 
     	street = address;
    	address = ++address_ptr;
	
	if (!regexMatch(street, "^[0-9]+[ ]{1}[a-zA-Z]+([ ]{1}[a-zA-Z]+)*$")) {
		invalid_postadd_msg(str);	
	}
	
	//find if there is suburb in address and check format
	address_ptr = strchr(address, ',');
	if (address_ptr == NULL) {
		invalid_postadd_msg(str);	
	}
	*address_ptr = '\0';
	suburb = address;
	address = ++address_ptr;

	if (!regexMatch(suburb, "^[ ]{1}[a-zA-Z]+([ ]{1}[a-zA-Z]+)*$")) {
		invalid_postadd_msg(str);	
	}

	//check format of state and postcode
	state_postcode = address;

	if (!regexMatch(state_postcode, "^[ ]{1}[A-Z]{2}[ ]{1}[0-9]{4}$")) {
		invalid_postadd_msg(str);	
	}
	
	total_size = strlen(str) + 1 + VARHDRSZ;
	result = (postaddress *) palloc(total_size);
	SET_VARSIZE(result, total_size);
	memcpy(result->data, str, strlen(str) + 1);
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(postadd_out);

Datum
postadd_out(PG_FUNCTION_ARGS)
{
	postaddress	*address = (postaddress *) PG_GETARG_POINTER(0);
	char		*result;
	// result = psprintf("%s", address->data);
	result = pstrdup(address->data);
	PG_RETURN_CSTRING(result);
}


PG_FUNCTION_INFO_V1(show_postcode);

Datum
show_postcode(PG_FUNCTION_ARGS)
{
	postaddress	*a = (postaddress *) PG_GETARG_POINTER(0);
	char 		*postcode = pstrdup(a->data);
	int 		address_len = strlen(postcode);
	postcode += address_len - 4;
	
	PG_RETURN_TEXT_P(cstring_to_text(postcode));
}

PG_FUNCTION_INFO_V1(show_unit);

Datum
show_unit(PG_FUNCTION_ARGS)
{
	postaddress	*a = (postaddress *) PG_GETARG_POINTER(0);
	char 		*unit = pstrdup(a->data);
	char		*address_ptr;
	address_ptr = strchr(unit, '/');
	if (address_ptr == NULL) {
		unit = "NULL";
	}
	else {
		*address_ptr = '\0';
	}
	PG_RETURN_TEXT_P(cstring_to_text(unit));
}

PG_FUNCTION_INFO_V1(show);

Datum
show(PG_FUNCTION_ARGS)
{
	postaddress	*a = (postaddress *) PG_GETARG_POINTER(0);
	char 		*address = pstrdup(a->data);
	char		*address_end_ptr;
	char		*address_start_ptr;
	char 		*comma;
	char		*state;
	int		address_len;
	
	address_len = strlen(address);
	address_end_ptr = address;
	address_end_ptr += address_len - 5;
	*address_end_ptr = '\0';
	
	address_start_ptr = strchr(address, ' ');
	*address_start_ptr = '\0';
	address = ++address_start_ptr;

	comma = strchr(address, ',');
	*comma = '\0';
	state = ++comma;
	comma = strchr(state, ',');
	state = comma;
	
	strcat(address, state);

	PG_RETURN_TEXT_P(cstring_to_text(address));
}

static int
postaddress_abs_cmp_internal(postaddress * a, postaddress * b)
{
	char	*addressA = strdup(a->data);
	char	*addressB = strdup(b->data);
	char	*unitA = NULL;
	char	*unitB = NULL;
	char	*streetA;
	char	*streetB;
	char	*suburbA;
	char	*suburbB;
	char	*stateA;
	char	*stateB;
	char	*address_ptr;

	int result;
	address_ptr = strchr(addressA, '/');
	if (address_ptr != NULL) {
		*address_ptr = '\0';
		unitA = addressA;
		addressA = ++address_ptr;
	}
	address_ptr = strchr(addressB, '/');
	if (address_ptr != NULL) {
		*address_ptr = '\0';
		unitB = addressB;
		addressB = ++address_ptr;
	}

	address_ptr = strchr(addressA, ',');
	*address_ptr = '\0';
	streetA = addressA;
	address_ptr += 2;
	addressA = address_ptr;

	address_ptr = strchr(addressB, ',');
	*address_ptr = '\0';
	streetB = addressB;
	address_ptr += 2;
	addressB = address_ptr;

	address_ptr = strchr(addressA, ',');
	*address_ptr = '\0';
	suburbA = addressA;
	address_ptr += 2;
	addressA = address_ptr;

	address_ptr = strchr(addressB, ',');
	*address_ptr = '\0';
	suburbB = addressB;
	address_ptr += 2;
	addressB = address_ptr;

	address_ptr = strchr(addressA, ' ');
	*address_ptr = '\0';
	stateA = addressA;

	address_ptr = strchr(addressB, ' ');
	*address_ptr = '\0';
	stateB = addressB;

	result = strcasecmp(stateA, stateB);
	if (result > 0) {
		return 2;
	}
	else if (result < 0) {
		return -2;
	}

	result = strcasecmp(suburbA, suburbB);
	if (result > 0) {
		return 2;
	}
	else if (result < 0) {
		return -2;
	}

	result = strcasecmp(streetA, streetB);
	if (result > 0) {
		return 1;
	}
	else if (result < 0) {
		return -1;
	}

	if (unitA != NULL && unitB == NULL) {
		return 1;
	}
	if (unitA == NULL && unitB != NULL) {
		return -1;
	}

	if (unitA == NULL && unitB == NULL) {
		return 0;
	}

	result = strcasecmp(unitA, unitB);
	if (result > 0) {
		return 1;
	}
	else if (result < 0) {
		return -1;
	}

	return 0;
}


PG_FUNCTION_INFO_V1(postaddress_abs_lt);

Datum
postaddress_abs_lt(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_le);

Datum
postaddress_abs_le(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_eq);

Datum
postaddress_abs_eq(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_ne);

Datum
postaddress_abs_ne(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) != 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_ge);

Datum
postaddress_abs_ge(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_gt);

Datum
postaddress_abs_gt(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(postaddress_abs_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(postaddress_abs_ti);

Datum
postaddress_abs_ti(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	int tilde_bool;
	if (postaddress_abs_cmp_internal(a, b) == 2 || postaddress_abs_cmp_internal(a, b) == -2) {
		tilde_bool = 0;
	} else {
		tilde_bool = 1;
	}
	
	PG_RETURN_BOOL(tilde_bool);
}

PG_FUNCTION_INFO_V1(postaddress_abs_nt);

Datum
postaddress_abs_nt(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	int not_tilde_bool;
	if (postaddress_abs_cmp_internal(a, b) == 2 || postaddress_abs_cmp_internal(a, b) == -2) {
		not_tilde_bool = 1;
	} else {
		not_tilde_bool = 0;
	}
	
	PG_RETURN_BOOL(not_tilde_bool);
}

PG_FUNCTION_INFO_V1(postaddress_abs_cmp);

Datum
postaddress_abs_cmp(PG_FUNCTION_ARGS)
{
	postaddress    *a = (postaddress *) PG_GETARG_POINTER(0);
	postaddress    *b = (postaddress *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(postaddress_abs_cmp_internal(a, b));
}

PG_FUNCTION_INFO_V1(postaddress_hash);

Datum
postaddress_hash(PG_FUNCTION_ARGS)
{
	postaddress	*a = (postaddress *) PG_GETARG_POINTER(0);
	PG_RETURN_INT32(DatumGetUInt32(hash_any((unsigned char *) a->data, strlen(a->data))));
}

