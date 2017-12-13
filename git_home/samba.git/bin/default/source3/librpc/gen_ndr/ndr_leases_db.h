/* header auto-generated by pidl */

#include "librpc/ndr/libndr.h"
#include "bin/default/source3/librpc/gen_ndr/leases_db.h"

#ifndef _HEADER_NDR_leases_db
#define _HEADER_NDR_leases_db

#define NDR_LEASES_DB_CALL_COUNT (0)
enum ndr_err_code ndr_push_leases_db_key(struct ndr_push *ndr, int ndr_flags, const struct leases_db_key *r);
enum ndr_err_code ndr_pull_leases_db_key(struct ndr_pull *ndr, int ndr_flags, struct leases_db_key *r);
void ndr_print_leases_db_key(struct ndr_print *ndr, const char *name, const struct leases_db_key *r);
enum ndr_err_code ndr_push_leases_db_file(struct ndr_push *ndr, int ndr_flags, const struct leases_db_file *r);
enum ndr_err_code ndr_pull_leases_db_file(struct ndr_pull *ndr, int ndr_flags, struct leases_db_file *r);
void ndr_print_leases_db_file(struct ndr_print *ndr, const char *name, const struct leases_db_file *r);
enum ndr_err_code ndr_push_leases_db_value(struct ndr_push *ndr, int ndr_flags, const struct leases_db_value *r);
enum ndr_err_code ndr_pull_leases_db_value(struct ndr_pull *ndr, int ndr_flags, struct leases_db_value *r);
void ndr_print_leases_db_value(struct ndr_print *ndr, const char *name, const struct leases_db_value *r);
#endif /* _HEADER_NDR_leases_db */
