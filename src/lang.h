/*
 * lang.h
 *   Conversion definitions for language support
 *
 */
#define STR(x) x

#ifndef _EGG_LANG_H
#define _EGG_LANG_H


#define DETEST			STR("WORKS")

#define MISC_USAGE		STR("Usage")
#define MISC_FAILED		STR("Failed.\n")

/* Userfile messages
 */
#define USERF_XFERDONE		STR("Userlist transfer complete; switched over")
/* was: USERF_BADREREAD - 0x401 */
#define USERF_CANTREAD		STR("CAN'T READ NEW USERFILE")
#define USERF_CANTSEND		STR("Can't send userfile to you (internal error)")
#define USERF_NOMATCH		STR("Can't find anyone matching that")
#define USERF_INVALID		STR("Invalid userfile format.")
#define USERF_CORRUPT		STR("Corrupt user record")
#define USERF_DUPE		STR("Duplicate user record")
#define USERF_BROKEPASS		STR("Corrupted password reset for")
#define USERF_IGNBANS		STR("Ignored masks for channel(s):")
#define USERF_WRITING		STR("Writing user file...")
#define USERF_ERRWRITE		STR("ERROR writing user file.")
#define USERF_ERRWRITE2		STR("ERROR writing user file to transfer.")
#define USERF_NONEEDNEW		STR("Userfile creation not necessary--skipping")
#define USERF_REHASHING		STR("Rehashing...")
#define USERF_UNKNOWN		STR("I don't know anyone by that name.\n")
#define USERF_NOUSERREC		STR("No user record.")
#define USERF_BACKUP		STR("Backing up user file...")
#define USERF_FAILEDXFER	STR("Failed connection; aborted userfile transfer.")
#define USERF_OLDSHARE		STR("Old style share request by")
#define USERF_ANTIQUESHARE	STR("Antiquated sharing request")
#define USERF_REJECTED		STR("User file rejected by")

/* Misc messages
 */

#define USERF_OLDFMT		STR("boring....")
#define MISC_EXPIRED		STR("expired")
#define MISC_TOTAL		STR("total")
#define MISC_ERASED		STR("Erased")
/* was: MISC_LEFT - 0x503 */
#define MISC_ONLOCALE		STR("on")
#define MISC_MATCHING		STR("Matching")
#define MISC_SKIPPING		STR("skipping first")
#define MISC_TRUNCATED		STR("(more than %d matches; list truncated)\n")
#define MISC_FOUNDMATCH		STR("--- Found %d match%s.\n")
#define MISC_AMBIGUOUS		STR("Ambiguous command.\n")
#define MISC_NOSUCHCMD		STR("What?  You need 'help'\n")
#define MISC_CMDBINDS		STR("Command bindings:\n")
#define MISC_RESTARTING		STR("Restarting...")
#define MISC_MATCH_PLURAL	STR("es")
#define MISC_LOGSWITCH		STR("Switching logfiles...")
#define MISC_OWNER		STR("owner")
#define MISC_MASTER		STR("master")
#define MISC_OP			STR("op")
#define MISC_IDLE		STR("idle")
#define MISC_AWAY		STR("AWAY")
/* was: MISC_IGNORING - 0x514 */
/* was: MISC_UNLINKED - 0x515 */
#define MISC_DISCONNECTED	STR("Disconnected")
#define MISC_INVALIDBOT		STR("invalid bot")
#define MISC_LOOP		STR("Detected loop: two bots exist named")
/* was: MISC_MUTUAL - 0x519 */
#define MISC_FROM		STR("from")
#define MISC_OUTDATED		STR("outdated")
#define MISC_REJECTED		STR("rejected")
#define MISC_IMPOSTER		STR("imposter")
#define MISC_TRYING		STR("trying")
#define MISC_MOTDFILE		STR("MOTD file:")
#define MISC_NOMOTDFILE		STR("No MOTD file.")
#define MISC_USEFORMAT		STR("Use:")
#define MISC_CHADDRFORMAT	STR("<address>:<port#>[/<relay-port#>]")
/* was: MISC_UNKNOWN - 0x523 */
/* was: MISC_CHANNELS - 0x524 */
/* was: MISC_TRYINGMISTAKE - 0x525 */
#define MISC_PENDING		STR("pending")
#define MISC_WANTOPS		STR("want ops!")
#define MISC_LURKING		STR("lurking")
#define MISC_BACKGROUND		STR("background")
#define MISC_TERMMODE		STR("terminal mode")
#define MISC_STATMODE		STR("status mode")
#define MISC_LOGMODE		STR("log dump mode")
#define MISC_ONLINEFOR		STR("Online for")
#define MISC_CACHEHIT		STR("cache hit")
#define MISC_TCLLIBRARY		STR("Tcl library:")
#define MISC_NEWUSERFLAGS	STR("New users get flags")
#define MISC_NOTIFY		STR("notify")
#define MISC_PERMOWNER		STR("Permanent owner(s)")
/* was: MISC_ROOTWARN - 0x533 */
#define MISC_NOUSERFILE		STR("USER FILE NOT FOUND!  (try './eggdrop -m %s' to make one)\n")
#define MISC_NOUSERFILE2	STR("STARTING BOT IN USERFILE CREATION MODE.\nTelnet to the bot and enter 'NEW' as your nickname.")
#define MISC_USERFCREATE1	STR("OR go to IRC and type:  /msg %s hello\n")
#define MISC_USERFCREATE2	STR("This will make the bot recognize you as the master.")
#define MISC_USERFEXISTS	STR("USERFILE ALREADY EXISTS (drop the '-m')")
#define MISC_CANTWRITETEMP	STR("CAN'T WRITE TO TEMP DIR")
#define MISC_CANTRELOADUSER	STR("Can't reload user file!")
#define MISC_MISSINGUSERF	STR("User file is missing!")
/* was: MISC_BOTSCONNECTED - 0x53d */
#define MISC_BANNER		STR("%B  (%E)\n\nPlease enter your nickname.\n")
#define MISC_CLOGS		STR("Cycling logfile %s, over max-logsize (%d)")
#define MISC_BANNER_STEALTH	STR("\nNickname.\n")
#define MISC_LOGREPEAT		STR("Last message repeated %d time(s).\n")
#define MISC_JUPED		STR("juped")
#define MISC_NOFREESOCK		STR("No free sockets available.")
#define MISC_TCLVERSION		STR("Tcl version:")
#define MISC_TCLHVERSION	STR("header version")

/* IRC */
#define IRC_BANNED		STR("Banned")
#define IRC_YOUREBANNED		STR("You are banned")
/* BOT log messages when attempting to place a ban which matches me */
#define IRC_IBANNEDME		STR("Wanted to ban myself--deflected.")
#define IRC_FUNKICK		STR("that was fun, let's do it again!")
#define IRC_HI			STR("Hi")
#define IRC_GOODBYE		STR("Goodbye")
#define IRC_BANNED2		STR("You're banned, goober.")
#define IRC_NICKTOOLONG		STR("NOTICE %s :Your nick was too long and therefore it was truncated to '%s'.\n")
#define IRC_INTRODUCED		STR("Introduced to")
#define IRC_COMMONSITE		STR("common site")
#define IRC_SALUT1		STR("NOTICE %s :Hi %s!  I'm %s, an eggdrop bot.\n")
#define IRC_SALUT2		STR("NOTICE %s :I'll recognize you by hostmask '%s' from now on.\n")
#define IRC_SALUT2A		STR("Since you come from a common irc site, this means you should")
#define IRC_SALUT2B		STR("always use this nickname when talking to me.")
#define IRC_INITOWNER1		STR("YOU ARE THE OWNER ON THIS BOT NOW")
#define IRC_INIT1		STR("Bot installation complete, first master is %s")
#define IRC_INITNOTE		STR("Welcome to Eggdrop! =]")
#define IRC_INITINTRO		STR("introduced to %s from %s")
#define IRC_PASS		STR("You have a password set.")
#define IRC_NOPASS		STR("You don't have a password set.")
/* was: IRC_NOPASS2 - 0x614 */
#define IRC_EXISTPASS		STR("You already have a password set.")
#define IRC_PASSFORMAT		STR("Please use at least 6 characters.")
#define IRC_SETPASS		STR("Password set to:")
#define IRC_FAILPASS		STR("Incorrect password.")
#define IRC_CHANGEPASS		STR("Password changed to:")
#define IRC_FAILCOMMON		STR("You're at a common site; you can't IDENT.")
#define IRC_MISIDENT		STR("NOTICE %s :You're not %s, you're %s.\n")
#define IRC_DENYACCESS		STR("Access denied.")
#define IRC_RECOGNIZED		STR("I recognize you there.")
#define IRC_ADDHOSTMASK		STR("Added hostmask")
/* was: IRC_DELMAILADDR - 0x61f */
#define IRC_FIELDCURRENT	STR("Currently:")
#define IRC_FIELDCHANGED	STR("Now:")
#define IRC_FIELDTOREMOVE	STR("To remove it:")
/* was: IRC_NOEMAIL - 0x623 */
#define IRC_INFOLOCKED		STR("Your info line is locked")
#define IRC_REMINFOON		STR("Removed your info line on")
#define IRC_REMINFO		STR("Removed your info line.")
#define IRC_NOINFOON		STR("You have no info set on")
#define IRC_NOINFO		STR("You have no info set.")
#define IRC_NOMONITOR		STR("I don't monitor that channel.")
#define IRC_RESETCHAN		STR("Resetting channel info.")
#define IRC_JUMP		STR("Jumping servers...")
#define IRC_CHANHIDDEN		STR("Channel is currently hidden.")
#define IRC_ONCHANNOW		STR("Now on channel")
#define IRC_NEVERJOINED		STR("Never joined one of my channels.")
#define IRC_LASTSEENAT		STR("Last seen at")
#define IRC_DONTKNOWYOU		STR("I don't know you; please introduce yourself first.")
#define IRC_NOHELP		STR("No help.")
#define IRC_NOHELP2		STR("No help available on that.")
#define IRC_NOTONCHAN		STR("Not on that channel right now.")
#define IRC_GETORIGNICK		STR("Switching back to nick %s")
#define IRC_BADBOTNICK		STR("Server says my nickname is invalid.")
#define IRC_BOTNICKINUSE	STR("NICK IN USE: Trying '%s'")
#define IRC_BOTNICKJUPED	STR("Nickname has been juped")
#define IRC_CHANNELJUPED	STR("Channel %s is juped. :(")
#define IRC_NOTREGISTERED1	STR("%s says I'm not registered, trying next one.")
#define	IRC_NOTREGISTERED2	STR("The server says we are not registered yet..")
#define IRC_FLOODIGNORE1	STR("Flood from @%s!  Placing on ignore!")
/* was: IRC_FLOODIGNORE2 - 0x63e */
#define IRC_FLOODIGNORE3	STR("JOIN flood from @%s!  Banning.")
#define IRC_FLOODKICK		STR("Channel flood from %s -- kicking")
#define IRC_SERVERTRY		STR("Trying server")
#define IRC_DNSFAILED		STR("DNS lookup failed")
#define IRC_FAILEDCONNECT	STR("Failed connect to")
#define IRC_SERVERSTONED	STR("Server got stoned; jumping...")
#define IRC_DISCONNECTED	STR("Disconnected from")
#define IRC_NOSERVER		STR("No server currently.")
#define IRC_MODEQUEUE		STR("Mode queue is at")
#define IRC_SERVERQUEUE		STR("Server queue is at")
#define IRC_HELPQUEUE		STR("Help queue is at")
/* was: IRC_BOTNOTONIRC - 0x64a */
#define IRC_NOTACTIVECHAN	STR("Not active on channel")
#define IRC_PROCESSINGCHAN	STR("Processing channel")
#define IRC_CHANNEL		STR("Channel")
#define IRC_DESIRINGCHAN	STR("Desiring channel")
#define IRC_CHANNELTOPIC	STR("Channel Topic")
#define IRC_PENDINGOP		STR("pending +o -- I'm lagged")
#define IRC_PENDINGDEOP		STR("pending -o -- I'm lagged")
#define IRC_PENDINGKICK		STR("pending kick")
#define IRC_FAKECHANOP		STR("FAKE CHANOP GIVEN BY SERVER")
#define IRC_ENDCHANINFO		STR("End of channel info.")
#define IRC_MASSKICK		STR("mass kick, go sit in a corner")
#define IRC_REMOVEDBAN		STR("Removed ban")
#define IRC_UNEXPECTEDMODE	STR("Hmm, mode info from a channel I'm not on")
#define IRC_POLITEKICK		STR("...and thank you for playing.")
#define IRC_AUTOJUMP		STR("Jumping servers (need %d servers, only have %d)")
#define IRC_CHANGINGSERV	STR("changing servers")
#define IRC_TOOMANYCHANS	STR("I'm on too many channels--can't join: %s")
#define IRC_CHANFULL		STR("Channel full--can't join: %s")
#define IRC_CHANINVITEONLY	STR("Channel invite only--can't join: %s")
#define IRC_BANNEDFROMCHAN	STR("Banned from channel--can't join: %s")
#define IRC_SERVNOTONCHAN	STR("Server says I'm not on channel: %s")
#define IRC_BADCHANKEY		STR("Bad key--can't join: %s")
#define IRC_TELNETFLOOD		STR("Telnet connection flood from %s!  Placing on ignore!")
#define IRC_PREBANNED		STR("banned:")
#define IRC_JOIN_FLOOD		STR("join flood")
#define IRC_KICK_PROTECT	STR("don't kick my friends, bud")
#define IRC_DEOP_PROTECT	STR("don't deop my friends, bud")
#define IRC_COMMENTKICK		STR("...and don't come back.")
#define IRC_REMOVEDEXEMPT	STR("Removed exempt")
#define IRC_REMOVEDINVITE	STR("Removed invite")
#define IRC_FLOODIGNORE4	STR("NICK flood from @%s!  Banning.")
#define IRC_NICK_FLOOD		STR("nick flood")

/* Eggdrop command line usage
 */
#define EGG_RUNNING1		STR("I detect %s already running from this directory.\n")
#define EGG_RUNNING2		STR("If this is incorrect, erase the '%s'\n")
#define EGG_NOWRITE		STR("* Warning!  Could not write %s file!\n")

#define USER_ISGLOBALOP		STR("(is a global op)")
#define USER_ISBOT		STR("(is a bot)")
#define USER_ISMASTER		STR("(is a master)")

/* '.bans/.invites/.exempts' common messages
 */
#define MODES_CREATED		STR("Created")
#define MODES_LASTUSED		STR("last used")
#define MODES_INACTIVE		STR("inactive")
#define MODES_PLACEDBY		STR("placed by")
#define MODES_NOTACTIVE		STR("not active on")
#define MODES_NOTACTIVE2	STR("not active")
#define MODES_NOTBYBOT		STR("not placed by bot")

/* Messages used when listing with `.bans'
 */
#define BANS_GLOBAL		STR("Global bans")
#define BANS_BYCHANNEL		STR("Channel bans for")
#define BANS_USEBANSALL		STR("Use 'bans all' to see the total list")
#define BANS_NOLONGER		STR("No longer banning")

/* Messages used when listing with '.exempts'
 */
#define EXEMPTS_GLOBAL		STR("Global exempts")
#define EXEMPTS_BYCHANNEL	STR("Channel exempts for")
#define EXEMPTS_USEEXEMPTSALL	STR("Use 'exempts all' to see the total list")
#define EXEMPTS_NOLONGER	STR("No longer ban exempting")

/* Messages used when listing with '.invites'
 */
#define INVITES_GLOBAL		STR("Global invites")
#define INVITES_BYCHANNEL	STR("Channel invites for")
#define INVITES_USEINVITESALL	STR("Use 'invites all' to see the total list")
#define INVITES_NOLONGER	STR("No longer inviting")


/* Messages referring to channels
 */
#define CHAN_NOSUCH		STR("No such channel defined")
#define CHAN_BADCHANMODE	STR("* Mode change on %s for nonexistant %s!")
#define CHAN_MASSDEOP		STR("Mass deop on %s by %s")
#define CHAN_MASSDEOP_KICK	STR("Mass deop.  Go sit in a corner.")
#define CHAN_FORCEJOIN		STR("Oops.   Someone made me join %s... leaving...")
#define CHAN_FAKEMODE		STR("Mode change by fake op on %s!  Reversing...")
#define CHAN_FAKEMODE_KICK	STR("Abusing ill-gained server ops")
#define CHAN_DESYNCMODE		STR("Mode change by non-chanop on %s!  Reversing...")
#define CHAN_DESYNCMODE_KICK	STR("Abusing desync")
#define CHAN_FLOOD		STR("flood")

/* Messages referring to ignores
 */
#define IGN_NONE		STR("No ignores")
#define IGN_CURRENT		STR("Currently ignorin")
#define IGN_NOLONGER		STR("No longer ignoring")

/* Messages referring to bots
 */
#define BOT_NOTHERE		STR("That bot isn't here.\n")
#define BOT_NONOTES		STR("That's a bot.  You can't leave notes for a bot.\n")
#define BOT_USERAWAY		STR("is away")
#define BOT_NOTEARRIVED		STR("Note arrived for you")
#define BOT_MSGDIE		STR("Bot shut down beginning....")
#define BOT_NOSUCHUSER		STR("No such user")
#define BOT_NOCHANNELS		STR("no channels")
#define BOT_PARTYMEMBS		STR("Party line members:")
#define BOT_BOTSCONNECTED	STR("Bots connected")
#define BOT_OTHERPEOPLE		STR("Other people on the bot")
/* was: BOT_OUTDATEDWHOM - 0xb1e */
#define BOT_LINKATTEMPT		STR("Attempting to link")
#define BOT_NOTESTORED2		STR("Not online; note stored.")
#define BOT_NOTEBOXFULL		STR("Notebox is full, sorry.")
#define BOT_NOTEISAWAY		STR("is away; note stored.")
#define BOT_NOTESENTTO		STR("Note sent to")
#define BOT_DISCONNECTED	STR("Disconnected from:")
#define BOT_PEOPLEONCHAN	STR("People on channel")
#define BOT_CANTLINKTHERE	STR("Can't link there")
#define BOT_CANTUNLINK		STR("Can't unlink")
#define BOT_LOOPDETECT		STR("Loop detected")
#define BOT_BOGUSLINK		STR("Bogus link notice from")
/* was: BOT_BOGUSLINK2 - 0xb2a */
#define BOT_DISCONNLEAF		STR("Disconnected left")
#define BOT_LINKEDTO		STR("Linked to")
#define BOT_ILLEGALLINK		STR("Illegal link by leaf")
#define BOT_YOUREALEAF		STR("You are supposed to be a leaf!")
#define BOT_REJECTING		STR("Rejecting bot")
#define BOT_OLDBOT		STR("Older bot detected (unsupported)")
#define BOT_TRACERESULT		STR("Trace result")
#define BOT_DOESNTEXIST		STR("doesn't exist")
#define BOT_NOREMOTEBOOT	STR("Remote boots are not allowed.")
#define BOT_NOOWNERBOOT		STR("Can't boot the bot owner.")
#define BOT_XFERREJECTED	STR("FILE TRANSFER REJECTED")
/* was: BOT_NOFILESYS - 0xb36 */
#define BOT_BOTNETUSERS		STR("Users across the botnet")
#define BOT_PARTYLINE		STR("Party line")
#define BOT_LOCALCHAN		STR("Local channel")
#define BOT_USERSONCHAN		STR("Users on channel")
#define BOT_NOBOTSLINKED	STR("No bots linked.")
#define BOT_NOTRACEINFO		STR("No trace info for:")
#define BOT_COMPLEXTREE		STR("Tree too complex!")
#define BOT_UNLINKALL		STR("Unlinking all bots...")
#define BOT_KILLLINKATTEMPT	STR("Killed link attempt to")
#define BOT_ENDLINKATTEMPT	STR("No longer trying to link:")
#define BOT_BREAKLINK		STR("Breaking link with")
#define BOT_UNLINKEDFROM	STR("Unlinked from:")
#define BOT_NOTCONNECTED	STR("Not connected to that bot.")
#define BOT_WIPEBOTTABLE	STR("Smooshing bot tables and assocs...")
#define BOT_BOTUNKNOWN		STR("is not a known bot.")
#define BOT_CANTLINKMYSELF	STR("Link to myself?  Oh boy, Freud would have a field day.")
#define BOT_ALREADYLINKED	STR("That bot is already connected up.")
#define BOT_NOTELNETADDY	STR("Invalid telnet address:port stored for")
#define BOT_LINKING		STR("Linking to")
#define BOT_CANTFINDRELAYUSER	STR("Can't find user for relay!")
#define BOT_CANTLINKTO		STR("Could not link to")
#define BOT_CANTRELAYMYSELF	STR("Relay to myself?  What on EARTH would be the point?!")
#define BOT_CONNECTINGTO	STR("Connecting to")
#define BOT_BYEINFO1		STR("(Type *BYE* on a line by itself to abort.)")
#define BOT_ABORTRELAY1		STR("Aborting relay attempt to")
#define BOT_ABORTRELAY2		STR("You are now back on")
#define BOT_ABORTRELAY3		STR("Relay aborted:")
/* was: BOT_PARTYJOINED - 0xb52 */
#define BOT_LOSTDCCUSER		STR("Lost dcc connection to")
#define BOT_DROPPINGRELAY	STR("Dropping relay attempt to")
#define BOT_RELAYSUCCESS	STR("Success!\n\nNOW CONNECTED TO RELAY BOT")
#define BOT_BYEINFO2		STR("(You can type *BYE* to prematurely close the connection.)")
#define BOT_RELAYLINK		STR("Relay link:")
#define BOT_PARTYLEFT		STR("left the party line.")
#define BOT_ENDRELAY1		STR("Ended relay link")
#define BOT_ENDRELAY2		STR("RELAY CONNECTION DROPPED.\nYou are now back on")
#define BOT_PARTYREJOINED	STR("rejoined the party line.")
#define BOT_DROPPEDRELAY	STR("Dropping relay link to")
#define BOT_BREAKRELAY		STR("Breaking connection to")
#define BOT_RELAYBROKEN		STR("Relay broken")
#define BOT_PINGTIMEOUT		STR("Ping timeout")
#define BOT_BOTNOTLEAFLIKE	STR("unleaflike behavior")
#define BOT_BOTDROPPED		STR("Dropped bot")
#define BOT_ALREADYLINKING	STR("Already linking to that bot.")

/* Messages pertaining to MODULES
 */
#define MOD_ALREADYLOAD		STR("Already loaded.")
#define MOD_BADCWD		STR("Can't determine current directory")
#define MOD_NOSTARTDEF		STR("No start function defined")
#define MOD_NEEDED		STR("Needed by another module")
#define MOD_NOCLOSEDEF		STR("No close function")
#define MOD_UNLOADED		STR("Module unloaded:")
#define MOD_NOSUCH		STR("No such module")
/* was: MOD_NOINFO - 0x208 */
#define MOD_LOADERROR		STR("Error loading module:")
#define MOD_UNLOADERROR		STR("Error unloading module:")
#define MOD_CANTLOADMOD		STR("Can't load modules")
#define MOD_STAGNANT		STR("Stagnant module; there WILL be memory leaks!")
#define MOD_NOCRYPT		STR("You have installed modules but have not selected an encryption\nmodule, please consult the default config file for info.\n")
#define MOD_NOFILESYSMOD	STR("Filesys module not loaded.")
#define MOD_LOADED_WITH_LANG	STR("Module loaded: %-16s (with lang support)")
#define MOD_LOADED		STR("Module loaded: %-16s")


#define DCC_NOSTRANGERS		STR("I don't accept DCC chats from strangers.")
#define DCC_REFUSED		STR("Refused DCC chat (no access)")
#define DCC_REFUSEDNC		STR("Refused DCC chat (I'm not a chathub (+c))")
#define DCC_REFUSED2		STR("No access")
#define DCC_REFUSED3		STR("You must have a password set.")
#define DCC_REFUSED4		STR("Refused DCC chat (no password)")
#define DCC_REFUSED5		STR("Refused DCC chat (+x but no file area)")
/* was: DCC_REFUSED6 - 0xc06 */
#define DCC_REFUSED7		STR("Refused DCC chat (invalid port)")
#define DCC_TOOMANY		STR("Too many people are in the file area right now.")
/* was: DCC_TRYLATER - 0xc08 */
/* was: DCC_REFUSEDTAND - 0xc09 */
/* was: DCC_NOSTRANGERFILES1 - 0xc0a */
/* was: DCC_NOSTRANGERFILES2 - 0xc0b */
#define DCC_TOOMANYDCCS1	STR("Sorry, too many DCC connections.")
#define DCC_TOOMANYDCCS2	STR("DCC connections full: %s %s (%s!%s)")
/* was: DCC_DCCNOTSUPPORTED - 0xc0e */
/* was: DCC_REFUSEDNODCC - 0xc0f */
/* was: DCC_FILENAMEBADSLASH - 0xc10 */
/* was: DCC_MISSINGFILESIZE - 0xc11 */
/* was: DCC_FILEEXISTS - 0xc12 */
/* was: DCC_CREATEERROR - 0xc13 */
/* was: DCC_FILEBEINGSENT - 0xc14 */
/* was: DCC_REFUSEDNODCC2 - 0xc15 */
/* was: DCC_REFUSEDNODCC3 - 0xc16 */
/* was: DCC_FILETOOLARGE - 0xc17 */
/* was: DCC_FILETOOLARGE2 - 0xc18 */
#define DCC_CONNECTFAILED1	STR("Failed to connect")
#define DCC_CONNECTFAILED2	STR("DCC connection failed")
#define DCC_CONNECTFAILED3	STR("DCC invalid port")
/* was: DCC_FILESYSBROKEN - 0xc1b */
#define DCC_ENTERPASS		STR("Enter your password")
#define DCC_FLOODBOOT		STR("%s has been forcibly removed for flooding.\n")
#define DCC_BOOTED1		STR("-=- poof -=-\n")
#define DCC_BOOTED2		STR("You've been booted from the %s by %s%s%s\n")
#define DCC_BOOTED3		STR("%s booted %s from the party line%s%s")

/* Stuff from chan.c
 */
/* was: CHAN_LIMBOBOT - 0xd00 */

/* BOTNET messages
 */
#define NET_FAKEREJECT		STR("Fake message rejected")
#define NET_LINKEDTO		STR("Linked to")
#define NET_WRONGBOT		STR("Wrong bot--wanted %s, got %s")
#define NET_LEFTTHE		STR("has left the")
#define NET_JOINEDTHE		STR("has joined the")
#define NET_AWAY		STR("is now away")
#define NET_UNAWAY		STR("is no longer away")
#define NET_NICKCHANGE		STR("Nick Change:")

/* Stuff from dcc.c
 */
#define DCC_REJECT		STR("Rejecting link from %s")
#define DCC_LINKED		STR("Linked to %s.")
#define DCC_LINKFAIL		STR("Failed link to %s.")
#define DCC_BADPASS		STR("Bad password on connect attempt to %s.")
#define DCC_PASSREQ		STR("Password required for connection to %s.")
#define DCC_LINKERROR		STR("ERROR linking %s: %s")
#define DCC_LOSTBOT		STR("Lost Bot: %s")
#define DCC_TIMEOUT		STR("Timeout: bot link to %s at %s:%d")
#define DCC_LOGGEDIN		STR("Logged in: %s (%s/%d)")
#define DCC_BADLOGIN		STR("Bad Password: [%s]%s/%d")
#define DCC_BADAUTH		STR("Bad Auth: [%s]%s/%d")
#define DCC_HOUSTON		STR("Negative on that, Houston.\n")
#define DCC_JOIN		STR("*** %s has joined the party line.\n")
#define DCC_LOSTDCC		STR("Lost dcc connection to %s (%s/%d)")
#define DCC_PWDTIMEOUT		STR("Password timeout on dcc chat: [%s]%s")
#define DCC_SPWDTIMEOUT		STR("Auth timeout on dcc chat: [%s]%s")
#define DCC_CLOSED		STR("DCC connection closed (%s!%s)")
#define DCC_FAILED		STR("Failed TELNET incoming (%s)")
#define DCC_BADSRC		STR("Refused %s/%d (bad src port)")
/* was: DCC_BADIP 0xe19 */

#define DCC_BADHOST		STR("Refused %s (bad hostname)")
#define DCC_TELCONN		STR("Telnet connection: %s/%d")
#define DCC_IDENTFAIL		STR("Ident failed for %s: %s")
#define DCC_PORTDIE		STR("(!) Listening port %d abruptly died.")
#define DCC_BADNICK		STR("Refused %s (bad nick)")
#define DCC_NONBOT		STR("Refused %s (non-bot)")
#define DCC_NONUSER		STR("Refused %s (non-user)")
#define DCC_INVHANDLE		STR("Refused %s (invalid handle: %s)")
#define DCC_DUPLICATE		STR("Refused telnet connection from %s (duplicate)")
#define DCC_NOPASS		STR("Refused [%s]%s (no password)")
#define DCC_LOSTCON		STR("Lost telnet connection to %s/%d")
#define DCC_TTIMEOUT		STR("Ident timeout on telnet: %s")
#define DCC_TCLERROR		STR("Tcl error [%s]: %s")
#define DCC_DEADSOCKET		STR("*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED")
#define DCC_LOSTCONN		STR("Lost connection while identing [%s/%d]")
#define DCC_EOFIDENT		STR("Timeout/EOF ident connection")
#define DCC_LOSTIDENT		STR("Lost ident wait telnet socket!!")
#define DCC_NOACCESS		STR("Denied telnet: %s, No Access")
#define DCC_MYBOTNETNICK	STR("Refused telnet connection from %s (tried using my botnetnick)")
#define DCC_LOSTDUP		STR("Lost telnet connection from %s while checking for duplicate")

#endif				/* _EGG_LANG_H */
