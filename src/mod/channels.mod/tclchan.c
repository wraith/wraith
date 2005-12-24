/*
 * tclchan.c -- part of channels.mod
 *
 */


static int FindElement(char *resultBuf, const char *list, size_t listLength, 
                       const char **elementPtr, const char **nextPtr, 
                       size_t *sizePtr, int *bracePtr)
{
    const char *p = list;
    const char *elemStart = NULL;	/* Points to first byte of first element. */
    const char *limit = (list + listLength);		/* Points just after list's last byte. */
    int openBraces = 0;   	/* Brace nesting level during parse. */
    int inQuotes = 0;
    size_t size = 0;               /* lint. */
    const char *p2 = NULL;

    /*
     * Skim off leading white space and check for an opening brace or
     * quote. We treat embedded NULLs in the list as bytes belonging to
     * a list element.
     */

    while ((p < limit) && egg_isspace(*p)) { /* INTL: ISO space. */
        p++;
    }

    if (p == limit) {           /* no element found */
        elemStart = limit;
        goto done;
    }

    if (*p == '{') {
        openBraces = 1;
        p++;
    } else if (*p == '"') {
        inQuotes = 1;
        p++;
    }
    elemStart = p;
    if (bracePtr != 0) {
        *bracePtr = openBraces;
    }

    /*
     * Find element's end (a space, close brace, or the end of the string).
     */

    while (p < limit) {
        switch (*p) {
            /*
             * Open brace: don't treat specially unless the element is in
             * braces. In this case, keep a nesting count.
             */

            case '{':
                if (openBraces != 0) {
                    openBraces++;
                }
                break;

            /*
             * Close brace: if element is in braces, keep nesting count and
             * quit when the last close brace is seen.
             */

            case '}':
                if (openBraces > 1) {
                    openBraces--;
                } else if (openBraces == 1) {
                    size = (p - elemStart);
                    p++;
                    if ((p >= limit) || egg_isspace(*p)) { /* INTL: ISO space. */
                        goto done;
                    }

                    /*
                     * Garbage after the closing brace; return an error.
                     */

                    if (resultBuf) {
                        p2 = p;
                        while ((p2 < limit)
                                && (!egg_isspace(*p2)) /* INTL: ISO space. */
                                && (p2 < p+20)) {
                            p2++;
                        }
                        sprintf(resultBuf, "list element in braces followed by \"%.*s\" instead of space", (int) (p2-p), p);
                    }
                    return ERROR;
                }
                break;

            /*
             * Backslash:  skip over everything up to the end of the
             * backslash sequence.
             */

/*            case '\\': {
                Tcl_UtfBackslash(p, &numChars, NULL);
                p += (numChars - 1);
                break;
            }
*/
            /*
             * Space: ignore if element is in braces or quotes; otherwise
             * terminate element.
             */

            case ' ':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
            case '\v':
                if ((openBraces == 0) && !inQuotes) {
                    size = (p - elemStart);
                    goto done;
                }
                break;

            /*
             * Double-quote: if element is in quotes then terminate it.
             */

            case '"':
                if (inQuotes) {
                    size = (p - elemStart);
                    p++;
                    if ((p >= limit) || egg_isspace(*p)) { /* INTL: ISO space */
                        goto done;
                    }

                    /*
                     * Garbage after the closing quote; return an error.
                     */

                    if (resultBuf) {
                        p2 = p;
                        while ((p2 < limit)
                                && (!egg_isspace(*p2)) /* INTL: ISO space */
                                 && (p2 < p+20)) {
                            p2++;
                        }
                        sprintf(resultBuf, "list element in quotes followed by \"%.*s\" %s", (int) (p2-p), p, "instead of space");
                    }
                    return ERROR;
                }
                break;
        }
        p++;
    }


    /*
     * End of list: terminate element.
     */

    if (p == limit) {
        if (openBraces != 0) {
            if (resultBuf) {
                simple_snprintf(resultBuf, RESULT_LEN, "unmatched open brace in list");
            }
            return ERROR;
        } else if (inQuotes) {
            if (resultBuf) {
                simple_snprintf(resultBuf, RESULT_LEN, "unmatched open quote in list");
            }
            return ERROR;
        }
        size = (p - elemStart);
    }

done:
    while ((p < limit) && (egg_isspace(*p))) { /* INTL: ISO space. */
        p++;
    }
    *elementPtr = elemStart;
    *nextPtr = p;
    if (sizePtr != 0) {
        *sizePtr = size;
    }
    return OK;
}

/* unneeded?
int CopyAndCollapse(int count, const char *src, char *dst)
{
    register char c;
    int numRead; 
    int newCount = 0;
    int backslashCount; 

    for (c = *src;  count > 0;  src++, c = *src, count--) {
        if (c == '\\') {
           backslashCount = Tcl_UtfBackslash(src, &numRead, dst);
            dst += backslashCount;
            newCount += backslashCount;
            src += numRead-1;
            count -= numRead-1;
        } else { 
            *dst = c;
            dst++;
            newCount++;
        } 
    }
    *dst = 0;
    return newCount;
}
*/


int SplitList(char *resultBuf, const char *list, int *argcPtr, const char ***argvPtr)
{
    const char **argv = NULL;
    const char *l = NULL;
    const char *element = NULL;
    char *p = NULL;
    int result, brace;
    size_t size = 2;		/* initialized to 1 for NULL pointer */
    size_t length, elSize, i = 0;

    /*
     * Figure out how much space to allocate.  There must be enough
     * space for both the array of pointers and also for a copy of
     * the list.  To estimate the number of pointers needed, count
     * the number of space characters in the list.
     */

    for (l = list; *l != 0; l++) {
        if (egg_isspace(*l)) { /* INTL: ISO space. */
            size++;
        }
    }

    argv = (const char **) my_calloc(1, (unsigned) ((size * sizeof(char *)) + (l - list) + 1 + 15));	/* 15 cuz the tcl src is hard to follow */

    length = strlen(list);

    for (p = ((char *) argv) + size*sizeof(char *); *list != 0; i++) {
        const char *prevList = list;

        result = FindElement(resultBuf, list, length, &element, &list, &elSize, &brace);

        length -= (list - prevList);

        if (result != OK) {
            free((char *) argv);
            return result;
        }

        if (*element == 0) {
            break;
        }

        if (i >= size) {
            free((char *) argv);
            if (resultBuf)
                simple_snprintf(resultBuf, RESULT_LEN, "internal error in SplitList");
            return ERROR;
        }

        argv[i] = p;

        if (brace) {
            egg_memcpy(p, element, elSize);
            p += elSize;
            *p = 0;
            p++;
        } else {
/*            CopyAndCollapse(elSize, element, p); */
            egg_memcpy(p, element, elSize);
            p += elSize + 1;
        }
    }

    argv[i] = NULL;
    *argvPtr = argv;
    *argcPtr = i;
    return OK;
}


/* Parse options for a channel.
 */
int channel_modify(char *result, struct chanset_t *chan, int items, char **item)
{
  bool error = 0;
  int old_status = chan->status,
      old_mode_mns_prot = chan->mode_mns_prot,
      old_mode_pls_prot = chan->mode_pls_prot;
  char s[121] = "", result_extra[RESULT_LEN / 2] = "";

  for (register int i = 0; i < items; i++) {
/* Chanchar template
    } else if (!strcmp(item[i], "temp")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel temp needs argument");
        return ERROR;
      }
      strlcpy(chan->temp, item[i], sizeof(chan->temp));
      check_temp(chan);
 */
    if (!strcmp(item[i], "chanmode")) {
      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "channel chanmode needs argument");
	return ERROR;
      }
      strlcpy(s, item[i], 121);
      set_mode_protect(chan, s);
    } else if (!strcmp(item[i], "topic")) {
      char *p = NULL;

      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "topic needs argument");
	return ERROR;
      }
      p = replace(item[i], "{", "[");
      p = replace(p, "}", "]");
      strlcpy(chan->topic, p, 121);
    } else if (!strcmp(item[i], "addedby")) {
      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "addedby needs argument");
	return ERROR;
      }
      strlcpy(chan->added_by, item[i], NICKLEN);
    } else if (!strcmp(item[i], "addedts")) {
      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "addedts needs argument");
	return ERROR;
      }
      chan->added_ts = atoi(item[i]);
    } else if (!strcmp(item[i], "idle-kick")) {
      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "channel idle-kick needs argument");
	return ERROR;
      }
      chan->idle_kick = atoi(item[i]);
    } else if (!strcmp(item[i], "limit")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel limit needs argument");
        return ERROR;
      }
      chan->limitraise = atoi(item[i]);
      chan->limit_prot = 0;
    } else if (!strcmp(item[i], "dont-idle-kick"))
      chan->idle_kick = 0;
    else if (!strcmp(item[i], "stopnethack-mode")) {
      i++;
      if (i >= items) {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "channel stopnethack-mode needs argument");
	return ERROR;
      }
      chan->stopnethack_mode = atoi(item[i]);
/*
    } else if (!strcmp(item[i], "revenge-mode")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel revenge-mode needs argument");
        return ERROR;
      }
      chan->revenge_mode = atoi(item[i]);
*/
    } else if (!strcmp(item[i], "ban-time")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel ban-time needs argument");
        return ERROR;
      }
      chan->ban_time = atoi(item[i]);
    } else if (!strcmp(item[i], "exempt-time")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel exempt-time needs argument");
        return ERROR;
      }
      chan->exempt_time = atoi(item[i]);
    } else if (!strcmp(item[i], "invite-time")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel invite-time needs argument");
        return ERROR;
      }
      chan->invite_time = atoi(item[i]);
    } else if (!strcmp(item[i], "closed-ban")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel closed-ban needs argument");
        return ERROR;
      }
      chan->closed_ban = atoi(item[i]);
    } else if (!strcmp(item[i], "closed-invite")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel closed-invite needs argument");
        return ERROR;
      }
      chan->closed_invite = atoi(item[i]);
    } else if (!strcmp(item[i], "closed-private")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel closed-private needs argument");
        return ERROR;
      }
      chan->closed_private = atoi(item[i]);
    } else if (!strcmp(item[i], "voice-non-ident")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel voice-ident needs argument");
        return ERROR;
      }
      chan->voice_non_ident = atoi(item[i]);
    } else if (!strcmp(item[i], "bad-cookie")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel bad-cookie needs argument");
        return ERROR;
      }
      chan->bad_cookie = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "manop")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel manop needs argument");
        return ERROR;
      }
      chan->manop = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "mdop")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel mdop needs argument");
        return ERROR;
      }
      chan->mdop = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "mop")) {
      i++;
      if (i >= items) {
        if (result)
          simple_snprintf(result, RESULT_LEN, "channel mop needs argument");
        return ERROR;
      }
      chan->mop = deflag_translate(item[i]);
/* Chanint template
 *  } else if (!strcmp(item[i], "temp")) {
 *    i++;
 *    if (i >= items) {
 *      if (result)
 *        simple_snprintf(result, RESULT_LEN, "channel temp needs argument");
 *      return ERROR;
 *    }
 *    chan->temp = atoi(item[i]);
 */
    }


    else if (!strcmp(item[i], "+enforcebans"))
      chan->status |= CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], "-enforcebans"))
      chan->status &= ~CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], "+dynamicbans"))
      chan->status |= CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], "-dynamicbans"))
      chan->status &= ~CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], "-userbans"))
      chan->status |= CHAN_NOUSERBANS;
    else if (!strcmp(item[i], "+userbans"))
      chan->status &= ~CHAN_NOUSERBANS;
    else if (!strcmp(item[i], "+bitch"))
      chan->status |= CHAN_BITCH;
    else if (!strcmp(item[i], "-bitch"))
      chan->status &= ~CHAN_BITCH;
    else if (!strcmp(item[i], "+nodesynch"))
      chan->status |= CHAN_NODESYNCH;
    else if (!strcmp(item[i], "-nodesynch"))
      chan->status &= ~CHAN_NODESYNCH;
    else if (!strcmp(item[i], "+protectops"))
      chan->status |= CHAN_PROTECTOPS;
    else if (!strcmp(item[i], "-protectops"))
      chan->status &= ~CHAN_PROTECTOPS;
    else if (!strcmp(item[i], "+inactive"))
      chan->status |= CHAN_INACTIVE;
    else if (!strcmp(item[i], "-inactive"))
      chan->status&= ~CHAN_INACTIVE;
#ifdef no
    else if (!strcmp(item[i], "+revenge"))
      chan->status |= CHAN_REVENGE;
    else if (!strcmp(item[i], "-revenge"))
      chan->status &= ~CHAN_REVENGE;
    else if (!strcmp(item[i], "+revengebot"))
      chan->status |= CHAN_REVENGEBOT;
    else if (!strcmp(item[i], "-revengebot"))
      chan->status &= ~CHAN_REVENGEBOT;
#endif
    else if (!strcmp(item[i], "+secret"))
      chan->status |= CHAN_SECRET;
    else if (!strcmp(item[i], "-secret"))
      chan->status &= ~CHAN_SECRET;
    else if (!strcmp(item[i], "+cycle"))
      chan->status |= CHAN_CYCLE;
    else if (!strcmp(item[i], "-cycle"))
      chan->status &= ~CHAN_CYCLE;
    else if (!strcmp(item[i], "+dynamicexempts"))
      chan->ircnet_status|= CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-dynamicexempts"))
      chan->ircnet_status&= ~CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-userexempts"))
      chan->ircnet_status|= CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+userexempts"))
      chan->ircnet_status&= ~CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+dynamicinvites"))
      chan->ircnet_status|= CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-dynamicinvites"))
      chan->ircnet_status&= ~CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-userinvites"))
      chan->ircnet_status|= CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], "+userinvites"))
      chan->ircnet_status&= ~CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], "+closed"))
      chan->status |= CHAN_CLOSED;
    else if (!strcmp(item[i], "-closed"))
      chan->status &= ~CHAN_CLOSED;
    else if (have_take && !strcmp(item[i], "+take"))
      chan->status |= CHAN_TAKE;
    else if (have_take && !strcmp(item[i], "-take"))
      chan->status &= ~CHAN_TAKE;
    else if (!strcmp(item[i], "+voice"))
      chan->status |= CHAN_VOICE;
    else if (!strcmp(item[i], "-voice"))
      chan->status &= ~CHAN_VOICE;
    else if (!strcmp(item[i], "+autoop"))
      chan->status |= CHAN_AUTOOP;
    else if (!strcmp(item[i], "-autoop"))
      chan->status &= ~CHAN_AUTOOP;
    else if (!strcmp(item[i], "+botbitch"))
      chan->status |= CHAN_BOTBITCH;
    else if (!strcmp(item[i], "-botbitch"))
      chan->status &= ~CHAN_BOTBITCH;
/* Chanflag template
 *  else if (!strcmp(item[i], "+temp"))
 *    chan->status |= CHAN_TEMP;
 *  else if (!strcmp(item[i], "-temp"))
 *    chan->status &= ~CHAN_TEMP;
 */
    else if (!strcmp(item[i], "+fastop")) {
      chan->status |= CHAN_FASTOP;
    }
    else if (!strcmp(item[i], "-fastop"))
      chan->status &= ~(CHAN_FASTOP | CHAN_PROTECTOPS);
    else if (!strcmp(item[i], "+private")) {
      chan->status |= CHAN_PRIVATE;
    }
    else if (!strcmp(item[i], "-private"))
      chan->status &= ~CHAN_PRIVATE;

    /* ignore wasoptest, stopnethack and clearbans in chanfile, remove
       this later */
    else if (!have_take && !strcmp(item[i], "+take")) ;
    else if (!have_take && !strcmp(item[i], "-take")) ;
    else if (!strcmp(item[i], "revenge-mode")) ;
    else if (!strcmp(item[i], "+revenge")) ;
    else if (!strcmp(item[i], "-revenge")) ;
    else if (!strcmp(item[i], "+revengebot")) ;
    else if (!strcmp(item[i], "-revengebot")) ;
    else if (!strcmp(item[i], "+manop")) ;
    else if (!strcmp(item[i], "-manop")) ;
    else if (!strcmp(item[i], "+dontkickops")) ;
    else if (!strcmp(item[i], "-dontkickops")) ;
    else if (!strcmp(item[i], "+nomdop"))  ;
    else if (!strcmp(item[i], "-nomdop"))  ;
    else if (!strcmp(item[i], "+protectfriends"))  ;
    else if (!strcmp(item[i], "-protectfriends"))  ;
    else if (!strcmp(item[i], "+punish"))  ;
    else if (!strcmp(item[i], "-punish"))  ;
    else if (!strcmp(item[i], "+seen"))  ;
    else if (!strcmp(item[i], "-seen"))  ;
    else if (!strcmp(item[i], "+secret"))  ;
    else if (!strcmp(item[i], "-secret"))  ;
    else if (!strcmp(item[i], "-stopnethack"))  ;
    else if (!strcmp(item[i], "+stopnethack"))  ;
    else if (!strcmp(item[i], "-wasoptest"))  ;
    else if (!strcmp(item[i], "+wasoptest"))  ;  /* Eule 01.2000 */
    else if (!strcmp(item[i], "+clearbans"))  ;
    else if (!strcmp(item[i], "-clearbans"))  ;
    else if (!strncmp(item[i], "need-", 5))   ;
    else if (!strncmp(item[i], "flood-", 6)) {
      int *pthr = NULL;
      time_t *ptime = NULL;
      char *p = NULL;

      if (!strcmp(item[i] + 6, "*")) {
        i++;
        if (i >= items) {
          if (result)
            simple_snprintf(result, RESULT_LEN, "%s needs argument", item[i - 1]);
          return ERROR;
        }
        p = strchr(item[i], ':');

        int thr = 0;
        time_t time = 0;

        if (p) {
          *p++ = 0;

          thr = atoi(item[i]);
          time = atoi(p);

          *--p = ':';
        } else {
          thr = atoi(item[i]);
          time = 1;
        }

        chan->flood_pub_thr = thr;
        chan->flood_pub_time = time;
        chan->flood_join_thr = thr;
        chan->flood_join_time = time;
        chan->flood_ctcp_thr = thr;
        chan->flood_ctcp_time = time;
        chan->flood_kick_thr = thr;
        chan->flood_kick_time = time;
        chan->flood_deop_thr = thr;
        chan->flood_deop_time = time;
        chan->flood_nick_thr = thr;
        chan->flood_nick_time = time;
      } else if (!strcmp(item[i] + 6, "chan")) {
	pthr = &chan->flood_pub_thr;
	ptime = &chan->flood_pub_time;
      } else if (!strcmp(item[i] + 6, "join")) {
	pthr = &chan->flood_join_thr;
	ptime = &chan->flood_join_time;
      } else if (!strcmp(item[i] + 6, "ctcp")) {
	pthr = &chan->flood_ctcp_thr;
	ptime = &chan->flood_ctcp_time;
      } else if (!strcmp(item[i] + 6, "kick")) {
	pthr = &chan->flood_kick_thr;
	ptime = &chan->flood_kick_time;
      } else if (!strcmp(item[i] + 6, "deop")) {
	pthr = &chan->flood_deop_thr;
	ptime = &chan->flood_deop_time;
      } else if (!strcmp(item[i] + 6, "nick")) {
	pthr = &chan->flood_nick_thr;
	ptime = &chan->flood_nick_time;
      } else {
	if (result)
	  simple_snprintf(result, RESULT_LEN, "illegal channel flood type: %s", item[i]);
	return ERROR;
      }

      if (pthr && ptime) { //Ignore flood-*
        i++;
        if (i >= items) {
          if (result)
            simple_snprintf(result, RESULT_LEN, "%s needs argument", item[i - 1]);
          return ERROR;
        }
        p = strchr(item[i], ':');
        if (p) {
          *p++ = 0;
          *pthr = atoi(item[i]);
          *ptime = atoi(p);
          *--p = ':';
        } else {
          *pthr = atoi(item[i]);
          *ptime = 1;
        }
      }
    } else {
      if (result && item[i][0]) /* ignore "" */
        simple_snprintf(result, RESULT_LEN, "illegal channel option: ");
        simple_snprintf(result_extra, sizeof(result_extra), "%s %s", result_extra[0] ? result_extra : "", item[i]);
      error = 1;
    }
  }

  if ((chan->status ^ old_status) & CHAN_TAKE)
    chan->status |= CHAN_FASTOP;		// to avoid bots still mass opping from +take from not using cookies

  if (!conf.bot->hub) {
    if ((old_status ^ chan->status) & CHAN_INACTIVE) {
      if (!shouldjoin(chan) && (chan->status & (CHAN_ACTIVE | CHAN_PEND)))
        dprintf(DP_SERVER, "PART %s\n", chan->name);
      if (shouldjoin(chan) && !(chan->status & (CHAN_ACTIVE | CHAN_PEND | CHAN_JOINING))) {
        dprintf(DP_SERVER, "JOIN %s %s\n", (chan->name[0]) ?
  			   chan->name : chan->dname,
  			   chan->channel.key[0] ?
    			   chan->channel.key : chan->key_prot);
        chan->status |= CHAN_JOINING;
      }
    }
    if ((old_status ^ chan->status) & (CHAN_ENFORCEBANS | CHAN_BITCH | CHAN_BOTBITCH | CHAN_CLOSED | CHAN_PRIVATE)) {
      recheck_channel(chan, 1);
    /* if we -take, recheck the chan for modes and shit */
    /* also -[bot]bitch/-private might allow for auto-opping users */
    } else if ((chan->status ^ old_status) & (CHAN_TAKE | CHAN_BITCH | CHAN_BOTBITCH | CHAN_PRIVATE)) {
      recheck_channel(chan, 1);
    } else if (old_mode_pls_prot != chan->mode_pls_prot || old_mode_mns_prot != chan->mode_mns_prot) {
      recheck_channel_modes(chan);
    }
  }
  if (result && result[0] && result_extra[0])
    strcat(result, result_extra);

  if (error)
    return ERROR;
  return OK;
}

static void init_masklist(masklist *m)
{
  m->mask = (char *) my_calloc(1, 1);
  m->who = NULL;
  m->next = NULL;
}

/* Initialize out the channel record.
 */
static void init_channel(struct chanset_t *chan, bool reset)
{
  chan->channel.maxmembers = 0;
  chan->channel.mode = 0;
  chan->channel.members = 0;

  if (!reset)
    chan->channel.key = (char *) my_calloc(1, 1);

  chan->channel.ban = (masklist *) my_calloc(1, sizeof(masklist));
  init_masklist(chan->channel.ban);

  chan->channel.exempt = (masklist *) my_calloc(1, sizeof(masklist));
  init_masklist(chan->channel.exempt);

  chan->channel.invite = (masklist *) my_calloc(1, sizeof(masklist));
  init_masklist(chan->channel.invite);

  chan->channel.member = (memberlist *) my_calloc(1, sizeof(memberlist));
  chan->channel.member->nick[0] = 0;
  chan->channel.member->next = NULL;
  chan->channel.topic = NULL;
}

static void clear_masklist(masklist *m)
{
  masklist *temp = NULL;

  for (; m; m = temp) {
    temp = m->next;
    if (m->mask)
      free(m->mask);
    if (m->who)
      free(m->who);
    free(m);
  }
}

/* Clear out channel data from memory.
 */
void clear_channel(struct chanset_t *chan, bool reset)
{
  memberlist *m = NULL, *m1 = NULL;

  if (chan->channel.topic)
    free(chan->channel.topic);
  for (m = chan->channel.member; m; m = m1) {
    m1 = m->next;
    free(m);
  }

  clear_masklist(chan->channel.ban);
  chan->channel.ban = NULL;
  clear_masklist(chan->channel.exempt);
  chan->channel.exempt = NULL;
  clear_masklist(chan->channel.invite);
  chan->channel.invite = NULL;

  if (reset)
    init_channel(chan, 1);
}

/* Create new channel and parse commands.
 */
int channel_add(char *result, char *newname, char *options)
{
  if (!newname || !newname[0] || !strchr(CHANMETA, newname[0])) {
    if (result)
      simple_snprintf(result, RESULT_LEN, "invalid channel prefix");
    return ERROR;
  }

  if (strchr(newname, ',') != NULL) {
    if (result)
      simple_snprintf(result, RESULT_LEN, "invalid channel name");
    return ERROR;
  }

  const char **item = NULL;
  int items = 0;
  char buf[3001] = "";

  simple_sprintf(buf, "chanmode { %s } ", glob_chanmode);
  strcat(buf, def_chanset);
  strcat(buf, " ");
  strcat(buf, glob_chanset);
  strcat(buf, " ");
  strcat(buf, options);
  buf[strlen(buf)] = 0;

  if (SplitList(result, buf, &items, &item) != OK)
    return ERROR;

  bool join = 0;
  struct chanset_t *chan = NULL;
  int ret = OK;

  if ((chan = findchan_by_dname(newname))) {
    /* Already existing channel, maybe a reload of the channel file */
    chan->status &= ~CHAN_FLAGGED;	/* don't delete me! :) */
  } else {
    chan = (struct chanset_t *) my_calloc(1, sizeof(struct chanset_t));

    /* These are defaults, bzero already set them 0, but we set them for future reference */
    chan->limit_prot = 0;
    chan->limit = 0;
    chan->closed_ban = 0;
    chan->closed_private = 1;
    chan->closed_invite = 1;
    chan->voice_non_ident = 1;
/* Chanint template
 *  chan->temp = 0;
 */
    chan->flood_pub_thr = gfld_chan_thr;
    chan->flood_pub_time = gfld_chan_time;
    chan->flood_ctcp_thr = gfld_ctcp_thr;
    chan->flood_ctcp_time = gfld_ctcp_time;
    chan->flood_join_thr = gfld_join_thr;
    chan->flood_join_time = gfld_join_time;
    chan->flood_deop_thr = gfld_deop_thr;
    chan->flood_deop_time = gfld_deop_time;
    chan->flood_kick_thr = gfld_kick_thr;
    chan->flood_kick_time = gfld_kick_time;
    chan->flood_nick_thr = gfld_nick_thr;
    chan->flood_nick_time = gfld_nick_time;
    chan->stopnethack_mode = global_stopnethack_mode;
//    chan->revenge_mode = global_revenge_mode;
    chan->idle_kick = global_idle_kick;
    chan->limitraise = 20;
    chan->ban_time = global_ban_time;
    chan->exempt_time = global_exempt_time;
    chan->invite_time = global_invite_time;
    /* let's initialize this stuff for shits & giggles */
    chan->channel.jointime = 0;
    chan->channel.parttime = 0;
    chan->channel.fighting = 0;

    /* We _only_ put the dname (display name) in here so as not to confuse
     * any code later on. chan->name gets updated with the channel name as
     * the server knows it, when we join the channel. <cybah>
     */
    strlcpy(chan->dname, newname, 81);

    /* Initialize chan->channel info */
    init_channel(chan, 0);
    list_append((struct list_type **) &chanset, (struct list_type *) chan);
    /* Channel name is stored in xtra field for sharebot stuff */
    if (!conf.bot->hub)
      join = 1;
  }
  /* If loading is set, we're loading the userfile. Ignore errors while
   * reading userfile and just return OK. This is for compatability
   * if a user goes back to an eggdrop that no-longer supports certain
   * (channel) options.
   */
  if ((channel_modify(result, chan, items, (char **) item) != OK) && !loading)
    ret = ERROR;

  free(item);
  if (join && shouldjoin(chan))
    dprintf(DP_SERVER, "JOIN %s %s\n", chan->dname, chan->key_prot);
  return ret;
}
