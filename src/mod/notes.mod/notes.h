/*
 * notes.h -- part of notes.mod
 *
 */

#ifndef _EGG_MOD_NOTES_NOTES_H
#define _EGG_MOD_NOTES_NOTES_H

#include "src/common.h"
#include "src/users.h"

#define NOTES_IGNKEY "NOTESIGNORE"

/* language #define's */


#define NOTES_USAGE			MISC_USAGE
#define NOTES_USERF_UNKNOWN		USERF_UNKNOWN
#define NOTES_FORWARD_TO		"  Forward notes to: %.70s\n"
#define NOTES_SWITCHED_NOTES		"Switched %d note%s from %s to %s."
#define NOTES_EXPIRED			"Expired %d note%s"
#define NOTES_FORWARD_NOTONLINE		"Not online; forwarded to %s.\n"
#define NOTES_UNSUPPORTED		"Notes are not supported by this bot."
#define NOTES_NOTES2MANY		"Sorry, that user has too many notes already."
#define NOTES_NOTEFILE_FAILED		"Can't create notefile.  Sorry."
#define NOTES_NOTEFILE_UNREACHABLE	"Notefile unreachable!"
#define NOTES_STORED_MESSAGE		"Stored message"
#define NOTES_NO_MESSAGES		"You have no messages"
#define NOTES_EXPIRE_TODAY		" -- EXPIRES TODAY"
#define NOTES_EXPIRE_XDAYS		" -- EXPIRES IN %d DAY%s"
#define NOTES_WAITING			"You have the following notes waiting"
#define NOTES_NOT_THAT_MANY		"You don't have that many messages"
#define NOTES_DCC_USAGE_READ		"Use 'notes read' to read them."
#define NOTES_FAILED_CHMOD		"Can't modify the note file"
#define NOTES_ERASED_ALL		"Erased all notes"
#define NOTES_ERASED			"Erased"
#define NOTES_LEFT			"left"
#define NOTES_MAYBE			"# may be numbers and/or intervals separated by ;"
#define NOTES_NOTTO_BOT			"That's a bot.  You can't leave notes for a bot."
#define NOTES_OUTSIDE			"Outside yadda yadda"
#define NOTES_DELIVERED			"Note delivered."
#define NOTES_FORLIST			"For a list:"
#define NOTES_WAITING_ON		"NOTICE %s :You have %d note%s waiting on %s.\n"
#define NOTES_WAITING2			"### You have %d note%s waiting.\n"
#define NOTES_DCC_USAGE_READ2		"### Use 'notes read' to read them.\n"
#define NOTES_STORED			"Notes will be stored."
#define NOTES_IGN_OTHERS		"You are not allowed to change note ignores for %s\n"
#define NOTES_UNKNOWN_USER		"User %s does not exist.\n"
#define NOTES_IGN_NEW			"Now ignoring notes from %s\n"
#define NOTES_IGN_ALREADY		"Already ignoring %s\n"
#define NOTES_IGN_REM			"No longer ignoring notes from %s\n"
#define NOTES_IGN_NOTFOUND		"Note ignore %s not found in list.\n"
#define NOTES_IGN_NONE			"No note ignores present.\n"
#define NOTES_IGN_FOR			"Note ignores for %s:\n"
#define NOTES_NO_SUCH_USER		"No such user."
#define NOTES_FWD_OWNER			"Can't change notes forwarding of the bot owner.\n"
#define NOTES_FWD_FOR			"Wiped notes forwarding for %s\n"
#define NOTES_FWD_BOTNAME		"You must supply a botname to forward to."
#define NOTES_FWD_CHANGED		"Changed notes forwarding for %s to: %s\n"
#define NOTES_MUSTBE			"Function must be one of INDEX, READ, or ERASE."

void notes_read(char *, char *, char *, int);
void notes_del(char *, char *, char *, int);
void fwd_display(int, struct user_entry *, struct userrec *);
int num_notes(char *);
void notes_report(int, int);
int storenote(char *, char *, char *, int, char *, int);

#endif				/* _EGG_MOD_NOTES_H */
