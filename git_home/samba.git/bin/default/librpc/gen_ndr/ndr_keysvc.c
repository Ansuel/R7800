/* parser auto-generated by pidl */

#include "includes.h"
#include "bin/default/librpc/gen_ndr/ndr_keysvc.h"

static enum ndr_err_code ndr_push_keysvc_Unknown0(struct ndr_push *ndr, int flags, const struct keysvc_Unknown0 *r)
{
	NDR_PUSH_CHECK_FN_FLAGS(ndr, flags);
	if (flags & NDR_IN) {
	}
	if (flags & NDR_OUT) {
		NDR_CHECK(ndr_push_WERROR(ndr, NDR_SCALARS, r->out.result));
	}
	return NDR_ERR_SUCCESS;
}

static enum ndr_err_code ndr_pull_keysvc_Unknown0(struct ndr_pull *ndr, int flags, struct keysvc_Unknown0 *r)
{
	NDR_PULL_CHECK_FN_FLAGS(ndr, flags);
	if (flags & NDR_IN) {
	}
	if (flags & NDR_OUT) {
		NDR_CHECK(ndr_pull_WERROR(ndr, NDR_SCALARS, &r->out.result));
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_keysvc_Unknown0(struct ndr_print *ndr, const char *name, int flags, const struct keysvc_Unknown0 *r)
{
	ndr_print_struct(ndr, name, "keysvc_Unknown0");
	if (r == NULL) { ndr_print_null(ndr); return; }
	ndr->depth++;
	if (flags & NDR_SET_VALUES) {
		ndr->flags |= LIBNDR_PRINT_SET_VALUES;
	}
	if (flags & NDR_IN) {
		ndr_print_struct(ndr, "in", "keysvc_Unknown0");
		ndr->depth++;
		ndr->depth--;
	}
	if (flags & NDR_OUT) {
		ndr_print_struct(ndr, "out", "keysvc_Unknown0");
		ndr->depth++;
		ndr_print_WERROR(ndr, "result", r->out.result);
		ndr->depth--;
	}
	ndr->depth--;
}

static const struct ndr_interface_call keysvc_calls[] = {
	{
		"keysvc_Unknown0",
		sizeof(struct keysvc_Unknown0),
		(ndr_push_flags_fn_t) ndr_push_keysvc_Unknown0,
		(ndr_pull_flags_fn_t) ndr_pull_keysvc_Unknown0,
		(ndr_print_function_t) ndr_print_keysvc_Unknown0,
		{ 0, NULL },
		{ 0, NULL },
	},
	{ NULL, 0, NULL, NULL, NULL }
};

static const char * const keysvc_endpoint_strings[] = {
	"ncacn_np:[\\pipe\\keysvc]", 
};

static const struct ndr_interface_string_array keysvc_endpoints = {
	.count	= 1,
	.names	= keysvc_endpoint_strings
};

static const char * const keysvc_authservice_strings[] = {
	"host", 
};

static const struct ndr_interface_string_array keysvc_authservices = {
	.count	= 1,
	.names	= keysvc_authservice_strings
};


const struct ndr_interface_table ndr_table_keysvc = {
	.name		= "keysvc",
	.syntax_id	= {
		{0x8d0ffe72,0xd252,0x11d0,{0xbf,0x8f},{0x00,0xc0,0x4f,0xd9,0x12,0x6b}},
		NDR_KEYSVC_VERSION
	},
	.helpstring	= NDR_KEYSVC_HELPSTRING,
	.num_calls	= 1,
	.calls		= keysvc_calls,
	.endpoints	= &keysvc_endpoints,
	.authservices	= &keysvc_authservices
};

