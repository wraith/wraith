/*
 * lang.h
 *   Conversion definitions for language support
 *
 */
#ifndef _EGG_LANG_H
#define _EGG_LANG_H


#define DETEST			"WORKS"

#define MISC_USAGE		"Usage"
#define MISC_FAILED		"Failed.\n"

/* Userfile messages
 */
#define USERF_XFERDONE		"Userlist transfer complete; switched over"
/* was: USERF_BADREREAD - 0x401 */
#define USERF_CANTREAD		"CAN'T READ NEW USERFILE"
#define USERF_CANTSEND		"Can't send userfile to you (internal error)"
#define USERF_NOMATCH		"Can't find anyone matching that"
#define USERF_INVALID		"Invalid userfile format."
#define USERF_CORRUPT		"Corrupt user record"
#define USERF_DUPE		"Duplicate user record"
#define USERF_BROKEPASS		"Corrupted password reset for"
#define USERF_IGNBANS		"Ignored masks for channel(s):"
#define USERF_WRITING		"Writing user file..."
#define USERF_ERRWRITE		"ERROR writing user file."
#define USERF_ERRWRITE2		"ERROR writing user file to transfer."
#define USERF_NONEEDNEW		"Userfile creation not necessary--skipping"
#define USERF_REHASHING		"Rehashing..."
#define USERF_UNKNOWN		"I don't know anyone by that name.\n"
#define USERF_NOUSERREC		"No user record."
#define USERF_BACKUP		"Backing up user file..."
#define USERF_FAILEDXFER	"Failed connection; aborted userfile transfer."
#define USERF_OLDSHARE		"Old style share request by"
#define USERF_ANTIQUESHARE	"Antiquated sharing request"
#define USERF_REJECTED		"User file rejected by"

/* Misc messages
 */

#define USERF_OLDFMT		"boring...."
#define MISC_EXPIRED		"expired"
#define MISC_TOTAL		"total"
#define MISC_ERASED		"Erased"
/* was: MISC_LEFT - 0x503 */
#define MISC_ONLOCALE		"on"
#define MISC_MATCHING		"Matching"
#define MISC_SKIPPING		"skipping first"
#define MISC_TRUNCATED		"(more than %d matches; list truncated)\n"
#define MISC_FOUNDMATCH		"--- Found %d match%s.\n"
#define MISC_AMBIGUOUS		"Ambiguous command.\n"
#define MISC_NOSUCHCMD		"What?  You need 'help'\n"
#define MISC_CMDBINDS		"Command bindings:\n"
#define MISC_RESTARTING		"Restarting..."
#define MISC_MATCH_PLURAL	"es"
#define MISC_LOGSWITCH		"Switching logfiles..."
#define MISC_OWNER		"owner"
#define MISC_MASTER		"master"
#define MISC_OP			"op"
#define MISC_IDLE		"idle"
#define MISC_AWAY		"AWAY"
/* was: MISC_IGNORING - 0x514 */
/* was: MISC_UNLINKED - 0x515 */
#define MISC_DISCONNECTED	"Disconnected"
#define MISC_INVALIDBOT		"invalid bot"
#define MISC_LOOP		"Detected loop: two bots exist named"
/* was: MISC_MUTUAL - 0x519 */
#define MISC_FROM		"from"
#define MISC_OUTDATED		"outdated"
#define MISC_REJECTED		"rejected"
#define MISC_IMPOSTER		"imposter"
#define MISC_TRYING		"trying"
#define MISC_MOTDFILE		"MOTD file:"
#define MISC_NOMOTDFILE		"No MOTD file."
#define MISC_USEFORMAT		"Use:"
#define MISC_CHADDRFORMAT	"<address>:<port#>[/<relay-port#>]"
/* was: MISC_UNKNOWN - 0x523 */
/* was: MISC_CHANNELS - 0x524 */
/* was: MISC_TRYINGMISTAKE - 0x525 */
#define MISC_PENDING		"pending"
#define MISC_WANTOPS		"want ops!"
#define MISC_LURKING		"lurking"
#define MISC_BACKGROUND		"background"
#define MISC_TERMMODE		"terminal mode"
#define MISC_STATMODE		"status mode"
#define MISC_LOGMODE		"log dump mode"
#define MISC_ONLINEFOR		"Online for"
#define MISC_CACHEHIT		"cache hit"
#define MISC_TCLLIBRARY		"Tcl library:"
#define MISC_NEWUSERFLAGS	"New users get flags"
#define MISC_NOTIFY		"notify"
#define MISC_PERMOWNER		"Permanent owner(s)"
/* was: MISC_ROOTWARN - 0x533 */
#define MISC_NOUSERFILE		"USER FILE NOT FOUND!  (try './eggdrop -m %s' to make one)\n"
#define MISC_NOUSERFILE2	"STARTING BOT IN USERFILE CREATION MODE.\nTelnet to the bot and enter 'NEW' as your nickname."
#define MISC_USERFCREATE1	"OR go to IRC and type:  /msg %s hello\n"
#define MISC_USERFCREATE2	"This will make the bot recognize you as the master."
#define MISC_USERFEXISTS	"USERFILE ALREADY EXISTS (drop the '-m')"
#define MISC_CANTWRITETEMP	"CAN'T WRITE TO TEMP DIR"
#define MISC_CANTRELOADUSER	"Can't reload user file!"
#define MISC_MISSINGUSERF	"User file is missing!"
/* was: MISC_BOTSCONNECTED - 0x53d */
#define MISC_BANNER		"%B  (%E)\n\nPlease enter your nickname.\n"
#define MISC_CLOGS		"Cycling logfile %s, over max-logsize (%d)"
#define MISC_BANNER_STEALTH	"\nNickname.\n"
#define MISC_LOGREPEAT		"Last message repeated %d time(s).\n"
#define MISC_JUPED		"juped"
#define MISC_NOFREESOCK		"No free sockets available."
#define MISC_TCLVERSION		"Tcl version:"
#define MISC_TCLHVERSION	"header version"

/* IRC */
#define IRC_BANNED		"Banned"
#define IRC_YOUREBANNED		"You are banned"
/* BOT log messages when attempting to place a ban which matches me */
#define IRC_IBANNEDME		"Wanted to ban myself--deflected."
#define IRC_FUNKICK		"that was fun, let's do it again!"
#define IRC_HI			"Hi"
#define IRC_GOODBYE		"Goodbye"
#define IRC_BANNED2		"You're banned, goober."
#define IRC_NICKTOOLONG		"NOTICE %s :Your nick was too long and therefore it was truncated to '%s'.\n"
#define IRC_INTRODUCED		"Introduced to"
#define IRC_COMMONSITE		"common site"
#define IRC_SALUT1		"NOTICE %s :Hi %s!  I'm %s, an eggdrop bot.\n"
#define IRC_SALUT2		"NOTICE %s :I'll recognize you by hostmask '%s' from now on.\n"
#define IRC_SALUT2A		"Since you come from a common irc site, this means you should"
#define IRC_SALUT2B		"always use this nickname when talking to me."
#define IRC_INITOWNER1		"YOU ARE THE OWNER ON THIS BOT NOW"
#define IRC_INIT1		"Bot installation complete, first master is %s"
#define IRC_INITNOTE		"Welcome to Eggdrop! =]"
#define IRC_INITINTRO		"introduced to %s from %s"
#define IRC_PASS		"You have a password set."
#define IRC_NOPASS		"You don't have a password set."
/* was: IRC_NOPASS2 - 0x614 */
#define IRC_EXISTPASS		"You already have a password set."
#define IRC_PASSFORMAT		"Please use at least 6 characters."
#define IRC_SETPASS		"Password set to:"
#define IRC_FAILPASS		"Incorrect password."
#define IRC_CHANGEPASS		"Password changed to:"
#define IRC_FAILCOMMON		"You're at a common site; you can't IDENT."
#define IRC_MISIDENT		"NOTICE %s :You're not %s, you're %s.\n"
#define IRC_DENYACCESS		"Access denied."
#define IRC_RECOGNIZED		"I recognize you there."
#define IRC_ADDHOSTMASK		"Added hostmask"
/* was: IRC_DELMAILADDR - 0x61f */
#define IRC_FIELDCURRENT	"Currently:"
#define IRC_FIELDCHANGED	"Now:"
#define IRC_FIELDTOREMOVE	"To remove it:"
/* was: IRC_NOEMAIL - 0x623 */
#define IRC_INFOLOCKED		"Your info line is locked"
#define IRC_REMINFOON		"Removed your info line on"
#define IRC_REMINFO		"Removed your info line."
#define IRC_NOINFOON		"You have no info set on"
#define IRC_NOINFO		"You have no info set."
#define IRC_NOMONITOR		"I don't monitor that channel."
#define IRC_RESETCHAN		"Resetting channel info."
#define IRC_JUMP		"Jumping servers..."
#define IRC_CHANHIDDEN		"Channel is currently hidden."
#define IRC_ONCHANNOW		"Now on channel"
#define IRC_NEVERJOINED		"Never joined one of my channels."
#define IRC_LASTSEENAT		"Last seen at"
#define IRC_DONTKNOWYOU		"I don't know you; please introduce yourself first."
#define IRC_NOHELP		"No help."
#define IRC_NOHELP2		"No help available on that."
#define IRC_NOTONCHAN		"Not on that channel right now."
#define IRC_GETORIGNICK		"Switching back to nick %s"
#define IRC_BADBOTNICK		"Server says my nickname is invalid."
#define IRC_BOTNICKINUSE	"NICK IN USE: Trying '%s'"
#define IRC_BOTNICKJUPED	"Nickname has been juped"
#define IRC_CHANNELJUPED	"Channel %s is juped. :("
#define IRC_NOTREGISTERED1	"%s says I'm not registered, trying next one."
#define	IRC_NOTREGISTERED2	"The server says we are not registered yet.."
#define IRC_FLOODIGNORE1	"Flood from @%s!  Placing on ignore!"
/* was: IRC_FLOODIGNORE2 - 0x63e */
#define IRC_FLOODIGNORE3	"JOIN flood from @%s!  Banning."
#define IRC_FLOODKICK		"Channel flood from %s -- kicking"
#define IRC_SERVERTRY		"Trying server"
#define IRC_DNSFAILED		"DNS lookup failed"
#define IRC_FAILEDCONNECT	"Failed connect to"
#define IRC_SERVERSTONED	"Server got stoned; jumping..."
#define IRC_DISCONNECTED	"Disconnected from"
#define IRC_NOSERVER		"No server currently."
#define IRC_MODEQUEUE		"Mode queue is at"
#define IRC_SERVERQUEUE		"Server queue is at"
#define IRC_HELPQUEUE		"Help queue is at"
/* was: IRC_BOTNOTONIRC - 0x64a */
#define IRC_NOTACTIVECHAN	"Not active on channel"
#define IRC_PROCESSINGCHAN	"Processing channel"
#define IRC_CHANNEL		"Channel"
#define IRC_DESIRINGCHAN	"Desiring channel"
#define IRC_CHANNELTOPIC	"Channel Topic"
#define IRC_PENDINGOP		"pending +o -- I'm lagged"
#define IRC_PENDINGDEOP		"pending -o -- I'm lagged"
#define IRC_PENDINGKICK		"pending kick"
#define IRC_FAKECHANOP		"FAKE CHANOP GIVEN BY SERVER"
#define IRC_ENDCHANINFO		"End of channel info."
#define IRC_MASSKICK		"mass kick, go sit in a corner"
#define IRC_REMOVEDBAN		"Removed ban"
#define IRC_UNEXPECTEDMODE	"Hmm, mode info from a channel I'm not on"
#define IRC_POLITEKICK		"...and thank you for playing."
#define IRC_AUTOJUMP		"Jumping servers (need %d servers, only have %d)"
#define IRC_CHANGINGSERV	"changing servers"
#define IRC_TOOMANYCHANS	"I'm on too many channels--can't join: %s"
#define IRC_CHANFULL		"Channel full--can't join: %s"
#define IRC_CHANINVITEONLY	"Channel invite only--can't join: %s"
#define IRC_BANNEDFROMCHAN	"Banned from channel--can't join: %s"
#define IRC_SERVNOTONCHAN	"Server says I'm not on channel: %s"
#define IRC_BADCHANKEY		"Bad key--can't join: %s"
#define IRC_TELNETFLOOD		"Telnet connection flood from %s!  Placing on ignore!"
#define IRC_JOIN_FLOOD		"join flood"
#define IRC_KICK_PROTECT	"don't kick my friends, bud"
#define IRC_DEOP_PROTECT	"don't deop my friends, bud"
#define IRC_COMMENTKICK		"...and don't come back."
#define IRC_REMOVEDEXEMPT	"Removed exempt"
#define IRC_REMOVEDINVITE	"Removed invite"
#define IRC_FLOODIGNORE4	"NICK flood from @%s!  Banning."
#define IRC_NICK_FLOOD		"nick flood"

/* Eggdrop command line usage
 */
#define EGG_RUNNING1		"I detect %s already running from this directory.\n"
#define EGG_RUNNING2		"If this is incorrect, erase the '%s'\n"
#define EGG_NOWRITE		"* Warning!  Could not write %s file!\n"

#define USER_ISGLOBALOP		"(is a global op)"
#define USER_ISBOT		"(is a bot)"
#define USER_ISMASTER		"(is a master)"

/* '.bans/.invites/.exempts' common messages
 */
#define MODES_CREATED		"Created"
#define MODES_LASTUSED		"last used"
#define MODES_INACTIVE		"inactive"
#define MODES_PLACEDBY		"placed by"
#define MODES_NOTACTIVE		"not active on"
#define MODES_NOTACTIVE2	"not active"
#define MODES_NOTBYBOT		"not placed by bot"

/* Messages used when listing with `.bans'
 */
#define BANS_GLOBAL		"Global bans"
#define BANS_BYCHANNEL		"Channel bans for"
#define BANS_USEBANSALL		"Use 'bans all' to see the total list"
#define BANS_NOLONGER		"No longer banning"

/* Messages used when listing with '.exempts'
 */
#define EXEMPTS_GLOBAL		"Global exempts"
#define EXEMPTS_BYCHANNEL	"Channel exempts for"
#define EXEMPTS_USEEXEMPTSALL	"Use 'exempts all' to see the total list"
#define EXEMPTS_NOLONGER	"No longer ban exempting"

/* Messages used when listing with '.invites'
 */
#define INVITES_GLOBAL		"Global invites"
#define INVITES_BYCHANNEL	"Channel invites for"
#define INVITES_USEINVITESALL	"Use 'invites all' to see the total list"
#define INVITES_NOLONGER	"No longer inviting"


/* Messages referring to channels
 */
#define CHAN_NOSUCH		"No such channel defined"
#define CHAN_BADCHANMODE	"* Mode change on %s for nonexistant %s!"
#define CHAN_MASSDEOP		"Mass deop on %s by %s"
#define CHAN_MASSDEOP_KICK	"Mass deop.  Go sit in a corner."
#define CHAN_FORCEJOIN		"Oops.   Someone made me join %s... leaving..."
#define CHAN_FAKEMODE		"Mode change by fake op on %s!  Reversing..."
#define CHAN_FAKEMODE_KICK	"Abusing ill-gained server ops"
#define CHAN_DESYNCMODE		"Mode change by non-chanop on %s!  Reversing..."
#define CHAN_DESYNCMODE_KICK	"Abusing desync"
#define CHAN_FLOOD		"flood"

/* Messages referring to ignores
 */
#define IGN_NONE		"No ignores"
#define IGN_CURRENT		"Currently ignoring"
#define IGN_NOLONGER		"No longer ignoring"

/* Messages referring to bots
 */
#define BOT_NOTHERE		"That bot isn't here.\n"
#define BOT_NONOTES		"That's a bot.  You can't leave notes for a bot.\n"
#define BOT_USERAWAY		"is away"
#define BOT_NOTEARRIVED		"Note arrived for you"
#define BOT_MSGDIE		"Bot shut down beginning...."
#define BOT_NOSUCHUSER		"No such user"
#define BOT_NOCHANNELS		"no channels"
#define BOT_PARTYMEMBS		"Party line members:"
#define BOT_BOTSCONNECTED	"Bots connected"
#define BOT_OTHERPEOPLE		"Other people on the bot"
/* was: BOT_OUTDATEDWHOM - 0xb1e */
#define BOT_LINKATTEMPT		"Attempting to link"
#define BOT_NOTESTORED2		"Not online; note stored."
#define BOT_NOTEBOXFULL		"Notebox is full, sorry."
#define BOT_NOTEISAWAY		"is away; note stored."
#define BOT_NOTESENTTO		"Note sent to"
#define BOT_DISCONNECTED	"Disconnected from:"
#define BOT_PEOPLEONCHAN	"People on channel"
#define BOT_CANTLINKTHERE	"Can't link there"
#define BOT_CANTUNLINK		"Can't unlink"
#define BOT_LOOPDETECT		"Loop detected"
#define BOT_BOGUSLINK		"Bogus link notice from"
/* was: BOT_BOGUSLINK2 - 0xb2a */
#define BOT_DISCONNLEAF		"Disconnected left"
#define BOT_LINKEDTO		"Linked to"
#define BOT_ILLEGALLINK		"Illegal link by leaf"
#define BOT_YOUREALEAF		"You are supposed to be a leaf!"
#define BOT_REJECTING		"Rejecting bot"
#define BOT_OLDBOT		"Older bot detected (unsupported)"
#define BOT_TRACERESULT		"Trace result"
#define BOT_DOESNTEXIST		"doesn't exist"
#define BOT_NOREMOTEBOOT	"Remote boots are not allowed."
#define BOT_NOOWNERBOOT		"Can't boot the bot owner."
#define BOT_XFERREJECTED	"FILE TRANSFER REJECTED"
/* was: BOT_NOFILESYS - 0xb36 */
#define BOT_BOTNETUSERS		"Users across the botnet"
#define BOT_PARTYLINE		"Party line"
#define BOT_LOCALCHAN		"Local channel"
#define BOT_USERSONCHAN		"Users on channel"
#define BOT_NOBOTSLINKED	"No bots linked."
#define BOT_NOTRACEINFO		"No trace info for:"
#define BOT_COMPLEXTREE		"Tree too complex!"
#define BOT_UNLINKALL		"Unlinking all bots..."
#define BOT_KILLLINKATTEMPT	"Killed link attempt to"
#define BOT_ENDLINKATTEMPT	"No longer trying to link:"
#define BOT_BREAKLINK		"Breaking link with"
#define BOT_UNLINKEDFROM	"Unlinked from:"
#define BOT_NOTCONNECTED	"Not connected to that bot."
#define BOT_WIPEBOTTABLE	"Smooshing bot tables and assocs..."
#define BOT_BOTUNKNOWN		"is not a known bot."
#define BOT_CANTLINKMYSELF	"Link to myself?  Oh boy, Freud would have a field day."
#define BOT_ALREADYLINKED	"That bot is already connected up."
#define BOT_NOTELNETADDY	"Invalid telnet address:port stored for"
#define BOT_LINKING		"Linking to"
#define BOT_CANTFINDRELAYUSER	"Can't find user for relay!"
#define BOT_CANTLINKTO		"Could not link to"
#define BOT_CANTRELAYMYSELF	"Relay to myself?  What on EARTH would be the point?!"
#define BOT_CONNECTINGTO	"Connecting to"
#define BOT_BYEINFO1		"(Type *BYE* on a line by itself to abort.)"
#define BOT_ABORTRELAY1		"Aborting relay attempt to"
#define BOT_ABORTRELAY2		"You are now back on"
#define BOT_ABORTRELAY3		"Relay aborted:"
/* was: BOT_PARTYJOINED - 0xb52 */
#define BOT_LOSTDCCUSER		"Lost dcc connection to"
#define BOT_DROPPINGRELAY	"Dropping relay attempt to"
#define BOT_RELAYSUCCESS	"Success!\n\nNOW CONNECTED TO RELAY BOT"
#define BOT_BYEINFO2		"(You can type *BYE* to prematurely close the connection.)"
#define BOT_RELAYLINK		"Relay link:"
#define BOT_PARTYLEFT		"left the party line."
#define BOT_ENDRELAY1		"Ended relay link"
#define BOT_ENDRELAY2		"RELAY CONNECTION DROPPED.\nYou are now back on"
#define BOT_PARTYREJOINED	"rejoined the party line."
#define BOT_DROPPEDRELAY	"Dropping relay link to"
#define BOT_BREAKRELAY		"Breaking connection to"
#define BOT_RELAYBROKEN		"Relay broken"
#define BOT_PINGTIMEOUT		"Ping timeout"
#define BOT_BOTNOTLEAFLIKE	"unleaflike behavior"
#define BOT_BOTDROPPED		"Dropped bot"
#define BOT_ALREADYLINKING	"Already linking to that bot."

/* Messages pertaining to MODULES
 */
#define MOD_ALREADYLOAD		"Already loaded."
#define MOD_BADCWD		"Can't determine current directory"
#define MOD_NOSTARTDEF		"No start function defined"
#define MOD_NEEDED		"Needed by another module"
#define MOD_NOCLOSEDEF		"No close function"
#define MOD_UNLOADED		"Module unloaded:"
#define MOD_NOSUCH		"No such module"
/* was: MOD_NOINFO - 0x208 */
#define MOD_LOADERROR		"Error loading module:"
#define MOD_UNLOADERROR		"Error unloading module:"
#define MOD_CANTLOADMOD		"Can't load modules"
#define MOD_STAGNANT		"Stagnant module; there WILL be memory leaks!"
#define MOD_NOCRYPT		"You have installed modules but have not selected an encryption\nmodule, please consult the default config file for info.\n"
#define MOD_NOFILESYSMOD	"Filesys module not loaded."
#define MOD_LOADED_WITH_LANG	"Module loaded: %-16s (with lang support)"
#define MOD_LOADED		"Module loaded: %-16s"


#define DCC_NOSTRANGERS		"I don't accept DCC chats from strangers."
#define DCC_REFUSED		"Refused DCC chat (no access)"
#define DCC_REFUSEDNC		"Refused DCC chat (I'm not a chathub (+c))"
#define DCC_REFUSED2		"No access"
#define DCC_REFUSED3		"You must have a password set."
#define DCC_REFUSED4		"Refused DCC chat (no password)"
#define DCC_REFUSED5		"Refused DCC chat (+x but no file area)"
/* was: DCC_REFUSED6 - 0xc06 */
#define DCC_REFUSED7		"Refused DCC chat (invalid port)"
#define DCC_TOOMANY		"Too many people are in the file area right now."
/* was: DCC_TRYLATER - 0xc08 */
/* was: DCC_REFUSEDTAND - 0xc09 */
/* was: DCC_NOSTRANGERFILES1 - 0xc0a */
/* was: DCC_NOSTRANGERFILES2 - 0xc0b */
#define DCC_TOOMANYDCCS1	"Sorry, too many DCC connections."
#define DCC_TOOMANYDCCS2	"DCC connections full: %s %s (%s!%s)"
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
#define DCC_CONNECTFAILED1	"Failed to connect"
#define DCC_CONNECTFAILED2	"DCC connection failed"
#define DCC_CONNECTFAILED3	"DCC invalid port"
/* was: DCC_FILESYSBROKEN - 0xc1b */
#define DCC_ENTERPASS		"Enter your password"
#define DCC_FLOODBOOT		"%s has been forcibly removed for flooding.\n"
#define DCC_BOOTED1		"-=- poof -=-\n"
#define DCC_BOOTED2		"You've been booted from the %s by %s%s%s\n"
#define DCC_BOOTED3		"%s booted %s from the party line%s%s"

/* Stuff from chan.c
 */
/* was: CHAN_LIMBOBOT - 0xd00 */

/* BOTNET messages
 */
#define NET_FAKEREJECT		"Fake message rejected"
#define NET_LINKEDTO		"Linked to"
#define NET_WRONGBOT		"Wrong bot--wanted %s, got %s"
#define NET_LEFTTHE		"has left the"
#define NET_JOINEDTHE		"has joined the"
#define NET_AWAY		"is now away"
#define NET_UNAWAY		"is no longer away"
#define NET_NICKCHANGE		"Nick Change:"

/* Stuff from dcc.c
 */
#define DCC_REJECT		"Rejecting link from %s"
#define DCC_LINKED		"Linked to %s."
#define DCC_LINKFAIL		"Failed link to %s."
#define DCC_BADPASS		"Bad password on connect attempt to %s."
#define DCC_PASSREQ		"Password required for connection to %s."
#define DCC_LINKERROR		"ERROR linking %s: %s"
#define DCC_LOSTBOT		"Lost Bot: %s"
#define DCC_TIMEOUT		"Timeout: bot link to %s at %s:%d"
#define DCC_LOGGEDIN		"Logged in: %s (%s/%d)"
#define DCC_BADLOGIN		"Bad Password: [%s]%s/%d"
#define DCC_BADAUTH		"Bad Auth: [%s]%s/%d"
#define DCC_HOUSTON		"Negative on that, Houston.\n"
#define DCC_JOIN		"*** %s has joined the party line.\n"
#define DCC_LOSTDCC		"Lost dcc connection to %s (%s/%d)"
#define DCC_PWDTIMEOUT		"Password timeout on dcc chat: [%s]%s"
#define DCC_SPWDTIMEOUT		"Auth timeout on dcc chat: [%s]%s"
#define DCC_CLOSED		"DCC connection closed (%s!%s)"
#define DCC_FAILED		"Failed TELNET incoming (%s)"
#define DCC_BADSRC		"Refused %s/%d (bad src port)"
/* was: DCC_BADIP 0xe19 */

#define DCC_BADHOST		"Refused %s (bad hostname)"
#define DCC_TELCONN		"Telnet connection: %s/%d"
#define DCC_IDENTFAIL		"Ident failed for %s: %s"
#define DCC_PORTDIE		"(!) Listening port %d abruptly died."
#define DCC_BADNICK		"Refused %s (bad nick)"
#define DCC_NONBOT		"Refused %s (non-bot)"
#define DCC_NONUSER		"Refused %s (non-user)"
#define DCC_INVHANDLE		"Refused %s (invalid handle: %s)"
#define DCC_DUPLICATE		"Refused telnet connection from %s (duplicate)"
#define DCC_NOPASS		"Refused [%s]%s (no password)"
#define DCC_LOSTCON		"Lost telnet connection to %s/%d"
#define DCC_TTIMEOUT		"Ident timeout on telnet: %s"
#define DCC_TCLERROR		"Tcl error [%s]: %s"
#define DCC_DEADSOCKET		"*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"
#define DCC_LOSTCONN		"Lost connection while identing [%s/%d]"
#define DCC_EOFIDENT		"Timeout/EOF ident connection"
#define DCC_LOSTIDENT		"Lost ident wait telnet socket!!"
#define DCC_NOACCESS		"Denied telnet: %s, No Access"
#define DCC_MYBOTNETNICK	"Refused telnet connection from %s (tried using my botnetnick)"
#define DCC_LOSTDUP		"Lost telnet connection from %s while checking for duplicate"

#endif				/* _EGG_LANG_H */
