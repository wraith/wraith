#ifndef HELP_H
#define HELP_H
typedef struct {
  int type;
  char *cmd;
  char *desc;
} help_t;

help_t help[] =
{
{2, "cmd",
"Usage: cmd <blah> \n \
	Help text goes here \n \
	more here \n \
	etc .... \n"},
{2, "echo",
"	Testing the help system \n \
blah blah blah blah \n \
[12:42:49] <drewballs> hey, would it be possible to create 3 more vhosts with drewballs.org with different nicks? \n \
[12:43:20] <drewballs> but things like, got.0wned.at.drewballs.org \n"},
{2, "whois",
"Usage: whois <handle> \n \
    will show you the bot information about a user record. there are \n \
    five headings: \n \
    HANDLE the handle (nickname) of the user \n \
    PASS 'Yes' if they have a password set, 'No' if not \n \
    NOTES number of notes stored waiting for the user \n \
    FLAGS the list of flags for this user (see below) \n \
    LAST the time or date that the user was last on the channel \n \
    the valid flags under FLAGS are: \n \
      a administrator (user is higher than an owner but not a perm owner) \n \
      b bot (user is another bot) \n \
      c leaf 'chat hub' (bot will accept MSG cmds and dcc chat from users) \n \
      d global deop (user cannot get ops) \n \
      i hub access (user can .relay to hubs) \n \
      j leaf 'chat hub' access (user has DCC access to leaf 'chat hubs') \n \
      k global auto-kick (user kicked & banned automatically) \n \
      l bot flag - sets limit (bot will set limit in all chans) \n \
      m master (user is a bot master) \n \
      n owner (user is the bot owner) \n \
      o global op (bot will op this user on any channel upon request) \n \
      p party-line (user has party-line chat access [Not required for dcc, see: +j]) \n \
      q quiet (user never gets +v on channels) \n \
      v global voice (user get +v automatically on +autovoice channels - CelBots delayed) \n \
      w wasop-test (needs wasop test for +stopnethack procedure) \n \
      x flood exempt (user will not be punished for flooding) \n \
      y bot flag - voices users (bot will voice +v users in all chans) \n \
    each channel that the user has joined will have a specific record \n \
    for it, with the channel-specific flags and possibly an info \n \
    line. the channel-specific flags are: \n \
      d deop (bot will not allow this user to become a chanop) \n \
      k kick (user is auto kicked-banned from chan) \n \
      l bot flag - sets limit (bot will set limit in all chans) \n \
      m master (user is a master for the channel) \n \
      n owner (user is an owner for the channel) \n \
      o op (bot will give this user chanop) \n \
      q quiet (user never gets +v in chan) \n \
      v voice (user gets +v automatically on +autovoice channels - CelBots delayed) \n \
      w wasop-test (needs wasop test for +stopnethack procedure) \n \
      x flood exempt (user will not be punished for flooding) \n \
      y bot flag - voices users (bot will voice +v users in all chans) \n \
    hostmasks for the user are displayed on the following lines. \n \
    (not on leaf bots though) \n \
    if the user entry is for a bot, there will be a line below which \n \
    says 'ADDRESS:' and gives the bot's telnet address. some user \n \
    entries may have 'EMAIL:' and 'INFO:' entries too. \n \
    %{+m} masters: if there is a comment, you will see it under 'COMMENT:' \n \
      %{-} See also: match \n"},

{0, NULL}	/* end of the list */
};
#endif /* HELP_H */
