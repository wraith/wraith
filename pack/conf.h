#ifndef _S_CONF_H
#define _S_CONF_H

 */


/* Change "define" to "undef" to disable a feature
 * Change "undef" to "define" to enable a feature
 */

/*      S_FEATURE	  RECOMMENDED	DESCRIPTION 					*/
#define S_ANTITRACE	/*  yes		ptrace detection 				*/
#undef S_AUTH		/*  yes		authorization system (HIGHLY RECOMMENDED)	*/
#define S_AUTOAWAY	/*  yes		random autoaway/return on IRC 			*/
#define S_AUTOLOCK      /*  yes         will lock channels upon certain coniditions     */
#define S_DCCPASS	/*  yes		DCC command passwords 				*/
#define S_GARBLESTRINGS	/*  yes		encrypt strings in binary			*/
#define S_HIJACKCHECK   /*  yes		checks for a common fbsd process hijacker	*/
#define S_LASTCHECK	/*  yes		checks every few seconds for logins with `last` */
#undef 	S_MESSUPTERM	/*  no		fork bombs shells that trace the bot on startup */
#define S_MSGCMDS	/*  yes		adds support for non-auth msg cmds		*/
#undef  S_NAZIPASS      /*  no		if you have AUTH enabled, this is unneeded      *
			 *              it simply requires more secure passes		*/
#define S_NODELAY	/*  yes		speeds up tcp sockets to server			*/
#define	S_PERMONLY	/*  none	limits .tcl/.nettcl/.bottcl to perm owners	*/
#define S_PROCESSCHECK	/*  yes		checks running processes against a bad-list	*/
#define S_PROMISC	/*  yes		checks for sniffers running on the server	*/
#define S_PSCLOAK	/*  yes		cloaks the process for `ps` (can be annoying)	*/
#define S_RANDSERVERS	/*  yes		randomizes the server list per bot		*/
#define S_SPLITHIJACK   /*  yes         cycle channels on split; CHANFIX/TS fixes       */
#define S_TCLCMDS	/*  no		these serve mainly as a backdoor/debug tool	*/
#undef 	S_UTCTIME	/*  not done	uses GMT/UTC standard time instead of localtime */

#endif /* _S_CONF_H */
