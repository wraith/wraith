/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
//                        snprintf(resultBuf, RESULT_LEN, "list element in braces followed by \"%.*s\" instead of space", (int) (p2-p), p);
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
//                        sprintf(resultBuf, "list element in quotes followed by \"%.*s\" %s", (int) (p2-p), p, "instead of space");
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
    char c;
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
    int result, brace = 0;
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

    argv = (const char **) calloc(1, (unsigned) ((size * sizeof(char *)) + (l - list) + 1 + 15));	/* 15 cuz the tcl src is hard to follow */

    length = strlen(list);

    for (p = ((char *) argv) + size*sizeof(char *); *list != 0; i++) {
        const char *prevList = list;

        result = FindElement(resultBuf, list, length, &element, &list, &elSize, &brace);

        length -= (list - prevList);

        if (result != OK) {
            free(argv);
            return result;
        }

        if (*element == 0) {
            break;
        }

        if (i >= size) {
            free(argv);
            if (resultBuf)
                strlcpy(resultBuf, "internal error in SplitList", RESULT_LEN);
            return ERROR;
        }

        argv[i] = p;

        if (brace) {
            memcpy(p, element, elSize);
            p += elSize;
            *p = 0;
            p++;
        } else {
/*            CopyAndCollapse(elSize, element, p); */
            memcpy(p, element, elSize);
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
int channel_modify(char *result, struct chanset_t *chan, int items, char **item, bool cmd)
{
  bool error = 0, changed_groups = false;
  int old_status = chan->status,
      old_mode_mns_prot = chan->mode_mns_prot,
      old_mode_pls_prot = chan->mode_pls_prot;
  char s[121] = "", result_extra[RESULT_LEN / 2] = "";

  if (result)
    result[0] = 0;

  for (int i = 0; i < items; i++) {
/* Chanchar template
    } else if (!strcmp(item[i], "temp")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel temp needs argument", RESULT_LEN);
        return ERROR;
      }
      strlcpy(chan->temp, item[i], sizeof(chan->temp));
      check_temp(chan);
 */
    if (!strcmp(item[i], "chanmode")) {
      i++;
      if (i >= items) {
	if (result)
	  strlcpy(result, "channel chanmode needs argument", RESULT_LEN);
	return ERROR;
      }
      strlcpy(s, item[i], sizeof(s));
      set_mode_protect(chan, s);
    } else if (!strcmp(item[i], "groups")) {
      i++;
      if (i >= items) {
	if (result)
	  strlcpy(result, "channel groups needs argument", RESULT_LEN);
	return ERROR;
      }
      // Get string into right format
      bd::String changroups(item[i]);
      // Replace commas with spaces to be in proper format
      changroups = changroups.sub(",", " ");
      changroups.trim();
      *(chan->groups) = changroups.split(" ");
      changed_groups = true;
    } else if (!strcmp(item[i], "topic")) {
      char *p = NULL;

      i++;
      if (i >= items) {
	if (result)
	  strlcpy(result, "topic needs argument", RESULT_LEN);
	return ERROR;
      }
      p = replace(item[i], "{", "[");
      p = replace(p, "}", "]");
      strlcpy(chan->topic, p, sizeof(chan->topic));
    } else if (!cmd && !strcmp(item[i], "addedby")) {
      i++;
      if (i >= items) {
	if (result)
	  strlcpy(result, "addedby needs argument", RESULT_LEN);
	return ERROR;
      }
      strlcpy(chan->added_by, item[i], sizeof(chan->added_by));
    } else if (!cmd && !strcmp(item[i], "addedts")) {
      i++;
      if (i >= items) {
	if (result)
	  strlcpy(result, "addedts needs argument", RESULT_LEN);
	return ERROR;
      }
      chan->added_ts = atoi(item[i]);
    } else if (!strcmp(item[i], "limit")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel limit needs argument", RESULT_LEN);
        return ERROR;
      }
      int limitraise = atoi(item[i]);
      if (limitraise < 0) {
        if (result)
          strlcpy(result, "channel limit must be a positive number", RESULT_LEN);
        return ERROR;
      }
      if (chan->limitraise && limitraise == 0 && dolimit(chan)) //limitraise was disabled by the user
        add_mode(chan, '-', 'l', "");
      chan->limitraise = atoi(item[i]);
      chan->limit_prot = 0;
    } else if (!strcmp(item[i], "fish-key")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel fish-key needs argument", RESULT_LEN);
        return ERROR;
      }
      bd::String key(item[i]);
      key.trim();
      set_fish_key(chan->dname, key);
      strlcpy(chan->fish_key, key.c_str(), sizeof(chan->fish_key));
    } else if (!strcmp(item[i], "ban-time")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel ban-time needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->ban_time = atoi(item[i]);
    } else if (!strcmp(item[i], "exempt-time")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel exempt-time needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->exempt_time = atoi(item[i]);
    } else if (!strcmp(item[i], "invite-time")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel invite-time needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->invite_time = atoi(item[i]);
    } else if (!strcmp(item[i], "capslimit") || !strcmp(item[i], "caps-limit")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel caps-limit needs argument", RESULT_LEN);
        return ERROR;
      }
      int capslimit = atoi(item[i]);
      if (capslimit > 100 || capslimit < 0 || item[i][0] == '-') {
        if (result)
          strlcpy(result, "channel caps-limit out of range (0-100)", RESULT_LEN);
        return ERROR;
      }
      chan->capslimit = capslimit;
    } else if (!strcmp(item[i], "colorlimit") || !strcmp(item[i], "color-limit")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel color-limit needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->colorlimit = atoi(item[i]);
   } else if (!strcmp(item[i], "closed-ban")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel closed-ban needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->closed_ban = atoi(item[i]);
    } else if (!strcmp(item[i], "voice-moderate")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel voice-moderate needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->voice_moderate = atoi(item[i]);
      if (chan->mode_mns_prot & CHANMODER && chan->voice_moderate)
        chan->voice_moderate = 0;
    } else if (!strcmp(item[i], "closed-invite")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel closed-invite needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->closed_invite = atoi(item[i]);
      if (chan->mode_mns_prot & CHANINV && chan->closed_invite)
        chan->closed_invite = 0;
    } else if (!strcmp(item[i], "closed-private")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel closed-private needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->closed_private = atoi(item[i]);
      if (chan->mode_mns_prot & CHANPRIV && chan->closed_private) 
        chan->closed_private = 0;
    } else if (!strcmp(item[i], "voice-non-ident")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel voice-ident needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->voice_non_ident = atoi(item[i]);
    } else if (!strcmp(item[i], "bad-cookie")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel bad-cookie needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->bad_cookie = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "manop")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel manop needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->manop = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "mdop")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel mdop needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->mdop = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "mop")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel mop needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->mop = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "revenge")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel revenge needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->revenge = deflag_translate(item[i]);
    } else if (!strcmp(item[i], "knock")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel knock needs argument", RESULT_LEN);
        return ERROR;
      }
      if (!str_isdigit(item[i])) {
        if (!strcasecmp("Op",  item[i]))
          chan->knock_flags = CHAN_FLAG_OP;
        else if (!strcasecmp("Voice", item[i]))
          chan->knock_flags = CHAN_FLAG_VOICE;
        else if (!strcasecmp("User", item[i]))
          chan->knock_flags = CHAN_FLAG_USER;
        else if (!strcasecmp("None", item[i]))
          chan->knock_flags = 0;
        else {
          if (result)
            strlcpy(result, "channel knock only accepts Op|Voice|User|None", RESULT_LEN);
          return ERROR;
        }
      } else
        chan->knock_flags = atoi(item[i]);
    } else if (!strcmp(item[i], "flood-exempt")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel flood-exempt needs argument", RESULT_LEN);
        return ERROR;
      }
      if (!str_isdigit(item[i])) {
        if (!strcasecmp("Op",  item[i]))
          chan->flood_exempt_mode = CHAN_FLAG_OP;
        else if (!strcasecmp("Voice", item[i]))
          chan->flood_exempt_mode = CHAN_FLAG_VOICE;
        else if (!strcasecmp("None", item[i]))
          chan->flood_exempt_mode = 0;
        else {
          if (result)
            strlcpy(result, "channel flood-exempt only accepts Op|Voice|None", RESULT_LEN);
          return ERROR;
        }
      } else
        chan->flood_exempt_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "closed-exempt")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel closed-exempt needs argument", RESULT_LEN);
        return ERROR;
      }
      if (!str_isdigit(item[i])) {
        if (!strcasecmp("Op",  item[i]))
          chan->closed_exempt_mode = CHAN_FLAG_OP;
        else if (!strcasecmp("Voice", item[i]))
          chan->closed_exempt_mode = CHAN_FLAG_VOICE;
        else if (!strcasecmp("None", item[i]))
          chan->closed_exempt_mode = 0;
        else {
          if (result)
            strlcpy(result, "channel closed-exempt only accepts Op|Voice|None", RESULT_LEN);
          return ERROR;
        }
      } else
        chan->closed_exempt_mode = atoi(item[i]);
    } else if (!strcmp(item[i], "flood-lock-time")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel flood-lock-time needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->flood_lock_time = atoi(item[i]);
    } else if (!strcmp(item[i], "auto-delay")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel auto-delay needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->auto_delay = atoi(item[i]);
    } else if (!strcmp(item[i], "ban-type")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel ban-type needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->ban_type = atoi(item[i]);
    } else if (!strcmp(item[i], "homechan-user")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel homechan-user needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->homechan_user = homechan_user_translate(item[i]);
    } else if (!strcmp(item[i], "protect-backup")) {
      i++;
      if (i >= items) {
        if (result)
          strlcpy(result, "channel protect-backup needs argument", RESULT_LEN);
        return ERROR;
      }
      chan->protect_backup = atoi(item[i]);
    }

/* Chanint template
 *  } else if (!strcmp(item[i], "temp")) {
 *    i++;
 *    if (i >= items) {
 *      if (result)
 *        strlcpy(result, "channel temp needs argument", RESULT_LEN);
 *      return ERROR;
 *    }
 *    chan->temp = atoi(item[i]);
 */

    else if (!strcmp(item[i], "+floodban"))
      chan->status |= CHAN_FLOODBAN;
    else if (!strcmp(item[i], "-floodban"))
      chan->status &= ~CHAN_FLOODBAN;
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
    else if (!strcmp(item[i], "+inactive"))
      chan->status |= CHAN_INACTIVE;
    else if (!strcmp(item[i], "-inactive"))
      chan->status&= ~CHAN_INACTIVE;
    else if (!strcmp(item[i], "+secret"))
      chan->status |= CHAN_SECRET;
    else if (!strcmp(item[i], "-secret"))
      chan->status &= ~CHAN_SECRET;
    else if (!strcmp(item[i], "+cycle"))
      chan->status |= CHAN_CYCLE;
    else if (!strcmp(item[i], "-cycle"))
      chan->status &= ~CHAN_CYCLE;
    else if (!strcmp(item[i], "+dynamicexempts"))
      chan->status |= CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-dynamicexempts"))
      chan->status &= ~CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], "-userexempts"))
      chan->status |= CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+userexempts"))
      chan->status &= ~CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], "+dynamicinvites"))
      chan->status |= CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-dynamicinvites"))
      chan->status &= ~CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], "-userinvites"))
      chan->status |= CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], "+userinvites"))
      chan->status &= ~CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], "+closed"))
      chan->status |= CHAN_CLOSED;
    else if (!strcmp(item[i], "-closed"))
      chan->status &= ~CHAN_CLOSED;
    else if (HAVE_TAKE && !strcmp(item[i], "+take"))
      chan->status |= CHAN_TAKE;
    else if (HAVE_TAKE && !strcmp(item[i], "-take"))
      chan->status &= ~CHAN_TAKE;
    else if (!HAVE_TAKE && !strcmp(item[i], "+take"))
      chan->status |= CHAN_BITCH;
    else if (!HAVE_TAKE && !strcmp(item[i], "-take"))
      chan->status &= ~CHAN_BITCH;
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
    else if (!strcmp(item[i], "+rbl"))
      chan->status |= CHAN_RBL;
    else if (!strcmp(item[i], "-rbl"))
      chan->status &= ~CHAN_RBL;
    else if (!strcmp(item[i], "+voicebitch"))
      chan->status |= CHAN_VOICEBITCH;
    else if (!strcmp(item[i], "-voicebitch"))
      chan->status &= ~CHAN_VOICEBITCH;
    else if (!strcmp(item[i], "+protect"))
      chan->status |= CHAN_PROTECT;
    else if (!strcmp(item[i], "-protect"))
      chan->status &= ~CHAN_PROTECT;
/* Chanflag template
 *  else if (!strcmp(item[i], "+temp"))
 *    chan->status |= CHAN_TEMP;
 *  else if (!strcmp(item[i], "-temp"))
 *    chan->status &= ~CHAN_TEMP;
 */
    else if (!strcmp(item[i], "+fastop"))
      chan->status |= CHAN_FASTOP;
    else if (!strcmp(item[i], "-fastop"))
      chan->status &= ~CHAN_FASTOP;
    else if (!strcmp(item[i], "+private"))
      chan->status |= CHAN_PRIVATE;
    else if (!strcmp(item[i], "-private"))
      chan->status &= ~CHAN_PRIVATE;
    else if (!strcmp(item[i], "+backup"))
      chan->status |= CHAN_BACKUP;
    else if (!strcmp(item[i], "-backup"))
      chan->status &= ~CHAN_BACKUP;

    /* ignore wasoptest, stopnethack and clearbans in chanfile, remove
       this later */

    else if (!cmd && !HAVE_TAKE && !strcmp(item[i], "+take")) ;
    else if (!cmd && !HAVE_TAKE && !strcmp(item[i], "-take")) ;
    else if (!cmd && !strcmp(item[i], "stopnethack-mode")) ;
    else if (!cmd && !strcmp(item[i], "revenge-mode")) ;
    else if (!cmd && !strcmp(item[i], "+meankicks")) ;
    else if (!cmd && !strcmp(item[i], "-meankicks")) ;
    else if (!cmd && !strcmp(item[i], "+nomassjoin")) ;
    else if (!cmd && !strcmp(item[i], "-nomassjoin")) ;
    else if (!cmd && !strcmp(item[i], "+revenge")) ;
    else if (!cmd && !strcmp(item[i], "-revenge")) ;
    else if (!cmd && !strcmp(item[i], "+revengebot")) ;
    else if (!cmd && !strcmp(item[i], "-revengebot")) ;
    else if (!cmd && !strcmp(item[i], "+manop")) ;
    else if (!cmd && !strcmp(item[i], "-manop")) ;
    else if (!cmd && !strcmp(item[i], "+dontkickops")) ;
    else if (!cmd && !strcmp(item[i], "-dontkickops")) ;
    else if (!cmd && !strcmp(item[i], "+nomdop"))  ;
    else if (!cmd && !strcmp(item[i], "-nomdop"))  ;
    else if (!cmd && !strcmp(item[i], "+protectfriends"))  ;
    else if (!cmd && !strcmp(item[i], "-protectfriends"))  ;
    else if (!cmd && !strcmp(item[i], "+protectops"))  ;
    else if (!cmd && !strcmp(item[i], "-protectops"))  ;
    else if (!cmd && !strcmp(item[i], "+punish"))  ;
    else if (!cmd && !strcmp(item[i], "-punish"))  ;
    else if (!cmd && !strcmp(item[i], "+seen"))  ;
    else if (!cmd && !strcmp(item[i], "-seen"))  ;
    else if (!cmd && !strcmp(item[i], "+secret"))  ;
    else if (!cmd && !strcmp(item[i], "-secret"))  ;
    else if (!cmd && !strcmp(item[i], "-stopnethack"))  ;
    else if (!cmd && !strcmp(item[i], "+stopnethack"))  ;
    else if (!cmd && !strcmp(item[i], "-wasoptest"))  ;
    else if (!cmd && !strcmp(item[i], "+wasoptest"))  ;  /* Eule 01.2000 */
    else if (!cmd && !strcmp(item[i], "+clearbans"))  ;
    else if (!cmd && !strcmp(item[i], "-clearbans"))  ;
    else if (!cmd && !strncmp(item[i], "need-", 5))   ;
    else if (!strncmp(item[i], "flood-", 6)) {
      int *pthr = NULL;
      interval_t *ptime = NULL;
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
        interval_t time = 0;

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
        chan->flood_mpub_thr = thr;
        chan->flood_mpub_time = time;
        chan->flood_bytes_thr = thr;
        chan->flood_bytes_time = time;
        chan->flood_mbytes_thr = thr;
        chan->flood_mbytes_time = time;
        chan->flood_join_thr = thr;
        chan->flood_join_time = time;
        chan->flood_ctcp_thr = thr;
        chan->flood_ctcp_time = time;
        chan->flood_mctcp_thr = thr;
        chan->flood_mctcp_time = time;
        chan->flood_kick_thr = thr;
        chan->flood_kick_time = time;
        chan->flood_deop_thr = thr;
        chan->flood_deop_time = time;
        chan->flood_nick_thr = thr;
        chan->flood_nick_time = time;
        chan->flood_mjoin_thr = thr;
        chan->flood_mjoin_time = time;
      } else if (!strcmp(item[i] + 6, "chan")) {
	pthr = &chan->flood_pub_thr;
	ptime = &chan->flood_pub_time;
      } else if (!strcmp(item[i] + 6, "bytes")) {
	pthr = &chan->flood_bytes_thr;
	ptime = &chan->flood_bytes_time;
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
      } else if (!strcmp(item[i] + 6, "mjoin")) {
	pthr = &chan->flood_mjoin_thr;
	ptime = &chan->flood_mjoin_time;
      } else if (!strcmp(item[i] + 6, "mpub")) {
	pthr = &chan->flood_mpub_thr;
	ptime = &chan->flood_mpub_time;
      } else if (!strcmp(item[i] + 6, "mbytes")) {
	pthr = &chan->flood_mbytes_thr;
	ptime = &chan->flood_mbytes_time;
      } else if (!strcmp(item[i] + 6, "mctcp")) {
	pthr = &chan->flood_mctcp_thr;
	ptime = &chan->flood_mctcp_time;
      } else { /* Ignore for optimal forward compatibility */
        i++;
        continue;
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
      if (result && item[i][0]) { /* ignore "" */
        if (!result[0])
          strlcpy(result, "illegal channel option: ", RESULT_LEN);
        if (result_extra[0])
          strlcat(result_extra, " ", sizeof(result_extra));
        strlcat(result_extra, item[i], sizeof(result_extra));
      }
      error = 1;
    }
  }

  if ((chan->status ^ old_status) & CHAN_TAKE)
    chan->status |= CHAN_BITCH;		// to avoid bots still mass opping from +take from not using cookies

  if (!conf.bot->hub && (chan != chanset_default)) {
    // Check if groups changed or +/-backup set
    if (!restarting && !loading && (changed_groups || ((old_status ^ chan->status) & (CHAN_INACTIVE | CHAN_BACKUP)))) {
      check_shouldjoin(chan);
    }
    if (me_op(chan)) {
      if ((old_status ^ chan->status) & (CHAN_ENFORCEBANS|CHAN_NOUSERBANS|CHAN_DYNAMICBANS|CHAN_NOUSEREXEMPTS|CHAN_NOUSERINVITES|CHAN_DYNAMICEXEMPTS|CHAN_DYNAMICINVITES)) {
        recheck_channel(chan, 1);
        /* Does a change in channel flag affect client status in the channel? */
      } else if ((chan->status ^ old_status) & (CHAN_BITCH | CHAN_BOTBITCH | CHAN_PRIVATE | CHAN_CLOSED | CHAN_VOICE | CHAN_VOICEBITCH )) {
        recheck_channel(chan, 0);
      } else if ((old_status & CHAN_TAKE) && !(chan->status & CHAN_TAKE)) {
        //Set -take, fetch bans/exempts/invites
        recheck_channel(chan, 2);
      } else if (old_mode_pls_prot != chan->mode_pls_prot || old_mode_mns_prot != chan->mode_mns_prot) {
        recheck_channel_modes(chan);
      }
    }
  }
  if (result && result[0] && result_extra[0])
    strlcat(result, result_extra, RESULT_LEN);

  if (error)
    return ERROR;
  return OK;
}

static void init_masklist(masklist *m)
{
  m->mask = (char *) calloc(1, 1);
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
  chan->channel.splitmembers = 0;

  if (!reset)
    my_setkey(chan, NULL);

  chan->channel.ban = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.ban);

  chan->channel.exempt = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.exempt);

  chan->channel.invite = (masklist *) calloc(1, sizeof(masklist));
  init_masklist(chan->channel.invite);

  chan->channel.member = new memberlist;
  chan->channel.member->nick[0] = 0;
  chan->channel.member->next = NULL;
  chan->channel.topic = NULL;
  chan->channel.floodtime = new bd::HashTable<bd::String, bd::HashTable<flood_t, time_t> >;
  chan->channel.floodnum  = new bd::HashTable<bd::String, bd::HashTable<flood_t, int> >;
  chan->channel.cached_members = new bd::HashTable<bd::String, memberlist*>;
  chan->channel.hashed_members = new bd::HashTable<RfcString, memberlist*>;
  /* Don't clear out existing roles, keep them until rebalancing
   * to not create a window of missing roles. */
  if (!chan->bot_roles) {
    chan->bot_roles = new bd::HashTable<bd::String, int>;
    chan->role_bots = new bd::HashTable<short, bd::Array<bd::String> >;
    chan->role = 0;
  }
  chan->needs_role_rebalance = 1;
}

static void clear_masklist(masklist *m)
{
  masklist *temp = NULL;

  for (; m; m = temp) {
    temp = m->next;
    free(m->mask);
    free(m->who);
    free(m);
  }
}

/* Clear out channel data from memory.
 */
void clear_channel(struct chanset_t *chan, bool reset)
{
  memberlist *m = NULL, *m1 = NULL;

  free(chan->channel.topic);
  for (m = chan->channel.member; m; m = m1) {
    m1 = m->next;
    delete_member(m);
  }

  clear_masklist(chan->channel.ban);
  chan->channel.ban = NULL;
  clear_masklist(chan->channel.exempt);
  chan->channel.exempt = NULL;
  clear_masklist(chan->channel.invite);
  chan->channel.invite = NULL;

  chan->channel.last_eI = 0;

  chan->ircnet_status = 0;
//  chan->ircnet_status &= ~CHAN_HAVEBANS;

  delete chan->channel.floodtime;
  chan->channel.floodtime = NULL;
  delete chan->channel.floodnum;
  chan->channel.floodnum = NULL;
  /* Don't clear out existing roles if resetting, keep them until rebalancing
   * to not create a window of missing roles. */
  if (!reset) {
    delete chan->bot_roles;
    chan->bot_roles = NULL;
    delete chan->role_bots;
    chan->role_bots = NULL;
  }

  if (chan->channel.cached_members) {
    if (chan->channel.cached_members->size()) {
      for (const auto& uhost : chan->channel.cached_members->keys()) {
        // Delete the cached member
        m = (*chan->channel.cached_members)[uhost];
        delete_member(m);
      }
    }
    delete chan->channel.cached_members;
    chan->channel.cached_members = NULL;
  }
  delete chan->channel.hashed_members;
  chan->channel.hashed_members = NULL;

  if (reset)
    init_channel(chan, 1);
  for (size_t i = 0; i < MODES_PER_LINE_MAX; ++i) {
    if (chan->cmode[i].op) {
      free(chan->cmode[i].op);
      chan->cmode[i].op = NULL;
    }
    if (chan->ccmode[i].op) {
      free(chan->ccmode[i].op);
      chan->ccmode[i].op = NULL;
    }
  }

}

/* Create new channel and parse commands.
 */
int channel_add(char *result, const char *newname, char *options, bool isdefault)
{
  /* When loading userfile */
  if (newname && newname[0] && loading && !strcmp(newname, "default"))
    isdefault = 1;

  if (!newname || !newname[0] || (!isdefault && !strchr(CHANMETA, newname[0]))) {
    if (result)
      strlcpy(result, "invalid channel prefix", RESULT_LEN);
    return ERROR;
  }

  if (strchr(newname, ',') != NULL) {
    if (result)
      strlcpy(result, "invalid channel name", RESULT_LEN);
    return ERROR;
  }

  const char **item = NULL;
  int items = 0;
  char buf[3001] = "";

  simple_snprintf(buf, sizeof(buf), "chanmode { %s }", glob_chanmode);
  if (!isdefault) { // These are passed in in 'options'
    strlcat(buf, " ", sizeof(buf));
    strlcat(buf, def_chanset, sizeof(buf));
  }
  // Add in 'default' channel options
  if (!isdefault && chanset_default) {
    bd::String default_chan_options = channel_to_string(chanset_default);
    strlcat(buf, " ", sizeof(buf));
    strlcat(buf, default_chan_options.c_str(), sizeof(buf));
  }
  if (options && options[0]) {
    strlcat(buf, " ", sizeof(buf));
    strlcat(buf, options, sizeof(buf));
  }

  if (SplitList(result, buf, &items, &item) != OK)
    return ERROR;

  bool join = 0;
  struct chanset_t *chan = NULL;
  int ret = OK;

  if ((chan = findchan_by_dname(newname))) {
    /* Already existing channel, maybe a reload of the channel file */
    chan->status &= ~CHAN_FLAGGED;	/* don't delete me! :) */
  } else {
    chan = (struct chanset_t *) calloc(1, sizeof(struct chanset_t));

    /* These are defaults, bzero already set them 0, but we set them for future reference */
    chan->limit_prot = 0;
    chan->limit = 0;
    chan->closed_ban = 0;
    chan->closed_private = 1;
    chan->closed_invite = 1;
    chan->voice_moderate = 1;
    chan->voice_non_ident = 1;
    chan->auto_delay = 5;
    chan->ban_type = 3;
    chan->revenge = DEFLAG_REACT;
/* Chanint template
 *  chan->temp = 0;
 */
    chan->groups = new bd::Array<bd::String>;
    *(chan->groups) << "main";
    chan->protect_backup = 1;
    chan->fish_key[0] = 0;
    chan->knock_flags = 0;
    chan->flood_lock_time = 120;
    chan->flood_exempt_mode = 0;
    chan->flood_pub_thr = 0;
    chan->flood_pub_time = 0;
    chan->flood_bytes_thr = 0;
    chan->flood_bytes_time = 0;
    chan->flood_ctcp_thr = 5;
    chan->flood_ctcp_time = 30;
    chan->flood_join_thr = 0;
    chan->flood_join_time = 0;
    chan->flood_deop_thr = 8;
    chan->flood_deop_time = 10;
    chan->flood_kick_thr = 0;
    chan->flood_kick_time = 0;
    chan->flood_nick_thr = 0;
    chan->flood_nick_time = 0;
    chan->flood_mjoin_thr = 6;
    chan->flood_mjoin_time = 1;
    chan->capslimit = 0;
    chan->colorlimit = 0;
    chan->flood_mpub_thr = 20;
    chan->flood_mpub_time = 1;
    chan->flood_mbytes_thr = 1000;
    chan->flood_mbytes_time = 1;
    chan->flood_mctcp_thr = 7;
    chan->flood_mctcp_time = 1;
    chan->limitraise = 20;
    chan->ban_time = global_ban_time;
    chan->exempt_time = global_exempt_time;
    chan->invite_time = global_invite_time;

    /* We _only_ put the dname (display name) in here so as not to confuse
     * any code later on. chan->name gets updated with the channel name as
     * the server knows it, when we join the channel. <cybah>
     */
    strlcpy(chan->dname, newname, sizeof(chan->dname));
    chanset_by_dname[chan->dname] = chan;

    /* Initialize chan->channel info */
    if (isdefault) {
      if (chanset_default)
        remove_channel(chanset_default);
      chanset_default = chan;
    } else {
      init_channel(chan, 0);
      list_append((struct list_type **) &chanset, (struct list_type *) chan);
      /* Channel name is stored in xtra field for sharebot stuff */
      if (!conf.bot->hub && !isdefault)
        join = 1;
    }
  }
  /* If loading is set, we're loading the userfile. Ignore errors while
   * reading userfile and just return OK. This is for compatability
   * if a user goes back to an eggdrop that no-longer supports certain
   * (channel) options.
   */
  if ((channel_modify(result, chan, items, (char **) item, 0) != OK)) {
    putlog(LOG_ERROR, "*", "Error parsing channel options for %s: %s", chan->dname, result);
    if (!loading)
      ret = ERROR;
  }
    

  free(item);
  if (join && shouldjoin(chan))
    join_chan(chan);
  return ret;
}
/* vim: set sts=2 sw=2 ts=8 et: */
