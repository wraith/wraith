/*
 * lang.h
 *   Conversion definitions for language support
 *
 */
#ifndef _EGG_LANG_H
#define _EGG_LANG_H




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
#define BOT_BOTSCONNECTED	"Bots connected"
#define BOT_OTHERPEOPLE		"Other people on the bot"
/* was: BOT_OUTDATEDWHOM - 0xb1e */
#define BOT_LINKATTEMPT		"Attempting to link"
#define BOT_NOTESTORED2		"Not online; note stored."
#define BOT_NOTEBOXFULL		"Notebox is full, sorry."
#define BOT_NOTEISAWAY		"is away; note stored."
#define BOT_NOTESENTTO		"Note sent to"
#define BOT_DISCONNECTED	"Disconnected from:"
#define BOT_CANTLINKTHERE	"Can't link there"
#define BOT_CANTUNLINK		"Can't unlink"
/* was: BOT_BOGUSLINK2 - 0xb2a */
#define BOT_DISCONNLEAF		"Disconnected left"
#define BOT_LINKEDTO		"Linked to"
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
#define BOT_CANTRELAYMYSELF	"Relay to myself?  What on EARTH would be the point?!"
#define BOT_CONNECTINGTO	"Connecting to"
/* was: BOT_PARTYJOINED - 0xb52 */
#define BOT_LOSTDCCUSER		"Lost dcc connection to"
#define BOT_DROPPINGRELAY	"Dropping relay attempt to"
#define BOT_RELAYSUCCESS	"Success!\n\nNOW CONNECTED TO RELAY BOT"
#define BOT_RELAYLINK		"Relay link:"
#define BOT_ENDRELAY1		"Ended relay link"
#define BOT_ENDRELAY2		"RELAY CONNECTION DROPPED.\nYou are now back on"
#define BOT_PARTYREJOINED	"rejoined the party line."
#define BOT_DROPPEDRELAY	"Dropping relay link to"
#define BOT_BREAKRELAY		"Breaking connection to"
#define BOT_RELAYBROKEN		"Relay broken"
#define BOT_PINGTIMEOUT		"Ping timeout"
#define BOT_BOTNOTLEAFLIKE	"unleaflike behavior"
#define BOT_BOTDROPPED		"Dropped bot"

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
