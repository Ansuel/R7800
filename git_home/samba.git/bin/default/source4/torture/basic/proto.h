#ifndef __DEFAULT_SOURCE4_TORTURE_BASIC_PROTO_H__
#define __DEFAULT_SOURCE4_TORTURE_BASIC_PROTO_H__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)
/* This file was automatically generated by mkproto.pl. DO NOT EDIT */

/* this file contains prototypes for functions that are private 
 * to this subsystem or library. These functions should not be 
 * used outside this particular subsystem! */


/* The following definitions come from ../source4/torture/basic/base.c  */


/**
  This checks file/dir birthtime
*/
NTSTATUS torture_base_init(void);

/* The following definitions come from ../source4/torture/basic/misc.c  */

bool run_torture(struct torture_context *tctx, struct smbcli_state *cli, int dummy);
bool run_pipe_number(struct torture_context *tctx, 
					 struct smbcli_state *cli1);
bool torture_holdcon(struct torture_context *tctx);
bool torture_holdopen(struct torture_context *tctx,
		      struct smbcli_state *cli);
bool torture_maxfid_test(struct torture_context *tctx, struct smbcli_state *cli);
bool torture_ioctl_test(struct torture_context *tctx, 
						struct smbcli_state *cli);
bool run_benchrw(struct torture_context *tctx);

/* The following definitions come from ../source4/torture/basic/scanner.c  */


/****************************************************************************
check for existance of a trans2 call
****************************************************************************/
bool torture_trans2_scan(struct torture_context *torture,
						 struct smbcli_state *cli);

/****************************************************************************
check for existance of a nttrans call
****************************************************************************/
bool torture_nttrans_scan(struct torture_context *torture, 
			  struct smbcli_state *cli);
bool torture_smb_scan(struct torture_context *torture);

/* The following definitions come from ../source4/torture/basic/utable.c  */

bool torture_utable(struct torture_context *tctx, 
					struct smbcli_state *cli);
bool torture_casetable(struct torture_context *tctx, 
					   struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/charset.c  */

struct torture_suite *torture_charset(TALLOC_CTX *mem_ctx);

/* The following definitions come from ../source4/torture/basic/mangle_test.c  */

bool torture_mangle(struct torture_context *torture, 
		    struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/denytest.c  */

bool torture_denytest1(struct torture_context *tctx, 
		       struct smbcli_state *cli1);
bool torture_denytest2(struct torture_context *tctx, 
		       struct smbcli_state *cli1, 
		       struct smbcli_state *cli2);
bool torture_denytest3(struct torture_context *tctx, 
		       struct smbcli_state *cli1,
		       struct smbcli_state *cli2);
bool torture_ntdenytest1(struct torture_context *tctx, 
			 struct smbcli_state *cli, int client);
bool torture_ntdenytest2(struct torture_context *torture, 
			 struct smbcli_state *cli1,
			 struct smbcli_state *cli2);
bool torture_denydos_sharing(struct torture_context *tctx, 
			     struct smbcli_state *cli);

/**
 * NTCREATEX and SHARE MODE test.
 *
 * Open with combinations of (access_mode, share_mode).
 *  - Check status
 * Open 2nd time with combination of (access_mode2, share_mode2).
 *  - Check status
 * Perform operations to verify?
 *  - Read
 *  - Write
 *  - Delete
 */
bool torture_createx_sharemodes(struct torture_context *tctx,
				struct smbcli_state *cli,
				struct smbcli_state *cli2,
				bool dir,
				bool extended);
bool torture_createx_sharemodes_file(struct torture_context *tctx,
    struct smbcli_state *cli, struct smbcli_state *cli2);
bool torture_createx_sharemodes_dir(struct torture_context *tctx,
    struct smbcli_state *cli, struct smbcli_state *cli2);
bool torture_createx_access(struct torture_context *tctx,
    struct smbcli_state *cli);
bool torture_createx_access_exhaustive(struct torture_context *tctx,
    struct smbcli_state *cli);
bool torture_maximum_allowed(struct torture_context *tctx,
    struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/aliases.c  */

struct torture_suite *torture_trans2_aliases(TALLOC_CTX *mem_ctx);

/* The following definitions come from ../source4/torture/basic/locking.c  */

struct torture_suite *torture_base_locktest(TALLOC_CTX *mem_ctx);

/* The following definitions come from ../source4/torture/basic/secleak.c  */

bool torture_sec_leak(struct torture_context *tctx, struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/rename.c  */

bool torture_test_rename(struct torture_context *tctx, 
			 struct smbcli_state *cli1);

/* The following definitions come from ../source4/torture/basic/dir.c  */

bool torture_dirtest1(struct torture_context *tctx, 
		      struct smbcli_state *cli);
bool torture_dirtest2(struct torture_context *tctx, 
		      struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/delete.c  */

struct torture_suite *torture_test_delete(void);

/* The following definitions come from ../source4/torture/basic/unlink.c  */

bool torture_unlinktest(struct torture_context *tctx, struct smbcli_state *cli);

/* The following definitions come from ../source4/torture/basic/disconnect.c  */

bool torture_disconnect(struct torture_context *torture);

/* The following definitions come from ../source4/torture/basic/delaywrite.c  */

struct torture_suite *torture_delay_write(void);

/* The following definitions come from ../source4/torture/basic/attr.c  */

bool torture_openattrtest(struct torture_context *tctx, 
			  struct smbcli_state *cli1);
bool torture_winattrtest(struct torture_context *tctx,
			  struct smbcli_state *cli1);

/* The following definitions come from ../source4/torture/basic/properties.c  */

bool torture_test_properties(struct torture_context *torture, 
			     struct smbcli_state *cli);
#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif /* __DEFAULT_SOURCE4_TORTURE_BASIC_PROTO_H__ */
