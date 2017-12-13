#ifndef __DEFAULT_SOURCE4_LIBCLI_LDAP_LDAP_PROTO_H__
#define __DEFAULT_SOURCE4_LIBCLI_LDAP_LDAP_PROTO_H__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)
/* This file was automatically generated by mkproto.pl. DO NOT EDIT */

/* this file contains prototypes for functions that are private 
 * to this subsystem or library. These functions should not be 
 * used outside this particular subsystem! */


/* The following definitions come from ../source4/libcli/ldap/ldap_client.c  */


/* The following definitions come from ../source4/libcli/ldap/ldap_bind.c  */


/* The following definitions come from ../source4/libcli/ldap/ldap_ildap.c  */


/* The following definitions come from ../source4/libcli/ldap/ldap_controls.c  */

const struct ldap_control_handler *samba_ldap_control_handlers(void);
#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* __DEFAULT_SOURCE4_LIBCLI_LDAP_LDAP_PROTO_H__ */

