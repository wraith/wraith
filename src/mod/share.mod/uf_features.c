typedef struct uff_list_struct
{
  struct uff_list_struct *next;
  struct uff_list_struct *prev;
  uff_table_t *entry;
} uff_list_t;
typedef struct
{
  uff_list_t *start;
  uff_list_t *end;
} uff_head_t;
static uff_head_t uff_list;
static char uff_sbuf[512];
static void
uff_init (void)
{
  egg_bzero (&uff_list, sizeof (uff_head_t));
}
static int
uff_expmem (void)
{
  uff_list_t *ul;
  int tot = 0;
  for (ul = uff_list.start; ul; ul = ul->next)
    tot += sizeof (uff_list_t);
  return tot;
}
static uff_list_t *
uff_findentry_byflag (int flag)
{
  uff_list_t *ul;
  for (ul = uff_list.start; ul; ul = ul->next)
    if (ul->entry->flag & flag)
      return ul;
  return NULL;
}
static uff_list_t *
uff_findentry_byname (char *feature)
{
  uff_list_t *ul;
  for (ul = uff_list.start; ul; ul = ul->next)
    if (!strcmp (ul->entry->feature, feature))
      return ul;
  return NULL;
}
static void
uff_insert_entry (uff_list_t * nul)
{
  uff_list_t *ul, *lul = NULL;
  ul = uff_list.start;
  while (ul && ul->entry->priority < nul->entry->priority)
    {
      lul = ul;
      ul = ul->next;
    }
  nul->prev = NULL;
  nul->next = NULL;
  if (lul)
    {
      if (lul->next)
	lul->next->prev = nul;
      nul->next = lul->next;
      nul->prev = lul;
      lul->next = nul;
    }
  else if (ul)
    {
      uff_list.start->prev = nul;
      nul->next = uff_list.start;
      uff_list.start = nul;
    }
  else
    uff_list.start = nul;
  if (!nul->next)
    uff_list.end = nul;
}
static void
uff_remove_entry (uff_list_t * ul)
{
  if (!ul->next)
    uff_list.end = ul->prev;
  else
    ul->next->prev = ul->prev;
  if (!ul->prev)
    uff_list.start = ul->next;
  else
    ul->prev->next = ul->next;
}
static void
uff_addfeature (uff_table_t * ut)
{
  uff_list_t *ul;
  if (uff_findentry_byname (ut->feature))
    {
      putlog (LOG_MISC, "*", "(!) share: same feature name used twice: %s",
	      ut->feature);
      return;
    }
  ul = uff_findentry_byflag (ut->flag);
  if (ul)
    {
      putlog (LOG_MISC, "*",
	      "(!) share: feature flag %d used twice by %s and %s", ut->flag,
	      ut->feature, ul->entry->feature);
      return;
    }
  ul = nmalloc (sizeof (uff_list_t));
  ul->entry = ut;
  uff_insert_entry (ul);
}
static void
uff_addtable (uff_table_t * ut)
{
  if (!ut)
    return;
  for (; ut->feature; ++ut)
    uff_addfeature (ut);
}
static int
uff_delfeature (uff_table_t * ut)
{
  uff_list_t *ul;
  for (ul = uff_list.start; ul; ul = ul->next)
    if (!strcmp (ul->entry->feature, ut->feature))
      {
	uff_remove_entry (ul);
	nfree (ul);
	return 1;
      }
  return 0;
}
static void
uff_deltable (uff_table_t * ut)
{
  if (!ut)
    return;
  for (; ut->feature; ++ut)
    (int) uff_delfeature (ut);
} static void
uf_features_parse (int idx, char *par)
{
  char *buf, *s, *p;
  uff_list_t *ul;
  uff_sbuf[0] = 0;
  p = s = buf = nmalloc (strlen (par) + 1);
  strcpy (buf, par);
  dcc[idx].u.bot->uff_flags = 0;
  while ((s = strchr (s, ' ')) != NULL)
    {
      *s = '\0';
      ul = uff_findentry_byname (p);
      if (ul && (ul->entry->ask_func == NULL || ul->entry->ask_func (idx)))
	{
	  dcc[idx].u.bot->uff_flags |= ul->entry->flag;
	  strcat (uff_sbuf, ul->entry->feature);
	  strcat (uff_sbuf, " ");
	}
      p = ++s;
    }
  nfree (buf);
  if (uff_sbuf[0])
    dprintf (idx, "s feats %s\n", uff_sbuf);
}
static char *
uf_features_dump (int idx)
{
  uff_list_t *ul;
  uff_sbuf[0] = 0;
  for (ul = uff_list.start; ul; ul = ul->next)
    if (ul->entry->ask_func == NULL || ul->entry->ask_func (idx))
      {
	strcat (uff_sbuf, ul->entry->feature);
	strcat (uff_sbuf, " ");
      }
  return uff_sbuf;
}
static int
uf_features_check (int idx, char *par)
{
  char *buf, *s, *p;
  uff_list_t *ul;
  uff_sbuf[0] = 0;
  p = s = buf = nmalloc (strlen (par) + 1);
  strcpy (buf, par);
  dcc[idx].u.bot->uff_flags = 0;
  while ((s = strchr (s, ' ')) != NULL)
    {
      *s = '\0';
      ul = uff_findentry_byname (p);
      if (ul && (ul->entry->ask_func == NULL || ul->entry->ask_func (idx)))
	dcc[idx].u.bot->uff_flags |= ul->entry->flag;
      else
	{
	  putlog (LOG_BOTS, "*", "Bot %s tried unsupported feature!",
		  dcc[idx].nick);
	  dprintf (idx, "s e Attempt to use an unsupported feature\n");
	  zapfbot (idx);
	  nfree (buf);
	  return 0;
	}
      p = ++s;
    }
  nfree (buf);
  return 1;
}
static int
uff_call_sending (int idx, char *user_file)
{
  uff_list_t *ul;
  for (ul = uff_list.start; ul; ul = ul->next)
    if (ul->entry && ul->entry->snd
	&& (dcc[idx].u.bot->uff_flags & ul->entry->flag))
      if (!(ul->entry->snd (idx, user_file)))
	return 0;
  return 1;
}
static int
uff_call_receiving (int idx, char *user_file)
{
  uff_list_t *ul;
  for (ul = uff_list.end; ul; ul = ul->prev)
    if (ul->entry && ul->entry->rcv
	&& (dcc[idx].u.bot->uff_flags & ul->entry->flag))
      if (!(ul->entry->rcv (idx, user_file)))
	return 0;
  return 1;
}
static int
uff_ask_override_bots (int idx)
{
  if (overr_local_bots)
    return 1;
  else
    return 0;
}
static uff_table_t internal_uff_table[] =
  { {"overbots", UFF_OVERRIDE, uff_ask_override_bots, 0, NULL, NULL},
  {"invites", UFF_INVITE, NULL, 0, NULL, NULL}, {"exempts", UFF_EXEMPT, NULL,
						 0, NULL, NULL}, {"chans",
								  UFF_CHANS,
								  NULL, 0,
								  NULL, NULL},
  {"tcl", UFF_TCL, NULL, 0, NULL, NULL}, {NULL, 0, NULL, 0, NULL, NULL} };
