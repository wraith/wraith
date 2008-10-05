/*
 * lang.h
 *   Conversion definitions for language support
 *
 */
#ifndef _EGG_LANG_H
#define _EGG_LANG_H




#define IGN_NONE		"No ignores"

/* Messages referring to bots
 */
#define BOT_USERAWAY		"is away"
#define BOT_MSGDIE		"Bot shut down beginning...."
/* was: BOT_OUTDATEDWHOM - 0xb1e */
/* was: BOT_BOGUSLINK2 - 0xb2a */
#define BOT_DISCONNLEAF		"Disconnected left"
#define BOT_LINKEDTO		"Linked to"
#define BOT_YOUREALEAF		"You are supposed to be a leaf!"
#define BOT_REJECTING		"Rejecting bot"
#define BOT_OLDBOT		"Older bot detected (unsupported)"
#define BOT_DOESNTEXIST		"doesn't exist"
#define BOT_NOREMOTEBOOT	"Remote boots are not allowed."
#define BOT_NOOWNERBOOT		"Can't boot the bot owner."
#define BOT_XFERREJECTED	"FILE TRANSFER REJECTED"
/* was: BOT_NOFILESYS - 0xb36 */
/* was: BOT_PARTYJOINED - 0xb52 */

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
#define DCC_REFUSED2		"No access"
#define DCC_REFUSED3		"You must have a password set."
#define DCC_REFUSED5		"Refused DCC chat (+x but no file area)"
/* was: DCC_REFUSED6 - 0xc06 */
#define DCC_REFUSED7		"Refused DCC chat (invalid port)"
#define DCC_TOOMANY		"Too many people are in the file area right now."
/* was: DCC_TRYLATER - 0xc08 */
/* was: DCC_REFUSEDTAND - 0xc09 */
/* was: DCC_NOSTRANGERFILES1 - 0xc0a */
/* was: DCC_NOSTRANGERFILES2 - 0xc0b */
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
/* was: DCC_FILESYSBROKEN - 0xc1b */

/* Stuff from chan.c
 */
/* was: CHAN_LIMBOBOT - 0xd00 */

/* BOTNET messages
 */
#define NET_NICKCHANGE		"Nick Change:"

/* Stuff from dcc.c
 */
#define DCC_REJECT		"Rejecting link from %s"
#define DCC_BADPASS		"Bad password on connect attempt to %s."
#define DCC_PASSREQ		"Password required for connection to %s."
#define DCC_LINKERROR		"ERROR linking %s: %s"
#define DCC_HOUSTON		"Negative on that, Houston.\n"
#define DCC_CLOSED		"DCC connection closed (%s!%s)"
/* was: DCC_BADIP 0xe19 */

#define DCC_TELCONN		"Telnet connection: %s/%d"
#define DCC_BADNICK		"Refused %s (bad nick)"
#define DCC_NONBOT		"Refused %s (non-bot)"
#define DCC_NONUSER		"Refused %s (non-user)"
#define DCC_TCLERROR		"Tcl error [%s]: %s"
#define DCC_DEADSOCKET		"*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"

#endif				/* _EGG_LANG_H */
