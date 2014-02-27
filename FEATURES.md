# Features

## General
 * Bot is written in a C/C++ mix. (slowly merging into a C++ OOP design)
 * Code base of [Eggdrop](http://www.eggheads.org) 1.6.12 (The code has evolved so much that it can't even be compared to a stock Eggdrop anymore.)
 * TCL is '''not''' required nor is it supported.
 * No module support
 * Initially influenced by the [ghost botpack](http://ghost.botpack.net)
 * There is only one version of a binary for each Operating System (i.e. hub and leaf binaries are identical).
 * Leaf bots act like dummy drones and save no botnet settings or files locally.
 * Only hubs store userfiles and temp data on shell; userfiles are sent to leafs during link and stored in runtime memory (1.3).
 * Userfiles store information about channels, users, and settings.
 * Binaries store information about bots [wiki:BotConfig internally]; no config files used.
 * One binary [wiki:BotConfig stores multiple bots inside it]. (Each bot gets its own process id)
 * Bots are quick and easy to [wiki:BotnetSetup setup]
 * A botnet can be '''easily''' updated with new binaries from the hub. (see [wiki:Updating])
 * Customizable DCC cmd prefix (i.e. !cmd %cmd *cmd)
 * IPv6 support for IRC/botnet
 * Asynchronous DNS
 * Easy default channel setting modifying with pseudo channel 'default'
 * No extra ports needed for userfile sharing, only the HUB port needs to be opened.
 * Passwords are stored in a SaltedSHA1 format.

## Encryption
 * All botnet traffic/files are encrypted with [AES-256](http://en.wikipedia.org/wiki/Advanced_Encryption_Standard)+base64.
 * Binaries store an assortment of MD5 checksums internally and verify them upon starting.
 * Binaries store data about bots internally with AES-256 encryption.
 * Botnet keys are randomly regenerated.
 * Preliminary support for FiSH

## DCC
 * Secure login using AuthSystem
 * Separation of cmds on hub and leaf bots
 * Remote control of leaf bots from hubs
 * Users cannot access/view users of higher level (see WhoisRestrictions).
 * Encrypted .relay between bots

## IRC
 * Hubs do not connect to IRC.
 * Autoaway at random intervals
 * Client cloaking (CTCPs / version)
 * Mass op/deop protections
 * Manual op protections
 * [wiki:CookieOps Op cookies] using custom hash/encryption scheme to protect from network stream hijack.
 * [wiki:CookieOps Op cookies] support opping multiple clients.
 * +take channel flag to quickly op botnet and mass deop channel
 * No user exemptions with flags or otherwise: I.e. if you manually op in a channel set to +dk, nobody -- including you -- is exempt.
 * +bitch channel flag uses multiple methods of protection.
 * Pre-defined list of kick reasons
 * Auto-limiter algorithm sets channel limit only when needed.
 * Configurable auto-voicer
 * Bots automatically assign roles to manage channel (limit, voice, kicks, bans, etc)
 * [CIDR ban](http://svn.ratbox.org/svnroot/ircd-ratbox/trunk/doc/CIDR.txt) support
 * Bots DNS clients to see if they match users/bans. (+r)
 * Bots prefer requesting op from bots on same server or from a list of bots sorted by hops.
 * Bots regain nicks automatically.
  * Jupenick is used for regaining your own nick, without the bot alternating on that nick.
 * [wiki:Homechan Home channel] support.
 * RBL banning support.
 * In-channel control of bots via AuthSystem.
 * Native support for floodless ilines.
 * Bot is optimized for [IRCD-Ratbox](http://www.ircd-ratbox.org/) and [EFNet](http://www.efnet.org), but should work on most IRCDs fine.
 * [005 numeric](http://www.irc.org/tech_docs/005.html) support. All on by default, if supported:
  * [CALLERID](http://svn.ratbox.org/svnroot/ircd-ratbox/trunk/doc/modeg.txt) support. Users are automatically accepted.
  * DEAF support. No channel text is sent to bot unless it has [wiki:AuthSystem authed] users on it.
  * [MONITOR](http://svn.ratbox.org/svnroot/ircd-ratbox/trunk/doc/monitor.txt) support.
  * KNOCK support.
  * [CPRIVMSG/CNOTICE](http://ircu.sourceforge.net/release.2.10.02-cprivmsg.html) support.
  * [WHOX](http://ircu.sourceforge.net/release.2.10.01-who.html) support. (Can help the bot know the IPs of clients without DNSing them)
 * Easy nickname release commands for regaining your nickname from a bot.
 * Voicebitch support for enforcing only certain users/hostmasks being voiced.
 * Protection from MAX SendQ from WHO requests replies.

## Users
 * [CIDR hostmask](http://svn.ratbox.org/svnroot/ircd-ratbox/trunk/doc/CIDR.txt) support
 * Multiple levels of control through flags
 * "Perm" owners are statically defined in the binaries.
 * Sensitive control is through userflag +a (admin -- shell access, bot configuration, etc.).
 * Botnet control is through userflag +n (owner).
 * User control is through userflag +m (master).
 * Channel control is through userflag +o (op).


## Leaf Bots
 * Botnet list is not visible.
 * Hubs are not visible.
 * Hostmask not visible in whois
 * Only bots with +c flag will accept /dcc chat or /ctcp CHAT.
 * No static telnet access
 * Ports opened for /ctcp CHAT are automatically closed after a minute.
 * Cloaked responses for /dcc chat and /ctcp CHAT
 * Userfile and other settings are not saved locally.
 * Bot data (bot.conf) stored [wiki:BotConfig internally in binary]
 * One binary may contain multiple bots..
 * One binary may spawn multiple bots..
 * Each bot gets its own process. (Not emech style or threaded)

## Shells
 * Bot config is password protected.
 * Bot automatically sets up a crontab entry.
 * All bots on the shell go into 1 binary. Each bots spawns its own process.
 * Bots on the same shell will link to the first bot in the binary (the localhub). The only the localhub links to the actual hubs.

## Botnet
 * All user/channel settings are shared automatically.
 * Users need userflag +j to access leaf bots.
 * Users need userflag +i to access hub bots through telnet or .relay.
 * Users need userflag +p to speak on partyline.

## Removed eggdrop features
 * Modules
 * TCL
 * File area (filesys.mod)
 * Greet
 * userflag +f (friend)
 * userflag +t (botmaster)
 * userflag +a (auto-op, replaced by +O)
 * Misc. channel flags
 * Blowfish
 * Assoc.mod
 * Seen.mod
