/* 
 * modules.c -- handles:
 *   support for modules in eggdrop
 * 
 * by Darrin Smith (beldin@light.iinet.net.au)
 * 
 */

#include "main.h"
#include "modules.h"
#include "tandem.h"
#include <ctype.h>
#include "core_binds.h"

extern struct dcc_t	*dcc;
#ifdef S_AUTH
extern struct auth_t    *auth;
#endif /* S_AUTH */

#include "users.h"

extern Tcl_Interp	*interp;
extern struct userrec	*userlist, *lastuser;
extern char		 tempdir[], botnetnick[], botname[], natip[], cmdprefix[],
			 hostname[], origbotname[], botuser[], admin[],
			 userfile[], ver[], notify_new[], kickprefix[], bankickprefix[],
			 version[], quit_msg[], hostname6[], bdhash[], dcc_prefix[],
#ifdef S_AUTH
                         authkey[], 
#endif /* S_AUTH */
			 myip[], myip6[];
extern int	 	 noshare, loading, role, server_lag, 
#ifdef S_AUTH
			 auth_total, 
#endif /* S_AUTH */
 			 dcc_total, egg_numver, userfile_perm,
			 use_console_r, ignore_time, must_be_owner,
			 debug_output, default_flags,  
			 max_dcc, password_timeout, localhub,
			 use_invites, use_exempts, 
                         force_expire, do_restart, timesync,
			 protect_readonly, reserved_port_min, reserved_port_max;
extern time_t now, online_since, buildts;
extern struct chanset_t *chanset;
extern tand_t *tandbot;
extern party_t *party;
extern int parties;
extern sock_list        *socklist;
#ifdef S_MSGCMDS
extern struct cfg_entry CFG_MSGOP, CFG_MSGPASS, CFG_MSGINVITE, CFG_MSGIDENT;
#endif /* S_MSGCMDS */
int xtra_kill();
int xtra_unpack();
static int module_rename(char *name, char *newname);


struct static_list {
  struct static_list *next;
  char *name;
  char *(*func) ();
} *static_modules = NULL;

void check_static(char *name, char *(*func) ())
{
  struct static_list *p = nmalloc(sizeof(struct static_list));

  p->name = nmalloc(strlen(name) + 1);
  strcpy(p->name, name);
  p->func = func;
  p->next = static_modules;
  static_modules = p;
}

void *mod_killsock(int size, const char *modname, const char *filename, int line)
{
#ifdef DEBUG_MEM
  char x[100], *p;

  p = strrchr(filename, '/');
  egg_snprintf(x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  real_killsock(size, x, line);
#else
  killsock(size);
#endif
  return NULL;
}


/* The null functions */
void null_func()
{
}
char *charp_func()
{
  return NULL;
}
int minus_func()
{
  return -1;
}
int false_func()
{
  return 0;
}


/*
 *     Various hooks & things
 */

/* The REAL hooks, when these are called, a return of 0 indicates unhandled
 * 1 is handled
 */
struct hook_entry *hook_list[REAL_HOOKS];

static void null_share(int idx, char *x)
{
  if ((x[0] == 'u') && (x[1] == 'n')) {
    putlog(LOG_BOTS, "*", "User file rejected by %s: %s",
	   dcc[idx].nick, x + 3);
    dcc[idx].status &= ~STAT_OFFERED;
    if (!(dcc[idx].status & STAT_GETTING)) {
      dcc[idx].status &= ~STAT_SHARE;
    }
  } else if ((x[0] != 'v') && (x[0] != 'e'))
    dprintf(idx, "s un Not sharing userfile.\n");
}

void (*shareout) () = null_func;
void (*sharein) (int, char *) = null_share;
void (*shareupdatein) (int, char *) = null_share;
void (*qserver) (int, char *, int) = (void (*)(int, char *, int)) null_func;
void (*add_mode) () = null_func;
int (*match_noterej) (struct userrec *, char *) = (int (*)(struct userrec *, char *)) false_func;
int (*storenote)(char *from, char *to, char *msg, int idx, char *who, int bufsize) = (int (*)(char *from, char *to, char *msg, int idx, char *who, int bufsize)) minus_func;
int (*rfc_casecmp) (const char *, const char *) = _rfc_casecmp;
int (*rfc_ncasecmp) (const char *, const char *, int) = _rfc_ncasecmp;
int (*rfc_toupper) (int) = _rfc_toupper;
int (*rfc_tolower) (int) = _rfc_tolower;
void (*dns_hostbyip) (IP) = block_dns_hostbyip;
void (*dns_ipbyhost) (char *) = block_dns_ipbyhost;

module_entry *module_list;
dependancy *dependancy_list = NULL;


/* The horrible global lookup table for functions
 * BUT it makes the whole thing *much* more portable than letting each
 * OS screw up the symbols their own special way :/
 */

Function global_table[] =
{
  /* 0 - 3 */
  (Function) mod_malloc,
  (Function) mod_free,
#ifdef DEBUG_CONTEXT
  (Function) eggContext,
#else
  (Function) 0,
#endif
  (Function) module_rename,
  /* 4 - 7 */
  (Function) module_register,
  (Function) module_find,
  (Function) module_depend,
  (Function) module_undepend,
  /* 8 - 11 */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  /* 12 - 15 */
  (Function) 0,
  (Function) 0,
  (Function) add_tcl_commands,
  (Function) rem_tcl_commands,
  /* 16 - 19 */
  (Function) add_tcl_ints,
  (Function) rem_tcl_ints,
  (Function) add_tcl_strings,
  (Function) rem_tcl_strings,
  /* 20 - 23 */
  (Function) base64_to_int,
  (Function) int_to_base64,
  (Function) int_to_base10,
  (Function) simple_sprintf,
  /* 24 - 27 */
  (Function) botnet_send_zapf,
  (Function) botnet_send_zapf_broad,
  (Function) botnet_send_unlinked,
  (Function) botnet_send_bye,
  /* 28 - 31 */
  (Function) botnet_send_chat,
  (Function) & server_lag, /* int					*/
  (Function) remove_crlf,
  (Function) shuffle,
  /* 32 - 35 */
  (Function) botnet_send_join_idx,
  (Function) botnet_send_part_idx,
  (Function) updatebot,
  (Function) nextbot,
  /* 36 - 39 */
  (Function) zapfbot,
  (Function) n_free,
  (Function) u_pass_match,
  (Function) _user_malloc,
  /* 40 - 43 */
  (Function) get_user,
  (Function) set_user,
  (Function) add_entry_type,
  (Function) del_entry_type,
  /* 44 - 47 */
  (Function) get_user_flagrec,
  (Function) set_user_flagrec,
  (Function) get_user_by_host,
  (Function) get_user_by_handle,
  /* 48 - 51 */
  (Function) find_entry_type,
  (Function) find_user_entry,
  (Function) adduser,
  (Function) deluser,
  /* 52 - 55 */
  (Function) addhost_by_handle,
  (Function) delhost_by_handle,
  (Function) readuserfile,
  (Function) write_userfile,
  /* 56 - 59 */
  (Function) geticon,
  (Function) clear_chanlist,
  (Function) reaffirm_owners,
  (Function) change_handle,
  /* 60 - 63 */
  (Function) write_user,
  (Function) clear_userlist,
  (Function) count_users,
  (Function) sanity_check,
  /* 64 - 67 */
  (Function) break_down_flags,
  (Function) build_flags,
  (Function) flagrec_eq,
  (Function) flagrec_ok,
  /* 68 - 71 */
  (Function) & shareout,
  (Function) dprintf,
  (Function) chatout,
  (Function) chanout_but,
  /* 72 - 75 */
  (Function) 0,
  (Function) list_delete,
  (Function) list_append,
  (Function) list_contains,
  /* 76 - 79 */
  (Function) answer,
  (Function) getmyip,
  (Function) neterror,
  (Function) tputs,
  /* 80 - 83 */
  (Function) new_dcc,
  (Function) lostdcc,
  (Function) getsock,
  (Function) mod_killsock,
  /* 84 - 87 */
  (Function) open_listen_by_af,
  (Function) open_telnet_dcc,
  (Function) _get_data_ptr,
  (Function) open_telnet,
  /* 88 - 91 */
  (Function) check_bind_event,
  (Function) egg_memcpy,
  (Function) my_atoul,
  (Function) my_strcpy,
  /* 92 - 95 */
  (Function) & dcc,		 /* struct dcc_t *			*/
  (Function) & chanset,		 /* struct chanset_t *			*/
  (Function) & userlist,	 /* struct userrec *			*/
  (Function) & lastuser,	 /* struct userrec *			*/
  /* 96 - 99 */
  (Function) & global_bans,	 /* struct banrec *			*/
  (Function) & global_ign,	 /* struct igrec *			*/
  (Function) & password_timeout, /* int					*/
  (Function) md5,
  /* 100 - 103 */
  (Function) & max_dcc,		 /* int					*/
  (Function) shouldjoin, 
  (Function) & ignore_time,	 /* int					*/
  (Function) & use_console_r,	 /* int					*/
  /* 104 - 107 */
  (Function) & reserved_port_min,
  (Function) & reserved_port_max,
  (Function) & debug_output,	 /* int					*/
  (Function) & noshare,		 /* int					*/
  /* 108 - 111 */
  (Function) do_chanset, 
  (Function) str_isdigit,
  (Function) & default_flags,	 /* int					*/
  (Function) & dcc_total,	 /* int					*/
  /* 112 - 115 */
  (Function) tempdir,		 /* char *				*/
  (Function) natip,		 /* char *				*/
  (Function) hostname,		 /* char *				*/
  (Function) origbotname,	 /* char *				*/
  /* 116 - 119 */
  (Function) botuser,		 /* char *				*/
  (Function) admin,		 /* char *				*/
  (Function) userfile,		 /* char *				*/
  (Function) ver,		 /* char *				*/
  /* 120 - 123 */
  (Function) notify_new,	 /* char *				*/
  (Function) dovoice,
  (Function) version,		 /* char *				*/
  (Function) botnetnick,	 /* char *				*/
  /* 124 - 127 */
  (Function) & DCC_CHAT_PASS,	 /* struct dcc_table *			*/
  (Function) & DCC_BOT,		 /* struct dcc_table *			*/
  (Function) & DCC_LOST,	 /* struct dcc_table *			*/
  (Function) & DCC_CHAT,	 /* struct dcc_table *			*/
  /* 128 - 131 */
  (Function) & interp,		 /* Tcl_Interp *			*/
  (Function) & now,		 /* time_t				*/
  (Function) findanyidx,
  (Function) findchan,
  /* 132 - 135 */
  (Function) dolimit,
  (Function) days,
  (Function) daysago,
  (Function) daysdur,
  /* 136 - 139 */
  (Function) ismember,
  (Function) newsplit,
  (Function) splitnick,
  (Function) splitc,
  /* 140 - 143 */
  (Function) addignore,
  (Function) match_ignore,
  (Function) delignore,
  (Function) fatal,
  /* 144 - 147 */
  (Function) xtra_kill, 
  (Function) xtra_unpack,
  (Function) movefile,
  (Function) copyfile,
  /* 148 - 151 */
  (Function) do_tcl,
  (Function) encrypt_string,
  (Function) decrypt_string,
  (Function) def_get,
  /* 152 - 155 */
  (Function) makepass,
  (Function) _wild_match,
  (Function) maskhost,
  (Function) private,
  /* 156 - 159 */
  (Function) chk_op,
  (Function) chk_deop,
  (Function) chk_voice,
  (Function) chk_devoice,
  /* 160 - 163 */
  (Function) touch_laston,
  (Function) & add_mode,	/* Function *				*/
  (Function) rmspace,
  (Function) in_chain,
  /* 164 - 167 */
  (Function) add_note,
  (Function) btoh,
  (Function) detect_dcc_flood,
  (Function) flush_lines,
  /* 168 - 171 */
  (Function) expected_memory,
  (Function) tell_mem_status,
  (Function) & do_restart,	/* int					*/
  (Function) 0,
  /* 172 - 175 */
  (Function) add_hook,
  (Function) del_hook,
  (Function) 0,
  (Function) 0,
  /* 176 - 179 */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  /* 180 - 183 */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  /* 184 - 187 */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  /* 188 - 191 */
  (Function) & USERENTRY_BOTADDR,	/* struct user_entry_type *	*/
  (Function) & USERENTRY_BOTFL,		/* struct user_entry_type *	*/
  (Function) & USERENTRY_HOSTS,		/* struct user_entry_type *	*/
  (Function) & USERENTRY_PASS,		/* struct user_entry_type *	*/
  /* 192 - 195 */
  (Function) & USERENTRY_XTRA,		/* struct user_entry_type *	*/
  (Function) user_del_chan,
  (Function) & USERENTRY_INFO,		/* struct user_entry_type *	*/
  (Function) & USERENTRY_COMMENT,	/* struct user_entry_type *	*/
  /* 196 - 199 */
  (Function) & USERENTRY_LASTON,	/* struct user_entry_type *	*/
  (Function) putlog,
  (Function) botnet_send_chan,
  (Function) list_type_kill,
  /* 200 - 203 */
  (Function) logmodes,
  (Function) masktype,
  (Function) stripmodes,
  (Function) stripmasktype,
  /* 204 - 207 */
  (Function) & online_since,	/* time_t *				*/
  (Function) & buildts,		/* time_t *				*/
  (Function) color,
  (Function) check_dcc_attrs,
  /* 208 - 211 */
  (Function) check_dcc_chanattrs,
  (Function) add_tcl_coups,
  (Function) rem_tcl_coups,
  (Function) botname,
  /* 212 - 215 */
  (Function) 0,			/* remove_gunk() -- UNUSED! (drummer)	*/
  (Function) check_tcl_chjn,
  (Function) sanitycheck_dcc,
  (Function) isowner,
  /* 216 - 219 */
  (Function) 0, /* min_dcc_port -- UNUSED! (guppy) */
  (Function) 0, /* max_dcc_port -- UNUSED! (guppy) */
  (Function) & rfc_casecmp,	/* Function *				*/
  (Function) & rfc_ncasecmp,	/* Function *				*/
  /* 220 - 223 */
  (Function) & global_exempts,	/* struct exemptrec *			*/
  (Function) & global_invites,	/* struct inviterec *			*/
  (Function) 0, /* ginvite_total -- UNUSED! (Eule) */
  (Function) 0, /* gexempt_total -- UNUSED! (Eule) */
  /* 224 - 227 */
  (Function) 0,
  (Function) & use_exempts,	/* int					*/
  (Function) & use_invites,	/* int					*/
  (Function) & force_expire,	/* int					*/
  /* 228 - 231 */
  (Function) 0,
  (Function) _user_realloc,
  (Function) mod_realloc,
  (Function) xtra_set, 
  /* 232 - 235 */
#ifdef DEBUG_CONTEXT
  (Function) eggContextNote,
#else
  (Function) 0,
#endif
#ifdef DEBUG_ASSERT
  (Function) eggAssert,
#else
  (Function) 0,
#endif
  (Function) allocsock,
  (Function) call_hostbyip,
  /* 236 - 239 */
  (Function) call_ipbyhost,
  (Function) iptostr,
  (Function) & DCC_DNSWAIT,	 /* struct dcc_table *			*/
  (Function) hostsanitycheck_dcc,
  /* 240 - 243 */
  (Function) dcc_dnsipbyhost,
  (Function) dcc_dnshostbyip,
  (Function) changeover_dcc,  
  (Function) make_rand_str,
  /* 244 - 247 */
  (Function) & protect_readonly, /* int					*/
  (Function) findchan_by_dname,
  (Function) removedcc,
  (Function) & userfile_perm,	 /* int					*/
  /* 248 - 251 */
  (Function) sock_has_data,
  (Function) bots_in_subtree,
  (Function) users_in_subtree,
  (Function) egg_inet_aton,
  /* 252 - 255 */
  (Function) egg_snprintf,
  (Function) egg_vsnprintf,
  (Function) egg_memset,
  (Function) egg_strcasecmp,
  /* 256 - 259 */
  (Function) egg_strncasecmp,
  (Function) is_file,
  (Function) & must_be_owner,	/* int					*/
  (Function) & tandbot,		/* tand_t *				*/
  /* 260 - 263 */
  (Function) & party,		/* party_t *				*/
  (Function) open_address_listen,
  (Function) str_escape,
  (Function) strchr_unescape,
  /* 264 - 267 */
  (Function) str_unescape,
  (Function) egg_strcatn,
  (Function) clear_chanlist_member,
  (Function) fixfrom,
  /* 268 - 272 */
  (Function) sockprotocol,
  (Function) & socklist,	/* sock_list *				*/
  (Function) sockoptions,
  (Function) flush_inbuf,
  (Function) kill_bot,
  /* 273 - 276 */
  (Function) quit_msg,		/* char *				*/
  (Function) module_load,
  (Function) 0,
  (Function) & parties,		/* int					*/
  /* 277 - 280 */
  (Function) ischanhub,        
  (Function) rand_dccresp,
  (Function) 0,
#ifdef LEAF
  (Function) listen_all,
#else
  (Function) 0, /* listen_all */
#endif
  /* 281 - 284 */
/* gay.
  (Function) MD5_Init,
  (Function) MD5_Update,
  (Function) MD5_Final,
*/
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) _wild_match_per,
  /* 285 - 288 */
  (Function) & role,	 /* int					*/
  (Function) & loading,	 /* int					*/
  (Function) & localhub, /* int					*/
  (Function) updatebin,
  (Function) stats_add,
  (Function) lower_bot_linked,
  (Function) add_cfg,
  (Function) set_cfg_str,
  (Function) trigger_cfg_changed,
  (Function) higher_bot_linked,
  (Function) bot_aggressive_to,
  (Function) botunlink,
  (Function) hostname6,		 /* char *				*/
  (Function) & timesync, /* int					*/
  (Function) 0, 
  (Function) kickreason,
  (Function) getting_users,
  (Function) 0,
  (Function) 0,
  (Function) & USERENTRY_ADDED,	/* struct user_entry_type *	*/
  (Function) bdhash,
  (Function) isupdatehub,
  (Function) 0,
  (Function) botlink,
  (Function) makeplaincookie,
  (Function) bankickprefix,
  (Function) kickprefix,
  (Function) deflag_user,
  (Function) dcc_prefix,
  (Function) goodpass,
#ifdef S_AUTH
  (Function) & auth, /* struct auth_t *auth */
  (Function) & auth_total,
  (Function) new_auth,
  (Function) findauth,
  (Function) removeauth,
  (Function) makehash,
  (Function) & USERENTRY_SECPASS,
  (Function) authkey,
#else
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) & USERENTRY_SECPASS,
  (Function) 0,
#endif /* S_AUTH */
  (Function) myip,
  (Function) myip6,
  (Function) cmdprefix,
  (Function) replace,
  (Function) degarble,
  (Function) egg_inet_ntop,
  (Function) open_listen,
  (Function) hostprotocol,
  (Function) sdprintf,
  (Function) putbot,
  (Function) putallbots,
  (Function) ssl_link,
  (Function) dropssl,
  (Function) myipstr,
  (Function) checkchans,
#ifdef S_MSGCMDS
  (Function) & CFG_MSGOP,
  (Function) & CFG_MSGPASS,
  (Function) & CFG_MSGINVITE,
  (Function) & CFG_MSGIDENT,
#else /* !S_MSGCMDS */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) 0,
#endif /* S_MSGCMDS */
  (Function) bind_table_add,
  (Function) bind_table_add,
  (Function) add_builtins,
  (Function) rem_builtins,
  (Function) bind_table_find,
  (Function) check_bind

};

static bind_table_t *BT_load;

void init_modules(void)
{
  int i;

  BT_load = bind_table_add("load", 1, "s", MATCH_MASK, 0);

  module_list = nmalloc(sizeof(module_entry));
  module_list->name = nmalloc(8);
  strcpy(module_list->name, "eggdrop");
  module_list->major = (egg_numver) / 10000;
  module_list->minor = ((egg_numver) / 100) % 100;
  module_list->next = NULL;
  module_list->funcs = NULL;
  for (i = 0; i < REAL_HOOKS; i++)
    hook_list[i] = NULL;
}

int expmem_modules(int y)
{
  int c = 0;
  int i;
  module_entry *p;
  dependancy *d;
  struct hook_entry *q;
  struct static_list *s;
  Function *f;

  for (s = static_modules; s; s = s->next)
    c += sizeof(struct static_list) + strlen(s->name) + 1;

  for (i = 0; i < REAL_HOOKS; i++)
    for (q = hook_list[i]; q; q = q->next)
      c += sizeof(struct hook_entry);

  for (d = dependancy_list; d; d = d->next)
    c += sizeof(dependancy);

  for (p = module_list; p; p = p->next) {
    c += sizeof(module_entry);
    c += strlen(p->name) + 1;
    f = p->funcs;
    if (f && f[MODCALL_EXPMEM] && !y)
      c += (int) (f[MODCALL_EXPMEM] ());
  }
  return c;
}

int module_register(char *name, Function * funcs,
		    int major, int minor)
{
  module_entry *p;

  for (p = module_list; p && p->name; p = p->next)
    if (!egg_strcasecmp(name, p->name)) {
      p->major = major;
      p->minor = minor;
      p->funcs = funcs;
      return 1;
    }
  return 0;
}

const char *module_load(char *name)
{
  module_entry *p;
  char *e;
  Function f;
  struct static_list *sl;

  sdprintf("module_load(\"%s\")", name);

  if (module_find(name, 0, 0) != NULL)
    return MOD_ALREADYLOAD;
  for (sl = static_modules; sl && egg_strcasecmp(sl->name, name); sl = sl->next);
  if (!sl)
    return "Unknown module.";
  f = (Function) sl->func;
  p = nmalloc(sizeof(module_entry));
  if (p == NULL)
    return "Malloc error";
  p->name = nmalloc(strlen(name) + 1);
  strcpy(p->name, name);
  p->major = 0;
  p->minor = 0;
  p->funcs = 0;
  p->next = module_list;
  module_list = p;
  e = (((char *(*)()) f) (global_table));
  if (e) {
    module_list = module_list->next;
    nfree(p->name);
    nfree(p);
    return e;
  }
  check_bind(BT_load, name, NULL, name);
  return NULL;
}

module_entry *module_find(char *name, int major, int minor)
{
  module_entry *p;

  for (p = module_list; p && p->name; p = p->next) 
    if ((major == p->major || !major) && minor <= p->minor && 
	!egg_strcasecmp(name, p->name))
      return p;
  return NULL;
}

static int module_rename(char *name, char *newname)
{
  module_entry *p;

  for (p = module_list; p; p = p->next)
    if (!egg_strcasecmp(newname, p->name))
      return 0;

  for (p = module_list; p && p->name; p = p->next)
    if (!egg_strcasecmp(name, p->name)) {
      nfree(p->name);
      p->name = nmalloc(strlen(newname) + 1);
      strcpy(p->name, newname);
      return 1;
    }
  return 0;
}

Function *module_depend(char *name1, char *name2, int major, int minor)
{
  module_entry *p = module_find(name2, major, minor);
  module_entry *o = module_find(name1, 0, 0);
  dependancy *d;

  if (!p) {
    if (module_load(name2))
      return 0;
    p = module_find(name2, major, minor);
  }
  if (!p || !o)
    return 0;
  d = nmalloc(sizeof(dependancy));

  d->needed = p;
  d->needing = o;
  d->next = dependancy_list;
  d->major = major;
  d->minor = minor;
  dependancy_list = d;
  return p->funcs ? p->funcs : (Function *) 1;
}

int module_undepend(char *name1)
{
  int ok = 0;
  module_entry *p = module_find(name1, 0, 0);
  dependancy *d = dependancy_list, *o = NULL;

  if (p == NULL)
    return 0;
  while (d != NULL) {
    if (d->needing == p) {
      if (o == NULL) {
	dependancy_list = d->next;
      } else {
	o->next = d->next;
      }
      nfree(d);
      if (o == NULL)
	d = dependancy_list;
      else
	d = o->next;
      ok++;
    } else {
      o = d;
      d = d->next;
    }
  }
  return ok;
}

void *mod_malloc(int size, const char *modname, const char *filename, int line)
{
#ifdef DEBUG_MEM
  char x[100], *p;

  p = strrchr(filename, '/');
  egg_snprintf(x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  return n_malloc(size, x, line);
#else
  return nmalloc(size);
#endif
}


void *mod_realloc(void *ptr, int size, const char *modname,
		  const char *filename, int line)
{
#ifdef DEBUG_MEM
  char x[100], *p;

  p = strrchr(filename, '/');
  egg_snprintf(x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  return n_realloc(ptr, size, x, line);
#else
  return nrealloc(ptr, size);
#endif
}

void mod_free(void *ptr, const char *modname, const char *filename, int line)
{
  char x[100], *p;

  p = strrchr(filename, '/');
  egg_snprintf(x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  n_free(ptr, x, line);
}

/* Hooks, various tables of functions to call on ceratin events
 */
void add_hook(int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p;

    for (p = hook_list[hook_num]; p; p = p->next)
      if (p->func == func)
	return;			/* Don't add it if it's already there */
    p = nmalloc(sizeof(struct hook_entry));

    p->next = hook_list[hook_num];
    hook_list[hook_num] = p;
    p->func = func;
  } else
    switch (hook_num) {
    case HOOK_SHAREOUT:
      shareout = (void (*)()) func;
      break;
    case HOOK_SHAREIN:
      sharein = (void (*)(int, char *)) func;
      break;
    case HOOK_SHAREUPDATEIN:
      shareupdatein = (void (*)(int, char *)) func;
      break;
    case HOOK_QSERV:
      if (qserver == (void (*)(int, char *, int)) null_func)
	qserver = (void (*)(int, char *, int)) func;
      break;
    case HOOK_ADD_MODE:
      if (add_mode == (void (*)()) null_func)
	add_mode = (void (*)()) func;
      break;
    /* special hook <drummer> */
    case HOOK_RFC_CASECMP:
      if (func == NULL) {
	rfc_casecmp = egg_strcasecmp;
	rfc_ncasecmp = (int (*)(const char *, const char *, int)) egg_strncasecmp;
	rfc_tolower = tolower;
	rfc_toupper = toupper;
      } else {
	rfc_casecmp = _rfc_casecmp;
	rfc_ncasecmp = _rfc_ncasecmp;
	rfc_tolower = _rfc_tolower;
	rfc_toupper = _rfc_toupper;
      }
      break;
    case HOOK_MATCH_NOTEREJ:
      if (match_noterej == (int (*)(struct userrec *, char *))false_func)
	match_noterej = func;
      break;
    case HOOK_STORENOTE:
	if (func == NULL) storenote = (int (*)(char *from, char *to, char *msg, int idx, char *who, int bufsize)) minus_func;
	else storenote = func;
	break;
    case HOOK_DNS_HOSTBYIP:
      if (dns_hostbyip == block_dns_hostbyip)
	dns_hostbyip = (void (*)(IP)) func;
      break;
    case HOOK_DNS_IPBYHOST:
      if (dns_ipbyhost == block_dns_ipbyhost)
	dns_ipbyhost = (void (*)(char *)) func;
      break;
    }
}

void del_hook(int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p = hook_list[hook_num], *o = NULL;

    while (p) {
      if (p->func == func) {
	if (o == NULL)
	  hook_list[hook_num] = p->next;
	else
	  o->next = p->next;
	nfree(p);
	break;
      }
      o = p;
      p = p->next;
    }
  } else
    switch (hook_num) {
    case HOOK_SHAREOUT:
      if (shareout == (void (*)()) func)
	shareout = null_func;
      break;
    case HOOK_SHAREIN:
      if (sharein == (void (*)(int, char *)) func)
	sharein = null_share;
      break;
    case HOOK_SHAREUPDATEIN:
      if (shareupdatein == (void (*)(int, char *)) func)
	shareupdatein = null_share;
      break;
    case HOOK_QSERV:
      if (qserver == (void (*)(int, char *, int)) func)
	qserver = null_func;
      break;
    case HOOK_ADD_MODE:
      if (add_mode == (void (*)()) func)
	add_mode = null_func;
      break;
    case HOOK_MATCH_NOTEREJ:
      if (match_noterej == (int (*)(struct userrec *, char *))func)
	match_noterej = false_func;
      break;
    case HOOK_STORENOTE:
	if (storenote == func) storenote = (int (*)(char *from, char *to, char *msg, int idx, char *who, int bufsize)) minus_func;
	break;
    case HOOK_DNS_HOSTBYIP:
      if (dns_hostbyip == (void (*)(IP)) func)
	dns_hostbyip = block_dns_hostbyip;
      break;
    case HOOK_DNS_IPBYHOST:
      if (dns_ipbyhost == (void (*)(char *)) func)
	dns_ipbyhost = block_dns_ipbyhost;
      break;
    }
}

int call_hook_cccc(int hooknum, char *a, char *b, char *c, char *d)
{
  struct hook_entry *p, *pn;
  int f = 0;

  if (hooknum >= REAL_HOOKS)
    return 0;
  p = hook_list[hooknum];
  for (p = hook_list[hooknum]; p && !f; p = pn) {
    pn = p->next;
    f = p->func(a, b, c, d);
  }
  return f;
}

void do_module_report(int idx, int details, char *which)
{
  module_entry *p = module_list;

  if (p && !which && details)
    dprintf(idx, "MODULES LOADED:\n");
  for (; p; p = p->next) {
    if (!which || !egg_strcasecmp(which, p->name)) {
      dependancy *d;

      if (details)
	dprintf(idx, "Module: %s, v %d.%d\n", p->name ? p->name : "CORE",
		p->major, p->minor);
      if (details > 1) {
	for (d = dependancy_list; d; d = d->next) 
	  if (d->needing == p)
	    dprintf(idx, "    requires: %s, v %d.%d\n", d->needed->name,
		    d->major, d->minor);
      }
      if (p->funcs) {
	Function f = p->funcs[MODCALL_REPORT];

	if (f != NULL)
	  f(idx, details);
      }
      if (which)
	return;
    }
  }
  if (which)
    dprintf(idx, "No such module.\n");
}
