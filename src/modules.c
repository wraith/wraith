#include "main.h"
#include "modules.h"
#include "tandem.h"
#include <ctype.h>
extern struct dcc_t *dcc;
#include "users.h"
extern Tcl_Interp *interp;
extern struct userrec *userlist, *lastuser;
extern char tempdir[], botnetnick[], botname[], natip[], hostname[],
  origbotname[], botuser[], admin[], userfile[], ver[], notify_new[],
  kickprefix[], bankickprefix[], version[], quit_msg[], hostname6[],
  netpass[], thepass[];
extern int noshare, loading, role, dcc_total, egg_numver, userfile_perm,
  use_console_r, ignore_time, must_be_owner, debug_output, default_flags,
  norestruct, max_dcc, share_greet, password_timeout, localhub,
#ifdef S_IRCNET
  use_invites, use_exempts,
#endif
 
  force_expire, do_restart, timesync, protect_readonly, reserved_port_min,
  reserved_port_max;
extern time_t now, online_since;
extern struct chanset_t *chanset;
extern tand_t *tandbot;
extern party_t *party;
extern int parties;
extern sock_list *socklist;
extern int getprotocol (char *);
int cmd_die ();
int xtra_kill ();
int xtra_unpack ();
static int module_rename (char *name, char *newname);
struct static_list
{
  struct static_list *next;
  char *name;
  char *(*func) ();
} *static_modules = NULL;
void
check_static (char *name, char *(*func) ())
{
  struct static_list *p = nmalloc (sizeof (struct static_list));
  p->name = nmalloc (strlen (name) + 1);
  strcpy (p->name, name);
  p->func = func;
  p->next = static_modules;
  static_modules = p;
} void

null_func ()
{
} char *

charp_func ()
{
  return NULL;
}

int
minus_func ()
{
  return -1;
}

int
false_func ()
{
  return 0;
}
struct hook_entry *hook_list[REAL_HOOKS];
static void
null_share (int idx, char *x)
{
  if ((x[0] == 'u') && (x[1] == 'n'))
    {
      putlog (LOG_BOTS, "*", "User file rejected by %s: %s", dcc[idx].nick,
	      x + 3);
      dcc[idx].status &= ~STAT_OFFERED;
      if (!(dcc[idx].status & STAT_GETTING))
	{
	  dcc[idx].status &= ~STAT_SHARE;
	}
    }
  else if ((x[0] != 'v') && (x[0] != 'e'))
    dprintf (idx, "s un Not sharing userfile.\n");
}

void (*encrypt_pass) (char *, char *) = 0;
char *(*encrypt_string) (char *, char *) = 0;
char *(*decrypt_string) (char *, char *) = 0;
void (*shareout) () = null_func;
void (*sharein) (int, char *) = null_share;
void (*shareupdatein) (int, char *) = null_share;
void (*qserver) (int, char *, int) = (void (*)(int, char *, int)) null_func;
void (*add_mode) () = null_func;
int (*match_noterej) (struct userrec *, char *) =
  (int (*)(struct userrec *, char *)) false_func;
int (*rfc_casecmp) (const char *, const char *) = _rfc_casecmp;
int (*rfc_ncasecmp) (const char *, const char *, int) = _rfc_ncasecmp;
int (*rfc_toupper) (int) = _rfc_toupper;
int (*rfc_tolower) (int) = _rfc_tolower;
void (*dns_hostbyip) (IP) = block_dns_hostbyip;
void (*dns_ipbyhost) (char *) = block_dns_ipbyhost;
module_entry *module_list;
dependancy *dependancy_list = NULL;
Function global_table[] = { (Function) mod_malloc, (Function) mod_free,
#ifdef DEBUG_CONTEXT
  (Function) eggContext,
#else
  (Function) 0,
#endif
  (Function) module_rename, (Function) module_register,
    (Function) module_find, (Function) module_depend,
    (Function) module_undepend, (Function) add_bind_table,
    (Function) del_bind_table, (Function) find_bind_table,
    (Function) check_tcl_bind, (Function) add_builtins,
    (Function) rem_builtins, (Function) add_tcl_commands,
    (Function) rem_tcl_commands, (Function) add_tcl_ints,
    (Function) rem_tcl_ints, (Function) add_tcl_strings,
    (Function) rem_tcl_strings, (Function) base64_to_int,
    (Function) int_to_base64, (Function) int_to_base10,
    (Function) simple_sprintf, (Function) botnet_send_zapf,
    (Function) botnet_send_zapf_broad, (Function) botnet_send_unlinked,
    (Function) botnet_send_bye, (Function) botnet_send_chat,
    (Function) botnet_send_filereject, (Function) botnet_send_filesend,
    (Function) botnet_send_filereq, (Function) botnet_send_join_idx,
    (Function) botnet_send_part_idx, (Function) updatebot, (Function) nextbot,
    (Function) zapfbot, (Function) n_free, (Function) u_pass_match,
    (Function) _user_malloc, (Function) get_user, (Function) set_user,
    (Function) add_entry_type, (Function) del_entry_type,
    (Function) get_user_flagrec, (Function) set_user_flagrec,
    (Function) get_user_by_host, (Function) get_user_by_handle,
    (Function) find_entry_type, (Function) find_user_entry,
    (Function) adduser, (Function) deluser, (Function) addhost_by_handle,
    (Function) delhost_by_handle, (Function) readuserfile,
    (Function) write_userfile, (Function) geticon, (Function) clear_chanlist,
    (Function) reaffirm_owners, (Function) change_handle,
    (Function) write_user, (Function) clear_userlist, (Function) count_users,
    (Function) sanity_check, (Function) break_down_flags,
    (Function) build_flags, (Function) flagrec_eq, (Function) flagrec_ok,
    (Function) & shareout, (Function) dprintf, (Function) chatout,
    (Function) chanout_but, (Function) check_validity, (Function) list_delete,
    (Function) list_append, (Function) list_contains, (Function) answer,
    (Function) getmyip, (Function) neterror, (Function) tputs,
    (Function) new_dcc, (Function) lostdcc, (Function) getsock,
    (Function) killsock, (Function) open_listen, (Function) open_telnet_dcc,
    (Function) _get_data_ptr, (Function) open_telnet,
    (Function) check_tcl_event, (Function) egg_memcpy, (Function) my_atoul,
    (Function) my_strcpy, (Function) & dcc, (Function) & chanset,
    (Function) & userlist, (Function) & lastuser, (Function) & global_bans,
    (Function) & global_ign, (Function) & password_timeout,
    (Function) & share_greet, (Function) & max_dcc, (Function) 0,
    (Function) & ignore_time, (Function) & use_console_r,
    (Function) & reserved_port_min, (Function) & reserved_port_max,
    (Function) & debug_output, (Function) & noshare, (Function) 0,
    (Function) 0, (Function) & default_flags, (Function) & dcc_total,
    (Function) tempdir, (Function) natip, (Function) hostname,
    (Function) origbotname, (Function) botuser, (Function) admin,
    (Function) userfile, (Function) ver, (Function) notify_new, (Function) 0,
    (Function) version, (Function) botnetnick, (Function) & DCC_CHAT_PASS,
    (Function) & DCC_BOT, (Function) & DCC_LOST, (Function) & DCC_CHAT,
    (Function) & interp, (Function) & now, (Function) findanyidx,
    (Function) findchan, (Function) cmd_die, (Function) days,
    (Function) daysago, (Function) daysdur, (Function) ismember,
    (Function) newsplit, (Function) splitnick, (Function) splitc,
    (Function) addignore, (Function) match_ignore, (Function) delignore,
    (Function) fatal, (Function) xtra_kill, (Function) xtra_unpack,
    (Function) movefile, (Function) copyfile, (Function) do_tcl,
    (Function) readtclprog, (Function) get_language, (Function) def_get,
    (Function) makepass, (Function) _wild_match, (Function) maskhost,
    (Function) show_motd, (Function) 0, (Function) 0, (Function) 0,
    (Function) 0, (Function) touch_laston, (Function) & add_mode,
    (Function) rmspace, (Function) in_chain, (Function) add_note,
    (Function) del_lang_section, (Function) detect_dcc_flood,
    (Function) flush_lines, (Function) expected_memory,
    (Function) tell_mem_status, (Function) & do_restart,
    (Function) check_tcl_filt, (Function) add_hook, (Function) del_hook,
    (Function) & H_dcc, (Function) & H_filt, (Function) & H_chon,
    (Function) & H_chof, (Function) & H_load, (Function) & H_unld,
    (Function) & H_chat, (Function) & H_act, (Function) & H_bcst,
    (Function) & H_bot, (Function) & H_link, (Function) & H_disc,
    (Function) & H_away, (Function) & H_nkch, (Function) & USERENTRY_BOTADDR,
    (Function) & USERENTRY_BOTFL, (Function) & USERENTRY_HOSTS,
    (Function) & USERENTRY_PASS, (Function) & USERENTRY_XTRA,
    (Function) user_del_chan, (Function) & USERENTRY_INFO,
    (Function) & USERENTRY_COMMENT, (Function) & USERENTRY_LASTON,
    (Function) putlog, (Function) botnet_send_chan, (Function) list_type_kill,
    (Function) logmodes, (Function) masktype, (Function) stripmodes,
    (Function) stripmasktype, (Function) 0, (Function) & online_since,
    (Function) 0, (Function) check_dcc_attrs, (Function) check_dcc_chanattrs,
    (Function) add_tcl_coups, (Function) rem_tcl_coups, (Function) botname,
    (Function) check_topic, (Function) check_tcl_chjn,
    (Function) sanitycheck_dcc, (Function) isowner, (Function) 0,
    (Function) 0, (Function) & rfc_casecmp, (Function) & rfc_ncasecmp,
#ifdef S_IRCNET
  (Function) & global_exempts, (Function) & global_invites,
#else
  (Function) 0, (Function) 0,
#endif
  (Function) 0, (Function) 0, (Function) & H_event,
#ifdef S_IRCNET
  (Function) & use_exempts, (Function) & use_invites,
#else
  (Function) 0, (Function) 0,
#endif
  (Function) & force_expire, (Function) add_lang_section,
    (Function) _user_realloc, (Function) mod_realloc, (Function) xtra_set,
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
  (Function) allocsock, (Function) call_hostbyip, (Function) call_ipbyhost,
    (Function) iptostr, (Function) & DCC_DNSWAIT,
    (Function) hostsanitycheck_dcc, (Function) dcc_dnsipbyhost,
    (Function) dcc_dnshostbyip, (Function) changeover_dcc,
    (Function) make_rand_str, (Function) & protect_readonly,
    (Function) findchan_by_dname, (Function) removedcc,
    (Function) & userfile_perm, (Function) sock_has_data,
    (Function) bots_in_subtree, (Function) users_in_subtree,
    (Function) egg_inet_aton, (Function) egg_snprintf,
    (Function) egg_vsnprintf, (Function) egg_memset,
    (Function) egg_strcasecmp, (Function) egg_strncasecmp, (Function) is_file,
    (Function) & must_be_owner, (Function) & tandbot, (Function) & party,
    (Function) open_address_listen, (Function) str_escape,
    (Function) strchr_unescape, (Function) str_unescape,
    (Function) egg_strcatn, (Function) clear_chanlist_member,
    (Function) fixfrom, (Function) getprotocol, (Function) & socklist,
    (Function) sockoptions, (Function) flush_inbuf, (Function) kill_bot,
    (Function) quit_msg, (Function) module_load, (Function) module_unload,
    (Function) & parties, (Function) ischanhub, (Function) rand_dccresp,
    (Function) issechub,
#ifdef LEAF
  (Function) listen_all,
#else
  (Function) 0,
#endif
  (Function) 0, (Function) 0, (Function) 0, (Function) _wild_match_per,
    (Function) & role, (Function) & loading, (Function) & localhub,
    (Function) updatebin, (Function) stats_add, (Function) lower_bot_linked,
    (Function) add_cfg, (Function) set_cfg_str,
    (Function) trigger_cfg_changed, (Function) higher_bot_linked,
    (Function) bot_aggressive_to, (Function) botunlink, (Function) hostname6,
    (Function) & timesync, (Function) netpass, (Function) kickreason,
    (Function) getting_users, (Function) add_builtins_dcc,
    (Function) rem_builtins_dcc, (Function) & USERENTRY_ADDED,
    (Function) thepass, (Function) isupdatehub, (Function) & norestruct,
    (Function) botlink, (Function) makeplaincookie, (Function) bankickprefix,
    (Function) kickprefix, (Function) deflag_user
};
void
init_modules (void)
{
  int i;
  module_list = nmalloc (sizeof (module_entry));
  module_list->name = nmalloc (8);
  strcpy (module_list->name, "eggdrop");
  module_list->major = (egg_numver) / 10000;
  module_list->minor = ((egg_numver) / 100) % 100;
  module_list->next = NULL;
  module_list->funcs = NULL;
  for (i = 0; i < REAL_HOOKS; i++)
    hook_list[i] = NULL;
}

int
expmem_modules (int y)
{
  int c = 0;
  int i;
  module_entry *p;
  dependancy *d;
  struct hook_entry *q;
  struct static_list *s;
  Function *f;
  for (s = static_modules; s; s = s->next)
    c += sizeof (struct static_list) + strlen (s->name) + 1;
  for (i = 0; i < REAL_HOOKS; i++)
    for (q = hook_list[i]; q; q = q->next)
      c += sizeof (struct hook_entry);
  for (d = dependancy_list; d; d = d->next)
    c += sizeof (dependancy);
  for (p = module_list; p; p = p->next)
    {
      c += sizeof (module_entry);
      c += strlen (p->name) + 1;
      f = p->funcs;
      if (f && f[MODCALL_EXPMEM] && !y)
	c += (int) (f[MODCALL_EXPMEM] ());
    } return c;
}

int
module_register (char *name, Function * funcs, int major, int minor)
{
  module_entry *p;
  for (p = module_list; p && p->name; p = p->next)
    if (!egg_strcasecmp (name, p->name))
      {
	p->major = major;
	p->minor = minor;
	p->funcs = funcs;
	return 1;
      }
  return 0;
}
const char *
module_load (char *name)
{
  module_entry *p;
  char *e;
  Function f;
  struct static_list *sl;
  if (module_find (name, 0, 0) != NULL)
    return MOD_ALREADYLOAD;
  for (sl = static_modules; sl && egg_strcasecmp (sl->name, name);
       sl = sl->next);
  if (!sl)
    return "Unknown module.";
  f = (Function) sl->func;
  p = nmalloc (sizeof (module_entry));
  if (p == NULL)
    return "Malloc error";
  p->name = nmalloc (strlen (name) + 1);
  strcpy (p->name, name);
  p->major = 0;
  p->minor = 0;
  p->funcs = 0;
  p->next = module_list;
  module_list = p;
  e = (((char *(*)()) f) (global_table));
  if (e)
    {
      module_list = module_list->next;
      nfree (p->name);
      nfree (p);
      return e;
    }
  check_tcl_load (name);
  return NULL;
}

char *
module_unload (char *name, char *user)
{
  module_entry *p = module_list, *o = NULL;
  char *e;
  Function *f;
  while (p)
    {
      if ((p->name != NULL) && (!strcmp (name, p->name)))
	{
	  dependancy *d;
	  for (d = dependancy_list; d; d = d->next)
	    if (d->needed == p)
	      return MOD_NEEDED;
	  f = p->funcs;
	  if (f && !f[MODCALL_CLOSE])
	    return MOD_NOCLOSEDEF;
	  if (f)
	    {
	      check_tcl_unld (name);
	      e = (((char *(*)()) f[MODCALL_CLOSE]) (user));
	      if (e != NULL)
		return e;
	    }
	  nfree (p->name);
	  if (o == NULL)
	    {
	      module_list = p->next;
	    }
	  else
	    {
	      o->next = p->next;
	    }
	  nfree (p);
	  putlog (LOG_MISC, "*", "%s %s", MOD_UNLOADED, name);
	  return NULL;
	}
      o = p;
      p = p->next;
    }
  return MOD_NOSUCH;
}

module_entry *
module_find (char *name, int major, int minor)
{
  module_entry *p;
  for (p = module_list; p && p->name; p = p->next)
    if ((major == p->major || !major) && minor <= p->minor
	&& !egg_strcasecmp (name, p->name))
      return p;
  return NULL;
}
static int
module_rename (char *name, char *newname)
{
  module_entry *p;
  for (p = module_list; p; p = p->next)
    if (!egg_strcasecmp (newname, p->name))
      return 0;
  for (p = module_list; p && p->name; p = p->next)
    if (!egg_strcasecmp (name, p->name))
      {
	nfree (p->name);
	p->name = nmalloc (strlen (newname) + 1);
	strcpy (p->name, newname);
	return 1;
      }
  return 0;
}

Function *
module_depend (char *name1, char *name2, int major, int minor)
{
  module_entry *p = module_find (name2, major, minor);
  module_entry *o = module_find (name1, 0, 0);
  dependancy *d;
  if (!p)
    {
      if (module_load (name2))
	return 0;
      p = module_find (name2, major, minor);
    }
  if (!p || !o)
    return 0;
  d = nmalloc (sizeof (dependancy));
  d->needed = p;
  d->needing = o;
  d->next = dependancy_list;
  d->major = major;
  d->minor = minor;
  dependancy_list = d;
  return p->funcs ? p->funcs : (Function *) 1;
}

int
module_undepend (char *name1)
{
  int ok = 0;
  module_entry *p = module_find (name1, 0, 0);
  dependancy *d = dependancy_list, *o = NULL;
  if (p == NULL)
    return 0;
  while (d != NULL)
    {
      if (d->needing == p)
	{
	  if (o == NULL)
	    {
	      dependancy_list = d->next;
	    }
	  else
	    {
	      o->next = d->next;
	    }
	  nfree (d);
	  if (o == NULL)
	    d = dependancy_list;
	  else
	    d = o->next;
	  ok++;
	}
      else
	{
	  o = d;
	  d = d->next;
	}
    }
  return ok;
}

void *
mod_malloc (int size, const char *modname, const char *filename, int line)
{
#ifdef DEBUG_MEM
  char x[100], *p;
  p = strrchr (filename, '/');
  egg_snprintf (x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  return n_malloc (size, x, line);
#else
  return nmalloc (size);
#endif
}

void *
mod_realloc (void *ptr, int size, const char *modname, const char *filename,
	     int line)
{
#ifdef DEBUG_MEM
  char x[100], *p;
  p = strrchr (filename, '/');
  egg_snprintf (x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  return n_realloc (ptr, size, x, line);
#else
  return nrealloc (ptr, size);
#endif
}

void
mod_free (void *ptr, const char *modname, const char *filename, int line)
{
  char x[100], *p;
  p = strrchr (filename, '/');
  egg_snprintf (x, sizeof x, "%s:%s", modname, p ? p + 1 : filename);
  x[19] = 0;
  n_free (ptr, x, line);
}

void
add_hook (int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS)
    {
      struct hook_entry *p;
      for (p = hook_list[hook_num]; p; p = p->next)
	if (p->func == func)
	  return;
      p = nmalloc (sizeof (struct hook_entry));
      p->next = hook_list[hook_num];
      hook_list[hook_num] = p;
      p->func = func;
    }
  else
    switch (hook_num)
      {
      case HOOK_ENCRYPT_PASS:
	encrypt_pass = (void (*)(char *, char *)) func;
	break;
      case HOOK_ENCRYPT_STRING:
	encrypt_string = (char *(*)(char *, char *)) func;
	break;
      case HOOK_DECRYPT_STRING:
	decrypt_string = (char *(*)(char *, char *)) func;
	break;
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
      case HOOK_RFC_CASECMP:
	if (func == NULL)
	  {
	    rfc_casecmp = egg_strcasecmp;
	    rfc_ncasecmp =
	      (int (*)(const char *, const char *, int)) egg_strncasecmp;
	    rfc_tolower = tolower;
	    rfc_toupper = toupper;
	  }
	else
	  {
	    rfc_casecmp = _rfc_casecmp;
	    rfc_ncasecmp = _rfc_ncasecmp;
	    rfc_tolower = _rfc_tolower;
	    rfc_toupper = _rfc_toupper;
	  }
	break;
      case HOOK_MATCH_NOTEREJ:
	if (match_noterej == (int (*)(struct userrec *, char *)) false_func)
	  match_noterej = func;
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
void
del_hook (int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS)
    {
      struct hook_entry *p = hook_list[hook_num], *o = NULL;
      while (p)
	{
	  if (p->func == func)
	    {
	      if (o == NULL)
		hook_list[hook_num] = p->next;
	      else
		o->next = p->next;
	      nfree (p);
	      break;
	    }
	  o = p;
	  p = p->next;
	}
    }
  else
    switch (hook_num)
      {
      case HOOK_ENCRYPT_PASS:
	if (encrypt_pass == (void (*)(char *, char *)) func)
	  encrypt_pass = (void (*)(char *, char *)) null_func;
	break;
      case HOOK_ENCRYPT_STRING:
	if (encrypt_string == (char *(*)(char *, char *)) func)
	  encrypt_string = (char *(*)(char *, char *)) null_func;
	break;
      case HOOK_DECRYPT_STRING:
	if (decrypt_string == (char *(*)(char *, char *)) func)
	  decrypt_string = (char *(*)(char *, char *)) null_func;
	break;
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
	if (match_noterej == (int (*)(struct userrec *, char *)) func)
	  match_noterej = false_func;
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
int
call_hook_cccc (int hooknum, char *a, char *b, char *c, char *d)
{
  struct hook_entry *p, *pn;
  int f = 0;
  if (hooknum >= REAL_HOOKS)
    return 0;
  p = hook_list[hooknum];
  for (p = hook_list[hooknum]; p && !f; p = pn)
    {
      pn = p->next;
      f = p->func (a, b, c, d);
    }
  return f;
}

void
do_module_report (int idx, int details, char *which)
{
  module_entry *p = module_list;
  if (p && !which && details)
    dprintf (idx, "MODULES LOADED:\n");
  for (; p; p = p->next)
    {
      if (!which || !egg_strcasecmp (which, p->name))
	{
	  dependancy *d;
	  if (details)
	    dprintf (idx, "Module: %s, v %d.%d\n", p->name ? p->name : "CORE",
		     p->major, p->minor);
	  if (details > 1)
	    {
	      for (d = dependancy_list; d; d = d->next)
		if (d->needing == p)
		  dprintf (idx, "    requires: %s, v %d.%d\n",
			   d->needed->name, d->major, d->minor);
	    }
	  if (p->funcs)
	    {
	      Function f = p->funcs[MODCALL_REPORT];
	      if (f != NULL)
		f (idx, details);
	    }
	  if (which)
	    return;
	}
    }
  if (which)
    dprintf (idx, "No such module.\n");
}
