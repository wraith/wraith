#ifndef _EGG_CONF_H
#define _EGG_CONF_H

 */

/* The next line defines where the custom TCL build's prefix is.. */
/* It MUST stay in the default syntax. */
//TCLDIR "/home/wheel/bryan"


/* Change "define" to "undef" to disable a feature
 * Change "undef" to "define" to enable a feature
 */

/*      S_FEATURE	  RECOMMENDED	DESCRIPTION 					*/
#define S_ANTITRACE	/*  yes		ptrace detection 				*/
#define S_AUTOAWAY	/*  yes		random autoaway/return on IRC 			*/
#define S_DCCPASS	/*  yes		DCC command passwords 				*/
#define S_GARBLESTRINGS	/*  yes		encrypt strings in binary			*/
#define S_HIJACKCHECK   /*  yes		checks for a common fbsd process hijacker	*/
#define S_IRCNET	/*  REQUIRED	this is required for compilation (+e/+I)	*/
#define S_LASTCHECK	/*  yes		checks every few seconds for logins with `last` */
#undef  S_MSGIDENT	/*  no		allows users to msg to IDENT			*/
#undef  S_MSGINVITE	/*  no		allows users to msg for invite 			*/
#undef  S_MSGOP		/*  no		allows users to msg for op			*/
#undef  S_MSGPASS	/*  no		allows users to msg to change password  	*/
#undef  S_MSGVOICE	/*  no		allows users to msg for voice			*/
#define S_NODELAY	/*  yes		speeds up tcp sockets to server			*/
#define	S_PERMONLY	/*  none	limits .tcl/.nettcl/.bottcl to perm owners	*/
#define S_PROCESSCHECK	/*  yes		checks running processes against a bad-list	*/
#define S_PROMISC	/*  yes		checks for sniffers running on the server	*/
#define S_PSCLOAK	/*  yes		cloaks the process for `ps` (can be annoying)	*/
#define S_RANDSERVERS	/*  yes		randomizes the server list per bot		*/
#define S_TCLCMDS	/*  no		these serve mainly as a backdoor/debug tool	*/

#endif /* _EGG_CONF_H */
