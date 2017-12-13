/* header auto-generated by pidl */

#include "librpc/ndr/libndr.h"
#include "bin/default/librpc/gen_ndr/negoex.h"

#ifndef _HEADER_NDR_negoex
#define _HEADER_NDR_negoex

#include "../librpc/ndr/ndr_negoex.h"
#define NDR_NEGOEX_UUID "fcc30ddc-98d0-11e5-8a56-83e9a6706f2f"
#define NDR_NEGOEX_VERSION 0.0
#define NDR_NEGOEX_NAME "negoex"
#define NDR_NEGOEX_HELPSTRING "NEGOEX messages"
extern const struct ndr_interface_table ndr_table_negoex;
#define NDR_DECODE_NEGOEX_MESSAGE (0x00)

#define NDR_NEGOEX_CALL_COUNT (1)
enum ndr_err_code ndr_push_negoex_BYTE_VECTOR(struct ndr_push *ndr, int ndr_flags, const struct negoex_BYTE_VECTOR *r);
enum ndr_err_code ndr_pull_negoex_BYTE_VECTOR(struct ndr_pull *ndr, int ndr_flags, struct negoex_BYTE_VECTOR *r);
void ndr_print_negoex_BYTE_VECTOR(struct ndr_print *ndr, const char *name, const struct negoex_BYTE_VECTOR *r);
enum ndr_err_code ndr_push_negoex_AUTH_SCHEME(struct ndr_push *ndr, int ndr_flags, const struct negoex_AUTH_SCHEME *r);
enum ndr_err_code ndr_pull_negoex_AUTH_SCHEME(struct ndr_pull *ndr, int ndr_flags, struct negoex_AUTH_SCHEME *r);
void ndr_print_negoex_AUTH_SCHEME(struct ndr_print *ndr, const char *name, const struct negoex_AUTH_SCHEME *r);
enum ndr_err_code ndr_push_negoex_AUTH_SCHEME_VECTOR(struct ndr_push *ndr, int ndr_flags, const struct negoex_AUTH_SCHEME_VECTOR *r);
enum ndr_err_code ndr_pull_negoex_AUTH_SCHEME_VECTOR(struct ndr_pull *ndr, int ndr_flags, struct negoex_AUTH_SCHEME_VECTOR *r);
void ndr_print_negoex_AUTH_SCHEME_VECTOR(struct ndr_print *ndr, const char *name, const struct negoex_AUTH_SCHEME_VECTOR *r);
void ndr_print_negoex_ExtensionTypes(struct ndr_print *ndr, const char *name, enum negoex_ExtensionTypes r);
enum ndr_err_code ndr_push_negoex_EXTENSION(struct ndr_push *ndr, int ndr_flags, const struct negoex_EXTENSION *r);
enum ndr_err_code ndr_pull_negoex_EXTENSION(struct ndr_pull *ndr, int ndr_flags, struct negoex_EXTENSION *r);
void ndr_print_negoex_EXTENSION(struct ndr_print *ndr, const char *name, const struct negoex_EXTENSION *r);
enum ndr_err_code ndr_push_negoex_EXTENSION_VECTOR(struct ndr_push *ndr, int ndr_flags, const struct negoex_EXTENSION_VECTOR *r);
enum ndr_err_code ndr_pull_negoex_EXTENSION_VECTOR(struct ndr_pull *ndr, int ndr_flags, struct negoex_EXTENSION_VECTOR *r);
void ndr_print_negoex_EXTENSION_VECTOR(struct ndr_print *ndr, const char *name, const struct negoex_EXTENSION_VECTOR *r);
void ndr_print_negoex_ChecksumSchemes(struct ndr_print *ndr, const char *name, enum negoex_ChecksumSchemes r);
void ndr_print_negoex_CHECKSUM(struct ndr_print *ndr, const char *name, const struct negoex_CHECKSUM *r);
void ndr_print_negoex_AlertReason(struct ndr_print *ndr, const char *name, enum negoex_AlertReason r);
enum ndr_err_code ndr_push_negoex_ALERT_PULSE(struct ndr_push *ndr, int ndr_flags, const struct negoex_ALERT_PULSE *r);
enum ndr_err_code ndr_pull_negoex_ALERT_PULSE(struct ndr_pull *ndr, int ndr_flags, struct negoex_ALERT_PULSE *r);
void ndr_print_negoex_ALERT_PULSE(struct ndr_print *ndr, const char *name, const struct negoex_ALERT_PULSE *r);
void ndr_print_negoex_AlertTypes(struct ndr_print *ndr, const char *name, enum negoex_AlertTypes r);
enum ndr_err_code ndr_push_negoex_ALERT(struct ndr_push *ndr, int ndr_flags, const struct negoex_ALERT *r);
enum ndr_err_code ndr_pull_negoex_ALERT(struct ndr_pull *ndr, int ndr_flags, struct negoex_ALERT *r);
void ndr_print_negoex_ALERT(struct ndr_print *ndr, const char *name, const struct negoex_ALERT *r);
enum ndr_err_code ndr_push_negoex_ALERT_VECTOR(struct ndr_push *ndr, int ndr_flags, const struct negoex_ALERT_VECTOR *r);
enum ndr_err_code ndr_pull_negoex_ALERT_VECTOR(struct ndr_pull *ndr, int ndr_flags, struct negoex_ALERT_VECTOR *r);
void ndr_print_negoex_ALERT_VECTOR(struct ndr_print *ndr, const char *name, const struct negoex_ALERT_VECTOR *r);
enum ndr_err_code ndr_push_negoex_MESSAGE_TYPE(struct ndr_push *ndr, int ndr_flags, enum negoex_MESSAGE_TYPE r);
enum ndr_err_code ndr_pull_negoex_MESSAGE_TYPE(struct ndr_pull *ndr, int ndr_flags, enum negoex_MESSAGE_TYPE *r);
void ndr_print_negoex_MESSAGE_TYPE(struct ndr_print *ndr, const char *name, enum negoex_MESSAGE_TYPE r);
void ndr_print_negoex_NEGO_PAYLOAD(struct ndr_print *ndr, const char *name, const struct negoex_NEGO_PAYLOAD *r);
void ndr_print_negoex_EXCHANGE_PAYLOAD(struct ndr_print *ndr, const char *name, const struct negoex_EXCHANGE_PAYLOAD *r);
void ndr_print_negoex_VERIFY_PAYLOAD(struct ndr_print *ndr, const char *name, const struct negoex_VERIFY_PAYLOAD *r);
void ndr_print_negoex_ALERT_PAYLOAD(struct ndr_print *ndr, const char *name, const struct negoex_ALERT_PAYLOAD *r);
enum ndr_err_code ndr_push_negoex_PAYLOAD(struct ndr_push *ndr, int ndr_flags, const union negoex_PAYLOAD *r);
enum ndr_err_code ndr_pull_negoex_PAYLOAD(struct ndr_pull *ndr, int ndr_flags, union negoex_PAYLOAD *r);
void ndr_print_negoex_PAYLOAD(struct ndr_print *ndr, const char *name, const union negoex_PAYLOAD *r);
enum ndr_err_code ndr_push_negoex_MESSAGE(struct ndr_push *ndr, int ndr_flags, const struct negoex_MESSAGE *r);
enum ndr_err_code ndr_pull_negoex_MESSAGE(struct ndr_pull *ndr, int ndr_flags, struct negoex_MESSAGE *r);
void ndr_print_negoex_MESSAGE(struct ndr_print *ndr, const char *name, const struct negoex_MESSAGE *r);
size_t ndr_size_negoex_MESSAGE(const struct negoex_MESSAGE *r, int flags);
enum ndr_err_code ndr_push_negoex_MESSAGE_ARRAY(struct ndr_push *ndr, int ndr_flags, const struct negoex_MESSAGE_ARRAY *r);
enum ndr_err_code ndr_pull_negoex_MESSAGE_ARRAY(struct ndr_pull *ndr, int ndr_flags, struct negoex_MESSAGE_ARRAY *r);
void ndr_print_negoex_MESSAGE_ARRAY(struct ndr_print *ndr, const char *name, const struct negoex_MESSAGE_ARRAY *r);
void ndr_print_decode_negoex_MESSAGE(struct ndr_print *ndr, const char *name, int flags, const struct decode_negoex_MESSAGE *r);
#endif /* _HEADER_NDR_negoex */
