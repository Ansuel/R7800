/*
 *  GSSAPI Security Extensions
 *  RPC Pipe client and server routines
 *  Copyright (C) Simo Sorce 2010.
 *  Copyright (C) Andrew Bartlett 2004-2011.
 *  Copyright (C) Stefan Metzmacher <metze@samba.org> 2004-2005
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* We support only GSSAPI/KRB5 here */

#include "includes.h"
#include "gse.h"
#include "libads/kerberos_proto.h"
#include "auth/common_auth.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_internal.h"
#include "auth/credentials/credentials.h"
#include "../librpc/gen_ndr/dcerpc.h"

#if defined(HAVE_KRB5)

#include "auth/kerberos/pac_utils.h"
#include "auth/kerberos/gssapi_helper.h"
#include "gse_krb5.h"

static char *gse_errstr(TALLOC_CTX *mem_ctx, OM_uint32 maj, OM_uint32 min);
static size_t gensec_gse_sig_size(struct gensec_security *gensec_security,
				  size_t data_size);

struct gse_context {
	gss_ctx_id_t gssapi_context;
	gss_name_t server_name;
	gss_name_t client_name;
	OM_uint32 gss_want_flags, gss_got_flags;
	size_t max_wrap_buf_size;
	size_t sig_size;

	gss_cred_id_t delegated_cred_handle;

	NTTIME expire_time;

	/* gensec_gse only */
	krb5_context k5ctx;
	krb5_ccache ccache;
	krb5_keytab keytab;

	gss_OID_desc gss_mech;
	gss_cred_id_t creds;

	gss_OID ret_mech;
};

/* free non talloc dependent contexts */
static int gse_context_destructor(void *ptr)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_min;

	gse_ctx = talloc_get_type_abort(ptr, struct gse_context);
	if (gse_ctx->k5ctx) {
		if (gse_ctx->ccache) {
			krb5_cc_close(gse_ctx->k5ctx, gse_ctx->ccache);
			gse_ctx->ccache = NULL;
		}
		if (gse_ctx->keytab) {
			krb5_kt_close(gse_ctx->k5ctx, gse_ctx->keytab);
			gse_ctx->keytab = NULL;
		}
		krb5_free_context(gse_ctx->k5ctx);
		gse_ctx->k5ctx = NULL;
	}
	if (gse_ctx->gssapi_context != GSS_C_NO_CONTEXT) {
		(void)gss_delete_sec_context(&gss_min,
						 &gse_ctx->gssapi_context,
						 GSS_C_NO_BUFFER);
	}
	if (gse_ctx->server_name) {
		(void)gss_release_name(&gss_min,
					   &gse_ctx->server_name);
	}
	if (gse_ctx->client_name) {
		(void)gss_release_name(&gss_min,
					   &gse_ctx->client_name);
	}
	if (gse_ctx->creds) {
		(void)gss_release_cred(&gss_min,
					   &gse_ctx->creds);
	}
	if (gse_ctx->delegated_cred_handle) {
		(void)gss_release_cred(&gss_min,
					   &gse_ctx->delegated_cred_handle);
	}

	/* MIT and Heimdal differ as to if you can call
	 * gss_release_oid() on this OID, generated by
	 * gss_{accept,init}_sec_context().  However, as long as the
	 * oid is gss_mech_krb5 (which it always is at the moment),
	 * then this is a moot point, as both declare this particular
	 * OID static, and so no memory is lost.  This assert is in
	 * place to ensure that the programmer who wishes to extend
	 * this code to EAP or other GSS mechanisms determines an
	 * implementation-dependent way of releasing any dynamically
	 * allocated OID */
	SMB_ASSERT(smb_gss_oid_equal(&gse_ctx->gss_mech, GSS_C_NO_OID) ||
		   smb_gss_oid_equal(&gse_ctx->gss_mech, gss_mech_krb5));

	return 0;
}

static NTSTATUS gse_setup_server_principal(TALLOC_CTX *mem_ctx,
					   const char *target_principal,
					   const char *service,
					   const char *hostname,
					   const char *realm,
					   char **pserver_principal,
					   gss_name_t *pserver_name)
{
	char *server_principal = NULL;
	gss_buffer_desc name_token;
	gss_OID name_type;
	OM_uint32 maj_stat, min_stat = 0;

	if (target_principal != NULL) {
		server_principal = talloc_strdup(mem_ctx, target_principal);
		name_type = GSS_C_NULL_OID;
	} else {
		server_principal = talloc_asprintf(mem_ctx,
						   "%s/%s@%s",
						   service,
						   hostname,
						   realm);
		name_type = GSS_C_NT_USER_NAME;
	}
	if (server_principal == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	name_token.value = (uint8_t *)server_principal;
	name_token.length = strlen(server_principal);

	maj_stat = gss_import_name(&min_stat,
				   &name_token,
				   name_type,
				   pserver_name);
	if (maj_stat) {
		DBG_WARNING("GSS Import name of %s failed: %s\n",
			    server_principal,
			    gse_errstr(mem_ctx, maj_stat, min_stat));
		TALLOC_FREE(server_principal);
		return NT_STATUS_INVALID_PARAMETER;
	}

	*pserver_principal = server_principal;

	return NT_STATUS_OK;
}

static NTSTATUS gse_context_init(TALLOC_CTX *mem_ctx,
				 bool do_sign, bool do_seal,
				 const char *ccache_name,
				 uint32_t add_gss_c_flags,
				 struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	krb5_error_code k5ret;
	NTSTATUS status;

	gse_ctx = talloc_zero(mem_ctx, struct gse_context);
	if (!gse_ctx) {
		return NT_STATUS_NO_MEMORY;
	}
	talloc_set_destructor((TALLOC_CTX *)gse_ctx, gse_context_destructor);

	gse_ctx->expire_time = GENSEC_EXPIRE_TIME_INFINITY;
	gse_ctx->max_wrap_buf_size = UINT16_MAX;

	memcpy(&gse_ctx->gss_mech, gss_mech_krb5, sizeof(gss_OID_desc));

	gse_ctx->gss_want_flags = GSS_C_MUTUAL_FLAG |
				GSS_C_DELEG_POLICY_FLAG |
				GSS_C_REPLAY_FLAG |
				GSS_C_SEQUENCE_FLAG;
	if (do_sign) {
		gse_ctx->gss_want_flags |= GSS_C_INTEG_FLAG;
	}
	if (do_seal) {
		gse_ctx->gss_want_flags |= GSS_C_INTEG_FLAG;
		gse_ctx->gss_want_flags |= GSS_C_CONF_FLAG;
	}

	gse_ctx->gss_want_flags |= add_gss_c_flags;

	/* Initialize Kerberos Context */
	initialize_krb5_error_table();

	k5ret = krb5_init_context(&gse_ctx->k5ctx);
	if (k5ret) {
		DEBUG(0, ("Failed to initialize kerberos context! (%s)\n",
			  error_message(k5ret)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	if (!ccache_name) {
		ccache_name = krb5_cc_default_name(gse_ctx->k5ctx);
	}
	k5ret = krb5_cc_resolve(gse_ctx->k5ctx, ccache_name,
				&gse_ctx->ccache);
	if (k5ret) {
		DEBUG(1, ("Failed to resolve credential cache '%s'! (%s)\n",
			  ccache_name, error_message(k5ret)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

	/* TODO: Should we enforce a enc_types list ?
	ret = krb5_set_default_tgs_ktypes(gse_ctx->k5ctx, enc_types);
	*/

	*_gse_ctx = gse_ctx;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(gse_ctx);
	return status;
}

static NTSTATUS gse_init_client(TALLOC_CTX *mem_ctx,
				bool do_sign, bool do_seal,
				const char *ccache_name,
				const char *server,
				const char *service,
				const char *realm,
				const char *username,
				const char *password,
				uint32_t add_gss_c_flags,
				struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_maj, gss_min;
#ifdef HAVE_GSS_KRB5_CRED_NO_CI_FLAGS_X
	gss_buffer_desc empty_buffer = GSS_C_EMPTY_BUFFER;
	gss_OID oid = discard_const(GSS_KRB5_CRED_NO_CI_FLAGS_X);
#endif
	NTSTATUS status;

	if (!server || !service) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = gse_context_init(mem_ctx, do_sign, do_seal,
				  ccache_name, add_gss_c_flags,
				  &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_MEMORY;
	}

	/* TODO: get krb5 ticket using username/password, if no valid
	 * one already available in ccache */

	gss_maj = smb_gss_krb5_import_cred(&gss_min,
					   gse_ctx->k5ctx,
					   gse_ctx->ccache,
					   NULL, /* keytab_principal */
					   NULL, /* keytab */
					   &gse_ctx->creds);
	if (gss_maj) {
		char *ccache = NULL;
		int kret;

		kret = krb5_cc_get_full_name(gse_ctx->k5ctx,
					     gse_ctx->ccache,
					     &ccache);
		if (kret != 0) {
			ccache = NULL;
		}

		DEBUG(5, ("smb_gss_krb5_import_cred ccache[%s] failed with [%s] -"
			  "the caller may retry after a kinit.\n",
			  ccache, gse_errstr(gse_ctx, gss_maj, gss_min)));
		SAFE_FREE(ccache);
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}

#ifdef HAVE_GSS_KRB5_CRED_NO_CI_FLAGS_X
	/*
	 * Don't force GSS_C_CONF_FLAG and GSS_C_INTEG_FLAG.
	 *
	 * This allows us to disable SIGN and SEAL for
	 * AUTH_LEVEL_CONNECT and AUTH_LEVEL_INTEGRITY.
	 *
	 * https://groups.yahoo.com/neo/groups/cat-ietf/conversations/topics/575
	 * http://krbdev.mit.edu/rt/Ticket/Display.html?id=6938
	 */
	gss_maj = gss_set_cred_option(&gss_min, &gse_ctx->creds,
				      oid,
				      &empty_buffer);
	if (gss_maj) {
		DEBUG(0, ("gss_set_cred_option(GSS_KRB5_CRED_NO_CI_FLAGS_X), "
			  "failed with [%s]\n",
			  gse_errstr(gse_ctx, gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto err_out;
	}
#endif

	*_gse_ctx = gse_ctx;
	return NT_STATUS_OK;

err_out:
	TALLOC_FREE(gse_ctx);
	return status;
}

static NTSTATUS gse_get_client_auth_token(TALLOC_CTX *mem_ctx,
					  struct gensec_security *gensec_security,
					  const DATA_BLOB *token_in,
					  DATA_BLOB *token_out)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gse_context);
	OM_uint32 gss_maj, gss_min;
	gss_buffer_desc in_data;
	gss_buffer_desc out_data;
	DATA_BLOB blob = data_blob_null;
	NTSTATUS status;
	OM_uint32 time_rec = 0;
	struct timeval tv;
	struct cli_credentials *cli_creds = gensec_get_credentials(gensec_security);
	const char *target_principal = gensec_get_target_principal(gensec_security);
	const char *hostname = gensec_get_target_hostname(gensec_security);
	const char *service = gensec_get_target_service(gensec_security);
	const char *client_realm = cli_credentials_get_realm(cli_creds);
	char *server_principal = NULL;
	char *server_realm = NULL;
	bool fallback = false;

	in_data.value = token_in->data;
	in_data.length = token_in->length;

	/*
	 * With credentials for administrator@FOREST1.EXAMPLE.COM this patch
	 * changes the target_principal for the ldap service of host
	 * dc2.forest2.example.com from
	 *
	 *   ldap/dc2.forest2.example.com@FOREST1.EXAMPLE.COM
	 *
	 * to
	 *
	 *   ldap/dc2.forest2.example.com@FOREST2.EXAMPLE.COM
	 *
	 * Typically ldap/dc2.forest2.example.com@FOREST1.EXAMPLE.COM should be
	 * used in order to allow the KDC of FOREST1.EXAMPLE.COM to generate a
	 * referral ticket for krbtgt/FOREST2.EXAMPLE.COM@FOREST1.EXAMPLE.COM.
	 *
	 * The problem is that KDCs only return such referral tickets if
	 * there's a forest trust between FOREST1.EXAMPLE.COM and
	 * FOREST2.EXAMPLE.COM. If there's only an external domain trust
	 * between FOREST1.EXAMPLE.COM and FOREST2.EXAMPLE.COM the KDC of
	 * FOREST1.EXAMPLE.COM will respond with S_PRINCIPAL_UNKNOWN when being
	 * asked for ldap/dc2.forest2.example.com@FOREST1.EXAMPLE.COM.
	 *
	 * In the case of an external trust the client can still ask explicitly
	 * for krbtgt/FOREST2.EXAMPLE.COM@FOREST1.EXAMPLE.COM and the KDC of
	 * FOREST1.EXAMPLE.COM will generate it.
	 *
	 * From there the client can use the
	 * krbtgt/FOREST2.EXAMPLE.COM@FOREST1.EXAMPLE.COM ticket and ask a KDC
	 * of FOREST2.EXAMPLE.COM for a service ticket for
	 * ldap/dc2.forest2.example.com@FOREST2.EXAMPLE.COM.
	 *
	 * With Heimdal we'll get the fallback on S_PRINCIPAL_UNKNOWN behavior
	 * when we pass ldap/dc2.forest2.example.com@FOREST2.EXAMPLE.COM as
	 * target principal. As _krb5_get_cred_kdc_any() first calls
	 * get_cred_kdc_referral() (which always starts with the client realm)
	 * and falls back to get_cred_kdc_capath() (which starts with the given
	 * realm).
	 *
	 * MIT krb5 only tries the given realm of the target principal, if we
	 * want to autodetect support for transitive forest trusts, would have
	 * to do the fallback ourself.
	 */
#ifndef SAMBA4_USES_HEIMDAL
	if (gse_ctx->server_name == NULL) {
		OM_uint32 gss_min2 = 0;

		status = gse_setup_server_principal(mem_ctx,
						    target_principal,
						    service,
						    hostname,
						    client_realm,
						    &server_principal,
						    &gse_ctx->server_name);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		gss_maj = gss_init_sec_context(&gss_min,
					       gse_ctx->creds,
					       &gse_ctx->gssapi_context,
					       gse_ctx->server_name,
					       &gse_ctx->gss_mech,
					       gse_ctx->gss_want_flags,
					       0,
					       GSS_C_NO_CHANNEL_BINDINGS,
					       &in_data,
					       NULL,
					       &out_data,
					       &gse_ctx->gss_got_flags,
					       &time_rec);
		if (gss_maj != GSS_S_FAILURE) {
			goto init_sec_context_done;
		}
		if (gss_min != (OM_uint32)KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
			goto init_sec_context_done;
		}
		if (target_principal != NULL) {
			goto init_sec_context_done;
		}

		fallback = true;
		TALLOC_FREE(server_principal);
		gss_release_name(&gss_min2, &gse_ctx->server_name);
	}
#endif /* !SAMBA4_USES_HEIMDAL */

	if (gse_ctx->server_name == NULL) {
		server_realm = smb_krb5_get_realm_from_hostname(mem_ctx,
								hostname,
								client_realm);
		if (server_realm == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		if (fallback &&
		    strequal(client_realm, server_realm)) {
			goto init_sec_context_done;
		}

		status = gse_setup_server_principal(mem_ctx,
						    target_principal,
						    service,
						    hostname,
						    server_realm,
						    &server_principal,
						    &gse_ctx->server_name);
		TALLOC_FREE(server_realm);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		TALLOC_FREE(server_principal);
	}

	gss_maj = gss_init_sec_context(&gss_min,
					gse_ctx->creds,
					&gse_ctx->gssapi_context,
					gse_ctx->server_name,
					&gse_ctx->gss_mech,
					gse_ctx->gss_want_flags,
					0, GSS_C_NO_CHANNEL_BINDINGS,
					&in_data, NULL, &out_data,
					&gse_ctx->gss_got_flags, &time_rec);
	goto init_sec_context_done;
	/* JUMP! */
init_sec_context_done:

	switch (gss_maj) {
	case GSS_S_COMPLETE:
		/* we are done with it */
		tv = timeval_current_ofs(time_rec, 0);
		gse_ctx->expire_time = timeval_to_nttime(&tv);

		status = NT_STATUS_OK;
		break;
	case GSS_S_CONTINUE_NEEDED:
		/* we will need a third leg */
		status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		break;
	case GSS_S_CONTEXT_EXPIRED:
		/* Make SPNEGO ignore us, we can't go any further here */
		DBG_NOTICE("Context expired\n");
		status = NT_STATUS_INVALID_PARAMETER;
		goto done;
	case GSS_S_FAILURE:
		switch (gss_min) {
		case (OM_uint32)KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN:
			DBG_NOTICE("Server principal not found\n");
			/* Make SPNEGO ignore us, we can't go any further here */
			status = NT_STATUS_INVALID_PARAMETER;
			goto done;
		case (OM_uint32)KRB5KRB_AP_ERR_TKT_EXPIRED:
			DBG_NOTICE("Ticket expired\n");
			/* Make SPNEGO ignore us, we can't go any further here */
			status = NT_STATUS_INVALID_PARAMETER;
			goto done;
		case (OM_uint32)KRB5KRB_AP_ERR_TKT_NYV:
			DBG_NOTICE("Clockskew\n");
			/* Make SPNEGO ignore us, we can't go any further here */
			status = NT_STATUS_TIME_DIFFERENCE_AT_DC;
			goto done;
		case (OM_uint32)KRB5_KDC_UNREACH:
			DBG_NOTICE("KDC unreachable\n");
			/* Make SPNEGO ignore us, we can't go any further here */
			status = NT_STATUS_NO_LOGON_SERVERS;
			goto done;
		case (OM_uint32)KRB5KRB_AP_ERR_MSG_TYPE:
			/* Garbage input, possibly from the auto-mech detection */
			status = NT_STATUS_INVALID_PARAMETER;
			goto done;
		default:
			DBG_ERR("gss_init_sec_context failed with [%s](%u)\n",
				gse_errstr(talloc_tos(), gss_maj, gss_min),
				gss_min);
			status = NT_STATUS_LOGON_FAILURE;
			goto done;
		}
		break;
	default:
		DBG_ERR("gss_init_sec_context failed with [%s]\n",
			gse_errstr(talloc_tos(), gss_maj, gss_min));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	/* we may be told to return nothing */
	if (out_data.length) {
		blob = data_blob_talloc(mem_ctx, out_data.value, out_data.length);
		if (!blob.data) {
			status = NT_STATUS_NO_MEMORY;
		}

		gss_maj = gss_release_buffer(&gss_min, &out_data);
	}

done:
	*token_out = blob;
	return status;
}

static NTSTATUS gse_init_server(TALLOC_CTX *mem_ctx,
				bool do_sign, bool do_seal,
				uint32_t add_gss_c_flags,
				struct gse_context **_gse_ctx)
{
	struct gse_context *gse_ctx;
	OM_uint32 gss_maj, gss_min;
	krb5_error_code ret;
	NTSTATUS status;

	status = gse_context_init(mem_ctx, do_sign, do_seal,
				  NULL, add_gss_c_flags, &gse_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_NO_MEMORY;
	}

	ret = gse_krb5_get_server_keytab(gse_ctx->k5ctx,
					 &gse_ctx->keytab);
	if (ret) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	/* This creates a GSSAPI cred_id_t with the keytab set */
	gss_maj = smb_gss_krb5_import_cred(&gss_min, gse_ctx->k5ctx,
					   NULL, NULL, gse_ctx->keytab,
					   &gse_ctx->creds);

	if (gss_maj != 0) {
		DEBUG(0, ("smb_gss_krb5_import_cred failed with [%s]\n",
			  gse_errstr(gse_ctx, gss_maj, gss_min)));
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(gse_ctx);
	}

	*_gse_ctx = gse_ctx;
	return status;
}

static NTSTATUS gse_get_server_auth_token(TALLOC_CTX *mem_ctx,
					  struct gensec_security *gensec_security,
					  const DATA_BLOB *token_in,
					  DATA_BLOB *token_out)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gse_context);
	OM_uint32 gss_maj, gss_min;
	gss_buffer_desc in_data;
	gss_buffer_desc out_data;
	DATA_BLOB blob = data_blob_null;
	NTSTATUS status;
	OM_uint32 time_rec = 0;
	struct timeval tv;

	in_data.value = token_in->data;
	in_data.length = token_in->length;

	gss_maj = gss_accept_sec_context(&gss_min,
					 &gse_ctx->gssapi_context,
					 gse_ctx->creds,
					 &in_data,
					 GSS_C_NO_CHANNEL_BINDINGS,
					 &gse_ctx->client_name,
					 &gse_ctx->ret_mech,
					 &out_data,
					 &gse_ctx->gss_got_flags,
					 &time_rec,
					 &gse_ctx->delegated_cred_handle);
	switch (gss_maj) {
	case GSS_S_COMPLETE:
		/* we are done with it */
		tv = timeval_current_ofs(time_rec, 0);
		gse_ctx->expire_time = timeval_to_nttime(&tv);

		status = NT_STATUS_OK;
		break;
	case GSS_S_CONTINUE_NEEDED:
		/* we will need a third leg */
		status = NT_STATUS_MORE_PROCESSING_REQUIRED;
		break;
	default:
		DEBUG(1, ("gss_accept_sec_context failed with [%s]\n",
			  gse_errstr(talloc_tos(), gss_maj, gss_min)));

		if (gse_ctx->gssapi_context) {
			gss_delete_sec_context(&gss_min,
						&gse_ctx->gssapi_context,
						GSS_C_NO_BUFFER);
		}

		/*
		 * If we got an output token, make Windows aware of it
		 * by telling it that more processing is needed
		 */
		if (out_data.length > 0) {
			status = NT_STATUS_MORE_PROCESSING_REQUIRED;
			/* Fall through to handle the out token */
		} else {
			status = NT_STATUS_LOGON_FAILURE;
			goto done;
		}
	}

	/* we may be told to return nothing */
	if (out_data.length) {
		blob = data_blob_talloc(mem_ctx, out_data.value, out_data.length);
		if (!blob.data) {
			status = NT_STATUS_NO_MEMORY;
		}
		gss_maj = gss_release_buffer(&gss_min, &out_data);
	}


done:
	*token_out = blob;
	return status;
}

static char *gse_errstr(TALLOC_CTX *mem_ctx, OM_uint32 maj, OM_uint32 min)
{
	OM_uint32 gss_min, gss_maj;
	gss_buffer_desc msg_min;
	gss_buffer_desc msg_maj;
	OM_uint32 msg_ctx = 0;

	char *errstr = NULL;

	ZERO_STRUCT(msg_min);
	ZERO_STRUCT(msg_maj);

	gss_maj = gss_display_status(&gss_min, maj, GSS_C_GSS_CODE,
				     GSS_C_NO_OID, &msg_ctx, &msg_maj);
	if (gss_maj) {
		goto done;
	}
	errstr = talloc_strndup(mem_ctx,
				(char *)msg_maj.value,
					msg_maj.length);
	if (!errstr) {
		goto done;
	}
	gss_maj = gss_display_status(&gss_min, min, GSS_C_MECH_CODE,
				     (gss_OID)discard_const(gss_mech_krb5),
				     &msg_ctx, &msg_min);
	if (gss_maj) {
		goto done;
	}

	errstr = talloc_strdup_append_buffer(errstr, ": ");
	if (!errstr) {
		goto done;
	}
	errstr = talloc_strndup_append_buffer(errstr,
						(char *)msg_min.value,
							msg_min.length);
	if (!errstr) {
		goto done;
	}

done:
	if (msg_min.value) {
		gss_maj = gss_release_buffer(&gss_min, &msg_min);
	}
	if (msg_maj.value) {
		gss_maj = gss_release_buffer(&gss_min, &msg_maj);
	}
	return errstr;
}

static NTSTATUS gensec_gse_client_start(struct gensec_security *gensec_security)
{
	struct gse_context *gse_ctx;
	struct cli_credentials *creds = gensec_get_credentials(gensec_security);
	NTSTATUS nt_status;
	OM_uint32 want_flags = 0;
	bool do_sign = false, do_seal = false;
	const char *hostname = gensec_get_target_hostname(gensec_security);
	const char *service = gensec_get_target_service(gensec_security);
	const char *username = cli_credentials_get_username(creds);
	const char *password = cli_credentials_get_password(creds);
	const char *realm = cli_credentials_get_realm(creds);

	if (!hostname) {
		DEBUG(1, ("Could not determine hostname for target computer, cannot use kerberos\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (is_ipaddress(hostname)) {
		DEBUG(2, ("Cannot do GSE to an IP address\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	if (strcmp(hostname, "localhost") == 0) {
		DEBUG(2, ("GSE to 'localhost' does not make sense\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (gensec_security->want_features & GENSEC_FEATURE_SESSION_KEY) {
		do_sign = true;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		do_sign = true;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		do_seal = true;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_DCE_STYLE) {
		want_flags |= GSS_C_DCE_STYLE;
	}

	nt_status = gse_init_client(gensec_security, do_sign, do_seal, NULL,
				    hostname, service, realm,
				    username, password, want_flags,
				    &gse_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	gensec_security->private_data = gse_ctx;
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_server_start(struct gensec_security *gensec_security)
{
	struct gse_context *gse_ctx;
	NTSTATUS nt_status;
	OM_uint32 want_flags = 0;
	bool do_sign = false, do_seal = false;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		do_sign = true;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		do_seal = true;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_DCE_STYLE) {
		want_flags |= GSS_C_DCE_STYLE;
	}

	nt_status = gse_init_server(gensec_security, do_sign, do_seal, want_flags,
				    &gse_ctx);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	gensec_security->private_data = gse_ctx;
	return NT_STATUS_OK;
}

/**
 * Next state function for the GSE GENSEC mechanism
 *
 * @param gensec_gse_state GSE State
 * @param mem_ctx The TALLOC_CTX for *out to be allocated on
 * @param in The request, as a DATA_BLOB
 * @param out The reply, as an talloc()ed DATA_BLOB, on *mem_ctx
 * @return Error, MORE_PROCESSING_REQUIRED if a reply is sent,
 *                or NT_STATUS_OK if the user is authenticated.
 */

static NTSTATUS gensec_gse_update(struct gensec_security *gensec_security,
				  TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  const DATA_BLOB in, DATA_BLOB *out)
{
	NTSTATUS status;

	switch (gensec_security->gensec_role) {
	case GENSEC_CLIENT:
		status = gse_get_client_auth_token(mem_ctx,
						   gensec_security,
						   &in, out);
		break;
	case GENSEC_SERVER:
		status = gse_get_server_auth_token(mem_ctx,
						   gensec_security,
						   &in, out);
		break;
	}
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_wrap(struct gensec_security *gensec_security,
				TALLOC_CTX *mem_ctx,
				const DATA_BLOB *in,
				DATA_BLOB *out)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	input_token.length = in->length;
	input_token.value = in->data;

	maj_stat = gss_wrap(&min_stat,
			    gse_ctx->gssapi_context,
			    gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL),
			    GSS_C_QOP_DEFAULT,
			    &input_token,
			    &conf_state,
			    &output_token);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(0, ("gensec_gse_wrap: GSS Wrap failed: %s\n",
			  gse_errstr(talloc_tos(), maj_stat, min_stat)));
		return NT_STATUS_ACCESS_DENIED;
	}

	*out = data_blob_talloc(mem_ctx, output_token.value, output_token.length);
	gss_release_buffer(&min_stat, &output_token);

	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_unwrap(struct gensec_security *gensec_security,
				     TALLOC_CTX *mem_ctx,
				     const DATA_BLOB *in,
				     DATA_BLOB *out)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc input_token, output_token;
	int conf_state;
	gss_qop_t qop_state;
	input_token.length = in->length;
	input_token.value = in->data;

	maj_stat = gss_unwrap(&min_stat,
			      gse_ctx->gssapi_context,
			      &input_token,
			      &output_token,
			      &conf_state,
			      &qop_state);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(0, ("gensec_gse_unwrap: GSS UnWrap failed: %s\n",
			  gse_errstr(talloc_tos(), maj_stat, min_stat)));
		return NT_STATUS_ACCESS_DENIED;
	}

	*out = data_blob_talloc(mem_ctx, output_token.value, output_token.length);
	gss_release_buffer(&min_stat, &output_token);

	if (gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL)
	    && !conf_state) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_seal_packet(struct gensec_security *gensec_security,
				       TALLOC_CTX *mem_ctx,
				       uint8_t *data, size_t length,
				       const uint8_t *whole_pdu, size_t pdu_length,
				       DATA_BLOB *sig)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	bool hdr_signing = false;
	size_t sig_size = 0;
	NTSTATUS status;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		hdr_signing = true;
	}

	sig_size = gensec_gse_sig_size(gensec_security, length);

	status = gssapi_seal_packet(gse_ctx->gssapi_context,
				    &gse_ctx->gss_mech,
				    hdr_signing, sig_size,
				    data, length,
				    whole_pdu, pdu_length,
				    mem_ctx, sig);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("gssapi_seal_packet(hdr_signing=%u,sig_size=%zu,"
			  "data=%zu,pdu=%zu) failed: %s\n",
			  hdr_signing, sig_size, length, pdu_length,
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_unseal_packet(struct gensec_security *gensec_security,
					 uint8_t *data, size_t length,
					 const uint8_t *whole_pdu, size_t pdu_length,
					 const DATA_BLOB *sig)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	bool hdr_signing = false;
	NTSTATUS status;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		hdr_signing = true;
	}

	status = gssapi_unseal_packet(gse_ctx->gssapi_context,
				      &gse_ctx->gss_mech,
				      hdr_signing,
				      data, length,
				      whole_pdu, pdu_length,
				      sig);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("gssapi_unseal_packet(hdr_signing=%u,sig_size=%zu,"
			  "data=%zu,pdu=%zu) failed: %s\n",
			  hdr_signing, sig->length, length, pdu_length,
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_sign_packet(struct gensec_security *gensec_security,
				       TALLOC_CTX *mem_ctx,
				       const uint8_t *data, size_t length,
				       const uint8_t *whole_pdu, size_t pdu_length,
				       DATA_BLOB *sig)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	bool hdr_signing = false;
	NTSTATUS status;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		hdr_signing = true;
	}

	status = gssapi_sign_packet(gse_ctx->gssapi_context,
				    &gse_ctx->gss_mech,
				    hdr_signing,
				    data, length,
				    whole_pdu, pdu_length,
				    mem_ctx, sig);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("gssapi_sign_packet(hdr_signing=%u,"
			  "data=%zu,pdu=%zu) failed: %s\n",
			  hdr_signing, length, pdu_length,
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

static NTSTATUS gensec_gse_check_packet(struct gensec_security *gensec_security,
					const uint8_t *data, size_t length,
					const uint8_t *whole_pdu, size_t pdu_length,
					const DATA_BLOB *sig)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	bool hdr_signing = false;
	NTSTATUS status;

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		hdr_signing = true;
	}

	status = gssapi_check_packet(gse_ctx->gssapi_context,
				     &gse_ctx->gss_mech,
				     hdr_signing,
				     data, length,
				     whole_pdu, pdu_length,
				     sig);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("gssapi_check_packet(hdr_signing=%u,sig_size=%zu"
			  "data=%zu,pdu=%zu) failed: %s\n",
			  hdr_signing, sig->length, length, pdu_length,
			  nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/* Try to figure out what features we actually got on the connection */
static bool gensec_gse_have_feature(struct gensec_security *gensec_security,
				    uint32_t feature)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);

	if (feature & GENSEC_FEATURE_SESSION_KEY) {
		return gse_ctx->gss_got_flags & GSS_C_INTEG_FLAG;
	}
	if (feature & GENSEC_FEATURE_SIGN) {
		return gse_ctx->gss_got_flags & GSS_C_INTEG_FLAG;
	}
	if (feature & GENSEC_FEATURE_SEAL) {
		return gse_ctx->gss_got_flags & GSS_C_CONF_FLAG;
	}
	if (feature & GENSEC_FEATURE_DCE_STYLE) {
		return gse_ctx->gss_got_flags & GSS_C_DCE_STYLE;
	}
	if (feature & GENSEC_FEATURE_NEW_SPNEGO) {
		NTSTATUS status;
		uint32_t keytype;

		if (!(gse_ctx->gss_got_flags & GSS_C_INTEG_FLAG)) {
			return false;
		}

		status = gssapi_get_session_key(talloc_tos(), 
						gse_ctx->gssapi_context, NULL, &keytype);
		/* 
		 * We should do a proper sig on the mechListMic unless
		 * we know we have to be backwards compatible with
		 * earlier windows versions.  
		 * 
		 * Negotiating a non-krb5
		 * mech for example should be regarded as having
		 * NEW_SPNEGO
		 */
		if (NT_STATUS_IS_OK(status)) {
			switch (keytype) {
			case ENCTYPE_DES_CBC_CRC:
			case ENCTYPE_DES_CBC_MD5:
			case ENCTYPE_ARCFOUR_HMAC:
			case ENCTYPE_DES3_CBC_SHA1:
				return false;
			}
		}
		return true;
	}
	/* We can always do async (rather than strict request/reply) packets.  */
	if (feature & GENSEC_FEATURE_ASYNC_REPLIES) {
		return true;
	}
	if (feature & GENSEC_FEATURE_SIGN_PKT_HEADER) {
		if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
			return true;
		}

		if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
			return true;
		}

		return false;
	}
	return false;
}

static NTTIME gensec_gse_expire_time(struct gensec_security *gensec_security)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);

	return gse_ctx->expire_time;
}

/*
 * Extract the 'sesssion key' needed by SMB signing and ncacn_np
 * (for encrypting some passwords).
 *
 * This breaks all the abstractions, but what do you expect...
 */
static NTSTATUS gensec_gse_session_key(struct gensec_security *gensec_security,
				       TALLOC_CTX *mem_ctx,
				       DATA_BLOB *session_key)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);

	return gssapi_get_session_key(mem_ctx, gse_ctx->gssapi_context, session_key, NULL);
}

/* Get some basic (and authorization) information about the user on
 * this session.  This uses either the PAC (if present) or a local
 * database lookup */
static NTSTATUS gensec_gse_session_info(struct gensec_security *gensec_security,
					TALLOC_CTX *mem_ctx,
					struct auth_session_info **_session_info)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	NTSTATUS nt_status;
	TALLOC_CTX *tmp_ctx;
	struct auth_session_info *session_info = NULL;
	OM_uint32 maj_stat, min_stat;
	DATA_BLOB pac_blob, *pac_blob_ptr = NULL;

	gss_buffer_desc name_token;
	char *principal_string;

	tmp_ctx = talloc_named(mem_ctx, 0, "gensec_gse_session_info context");
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	maj_stat = gss_display_name(&min_stat,
				    gse_ctx->client_name,
				    &name_token,
				    NULL);
	if (GSS_ERROR(maj_stat)) {
		DEBUG(1, ("GSS display_name failed: %s\n",
			  gse_errstr(talloc_tos(), maj_stat, min_stat)));
		talloc_free(tmp_ctx);
		return NT_STATUS_FOOBAR;
	}

	principal_string = talloc_strndup(tmp_ctx,
					  (const char *)name_token.value,
					  name_token.length);

	gss_release_buffer(&min_stat, &name_token);

	if (!principal_string) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = gssapi_obtain_pac_blob(tmp_ctx,  gse_ctx->gssapi_context,
					   gse_ctx->client_name,
					   &pac_blob);

	/* IF we have the PAC - otherwise we need to get this
	 * data from elsewere
	 */
	if (NT_STATUS_IS_OK(nt_status)) {
		pac_blob_ptr = &pac_blob;
	}
	nt_status = gensec_generate_session_info_pac(tmp_ctx,
						     gensec_security,
						     NULL,
						     pac_blob_ptr, principal_string,
						     gensec_get_remote_address(gensec_security),
						     &session_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	nt_status = gensec_gse_session_key(gensec_security, session_info,
					   &session_info->session_key);
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(tmp_ctx);
		return nt_status;
	}

	*_session_info = talloc_move(mem_ctx, &session_info);
	talloc_free(tmp_ctx);

	return NT_STATUS_OK;
}

static size_t gensec_gse_max_input_size(struct gensec_security *gensec_security)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	OM_uint32 maj_stat, min_stat;
	OM_uint32 max_input_size;

	maj_stat = gss_wrap_size_limit(&min_stat,
				       gse_ctx->gssapi_context,
				       gensec_have_feature(gensec_security, GENSEC_FEATURE_SEAL),
				       GSS_C_QOP_DEFAULT,
				       gse_ctx->max_wrap_buf_size,
				       &max_input_size);
	if (GSS_ERROR(maj_stat)) {
		TALLOC_CTX *mem_ctx = talloc_new(NULL);
		DEBUG(1, ("gensec_gssapi_max_input_size: determining signature size with gss_wrap_size_limit failed: %s\n",
			  gse_errstr(mem_ctx, maj_stat, min_stat)));
		talloc_free(mem_ctx);
		return 0;
	}

	return max_input_size;
}

/* Find out the maximum output size negotiated on this connection */
static size_t gensec_gse_max_wrapped_size(struct gensec_security *gensec_security)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);
	return gse_ctx->max_wrap_buf_size;
}

static size_t gensec_gse_sig_size(struct gensec_security *gensec_security,
				  size_t data_size)
{
	struct gse_context *gse_ctx =
		talloc_get_type_abort(gensec_security->private_data,
		struct gse_context);

	if (gse_ctx->sig_size > 0) {
		return gse_ctx->sig_size;
	}

	gse_ctx->sig_size = gssapi_get_sig_size(gse_ctx->gssapi_context,
					        &gse_ctx->gss_mech,
					        gse_ctx->gss_got_flags,
					        data_size);
	return gse_ctx->sig_size;
}

static const char *gensec_gse_krb5_oids[] = {
	GENSEC_OID_KERBEROS5_OLD,
	GENSEC_OID_KERBEROS5,
	NULL
};

const struct gensec_security_ops gensec_gse_krb5_security_ops = {
	.name		= "gse_krb5",
	.auth_type	= DCERPC_AUTH_TYPE_KRB5,
	.oid            = gensec_gse_krb5_oids,
	.client_start   = gensec_gse_client_start,
	.server_start   = gensec_gse_server_start,
	.magic  	= gensec_magic_check_krb5_oid,
	.update 	= gensec_gse_update,
	.session_key	= gensec_gse_session_key,
	.session_info	= gensec_gse_session_info,
	.sig_size	= gensec_gse_sig_size,
	.sign_packet	= gensec_gse_sign_packet,
	.check_packet	= gensec_gse_check_packet,
	.seal_packet	= gensec_gse_seal_packet,
	.unseal_packet	= gensec_gse_unseal_packet,
	.max_input_size	  = gensec_gse_max_input_size,
	.max_wrapped_size = gensec_gse_max_wrapped_size,
	.wrap           = gensec_gse_wrap,
	.unwrap         = gensec_gse_unwrap,
	.have_feature   = gensec_gse_have_feature,
	.expire_time    = gensec_gse_expire_time,
	.enabled        = true,
	.kerberos       = true,
	.priority       = GENSEC_GSSAPI
};

#endif /* HAVE_KRB5 */
