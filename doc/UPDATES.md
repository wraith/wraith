# master
  * Require C++11 compiler support (G++ 4.7+, clang32+)
  * Wraith now automatically assigns roles to bots for channels, no longer
    requiring manually assigning them with flags +flry for flood, limit,
    resolve, auto-voice, auto-op. These roles are decentralized and per-chan
    such that net-splits and botnet-splits and multiple groups in 1 chan
    will properly assign roles out to bots to not cause overlap. Only leaf
    bots know which bots have which roles. (#39)
  * Add cmd_roles (leaf only) to display roles for a channel. (#39)

# maint
  * Fix various compile warnings and spam
  * Fix ptrace detection on OpenBSD (after 1.4.6 regression for the Linux fix)
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Fix command executions.
  * Fix OpenSSL 1.1 build (API) and forward-runtime (ABI) compatibility. (#116)
  * Properly honor exemptions when kicking matched RBL clients
  * Fix LASTON not being shared

# 1.4.8
  * Support Debian/Ubuntu's libssl1.0.0[:i386] package.
  * Improve hints about OpenSSL library packages to install.
  * Fix AddressSanitizer [1] being enabled by default causing 16TB+ of VM to be
    used.  On most systems this was harmless since the memory was not actually
    used.  This feature is still enabled by default for debug binaries.
    [1] http://clang.llvm.org/docs/AddressSanitizer.html
  * Fix './wraith -C' file being immediately modified when saving on
    FreeBSD (#94)
  * Support FreeBSD closefrom(2)
  * Use vfork(2) in some places

# 1.4.7
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Fix Linux binary compat on FreeBSD due to lack of ptrace(2).
  * Avoid warnings from Debian's FORTIFY_SOURCE
  * Remove an old +take limiter that was forgotten.
  * Use Linux's prctl(PR_SET_DUMPABLE) to disable core dumps and ptrace(2).
  * Use FreeBSD 10's procctl(PROC_TRACE_CTL) to disable core dumps and tracing.
  * Fix binary compat issue causing ptrace permission errors on Linux 3.4+
  * Fix ban/exempt/invite masking not working with 10-char idents.
  * Fallback to ISON if the server falsely claims to support MONITOR.
  * Fix bot not auto-opping after just connecting.
  * Fix invites not being applied in -dynamicinvites channels when +i is set.
  * Fix not handling auto-op in minutely channel rechecks.
  * Fix auto-voice and auto-op not applying after a nick change.
  * Don't truncate bot's join time on .reset.
  * Fix various small memory leaks.
  * Fix case where .[bot]set would not share to new bots until their localhub
    was relinked.
  * Raise netsplit timeout to 1000 seconds and allow it to be configured with
    'set wait-split' (#60).
  * Indent BOTNET entries some in .whois.
  * Disallow negative 'chanset limit' (#96).
  * Raise server cycle time from 15 to 30 seconds and add
    'set server-cycle-wait for configuring it. (#81)
  * Show reason when initiating FiSH Key exchange
  * Auto initiate FiSH key exchange (with fish-auto=1) when invalid message
    received. (Invalid or unknown key) (#74)
  * Show more clear error on Centos 7 that static-libstdc++ is required,
    rather than an obscure Libcrypto error.
  * Restrict 'chanset groups' to owners.
  * Stop building the binary as i486.  Let it use modern x86/x86_64.
  * Stop trying to regain jupenick when it is unavailable and main nick is
    temporarily juped (#101).
  * Fix bots not tracking groups for other bots.  This also fixes slowjoin
    with groups.
  * Fix bot forgetting its nick/jupenick during restart and reverting to
    botnick if restarting in the middle of a server connect or attempted
    NICK change that fails.
  * Fix some various internal caching issues.

# 1.4.6
  * Disable demo TCL support by default to prevent confusion during build.
  * Avoid apparmor ptrace(2) warnings on Ubuntu
  * Allow 'set trace ignore' again. Undoes a change from 2005.
  * Fix ptrace detection on Linux
  * Fix broken startup with GCC from ports on FreeBSD.
  * Fix simulated terminal mode (-nt) defaulting to all console flags.
  * Fix simulated terminal mode (-nt) not having color supported.
  * Fix race between DCC and terminal output which could result in
    bolds being backwards (#86).
  * Fix TCL warnings during build if TCL is not being used.
  * Rework the build to parallelize better and rebuild some things less often.
  * GCC: Always link libstdc++ statically. This avoids distributing
    a binary that would rely on having a specific compiler version installed
    and avoids issues when updating the local compiler. This also fixes
    building a binary with GCC on FreeBSD and using it on a clang-only system.
  * Fix conversion of command passes from ancient 1.3 format.

# 1.4.5
  * Remove ahbl as it now positively identifies all hosts as abusive
    http://www.ahbl.org/content/last-notice-wildcarding-services-jan-1st

# 1.4.4
  * Read in /etc/hosts on startup
  * Fix 'bl %group cmd' giving incorrect message when group/bots not found.
  * Fix cmd_slowjoin eating the first channel option (This fixes groups support)
  * Fix help for 'topic' to show optional channel argument
  * Fix cmd_hublevel not properly requiring a hublevel argument
  * Fix build with clang (FreeBSD 10)
  * Fix startup crash when some shared library symbols were missing
  * Fix build on systems without working libssp
  * Fix build detecting invalid openssl installations (ones with only static libs)
  * Give hint on bot when disconnected from hub for reason
  * Fix cmd_netnick formatting issue with offline bots
  * Fix crash when built with clang (FreeBSD)
  * Binary now uses libelf internally for handling conf data.
  * Bot responses are now clear and nice. Recompile with your own list if you want
    in doc/responses.txt
  * Remove channel limit when limitraise is disabled (#77)
  * 'fork-interval' removed.
  * Build with -fstack-protector by default
  * Remove 60 process limit

# 1.4.3
  * Default 'set promisc' to ignore since it's usually a false positive
    and doesn't matter much.
  * Bots linking in must now have a matching host on their user to succeed linking.
    This was always requiring a valid host, but was not restricted to that bot.
  * Fix leaf bots being able to initiate userfile transfer to hubs
  * Protect binary updates so that only hubs can offer/send them
  * Fix backwards compatibility with older 1.2/1.3 bots
  * Fix vulnerability in linking

# 1.4.2
  * Prevent crashing on startup if openssl can not be loaded
  * Rename option -c to -m for 'manual' edit, and redirect -c to -C
  * Show a nicer compile error when BUILDTS unable to be determined / gmake is not used.
  * Show 'Do!' in 'chaninfo' for 'Voice-moderate' to be consistent with other flags.
  * Fix CALLERID not respecting ignores (#63)
  * When using msg OP, check for and allow '#chan PASS' as well as 'PASS #chan'
  * Warn when using 'chattr +a' as some users may expect it to be auto-op (#58)
  * Really fix CAPS/COLOR flood detection not obeying x|x flags
  * Support TCL 8.6 in the beta libtcl support (no scripting support yet)
  * Fix hub startup not respecting changed HUB lines in config
  * Fix './wraith -V' not showing the updated HUB info

# 1.4.1
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Remove 'chanset +meankicks'. You can customize your kicks in doc/responses.txt and recompile.
  * Don't allow running as root
  * Don't allow more than 5 bots per binary.
  * Fix CAPS/COLOR flood detection not obeying x|x flags
  * Fix OPs engaging the mass flood protection
  * Fix +bitch not working under some conditions
  * Fix "+d virus" causing bots to enforce deop anyone who ops someone who opped a +d user
  * Fix compiling without TCL support (#50)
  * Reading in pack.cfg will now do some sanity checks
  * Add better explanation when rejecting botlink due to not being +o (#51)

# 1.4.0
  * Updated server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Change +bitch reaction to use normal queue for deopping opper/opped clients
  * Fix +take clearing -fastop (fixes #371)
  * Added botbitch to mmode (fixes #139)
  * Added SSL support
    This includes new set options: 'servers-ssl', 'servers6-ssl', 'server-port-ssl', 'server-use-ssl'
  * libcrypto (openssl) is now loaded at startup and is required.
  * Added TCL support. This is *only* a .tcl command currently, no scripts are loadable yet.
  * Fix blowfish not working correctly on 64bit
  * Suicide will now remove all bots related to the binary being removed (fixes #435)
  * FiSH message support added.
    * FiSH support for DH1080 key-exchange. 'keyx' command added to start from bot, and responds to key-exchanges.
    * Auto FiSH key-exchange when accepting users via callerid (controllable with set 'fish-auto-keyx')
    * Automatic re-key exchange after every message to avoid replay attacks (controllable with set 'fish-paranoid')
    * Set FiSH key via cmd_setkey and 'chanset fish-key'
    * Bot expires key exchange if there's no response in 7 seconds.
    * Bot expires key-exchanged keys after 60 minutes.
  * When 'mdop' protection is on, re-op all previously opped clients automatically.
  * When 'mop' protection is on, deop all previously regular clients automatically.
  * Add './wraith -V' which will display the packconfig that the bot is using.
  * Add cmd_newhub for adding a new hub. All bots will add this to their config.
  * Hubs can now be overridden inside the botconfig (-C)
  * Optimize userfile writing by doing it asynchronously
  * Groups support added. See: http://wraith.botpack.net/wiki/Groups
    * Add [bot]set var 'groups' to configure what groups bots are in
    * Add chanset 'groups' to take a list of groups that should join. 'chanset #chan groups { main backup }'
    * Added group support to 'botcmd': 'botcmd [*|&|?] %group cmd'
    * 'botjoin' and 'botpart' have been removed. Just use .+chan, .chanset, .botset to control groups.
    * Add command 'groups' to list all groups and which bots are in them.
    * Command 'channels' now accepts a '%group' param to show which channels a group are in.
    * Add '%group' support to 'bots' command.
  * Improve output of 'bots' command by displaying and sorting nodename
  * Minus chrec if .chattr set no flags
  * Added 'chanset +floodban' to ban clients that violate 'flood-chan' and 'flood-ctcp'
  * Bots now auto-reop other bots that get deopped
  * Bots now ask for ops quicker if another bot is opped
  * Bots now request/join +ilk channels much quicker
  * Add 'chanset revenge' which will react/kick/ban/remove users who kick/ban/deop bots.
  * Fix flood kicking not properly tracking multiple clients at once (#43)
  * Add chanset 'flood-bytes' to count how many bytes:second a user sends before getting kicked for flood. (#42)
  * Bot will now lockdown channel (+im) if banlist becomes full (#37)
  * Bot will now lockdown channel (+im) if a drone flood is detected (#37)
    * Add chansets 'flood-mchan', 'flood-mbytes', 'flood-mctcp' to control reactions to mass floods (#37)
  * Remove unneeded chanset 'nomassjoin'
  * Add chanset 'caps-limit' to handle % of message that can be in caps before kick (#8)
  * Add chanset 'color-limit' to handle how many color codes are allowed in a message before kick (#8)
  * Add chanset 'closed-exempt' which will allow exempting non-users (who are opped or voice) from being kicked. (fixes #171)
  * Tweak limit range checking such that bot will set limit less often
  * Bots now again respect devoices made by other bots and will not revoice those users.
  * Add chanset 'voice-moderate' which will auto set +m in '+voice' channel and devoice flooding clients instead of kick. (#48)
  * Bots now will enforce devoices made by ops (user +o) and allow any op (user +o) to revoice to disable that enforcement.
    This used to use +m to enforce/allowed revoicing.
  * Bots will now remember a client's flood/devoice settings if they cycle (#49)

# 1.3.4
  * Fix various compile warnings with newer GCC
  * Fix bot getting confused when changing to long nicks
  * Fix case where nick would rotate to NICK1 when already on NICK2 and NICK was unavailable
  * Documentation cleanups
  * Fixed 'request' kick msg with cmd_kick
  * Enforce cmd_invite syntax
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.

# 1.3.3
  * Fix --disable-ipv6 compiling
  * Update cmd_mop to support console channel (so it works via Auth commands better)
  * Add an extra 2 second delay before releasing nick to aide in syncing and time to type /nick
  * When a nick is released, start regaining as soon as it is witnessed to have been taken again
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * cmd_mns_user now accepts multiple users (fixes #77)
  * Permanent owners can no longer be removed via cmd_mns_user
  * Fix various places incorrectly truncating passwords at 15 characters
  * Fix BSDmakefile not being included in distribution package

# 1.3.2
  * Misc bug fixes
  * 'make' on BSD will now redirect to gmake.
  * Fix partyline cmds for |m and |o users who do not have console set correctly. (.adduser)
  * Allow simulated terminal (HQ) user to access owners (ie, .chpass owner)
  * Fix 'mop' causing a mass deop when -bitch
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Don't kick friendly bots that are desynced: op them.
  * Wraith is now linked dynamically. This should improve binary compat.
    Compile on the oldest OS you want to support, and ensure newer ones have
    all necessary compat libraries.

# 1.3.1
  * Fix crash related to slowpart
  * Fixed parsing errors in .chanset
  * Only global +n can use .chanset * now
  * cmd_die no longer puts user's nick into the quit msg.
  * Update rbl list, 'set -yes rbl-servers -' to get new list.
  * Update server list, 'set -yes servers -' and 'set -yes servers6 -' to get new list.
  * Fix default compile so it doesn't use a ton of processes.
  * Fix bot getting killed on OpenBSD 4.7 on startup
  * Ignore 'pflog' interfaces in promisc
  * Fix buffer overflow in DCC
  * Fix problems with changing IP
  * Fix bot going zombie if it jumps during a restart
  * Fix auth IRC lookups getting the wrong cached idx
  * Fix cmd_relay so it does not use IPv6 if not supported.
  * Fix bot linking so it does not use IPv6 if not supported.

# 1.3
  * Binary / shell / startup changes
    * Binary error messages are no longer obscure numbers or fake segfaults. (Compile with OBSCURE_ERRORS to re-enable)
    * Binary pass prompt has been changed to not be an obscure 'bash$' prompt anymore.
    * Removed pscloak feature, it was too malicious, pointless and stupid.
    * Fix problem of upgrading with uninitialized binaries causing corruption (will kick in on future upgrades)
    * Optimized how some shell operations are executed.
    * Uname checking (and associated emails) have been removed.
    * Email functionality has been removed.
    * Fix crash on OpenBSD due to mmap(2) bug.
    * Removed gethostbyname() dependency. This fixes Linux binary compatibility issues / crashes on startup.
  * Cookie op changes
    * Cookie op algorithm changed again, they can no longer be easily reused (fixes #402) Thank you mulder.
    * Cookies no longer fail if the opping bot changes nick after queueing the mode.
    * Cookie time window increased, false-positives from lag/netsplits should be completely eliminated now.
    * Auto disable cookies on ircu(undernet) and Unreal servers as they do not support unbanning nonexistant bans.
  * DNS changes
    * More DNS errors are now handled which fixes some misc DNS issues.
    * DNS now works on servers which tend to only return partial results (fixes #424)
    * Lower DNS lookup timeout from 40 seconds to 10 seconds.
    * Use random query ids for DNS lookups.
    * Remove cox.net nameservers from backup list.
    * Add Google DNS servers as backups: 8.8.8.8, 8.8.4.4 (http://code.google.com/speed/public-dns/)
    * If a .resolv.conf exists, only use the nameservers listed in it.
    * Fix problem of bots never reconnecting to hub after being up for long periods. (Was a DNS issue)
    * Bot no longer connects to IRC over IPv6 if it was specifically given an IPv4 IP.
  * Botlink
    * Bots now link to the first bot in their binary (the localhub) over a UNIX domain socket. Only the localhub connects to the main hubs.
    * Bots must have a matching host to be able to link now.
    * Telnet/DCC now checks both forward/reverse to ensure they match. (fixes #19)
    * Fix botlink problems when hub and leaf nicks are long.
    * Only use oident for server connects, not for bot linking.
  * Compiling changes
    * Prefer g++ 4.4.1 when compiling.
    * No longer bundling Openssl code; It is now required to be installed to compile.
    * build.sh cleanups.
    * Now including bdlib.
    * Enable SSP for debug binary/testing.
  * PackConfig changes
    * PackConfig can now be securely read over STDIN. Use ./wraith -Q, then paste it in.
    * Binary will no longer generate SALTS if missing from the PackConfig. This was causing too much confusion.
    * SHELLHASH renamed to BINARYPASS (SHELLHASH is still supported)
    * BINARYPASS and Owner passwords in the PackConfig can now be salted-sha1. '+' + RAND(5) + '$' + SHA1(RAND(5) + 'password')
    * BINARYPASS may no longer be an MD5 Hash.
  * Userfile / sharing changes
    * Userfile transfer is now made over botlink (no extra ports needed) (works behind NAT/firewall)
    * New userfile pass algorithm (salted-sha1)
    * New userfile cmdpass algorithm (salted-sha1)
    * New userfile storage format.
    * Raise Handle length up to 32 characters.
  * cmd_set fixes
    * Sanitize cmd_set input locally as well as remotely (fixes #273)
    * Ensure that some set variables are only 1 word long.
    * Fix cmd_botset not setting default/global value on leaf bots when clearing a var (fixes #389)
  * Misc fixes
    * All existing compile errors fixed.
    * Cleanup +r implementation a bit to lookup users more often than only on join. (fixes #127)
    * Bot now properly reacts to IP bans if it is +r.
    * If a user does not have a console setting, they will be given default settings on login.
    * Fix a case where the temporary file was not cleaned up when exiting config editor (-C) (fixes #428)
    * Fix cmd_slowpart issues.
    * Fix cmd_pls_chan causing bots to reset a channels settings when it already existed.
    * Fix password length discrepencies / off-by-1 problems in cmd_chpass and cmd_nopass (fixes #436)
    * Fix botcmd to make sure that results are delivered only to the requester (fixes #208)
    * Fix issues with server-port not being updated to the current server list (fixes #176)
    * Fix aliases not properly indicating bad command on leaf bots (fixes #297)
    * Fix (+|-)(ban|exempt|invite) commands to properly handle if the channel is given first so that a #chan!*@* ban is not created.
    * Fix chanint 'flood-exempt' not exempting ops when set to 'voice'.
    * Fix segfault with /KNOCK on csircd.
    * Fix restart causing bot to change nick (this was a regression in jupenick)
    * Fix cmd_slowjoin not working on backup bots correctly.
    * Fix a crash from long hubnicks.
    * Fix +botbitch being very slow and inefficient
    * Better +bitch/+botbitch/+closed handling when in floodless mode.
    * Bot now attempts to join a channel immediately on invite.
    * Fix bot not setting 'chanmode' channel modes when getting opped.
    * Fix bot forgetting server uptime on restart
    * Fix '+user user' with no hostmask adding the hostmask '*!*@' to the user.
    * Fix case where bot would not get the +e and +I lists when getting opped.
    * If a bot devoices a user in a +voice channel, the +y bot will no longer enforce this.
    * Fix cmd_botjoin not properly setting up addedby/addedts for the channel.
    * Fix bots never timing out when connecting to servers sometimes.
    * Bot no longer tries opening an identd socket unless it's root or running on cygwin.
    * Fix crash when setting a bot +B
    * Fix crash and lockup in cmd_bottree
  * Misc changes
    * Cleanup DCC/Telnet listening so bot only listens on IPV6 if it was specifically given an IPV6 IP.
    * CTCP bot CHAT will no longer work if the bot does not have an IPV4 IP set.
    * Add cmd 'release', 'netrelease' and msg-release for releasing a bot's jupenick on a 7 second timer.
    * Add MONITOR support. This speeds up NICK grabbing. (fixes #320)
    * Add set 'deaf' to run bot in DEAF mode on supporting IRCDs. This prevents channel conversations coming to the bot. On by default.
    * Add set 'callerid' to run bot in CALLERID mode (+g). +c bots will automatically accept messages from known users. (fixes #45)
    * Rename set 'flood-g' to 'flood-callerid', which only takes effect if the server supports CALLERID.
    * Relays are now encrypted between bots. Username is sent automatically. Just enter in pass now.
    * Added pseudo-channel 'default' which can be modified with 'chanset' and 'chaninfo'.
      This is used to modify what the default channel options will be for new channels.
    * Removed 'set chanset' as it has been replaced by 'chanset default'
    * Add new channel setting 'ban-type' which support mIRC style ban types (adapted from recent eggdrop patch)
    * EFnet server list updates (To update yours: .set -YES servers - | .set -YES servers6 -)
    * Default alias updates (To update yours: .set -YES alias - ) 'cq' added as an alias to 'clearqueue play' to stop .play
    * EFnet's CSIRCD servers have been removed as they have proven to be highly incompatible with Ratbox features. (irc.nac.net/irc.efnet.no)
    * Give localhubs credit for adding their child bots in 'whois'.
    * Ident lookup timeout is now 5 seconds (will speed up linking / partyline connecting)
    * Cleanup all string parsing to trim excess whitespaces (fixes #268)
    * Cleanup cmd_botcmd restrictions/sanity checks.
    * RBL Checking: Add 'chanset +rbl' which uses list of servers from 'set rbl-servers'. Only +r bots will enforce this.
    * Removed 'chanset +knock' and replaced with 'chanset knock (Op|Voice|User)', see 'help chaninfo'.
    * Default log timestamp now includes seconds.
    * Hubs no longer include their own timestamp over botnet.
    * Floodbots (+f) and Resolvbots (+r) are now listed in cmd_userlist
    * Floodbots (+f) will automatically unDEAF themselves to they can monitor the channel for floods (namely flood-chan).
      Note that this is done about 10 seconds after adding +f to a bot. Removing +f will set DEAF again.
    * This auto DEAF/UNDEAF works if you auth with the bot and it supports channel cmds
    * Added SHA256 support (cmd_sha256, Auth cmd: sha256)
    * Add cmd_encrypt_fish and cmd_decrypt_fish which uses eggdrop's blowfish (same as FiSH)
    * Add cmd_hash which returns the MD5, SHA1 and SHA256 of the given string.
    * Credits/cmd_about updates.
    * Add chanset '+/-voicebitch' for a +bitch style enforcement over voices. (fixes #381)
    * Add chanset '+/-protect' which will set +botbitch and mass deop if any takeover is detected.
    * Add chanset 'protect-backup' which will optionally set +backup in the event of a takeover with +protect.
    * Channel settings 'mop' and 'mdop' now react to non-users.
    * set 'fight-threshold' now requires +protect and is a trigger for +protect. See 'help chanset' for +protect info.
    * Bots no longer cycle channels when restarting/upgrading. (This will take effect AFTER upgrading to 1.3) (fixes #187)
    * Speedup irc server queue by bursting more often and fully. Thanks VXP.
    * Add set 'msgrate' to define how often to dequeue to the server, if non-ratbox.
    * Add set 'msgburst' to define how many commands to burst to server per msgrate.
    * On hybrid/ratbox servers, burst some commands on connect
    * On ratbox type servers, use an optimized queue for better throughput on the queue.
    * Add cmd_play for playing files to irc. (ASCII art gallery exhibition)
    * Utilize CPRIVMSG/CNOTICE if available.
    * Chain WHO requests to try avoiding Max SendQ
    * More Auth channel commands are now available. Auth up and run +help to see them all. (fixes #2)
    * Add debug buffer output on crash - please give to bryan if it ever comes up.
    * Remove hardcoded -bot alias and move to 'set alias'. Also add a '+bot' alias to 'newleaf'

# 1.2.16.1
  * Fix linux compile errors

# 1.2.16
  * Add 'set altchars' so that alternative characters used for nicks can be changed. (fixes #418)
  * Fix channels added by cmd_slowjoin not having the user who added them associated with the channel.
  * Removed BDHASH as it wasn't even used.
  * SHELLHASH now supports SHA1 hashes.
  * Fix segfault from receiving truncated DNS replies.
  * Remove 'set mean-kicks' and change to 'chanset +meankicks' (default on, does not upgrade from old .set, must unset after upgrade if wanted)
  * Don't check last/promisc unless linked.
  * Speed up botnet parsing / set lookups /cmd_help a bit
  * Fix some various issues with initial setups and new PackConfig format
  * Greatly sped up binary startup - lower resource usage for cron checks, etc.
  * checkchannels now displays server. (fixes #420)
  * Added 'jupenick'. Jupenick is preferred over 'nick' but only 'nick' will be rotated with altchars. Ie, nick_, nick-, etc. (fixes #421)
  * Added 'link_cleartext' which allows disabling of cleartext bot linking (needed for upgrades)
  * Fixed bug where salts were not written to binary when first generating them
  * Fixed countless buffer overflows. (fixes #226)
  * Fixed a rare segfault on userfile transfer timeouts
  * Fixed bots getting confused about their hub/uplink after userfile transfer fails (fixes #61)
  * Fixed cmd_handle not properly bounds checking max handlen. (fixes #305)
  * Bans are now only retrieved from server after being opped. (addresses #406)
  * Update efnet server list and add new alias 'bs' for botset. (To get new defaults: 'set -YES servers -' and 'set -YES alias -')
  * Fix /ctcp FINGER leaking shell username when using oidentd
  * Fix ISON not working on some ircds.
  * Fix a socket leak from timed out userfile transfers.
  * Fixed bot dying off when it can't create a temporary file. (fixes #412)
  * Show connection time in cmd_(net|bot)server now.
  * Fix a segfault in the settings handling code. (Could create very random looking segfaults)
  * Disable bot +f as all it did was cause a segfault. The flood code is unfinished. (fixes #429)
  * Fix cmd_slowjoin not counting bots correctly. (fixes #431)
  * Added telnet 'tcl expect' script in scripts/telnet.exp. Can be used for easier hub management over telnet from shell.
  * Fix 'cmd_rehash' problems on hubs.
  * Fix 'HQ' user getting wiped in -t mode resulting in a crash. (fixes #298)
  * Fix bots linking to the wrong hub not giving a good reason why they failed in the log. (fixes #369)
  * Fix segfault on some ircds after restart due to them not giving 005 information on VERSION. (fixes #378)

# 1.2.15.1
  * Fix leaf bots not updating behind other hubs (fixes #419)

# 1.2.15
  * Fix a possible segfault when binaries compiled wrong
  * Fix a segfault in op cookies during a desync
  * Remove cmd_netmsg and restrict cmd_msg so it cannot be used from botcmd.
  * Fixed '+user' so that hostmasks are sanity checked. Ie, (test@test.com is added as *!test@test.com)
  * Add links to FAQ/new site in cmd_help/cmd_about
  * Cleanup some ctcp responses
  * Add more ctcp version responses (mirc scripts), and some others, such as irssi/xchat/snak/cgi::irc
  * Add more 'responses' (ie, kicks reasons)
  * Added new chanset flag '+knock' which will make +y bots auto invite USERS. (No flag restrictions currently)
  * Bots now check their own hostmask before opping or requesting assistance to join a channel
  * Fix bans not being removed from channels when removed from bot. (fixes #352)
  * Dont show portmin/portmax/pscloak/autouname/datadir/autocron in binary config unless they are not set to the default
  * Fix possible situation where an error while saving userfile is not reported (fixes #287)
  * Fix a bug where some nets did not save userfile correctly
  * Fix a case where 'botcmd ?' would loop forever when a hub had a 1 character handle
  * Change default realname to mimic bitchx
  * Make the 'detect login' use less shell resources
  * Fix bot hosts/users being cached internally as wrong, even when a new host is added
  * Fix segfault in cmd_mmode (fixes #399)
  * Fix segfault from receiving from non-recursive dns servers. (fixes #401)
  * Fix some cases where the bot would pointlessly do WHO on users instead of on the channel to resolve a desync.
  * Fix the bot not starting up when it is not allowed to run ptrace() (FreeBSD: security.bsd.unprivileged_proc_debug=0)
  * Misc help cleanups
  * Show kline reasons on connect to servers. (fixes #400)
  * Fix case where binpath/datadir were not updated correctly when homedir changed. (fixes #169)
  * For security purposes the following commands are now hub-only: botnick, botserver, botversion, botmsg, netnick, netserver, netversion
  * Added cmd_nick to display leaf's current nick
  * Change default ip/host/ip6 to a * in the binary config
  * Silently fix user mistake of giving hostname in ip field in binary config
  * Fix datadir being expanded to full path (fixes #405)
  * Bots can now be started as ./binary bot.conf instead of ./binary bot. (Disable autocron and manually crontab it to start like this)
  * Fix a very rare segfault when closing the identd socket (fixes #410)

# 1.2.14
  * Fix another bug in shell functions. (fixes #321)
  * Fix some bugs with bot flags. (fixes #334 and #337 and #344)
  * Remove the notes system and related commands. (fixes #275)
  * Fix ./binary -C not signalling the correct process under some situations. (actually fixes #315)
  * Fix bots being removed from binary not being killed. (fixes #338)
  * Fix typo in help for 'set' (fixes #342)
  * Fix typo in help for 'botset' in regards to wildcards. (fixes #365)
  * Fix cmd_swhois stripping multiple channel modes. (fixes #348)
  * Fix processing/killing/signalling of possible new localhub when editing binary.
  * The socket file generated for restarting is now encrypted.
  * Delay 'op-requests'.time seconds before re-requesting op on opeless channels.
  * Fix some issues with case with pid files, nicks, conf loading. (This will break some bots upgrading to 1.2.14)
  * Fix bot not reconnecting with new ip/host when -HUPd. (fixes #340)
  * All bots are now rehashed after editing binary - nick casing, ip changes are taken into effect.
  * Bots now use the shell username instead of botnick when ident does not work.
  * Localhubs now auto add *!~username@... hosts instead of *!~nick@... hosts for their leafs.
  * Make bot chmod() itself when accessing its binary.
  * Fix bots sharing wrong channel keys over botnet.
  * Cmd_swhois now displays a list of only 'public' channels as well.
  * Fix cmd_mns_user being ran on self-bot. (fixes #350)
  * Fix cmd_checkchannels not taking slowjoin into account.
  * Fix cmd_swhois returning results after a server jump.
  * Fix bots not connecting to their uplink.
  * Add ghost-inspired feature: backup bots. Bots marked +B will only join channels marked +backup.
  * Fixed cmd_nohelp (for viewing undocumented cmds - mainly debugging cmds)
  * cmd_adduser sends a PRIVMSG now instead of a NOTICE
  * Rewrote op-cookies to be fix a security hole. (They also react to '.set hijack' now)
  * Fixed +voice flaw where users in channels that were +voice AND +private would only be voiced with |v. (fixes #54)
  * Added cmd_clearhosts
  * Added 'whois restrictions' to more cmds.
  * Add channel flags 'nomassjoin' and 'flood-mjoin' to set +im on a timer during join floods (drones)
  * Add BOT flag +f for handling cpu intensive floods (none supported yet)
  * Add chanset 'flood-exempt' to exempt ops/voices from flood controls (see 'help chaninfo')
  * Add chanset 'flood-lock-time' to control how long to keep the channel locked during drone floods.
  * chanset flood-join now also counts PARTs as flooding.
  * Fix segfault in cmd_channel during splits.
  * Remove chanflag 'protectops' and 'idle-kick'
  * Fix bot not checking +e/+I modes when opped sometimes.
  * Fix chanset flag 'userbans' not being saved correctly. (may cause warnings on upgrade)
  * Fix authing on non-chathubs (fixes #356)
  * Fix segfault with cmd_set and adding to empty lists.
  * Cleaned up some bugs in the 'getin' system.
  * Add '$n' expansion into realname. (fixes #361)
  * Reformat CTCP logging. (fixes #358)
  * Cleanup cmd_userlist to list hubs/leafs/backups (fixes #363)
  * Bots will now cycle nicks on servers which claim their nick is invalid. (fixes #311)
  * Limit bots no longer set limit when 'chanmode -l' is set. (fixes #294)
  * Fixed +closed from interfering with 'chanmode -pi'. (fixes #294)
  * Fix misc bugs with cmd_mop. (fixes #313 and #392)
  * Bad uname EMAILS now include the localhub's nick. (fixes #354)
  * -host now accepts partial host masks, ie, '-host user@host'.
  * Rewrote cmd_mdop into cmd_mmode. See 'help mmode' on a leaf bot.
  * Fixed bots having problems with kick/bans on undernet.
  * Added oidentd support. set 'oidentd' on makes bot spoof as BOTNICK.
  * Made 'ident-botnick' variable to decide to send username vs botnick for non-ident.
  * Default servers/servers6/alias list updated ('set -YES var -' to use it)
  * Fix MODE parsing bug
  * Bot no longer requires (or uses) '-B' to spawn bots. (now: ./binary <botnick>)
  * Fixed +take opping the wrong clients due to desync (+take now sets +bitch)
  * Fix cmd/dcc prefix leaking to -p users (when chat is off) (fixes #376)
  * Fix checking for flood on hosts which are already ignored. (fixes #343)
  * Added OSVER entry for bots in .whois
  * Userfile transfers now use a random filename instead of .share.botnick.users.timestamp
  * Fix ambiguous warning when a bot tries linking after being added with +user (fixes #383)
  * Add set 'irc-autoaway' to be able to disable autoaway feature. (addresses #380)
  * Fix an automatic hostmask bug (fixes #339)
  * Bots no longer die when receiving a corrupt userfile under some conditions
  * Delay for auto-op/voice (+O, +v) and +voice can now be set with 'chanset auto-delay' (default 5 seconds)
  * Update system now uses revisions instead of build timestamp
  * Linux 2.2, Linux 2.4, and FreeBSD 4 are no longer supported.

# 1.2.13
  * Fix cmd_chanset accepting invalid flags
  * Fix Auth system not tracking nick changes correctly. (fixes #318)
  * Fix set not allowing defaults under the lower limit. (addresses #315)
  * For ./binary -C prefer environment variable EDITOR over VISUAL.
  * Fix ./binary -C not signalling the correct process under some situations. (fixes #315)
  * Fix security hole in all shell cmds. (fixes #321)
  * Make ./binary -E default to displaying every error. (fixes #322)
  * Fix a possible memory leak with DNS. (fixes #296)
  * cmd_[bot]set now accepts wildcards (*) for listing variables. (not for setting)
  * Added set var 'usermode' for IRC usermode to set on connect/rehash.
  * Added cmd_umode (+m) to manually set bots' usermodes at will.
  * Fix botnicks becoming lower case after being opped.
  * Minor documentation change for cmd_newleaf. (fixes #291)
  * Fix cmd_chattr accepting invalid flags. (fixes #293)
  * See 'help whois' as some flags are now user, bot, chan, global only.
  * Fix cmd_console not working for +private channels correctly (fixes #325)
  * -user on a bot now works on linked bots. (fixes #200)
  * Fix cmd_link being used to link to non-hubs. (fixes #329)
  * Fix cmd_mns_chan not sharing console changes. (fixes #328)
  * Fix many instances of hub nicks leaking to leaf bots. (fixes #326)
  * Fix bot not checking if it can access datadir on startup. (fixes #330)

# 1.2.12
  * Clearing a variable via 'set var -' now resets that variable to default settings. (implements #111)
  * Fix bug introduced from fixing #274
  * Fixed overflow with cmd_set (fixes #289)
  * cmd_set now sanitizes input with ranges/limits. (fixes #289)
  * cmd_set now accepts 'true', 'on', 'false' and 'off' for boolean variables.
  * Added somewhat advanced password scoring to replace current password restrictions. (#56)
  * Added build os/architechture to output for ./binary -v
  * Added ARCH entry for bots in .whois
  * Added a new error code (22) which displays if attempting to run binary before using -C.
  * cmd_swhois is now -|- and hides all restricted information, channels; see 'help swhois'.
  * cmd_mop no longer defaults to console channel if no channel is specified. (fixes #302)
  * Fix cmds: bot[jump|server|version|msg] not running locally correctly. (fixes #304)
  * Fix cmd_chinfo to use the standard 'whois restrictions' (fixes #303)
  * Fix ',' character being allowed with 'set list' functions. (fixes #308)
  * Make 'set list' check to see if an item exists or not before add/remove. (fixes #307)
  * Fix problems with alias rewriting. (fixes #306)
  * Fix bots trying to boot virtual user created via cmd_botcmd.

# 1.2.11
  * Fixed typo in help for .set/.conf/.channel
  * Fixed a bug in cmd_chattr with |m users. (fixes #267)
  * Suicide now removes userfile backups, most pid files, and crontab entries. (Fixes #125)
  * Fixed removed bots not being automatically unlinked from botnet. (fixes #254)
  * Aliases may no longer reference other aliases (only checked when used) (fixes #244)
  * Fixed cmd_checkchannels so that results are not shown in log, but to user only. (fixes #217)
  * Disabled watcher feature.
  * Fixed problems with symlinked binaries. (fixes #240)
  * Fixed CIDR masks not being checked on JOIN. (also fixes some other cases of CIDR matching) (fixes #270)
  * Fixed a segfault from removing bots. (fixes #271)
  * Fix /MSG invite not showing proper cmd for usage. (Fixes #278)
  * Fix cmdpass processing on ambiguous cmds. (fixes #279)
  * Fix ban/invite/exempt removal flaw. (fixes #280)
  * Clean and speedup ban/exempt/invite processing. (addresses #277)
  * Fix processing of localhubs when binary is editted. (fixes #274)
  * Fix some minor bugs with cmd_conf. (fixes #284)
  * Made requested changes to cmd_dump to help clear up confusion. (fixes #269)
  * Fix improper notice when doing .set <var> -- (this actually shouldn't say 'not set') (fixes #272)
  * cmd_bans/exempts/invites [global] now displays a list of bans for ALL channels. (fixes #276)
  * Fix a segfault with ctcp-cloak. (fairly random) (fixes #285)
  * Fixed cmd_botset not making a bot grab a different capitalization of a nick. (fixes #283)
  * Hub nicks no longer show on leaf bots for partyline chat.
  * Hub bots can no longer be whois'd or match'd from leaf bots.
  * Disabled the process list checking.

# 1.2.10
  * Removed old references to '+/-manop' and '+/-nomop' for chaninfo in help file.
  * Fixed auth cmds not working in privmsg. (fixes #228)
  * Fixed a major security hole in cmdpass checking.
  * cmd_chhandle can no longer be used on bots. (fixes #224)
  * Revised help listing for 'auth-chan' in 'help set'. (fixes #229)
  * Fixed an invalid socket warning with failed relay. (fixes #216)
  * Fixed a potential dns socket leak. (might fix #230)
  * Fixed leaf bots sending color to hub bots still. (fixes #161)
  * Fixed cmd_trace to use 2 decimal places in display for seconds. (fixes #207)
  * Fixed cmd_trace not displaying hops correctly.
  * Expand symbolic links when searching for tempdirs. (fixes #210)
  * Fixed a segfault in cmd_rehash and revised help listing for it. (fixes #242)
  * Fixed cmd_set not saving the userfile when needed. (fixes #245)
  * Fixed cmd_set list removal bugs. (fixes #246)
  * Fixed chanmode -i conflicting with closed-invite 1. (fixes #249)
  * Fixed yet another .relay bug (YARB) (fixes #250)
  * cmd_checkchannels now states if a bot is not online. (fixes #239)
  * Added mIRC 6.17 version reply
  * Fixed empty version replies. (fixes #236)
  * Fixed a sharing segfault with an empty userfile. (fixes #243)
  * Hubs will now boot removed users/bots upon userfile reload. (addresses #243)
  * Fix typo in help file for 'botpart' (fixes #259)
  * Now supressing "port taken" error on hub startup.
  * Removed stop-nethack and associated flags. (fixes #219)
  * +bitch is now properly enforced on a netsplit return. (fixes #219)
  * Fixed a timer bug which could cause prolonged looping.
  * Removed some stale (backup) dns servers. (fixes #263)
  * Bot handles are now restricted to 9 characters. (fixes #253)
  * Fixed hubs not preserving uptime when restarted.
  * Fixed hub flaw in cmd_uptime.
  * Fixed auth cmd: help.

# 1.2.9
  * Fixed cmd_[un]stick not properly using numbers for channel masks. (#160)
  * Fixed binary spawning/updating to escape needed chars for shell. (#158)
  * Fixed a flaw in binary checksums.
  * Removed references to the old 'greet' features of eggdrop from help.
  * Removed/fixed references to the old 'config' to the new 'set' in help.
  * Fixed references to 'set manop/mop' to point to chanset instead.
  * Fixed tempdir checking to attempt to 'mkdir ~/.ssh/' for '~/.ssh/...' tempdir.
  * Fixed bot link getting colors. (fixes #161)
  * Fixed +r bots resolving users by resolved ip before host.
  * Fixed 2 socket leaks in the dns/socket code. (#167)
  * Fixed cmd_[bot]set showing an invalid example for using '-YES'
  * Fixed a mem leak on bot unlinks.
  * Fixed an error with msg-* displaying as msg* in 'help set'. (seinfeld)
  * Setting 'manop-warn' was only being used for 'OP #chan' not 'OP' alone.
  * Fixed channel notices not being parsed.
  * Took out all punish/revenge code, it's been disabled since 1.2.7 anyway.
  * Fixed bots giving up on joining juped channels (may be due to split) (#168)
  * Fixed cmdpass list getting desynced during userfile transfer. (#143)
  * Removed old compatability code for converting CONFIG->SET during userfile loading.
  * Fixed "un-allocated socket" errors when a relay fails.
  * Fixed var 'server-port' not being used correctly. (Now lists/saves/loads before servers)
  * Fixed a typo in msg_pass.
  * Failed relay no longer says failed link.
  * Fixed a socket protocol error.
  * Fixed a small error in help file for 'whom'.
  * Removed some legacy code from failed relays that attempts to connect to port+1, port+2, port+3... (might fix #177)
  * Fixed an invalid killsock error in sharing.
  * Fixed chanset 'flood-*' not working correctly. (#69)
  * Fixed cmd_rehash killing hubs. And revised help for 'rehash'. (#186)
  * Fixed a bug in the listen code, which caused a segfault in /ctcp chat (a few other places too) (#189)
  * Fixed botcmd using wrong user on remote bot after doing su'ing to another user. (#190)
  * Fixed botcmd -> su/quit causing remote bots to close random socket.
  * Fixed another cmd_whom bug which showed remote users from .botcmd. (#9)
  * Fixed returning from 'su' not returning user to correct channel. (ie, partyline)
  * Fixed cmds showing failed cmdpass. (#188)
  * The respository is now stored in subversion; binaries now hold a revision number.
  * cmd_botcmd <?> now displays which bot is chosen in the log. (#192)
  * There is now a 'datadir' option in the binary config. The tempdir is still automatically found. (#162)
  * The datadir defaults to './.../' replacing the old '~/.ssh/.../'
  * Fixed ACTION ctcp log msg to be more standard. (fixes #138)
  * Added set var 'notify-time' to change the interval for regaining nick. (fixes #182)
  * Fixed segfault caused by CHANFIX/server modes. (fixes #25)
  * Modified a kick to be less offensive (fixes #175)
  * Fixed bots not enforcing bans when exempts were removed. (fixes #193)
  * Updated CREDITS/cmd_about
  * Fixed some startup segfaults resulting from bugs in glibc.
  * Fixed bot going floodless when it wasn't granted it.
  * Fixed bots requesting op in chans they don't have access to. (fixes #196)
  * Fixed a startup segfault. (fixes #195)
  * Unsetting auth-prefix disables authing/cmds.
  * Fixed segfault in cmd_slowpart (#191)
  * Fixed a memory leak in the cookie checking functions.
  * Fixed some segfaults with tempfiles
  * Update binaries can now be stored in bins/ within the hub's binary directory.
  * If a tempdir suddenly becomes unavailable, a new one is now immediately sought out.

# 1.2.8
  * Fixed [bot]* cmds depending on case of botnicks.
  * Fixed hubs attempting to generate a ctcp version reply after 'cloak-script' was set.
  * Fixed some similar problems with servers/servers6/nick
  * Fixed a memleak in var_set_mem()
  * Now specifying what was removed from a list with set -listvar and set +listvar.
  * Fixed hub forgetting it's uplink.
  * Fixed hubs not correctly installing/checking crontab on startup. (#156)
  * Fixed hub segfaults (on Linux) with bot links. (#155)
  * Removing an ignore by number now displays which ignore was removed. (#126)
  * Added hub cmd 'checkchannels' which displays in loglevel misc (+o) which chans leaf bots aren't on. (#149)
  * Fixed 'chanset chanmode { -p }' conflicting with 'closed-private 1' (closed-private now sets to 0 if -p) (#14)

# 1.2.7
  * Forgot 'log_bad = 0;' to fix the '!' showing up for aliases.
  * When a user is removed and is on partyline, they are now booted. (#142)
  * Booting a su'd user now returns the user to their original connection. (Problem when removing +i)
  * (eggdrop1.6) Fixed bot not requesting topic after reseting channel.
  * Fixed hubs not updating NODENAME/OS/USERNAME, which broke updating on new nets.
  * Added var 'auth-chan' which allows disabling auth cmds in all channels.
  * Fixed being able to 'chsecpass' a bot.
  * Fixed cmd_chsecpass to use whois_access() (#120)
  * Added code to eat zombie processes minutely (shouldn't interfere with anything)
  * Fixed ignores not being used. (#147) (Breaks a fix to not display msgs after added to ignore for floods)
  * Fixed improper hub recognition in binary config parser. (#148)
  * Help for 'botcmd' was off/outdated
  * Fixed cmd_[un]stick not using "ban" by default for mask type. (#145)
  * Fixed default binary name always becoming "wraith" instead of original binary name.
  * Cleaned up trace detection code.
  * Fixed hang on running binary on some FreeBSD systems.
  * Fixed bots not correctly recognizing changes in botset to empty vars after restart/update. (#150)
  * Disabled chanset flags '+/-revenge' and '+/-revengebot' as they are 100% incompatable/untouched egg code.
  * Fixed an issue with cmd_pls_host not adding all hosts given, and 'hostmask already there' now displays WHICH host. (#153)
  * Fixed binary data parsing to strip trailing spaces for entries.
  * Removed support for encrypted link for bots older than 1.2.4
  * Implemented a hack to fix a bug with link encryptions.

# 1.2.6
  * (REVERTED FROM 1.2.3) Disabled all memory allocations after a segfault (Fixes CPU spinning)
    -This actually CAUSED cpu-spins after segfaults.
  * Disabled debug contexts as they were useless.
  * Fixed many channel cmds blocking access due to +botbitch. (#141)
  * Now using g++3.3[.6] as g++3.4 was causing spontaneous SEGFAULTS(see cpu-spin bug) on FreeBSD.
  * Fixed aliases not properly using cmdpass given. (#140)
  * Changed display for when a user uses an alias over partyline. (#116)
  * Related to above change, invalid cmds are logged in a different order now for users.
  * Fixed cloak-script not updating correctly.
  * Some fixes to slowjoin/slowpart were mixed up, fixed.

# 1.2.5
  * Fixed a segfault/cpu-spin with mode parsing. (#25)
  * Fixed a segfault/cpu-spin during WHO parsing, although, it could have occurred at any time. (#110)
  * Possibly fixed a random segfault/cpu-spin after linking/connecting to irc.
  * Fixed spaces being stripped in .botcmd reply.
  * Changed order of 'rehashed config data from binary' notice to appear before any bot/userfile changes.
  * 'localhub's now check minutely if their bots need added or hosts added to userlist.
  * Fixed bot username casing causing bad cookies. (disable cookies for upgrade)
  * Fixed binary config stating there was no change with -C under certain conditions.
  * Fixed binary config not decrypting data on rare occasions.
  * Fixed bots set +k being able to request ops.
  * Fixed bots asking for ops/invite when set to +d or +k.
  * Fixed delay of requesting op spamming logs with delay msgs.
  * Added a 5 second delay on requesting/checking ops when no bots are available to ask.
  * Fixed 16 bytes after packdata being cleared in memory, causing random memory corruption.
  * Fixed localhub attempting to spawn itself during binary rehash and cmd_conf
  * Fixed another fatal bug with cmd_conf, this should fix it for good.
  * Added var 'manop-warn' to allow disabling the new manop NOTICE warnings when msg opping.
  * Added irc.ptptech.com/efnet.port80.se to default servers/servers6, added irc.pte.hu, irc.efnet.fr to servers.
  * Fixed an issue with bot respdoning to TCM CTCP VERSIONS too slowly at times on connect. (#128)
  * Added syslog style repeated log detection for all logging.
  * Fixed cmd_net[ps|last] not working. (#130, #131)
  * Added note at end of display for cmd_match to try using cmd_matchbot
  * Fixed 'reset*' being blocked with 'botcmd *'
  * Added code to prevent cmd_botcmd from being chained over botnet.
  * Prevent use of cmd_botcmd for cmds: cmd_me, cmd_away, cmd_back
  * Fixed security flaw with 'auth-obscure', and improved: auth-obscure now gives a hash regardless of 'dccauth'.
  * Fixed the whois_access() function to not allow non-perm owners to see/access perm-owners.
  * cmd_chpass now uses the whois_access() function. (#122)
  * Updated CREDITS/cmd_about (#119)
  * Fixed wrong notice on partyline when a user with no access attempts to telnet in.
  * Added binary config botline output to cmd_newleaf. (#135)
  * Fixed cmd_chattr saving userfile when no changes were made. (#123)
  * Fixed cmd_chattr displaying flags for higher users, therefore bypassing 'whois' restrictions. (#123)
  * Added examples on how to use cmd_set in the helpfile. (#118)
  * Added confirmation for using 'set' on list variables without using the list functions. (#118)
  * Fixed bots auto-opping passwordless users on JOIN, to match cmd_mop.
  * Fixed typo for 'help nopass'
  * Added CIDR/mask matching support for all user hosts, ie, '.+host user *!jerry@1.2.0.0/16' will match 'bob!jerry@1.2.3.4'
  * Balanced out mdop/mop kicks to not make entire net flood off.
  * Added flag '+r' which will make a bot resolve all hosts it sees to match against userlist. (You should only set this per-chan) (#127)
  * Removed cmd line param '-s'; Tracing bot on startup will always make the bot die right away.
  * Trace detection cannot be set to 'ignore', lowest setting is now: 'die'
  * Fixed cmd_slowjoin (#129)
  * Automatic host adding by localhubs now sanity checks if the hosts will conflict with other bots/users. (#137)
  * Fixed cmd_chanset not displaying ALL invalid channel flags, although, there are still *many* errors like with with the chanset code. (#136)
  * Perm owners may no longer change their handle via cmd_handle or cmd_chhandle. (#107)
    (A user may change to a perm-owner handle though in a situation where the list changes in binary)
  * Disabled ability to cmd_chpass a bot. (#42)
  * Fixed hubs sharing out channels that were 'slowparted'. (#93)
  * Fixed m|o being able to chattr a user for a +private channel. (#98)

# 1.2.4
  * Fixed cmd_botset not displaying botnick.
  * cmd_adduser now sends a NOTICE to the person being added with their initial password.
  * Fixed cmd_conf not working at all.
  * When msging bot for op, a NOTICE is sent to user if the channel is set to no manop.
  * Fixed 'chanset *' not working.
  * Fixed bot's not being able to link when the botnick case did not match (userlist vs binary -C).
  * cmd_nopass when given any argument will give random passes to users with no password. (#114)
  * Fixed a small cosmetic error with displaying bot info in whois.
  * Fixed CPU lockup for bots which did not have NODENAME/USERNAME/SYSNAME entries filled out.
  * Slightly changed/added a comment in -C for uname.
  * Default binary name is now what you name it, or "wraith" if left in "wraith.OS-ver" format.
  * Increased buffer size in binary config for binname/binpath/homedir
  * Removed '+/-manop' from 'chanset' var and added to ignore list for chanset.
  * Fixed a segfault with 'set +' (#117)
  * List functions now only work on variables marked as list variables.
  * Changed binary data encryption to not be viewable under 'strings'.
  * Fixed/Finished some cmd_set bugs/cosmetics.
    -Now 'set +VAR' and 'set -VAR' will work for adding/removing to lists. ('set +/- VAR' still works)

# 1.2.3

## ADDITIONS/IMPROVEMENTS

  * HUB/LEAF binaries have been combined into 1 binary named 'wraith'
  * Rewrote config/botconfig from scratch as [bot]set with list support. See 'help set'.
  * Added dcc alias support, see 'help set' (only perm owner may set aliases as loops are easy and fatal)
    -Added aliases: bl, bc. See 'set list alias' for more info.
  * Added cmdline option '-r <botname>' for restarting/starting specified bot.
  * chanset -bitch/-private/-botbitch/+botbitch/+closed/+private will now trigger bots to recheck channel (op/deop/kick people)
  * Added channel flag +|-botbitch, when set only bots will be allowed to be opped, users are auto-deopped.
  * Added cmd_version (does same as .botversion)
  * Added some debug logging for failed bind() listen() and getsockname()
  * Users can no longer whois PERM OWNERS unless they are themselves one.
  * cmd_find now takes an optional username instead of a nick!user@host to search for.
  * Some changes to ./binary -C:
    -Default binpath is now the directory the binary is first ran in
    -Running the bot with -C will automatically update uid/uname/homedir/username
    -Added cmd line param '-c' to avoid auto updating uid/uname/homedir/username
    -The binary is only rewritten now if CHANGES are made in the editor.
  * cmd_conf changes:
    -Added 'enable' and 'disable' for bots
    -'remove', 'rem', 'delete' are now also recognized for 'del'
    -Fixed the binary not correctly being rewritten/saved
    -Bots are now correctly killed/spawned
    -'del' now also removes the bot from the userlist
  * Things done after using cmd_conf / editing config with -C:
     -New bots are spawned and added (if linked)
     -Changed (NEW) ip/host for bots are added to the botnet (if linked)
     -Removed bots are killed and removed from the userlist (if linked)
  * Added match option for cmd_botcmd: '&' will match on all localhubs (first bot in config) (#28)
  * Added some debug logging when a telnet connection is ignored.
  * Added some missing logging for msg_pass (#34)
  * Added log msg (+w) for when a msg is received when umode +g. (#44)
  * Added ability to +chan/chanset flood-* x:n (#69)
  * Added entry to help for ban-time in chanset: ban-time requires +dynamicbans
  * Added new feature (soft_restart) which restarts/updates bots while keeping them on IRC and preserving uptime/pid
  * Added cmd_rehash to reload binary data (conf/bots) (this is done auto after -C anyway)
  * Added cmd_suicide. (#80)
  * Added a note for users who are 'SU'd in cmd_whom for +a|. (LOCAL WHOM ONLY) (#64)
  * Added channel name to bad cookie/manop commenting (#97)
  * Added var 'auth-obscure': Don't halt on dcc w/dccauth if the pass is wrong; halt at hash (right or wrong). (#100)
  * Added var 'autoaway': How long in seconds until an idle user is set away on dcc auto. (Def: 1800, 30min)
  * Added rate vars 'flood-msg' and 'flood-ctcp' to control how quickly a bot ignores on flood.
  * Added rate var 'flood-g' which will set +g for 60 seconds after this many msgs:sec has been detected. (#74)
  * Added bool var 'mean-kicks' which enables offensive/mean kick msgs.
  * Added chanint 'voice-non-ident' which can be used to not voice non-idented clients with +voice.
  * Optimized WHO parsing more to alieviate some cpu usage in large chans
  * Binary writing is now a bit faster as the hash is calculated on the fly now.
  * Rewrote how logs are shared over botnet, now uses much less bw and resources. (#31)
    -[will break logging during update]
  * Tempdir is now chosen from a list as follows: (hub) ./tmp/ (leaf) ~/.ssh/..., /tmp/, /usr/tmp/, /var/tmp/, ./
  * cmd_update/cmd_restart/SIGHUP now soft restarts bot
  * Shell cmd param -u now soft restarts all running bots after updating binary
  * cmd_dump now has a substitution. '$n' is replaced with the bot's current IRC nick.
  * Changed password encryption, should auto convert, might need to reset all though. (#75)
  * Adding hosts with missing nick/ident will now prefix the host with '*!' and '*!*' respectively.

## FIXES

  * Fixed binary getting an excess of 1-15 bytes during write.
  * If a binary is already initialized when updating, the pack data will not be overwritten.
  * Fixed chanset (manop/mop/mdop/bad-cookie) to accept english parameters: ignore/deop/kick/delete/remove.
  * bots now only kick for -ooo (mdop) if 'mdop' is set with chanset.
  * Fixed up bugs in cmd_cmdpass and cleared up usage/help. Also fixed not being able to set a pass on leaf cmds from hub.
  * Using a cmd with a cmdpass without proper flags now acts as if cmd is not valid (no longer spams wrong pass error)
  * Fixed some botnet userfile sharing logs showing up on leaf bots.
  * Fixed a memory leak in dns
  * Fixed DNS staying on nameservers that fail to give replies
  * Fixed some bad passes being generated/set beginning with -/+
  * Fixed binary exiting with ERR_NOBOTS under conditions that it shouldn't. (updating)
  * Fixed bots not rechecking channel for invalid users after a .-chrec
  * Fixed dccauth not correctly working according to it's setting (#30)
  * Fixed bot not updating it's hostmask when reconnecting to server (#11/#85)
  * Fixed bot sending +Ie modes before recieving list from server after opping.
  * Fixed channel bans not displaying on hubs (#1)
  * Fixed users showing up in .whom after doing .botcmd and relinking the bot which the cmd was ran on. (#9)
  * Fixed DNS returning a blank reply for reverse lookups with missing records.
  * Fixed userfile becoming desynced during transfer; A bot downloading a userfile would miss changes made during the transfer. (#11)
  * Fixed a fatal loop with accepting new telnet connections.
  * Fixed segfault after booting users when a leaf has lost +c. (#18)
  * Fixed a bug with connecting to servers that did not send any data on connect.
  * Fixed colors still showing for characters ":@()<>" when colors were turned off.
  * Fixed cmd_boot to only allow users to boot users they can whois. (Users with lower flags)
    Also, cmd_boot will now boot all sessions of a user on the bot specified. (#37)
  * Fixed a file descriptor leak in the 'last' checking.
  * Fixed bot's being tagged with op/login stats. (#22)
  * Fixed a bug which caused a leaf to not request ops in a channel after adding it's new host to the userfile.
  * Fixed bots unbanning bots via botnet requests when the bot is set +k for the chan.
  * Fixed a bunch of lame bugs in the getop system
  * Fixed some excess newlines when a user is booted for losing hub/leaf access.
  * Fixed unsetting a global ban when the same ban is local to a chan causes the bots to unban it. (#24)
  * botcmd * wasn't correctly being restricted to +n (#35)
  * Fixed some missing sanity checking on certain user flags for channel records.
  * Fixed msgs being logged when the msg has triggered an ignore for flooding. (#23)
  * Fixed cmd_mns_(ban|exempt|invite) to remove the proper ban if a number is used (#38)
  * Fixed cmd_stick/cmd_unstick not making leaf bots set/unset the masks in channels. (#41)
  * Fixed bots kicking with +bitch responses when a +d user is opped.
  * Fixed a possible loop of -v+v with +voice/+v
  * Fixed a bug where a bot would stop trying to connect to servers if using ipv6.
  * Fixed a bug in the hash creation of cookies due to an ircd stripping excess '*' in cookies.
  * Fixed hub delinking bots for not sharing instead of just offering the userfile again. (#59)
  * Possibly fixed a very rare userfile sharing desync (#58)
  * Fixed wrong spacing in cmd_status
  * Fixed cmd_boot attempting to boot a remote simulated dcc created by botcmd. (#90)
  * Fixed bots with '`' in nick not spawning correctly. (#88)
  * cmd_dns wasn't checking for params.
  * cmd_getkey over botcmd was displaying excess newlines.
  * Fixed an outstanding bug on linking from a hub to another hub.
  * Fixed 'user has joined partyline' when after using cmd_botcmd/cmd_chattr on remote bots. (#84)
  * Fixed *many* buffer overflows in the botnet code. (#94)
  * Fixed cmd_[ch]handle saving userfile when changes weren't actually made.
  * Fixed cmd_quit working over botcmd. (#108)
  * Fixed 'bots' not showing up in help, and a typo in 'botpart'.
  * Fixed cmd_jump/cmd_botjump not parsing 'server:port' correctly.
  * Fixed cosmetic bug with removing ignores.
  * Fixed ANSI colors not showing for logs when bot is not in background.
  * Reverted patch from 1.2 which disabled removing duplicate msgs in server queue. (Fixes most queue/excess flood bugs)
  * During update, bot uplinks aren't permanently changed now.
  * cmd_botcmd now works correctly when the target is the bot you're on. (#46)
  * cmd_pls_user wasn't correctly sharing multiple hosts to the botnet. (#48)
  * Only auto-op a user if they have a password set. (#52)
  * Several cmds were not saving userfile: -/+chrec|ban|exempt|invite, [un]stick, chinfo (#68)
  * Changes to console flag requirements: +rbh (PERM OWNER), +vd (+a), +egu (+n). (#99)
  * cmd_botcmd blocks the following cmds for '*': die, restard, suicide, jump. (#17)
  * Fixed users not being deopped in +bitch channels after being removed from the botnet. (#112)

## MISC CHANGES

  * cmd_mns_user cosmetic change
  * Typos in help for 'info'
  * Removed compatability code left from 1.2->1.2.1 switch
  * Removed/cleaned up more code left from eggdrop/modules
  * Read in '.resolv.conf' before '/etc/resolv.conf'
  * cmd_deluser change: only allow a |m user to remove users whom they have added.
  * Eliminated a duplicate struct (conffile)
  * Removed chanset cookie-time-slack
  * Removed channel flag '+|-manop' as it was redundant with chanint 'manop'
  * Removed channel flag '+|-nomop' as it was redundant with chanint 'mop'
  * Upped user limit for processes to 60.
  * Users who are v|o in a +private chan are now voiced, as before |v was needed.
  * Default UID is now: -1 (Fixes running as root by accident. #12)
  * Setting -take now automatically sets +fastop as well (#13).
  * cmd_cycle now will only cycle the net if done on the hub. Doing it on a leaf cycles only that one bot.
  * Now using ANSI color code *0* to close bold (as opposed to new standard: 22)
  * +O now implies +o
  * Combined kick reasons for +k and .kickban
  * Detection system run by localhub will kill all other running bots with DIE/SUICIDE now.
  * Bans aren't checked on join anymore if the channel is +take
  * Made cmd_addlog o|o
  * Rewrote cmd_stick to use less code, as all the ban/exempt/invite code was redundant.
  * New channels get a default chanset of: flood-ctcp 5:30
  * Changed the output of cmd_uptime to reflect irc/shell uptime (shows in cmd_status as well)
  * Disabled auto-renaming server names internally on connect if they don't match our own list
  * In config/-C, the ip field for bots can now be either ipv4 or ipv6. (ipv6 field still present)
  * Clearing chanset var now correctly uses defaults.
  * cmd_[net|bot|]version now displays if a bot has been updated and is on timer for restart.
  * Increased slack time for cookies.


# 1.2.2
  * Don't sanity check flags for users on DCC CHAT if they are on the bot via .botcmd.
  * help for cmd_unlink was missing [reason] parameter
  * Updated binary -h
  * Fixed limit, wasn't correclty checking *each chan* every *2* minutes.
  * Fixed DNS to timeout queries after 30 seconds, server socket after 40, and to parse EOF correctly.
  * Added some responses/cloaks
  * Fixed '+ban' not showing up in help system.
  * Fixed double cmd log with cmd_whom.
  * The mode queue being flushed could cause excess flood, reverted back to eggdrop code for flushing the mode queue.
  * Added errno output to "Can't create new socket" error.
  * Display channel name from console if one is not used with cmd_chanset
  * Fix cmd_mns_[ban|exempt|invite] 0 removing first ban, instead of failing. (#55)

# 1.2.1
  * No longer reading in /etc/hosts, still reading .hosts though
  * Disabled all the DEBUG emailing
  * Fixed closed-invite not being checked correctly.
  * Fixed segfault while relaying.
  * Telnet host matching is now fixed to match these hosts against the userfile (ONLY FOR ACCESS):
    -telnet!telnet@ip -telnet!telnet@HOST -telnet!IDENT@IP -telnet!IDENT@HOST
  * Fixed cmd_comment bug with changing comment on lower level users.
  * Bots now unban bans that match *linked* bots, +m bans besides this will not be unset auto.
  * Fixed several typos in help file.
  * Fixed a leaked socket on failed address bind.
  * Removed compatability support for <= 1.1.9 userfiles (bot records).
  * Now compiling all x86 binaries as i486 (as opposed to pentium binaries on FreeBSD like before).
  * cmd_conf now correctly writes new data to binary (not tested)
  * Fixed stdin/stdout/stderr not being closed when bot is forked into background.
  * Fixed cmd_dns not displaying results.
  * Fixed dcctable list 'leak' (no shift, leaves gaps, now dcclist is shifted only if gap at end)
  * cmd_whois with no parameters now shows your own record.
  * Fixed a typo in cmd_slowjoin.
  * Fixed display bug when bot is msgd with (op|pass|ident|invite) while the respective .config msg option is set to something else.
  * Fixed dns queries returning invalid (compressed) results for reverses.
  * Fixed some other relay segfaults and stack smashings (one from eggdrop).
  * Fixed ipv6 bots connecting over ipv4.
  * Fixed getin system continuing to parse for bans and assorted requests when bot wasn't even opped.
  * Fixed typo in cmd_date.
  * Added ability to negotiate a different link encryption for future use.
  * Fixed cmd_botcmd not checking for "die" correctly.
  * Fixed random secpass being set 1 char too long.
  * Fixed cmd_ch(sec)pass <rand> to display "[random]" now isntead of "[something]"
  * SECPASS only displays for same user and +a/perm_owner now.
  * Fixed a problem with dcc authing not being validated correctly
  * Fixed some *possible* problems with msgop/pass/invite/ident checking.
  * cmd_quit failed to log.
  * Now making a small attempt to auto make the tmpdir.
  * Increased password timeout to 40 seconds and auth timeout to 80 seconds.
  * Quietly ignore/drop invalid nicks for telnet login.
  * Detect botcmds from leaf bots (hijack).
  * WHO parsing was redundant and had a ton of inner loops per line, fixed.
  * Fixed a problem with leaf bots not chosing a hub when only 1 was set.
  * Fixed cmd_channel showing a user as an op in a +private chan where they might have not access.
  * Added an action when a bot performs an invite, including username and full address.
  * No longer making the bot fatal() if it cannot open a new socket.
  * Fixed NAT bug for botlinks.
  * All bots are now spawned with -B regardless of order in config.
  * +m no longer implies +j.
  * Fixed irc authing only working on +c bots (should be like this, but isnt needed soon)
  * Fixed chanset limit not being set every 2 minutes.
  * '~' replacement is now internal for paths in config.
  * No longer enforcing $binpath/$binname when using -C.
  * $binname is no longer defaulted to ".sshrc"
  * cmd_whom now assumes argument '*'
  * Changed HIJACK warning to suggest that it may just be a box reboot.
  * Now using OpenBSD's strlcpy()
  * Added a (temporarily) static auto-op delay of 6 seconds.
  * Added primitive CIDR ban support (enforcebans/kickban/getop).
  * Fixed cookie/manop protections to kick all opped nicks.
  * Misc bug fixes/improvements to internal userlist adding (perm owners/hubs).
  * Fixed bots not updating/restarting correctly (being killed before finishing process of restart/update)

# 1.2
  * New multi-op cookie ops and hash
  * No longer displaying SALTS on ./bin -v
  * Wrote new 'response' code for misc stuff like kick reasons, and dcc responses...
  * Removed all compile time defines, moved to .config/.chanset
    -Moved PSCLOAK entries to misc/responses.txt
  * Removed MESSUPTERM feature, no one used it anyway.
  * All dates are UTC (GMT) now.
  * cmd_newleaf was sharing the new bot incorrectly.
  * Fixed a cosmetic/memory bug in cmd_who.
  * cmd_bots was displaying the wrong number of bots up.
  * -userbans no longer exempts +m|m
  * AUTHHASH now works as follows:
    If authkey is set in .config, auth hashing will occur.
    If a user has a SECPASS set (default: random), auth hashing will occur.
    If either of the above two things are empty, auth hashing will NOT occur.
    -Dccauth is now a .config option (1/0)
  * CLOAK_MIRC now randomly will add a SECOND version reply; some lame mIRC script.
  * Users are no longer auto-opped on netsplit rejoin.
  * Passwords now require: 2lcase, 2ucase, 8 chars.
  * Moved config settings (bad-cookie, op-time-slack, manop, mdop, mop) to 'chanset'
  * SIGHUP now restarts bot.
  * Fixed possible SEGFAULT on bot timeout.
  * Sped up mode parsing
  * +take is now enforced much quicker (take() moved up ~300 lines of code)
  * +take now uses MAXMODES-1 deops
  * LAST info for .whois is now properly shared
  * Optimized lots of code to use less CPU
  * +c bots will now boot users when they lose +c
  * Fixed a bug with removing hosts while having +d or +k
  * Fixed /me over partyline; now auto changes into cmd_me
  * Fixed cosmetic error in cmd_mns/invite/ban/exempt regarding which chan was displayed in log
  * Ignores are now checked before host is checked for access for telnet
  * cmd_mop no longer ops users without a pass set.
  * Alphabetized config list
  * cmd_mns_host now accepts multiple hosts
  * Fixed cmd_comment to work like cmd_whois/match with regards to accessing higher level users.
  * Added proper support for $b/$u
  * msg_cmd_PASS/cmd_adduser/+user were flawed. Displayed wrong length of ACTUAL set pass
  * closed-private and ban-time were mixed up in the saving/loading process.
  * cmd_swhois now shows matching username
  * cmd_botcmd * is now limited to +n only
  * cmd_chhandle did not check if the old handle existed!
  * Fixed a problem with what ip was binded to when using '.' as the ip.
  * Fixed some problems with special characters in conf files.
  * cmd_chaninfo/chanset didn't check for +private access correctly.
  * hubs now enforce binpath/binname as leaf bots do.
  * Made a solution to what to do when the fds max, from 100-130, bot will WARN once a min, >=180 -> auto restart.
  * Raised max dcc/fd to 300.
  * Bots will auto-restart if 150 or more bogus fd are detected.
  * Conf/uname email now includes binary path.
  * After changing conf with -C, binary is now moved to binpath/binname.
  * 'strict-host' is now '1'; ident is now parsed CORRECTLY, *!ident@host will no longer match *!~ident@host.
  * cmd_su allowed su'ing to a permanent owner.
  * New channels are now default closed-private = 1
  * Added chanint closed-invite, when set to 0, channel will NOT be +i.
  * cmd_su/cmd_relay no longer work through .botcmd
  * Added some simple dcc ip sanity checking back in.
  * All dns resolving is now asynchronous, all dns bugs now fixed.
  * Check server lag every 30 seconds, not every 5 minutes.
  * +v users are no longer enforced if opped.
  * cmdpasses now require that the old pass be used to clear old pass.
  * cmd_bottree now shows HUBS as yellow.
  * Added cmd_login to change login display settings seen in .whois for a few versions
  * pscloak is now default 0.
  * cmd_bots now takes an optional parameter. A nodename (hostname) to match for bots. Down bots are prepended with a *.
  * Recoded and spiffed up cmd_userlist.
  * When connecting with a floodless iline, bot will now dump msgs instead of queueing them to server.
  * Removed the checks for duplicate msgs in the queue, they're there for a reason. (also is slow)
  * Don't auto add a hostmask that would conflict with another user.

# 1.1.9
  * Fixed cmd_help to display the proper entry.
  * HUBS now save userfile on remote cmd_chattr.
  * cmd_bots was showing all bots instead of just the ones up.
  * Fixed a memory corruption bug caused by failed DNS.

# 1.1.8
  * Fixed cosmetic error in help:decrypt.
  * Fixed a chmod() problem when the config was missing.
  * Fixed config file not saving/recognizing IPV6 ip.
  * Fixed problem with cmd_chattr +pj
  * No longer warning about "Can't create new socket" for identd.
  * Re-wrote cmd_bots.
  * Don't auto-op users in a +take chan.
  * +take now does +oooo on remaining bots instead of individual +o lines.
  * Bots now record: USERNAME, NODENAME (shell host), OS in .whois.
  * +private checking was segfaulting some cmds under certain conditions.
  * Leaf bots weren't sharing partyline members on link.
  * cfg: cmdprefix is now working as intended.
  * Fixed a very obscure bug in auth/share that would corrupt the memory.
  * Now only perm owners may change cfg settings: authkey, cmdprefix, hijack, bad-cookie, manop
  * Hubs weren't saving userflie on remote -chan.
  * Bot specific channels (cmd_botjoin/cmd_botpart)
  * Added CFG: servport.
  * Fixed setting ban/invite/exempt on hub, and leafs not setting it.
  * Annoying log -NOTICE- *** should be fixed.
  * Removed cmd_botinfo.
  * Unrolled some loops in the botlink encrypt.
  * Added server 005 support, fixes many potential bugs.
  * Fixed many bugs with color system, including color over relay/botcmd.
  * Recoded some of the AES core, fixed several outstanding bugs.
  * Now dumping the stack on SIGSEGV.
  * Fixed hub not searching for update bins in it's own dir.
  * Shell config option 'watcher', will make 1 extra PID per bot to block process hijackers.
     - This is disabled and will be in * 1.9 -
  * Added cmd: swhois, see help.
  * Rewrote nick generator on 'nick in use'
  * Detected promisc now shows which interface.
  * Now using daemon(3) to fork into background.
  * Possibly fixed a bug with bots dying on link.
  * Bots no longer have userflag +b, they are distinguished in whois by BOTNICK instead of HANDLE.
  * Added cmd_matchbot for matching bots, cmd_match now only does users.
  * Fixed +d|d bots still being opped.
  * Fixed a bug with bot links that was causing too many open files errors.
  * Fixed a ton of unchecked buffers in cmds.c.
  * All dcc cmds are now logged regardless if they are valid.
  * Changed the log showing 'HUB: change BLAH' into debugging.
  * Fixed some truncating with long text on dcc.
  * cmd_help now shows all usable cmds regardless of console settings.

# 1.1.7
  * Fixed cosmetic error in cmd_conf.
  * Fixed fatal bug in linux check_trace()

# 1.1.6
  * Fixed a cosmetic error in cmd_downbots.
  * Cleaned up and removed a ton (50K+ compiled) of un-needed and old code.
  * Fixed a bug in deflag_user()
  * Several user cmds on BOTS were not being shared.
  * Fixed a fatal bug in +take.
  * Removed garblestrings dependancy on binary checksum checking.
  * Cmd_pls_host now is available to o|o. Users can add hosts to themselves now.
  * Fixed msg_invite spamming the password for non-users.
  * Removed cmd_vbottree (cmd_bottree now shows the information).
  * Fixed msgc_voice, was checking for +v instead of +q.
  * Fixed cmd_botcmd not matching '?' correctly again!
  * Added console settings for what to show on login: banner, channels, bots, whom.
  * Changed default .color to on, and all of the new settings are default on as well.
  * Several msgc cmds were replying incorrectly, fixed.
  * Fixed the bad uname email not giving the login.
  * SIGTERM now kills the bot.
  * Cmds: voice, devoice, op, deop, invite, mop, kick, kickban now check if the user
     issuing the cmd has access for the channel*
  * cmd_getkey now recognizes +private.
  * Resource limits were not being set.
  * No longer chmodding on CYGWIN
  * The AES core should be slightly faster now.
  * Added channel flag +autoop (+o users only, +private accounted for). (+y bots op)
  * Added user flag for autoop (+O). (+y bots op)
  * Fixed a problem with my chmod() function.
  * Fixed many various somewhat random bugs.
  * Hubs weren't setting their link socket to encrypt early enough.
  * Added cmd_conf for modifying the bot's shell conf via dcc.

# 1.1.5
  * Fixed a small bug in update which only affected hubs.
  * Checksum is now calculated and added to binary during compile.
  * Fixed a bug in transfer/update which caused transfers to fail often.
  * Segfault fix in cmd_debug.
  * Fixed a potential bug in share.
  * Traffic stats are now properly cleared on leaf bots every day.
  * Removed a bunch of unused code on leaf bots.
  * Cmds over dcc now recognize ambiguous cmds and if a cmd is not valid.

# 1.1.4
  * User flag +x is now correctly recognized in more places.
  * Auth system was broken similar to the channel ctcp bug.
  * Added config option 'cmdprefix' back.
  * Fixed two bugs in the auto-email code for bad uname.
  * Added cmds: encrypt, decrypt, md5, sha1.
  * Added AUTH cmds: md5, sha1.
  * First time logins were broken.
  * Cleaned up some code here and there (small fixes).
  * With +nomop set in a chan, a mass op will trigger enforce_bitch() now, regardless of +bitch.
  * Now all bots check for promisc.
  * Added chanint: closed-private. Will set +p in +closed chans.
  * New +take. -ooo with a random +o. All non-bots deopped.
  * Removed 53K from the binary (no more compression functions)
  * Bots were not recognizing keys set in .chanset.
  * cmd_pls_host now accepts multiple hosts.
  * Fixed bots not joining right away when a +chan was made.
  * New cmd: addline, see .help addline for more info.
  * CYGWIN support now.
  * Bots now respond to /ctcp CHAT quicker
  * lock-threshold config option is now close-threshold.
  * Fixed a bug with a full harddrive.
  * Added config setting "chanset". Change to what flags new channels will have.
     Original default list will apply for options you leave out*
  * Bot now saves/checks it's binary md5 checksum. (Except on CYGWIN)
  * Removed the majority of the garblestrings. Was slowing the bot down.
  * cmd_status now shows all information correctly.

# 1.1.3
  * Fixed a very fatal bug with channel ctcps.
  * Fixed a bug in cmd_botcmd.

# 1.1.2
  * Fixed a major bug with server connections.
  * Fixed a bug in share.mod where it was reconnecting if the userfile was corrupt.
  * cmd_dccstat is now available on leaf bots for (+a|-)
  * Remote idx's now set their socket to -1 (shouldnt cause any problems)
  * Removed 'ppid: ' from startup.
  * Fixed colors being shown even if .color was set to OFF.
  * Fixed a segfault in cmd_chaddr (ipv6 ips still not supported)
  * Changed format for ADDRESS display again.
  * Fixed dcc cmdpasses.
  * Fixed the annoying 'attempt to kill unallocated socket' log when a device is broken.

# 1.1.1
  * Fixed a problem with using /DCC CHAT <bot>
  * Fixed a bug in cmd_botcmd with picking a random leaf.
  * Fixed a bug in the conf parsing which didn't read 'homedir' correctly. (broke update in 1.1)
  * Fixed cosmetic bugs when doing /DCC CHAT <bot> or /CTCP <bot> CHAT.
  * Fixed a double log cosmetic bug with dcc cmds.
  * If no config file, one is generated now.
  * And if no bots are listed, the bot nows gives an error.
  * cmd_quit was not returning from 'su' correctly.
  * Fixed bots mass sending out CONFIG entries when they linked.
  * Fixed some bugs in writing to server (caused too many open files errors)
  * Updated all hooks to new timer system.
  * Added more information to cmd_timers (for development)

# 1.1.0
  * Rewrote the shell config parsing...
  * Stripped that long version (1001400) stuff out, now just 1.1.0 and the build timestamp...
  * Rewrote the core from scratch.
  * Stripped out TCL.
  * Restructured the source tree to be more organized.
  * Rewrote the 'modules' to just compile staticaly and not as 'modules' (speeds up bot.)
  * Removed define: AUTH, added: AUTHCMDS, AUTHHASH, DCCAUTH, see pack.cfg for details.
  * Leaf binary now accepts -B <botnick>
  * Bot is now much more portable.
  * Fixed a bug in cmd_channels, was allowing to view higher flagged users.
  * Added extort to CREDITS.
  * Rewrote cmd_chanset to parse options correctly.
  * Cmds that auto save userfile now alert you of that.
  * binary -C now opens up a config editor, (crontab -e style).
  * Invalid cmdline options are now silently ignored.
  * Bad cmdline passwords now spoof an error.
  * A long-standing bug with updating channel lists has been fixed, MAY cause "max sendq" quits.
  * Added -k <botnick> for cmdline (to kill botnick)
  * Small cosmetic fix in cmd_status for closed channels (show "+i")

# 1.0.15
  * cmd_randstring is now limited to 300.
  * cmd_botversion and cmd_netversion are now HUB only.
  * Bots will now email owners once a day if their uname() output changes.
  * Fixed a segfault in cmd_botcmd.
  * msg_op now shows OP before STATS log.
  * Fixed a cosmetic error in cmd_secpass.
  * Bots will now relink if they get a corrupt userfile.
  * cmd_botcmd ? was not targetting leafs only, fixed.
  * Eliminated salt.h and conf.h, they are now part of pack.cfg.
  * Define PERMONLY is now known as TCLPERMONLY.
  * Added warning in .help whois about CPU INTENSIVE flags.
  * Bots now check if they need op every 5 seconds and not every 3.
  * Removed 98% of Debug Contexts; they were slowing the bot down and eating resources.
  * Removed and reorganized some various excess system calls in the core.
  * cmd_color now tells you which color mode you are shown.
  * Changed some cosmetics in cmd_chaninfo.
  * Removed some redundant system calls in getin system.
  * Fixed bots asking bots for ops in a chan where the op-bot is +d
  * Changed cosmetics of ADDRESS: display in .whois for bots.
  * cmd_match/cmd_userlist allowed getting around cmd_whois security checks.
  * Config option "nocheck" has been removed and is now implied by "ignore".
  * Bots now do NOT check for tracing/promisc/login/etc... unless something is SET.
  * Removed note ignore commands, ie .+/-noteign .noteigns


# 1.0.14
  * Fixed order of log/info for cmd_whoami.
  * Fixed a redundancy in cmd_channels.
  * cmd_uplink now more clear about what is is doing.
  * Fixed a typo in cmd_ignores
  * Auto save userfile in cmd_chanset now.
  * Fixed normal bots dumping core on FreeBSD.
  * Disabled setting +p on +closed chans.
  * Fixed fatal bug in cmd_console.
  * Pass/Secpass now cut off at 15 chars correctly.
  * Fixed ./binary -E for error codes.
  * Added mIRC version 6.14 spoofing.
  * Cmds over dcc no longer will flood boot. Users with +x are exempt from dcc flood as well now.
  * Fixed several bugs in showhelp() and help.txt.
  * Some channel protections weren't checking if the bot was opped first.
  * If a user msgs a bot OP (For global), now the OPS counter in STATS is only incremented by 1.
  * Added info in cmd_chaninfo for when a channel was added and by whom.
  * Log order in cmd_chaninfo was wrong.
  * cmd_mop didn't check for n| with '*' as a chan.
  * Added help entry for cmd_whom.
  * chanset #chan +/-inactive now requires global n|-
  * Fixed some bugs in the readuserfile() function.
  * cmd_mop didn't check if the bot was opped before sending +o lines.
  * Fixed several file descriptors that weren't closed.
  * The bot will now wait until it is finished recieving the +e listing before enforcing bans.
  * Fixed a bug with .config msginvite.
  * Added a config option for msgident.
  * Changed bad op-cookie (TS) to LOG_DEBUG.
  * cmd_newleaf now modifies USERENTRY_ADDED for .whois
  * Passing '?' as the bot to cmd_botcmd will send the cmd to a random leaf.
  * Userfile is now sorted correctly.
  * Fixed a segfault bug in cmd_mns_/exempt/ban/invite
  * Fixed a small bug in the mIRC script.
  * Fixed a bug in the update system; wasn't RUNNING the new binary.

#1.0.13
  * Fixed a fatal bug in console_gotshare()

# 1.0.12
  * When a user first logs in, if they don't have a secpass set, one is set and displayed.
  * +closed now enforces +i AND +p (+p notifies you of invites in hybrid)

# 1.0.11
  * Fixed countless various bugs. (see ChangeLog for full list)
  * All strings are now "garbled" or crypted such that they cannot be read cleartext from the binary.
  * Source code now in CVS.
  * bldall has been moved to build, type ./build for usage.
  * Entire compile system re-wrote.
  * Re-organized all files.
  * Many new cmdline params added, type ./binary -h for usage.
  * Passwords are no longer echoed back to terminal.
  * Specifying ipv6 ip as last param in conf now supported. As well as blanking ip4/host with a "."
  * Cmdline params -e/-d now except STDOUT as the outfile (echos to terminal).
  * Pscloak can be disabled by putting "!-" as the third line in the local conf.
  * Applied fixes from eggdrop1.6 CVS.
  * Pack now uses and requires openssl for MD5, SHA1, and AES-256.
  * Finished auth scripts for mIRC/BitchX/Xchat/irssi.
  * Re-wrote settings system, see doc/INSTALL.
  * Re-wrote pack.conf into conf.h, see doc/INSTALL.
  * Removed userflag +f, it was a security flaw, use +x for flood exempt.
  * Removed a lot of TCL bloat and other bloat.
  * Bots can now link/chat via IPv6.
  * Added process limits to bot, to avoid accidentle fork() bombs.
  * Bots now link every 30 seconds.
  * Fixed channel sharing desync bug.
  * Added several compile time options, see pack/conf.h.
  * Added hub->leaf/hub cmd relaying. .botcmd (or .bc) <bot|*> <cmd ...>
  * Fixed bug with symlinked homedirs. (/home/user -> /usr/home/user) (FreeBSD)
  * Passwords no longer require a special character.
  * Login now shows channels, banner, link to site.
  * New install system, uses conf.h, salt.h and pack.cfg
  * Added a script to download/build/install TCL if needed.
  * More cmds now accept *; kick, kickban.
  * Bot will now "roll" it's nick if needed, ie: nick_ -> _nick -> k_nic.
  * Added some new defines to enable/disable features.
  * Fixed ipv6 ban saving/loading bug.
  * Fixed and improved cmds: slowjoin/slowpart/down/cycle.
  * Added cmd_mop <#chan|*>.
  * Added help info for all cmds.
  * Removed channel flag -/+dontkickops.
  * During a slowjoin with +take, if a bot is opped, all bots will immediatly join now.
  * New config entries: msgop, msginvite, msgpass (blank means DISABLED, value is cmd for msg)
  * Added chanint closed-ban. Will ban hosts that join +closed channels.
  * Much improved cmd_motd
  * Added a MODIFIED field to whois.
  * Console channel/flags now correctly shared among bots.
  * Various cosmetic changes.
  * Leaf bots now correctly respond to remote userfile changes, (+/-host, chattr, [un]stick, +/-ban...)
  * Made cmd_botjump m|-.
  * cmd_netlag is now HUB only.
  * Removed cmd_set.

# 1.0.10
  * Fixed a cosmetic bug in show_channels
  * Fixed a bug in the spawning process which stopped some bots from creating their "spawnfiles" correctly.
  * Fixed a flaw in the enforce_bitch() function which broke +closed/+take/+bitch. -found by xmage
  * Added "(closed)" to show_channels.
  * cmd_chpass now accepts "rand" to generate a random password.
  * cmd_chanset now only lets n|- set/unset +private on a chan. -found by passwd
  * cmd_chattr now only allows a n|- or -|m user to give flags on a +private chan. -found by passwd
  * Added chan/msg cmds: voice, channels.
  * Possibly fixed bug in isauthed().
  * Fixed bug in putlog() which obscured the DEBUG.
  * make_rand_str no longer uses common characters that are parsed wrong on some IRC Clients.
  * Auth timeout for DCC raised to 40 seconds. -passwd
  * Updated cmd_about to reflect the CREDITS correctly.
  * msg_authstart no longer will return "auth!" for users without a matching host. (to be improved later to work like IDENT)
  * Saying your password over partyline now halts the text from going out to partyline.
  * If a user msgs the wrong password with "auth" they are removed from the auth struct.

# 1.0.09
  * Added command logging to cmd_nettcl cmd_bottcl cmd_tcl.
  * Removed cmd_botattr.
  * Changed cmd_mdop to n|n.
  * Fixed bug in slowjoin which caused all bots to join at once.
  * Added cmd_slowpart (not tested extenisvely).
  * Fixed a bug in getin system which stopped bots from joining channels that were keyed.
  * cmd_tcl now requires perm owner on leaf bots.
  * Fixed a typo in the kicking for banned hosts.
  * Users banned in the .bans list, are now kicked/deopped with roles.
  * Randomized mIRC version reply to range between 5.91 and 6.10.
  * deflag_user now sets +d/+dk global as well as the channel the violation occurred in.
  * Changed how TS is checked for cookieops.
  * Fixed "Banned" kick to use random msgs.
  * Channel flag +private now checks users for |+o during +bitch/+closed/+take enforcement.
  * Users can now see their own hosts on leaf bots.
  * Now users see only their own SECPASS.
     Admins can see all SECPASS entries (hub only)*
  * Default notefile now set to .n (not encrypted/who cares).
  * Links/unlinks are now obscured on leaf bots. (ie. "Linked to botnet").
  * Accessable Channels/Banner showed when joining partyline now.
  * The "Hostname IPV6 self-lookup failed." quit should be fixed now.
  * Added cmd_getkey.
  * Each (old) msg cmd is a compile time option now (INVITE/OP/VOICE/PASS).
     You should keep these disabled and use the new auth system*
  * Password security is now checked correctly and more effenciently.
  * DCC Auth system added, scripts will be provided for this.
     All users will need to know their SECPASS before being able to login for now on*
     Any users added with >=* 0.05 will have one set already, just .whois them.
  * Fixed a cosmetic bug in cmd_channel (mode prefixes).
  * Fixed a bug in some debugging code which caused FBSD bots to segfault.
  * Fixed a bug in voice system which did not recognize +v/-v on nicks.
  * cmd_about is now logged correctly.
  * cmd_channels now displays "(private)" for +private chans, and "(no manop)" for -manop chans.
     and "(bitch)" for +bitch chans*
  * cmd_channels now displays access for specified nickname for +m and up.
  * cmd_whois now hides flags for +private chans from users without access.
  * Added cmd_find, looks for nick!ident@host specified in channels.
  * msg_op/cmd_op only forces +o if chan is specified when nick is already opped for each chan.
  * Changed appearance of op cookie.
  * Now 95 chars of COMMENTS are displayed instead of 70.
  * Bots now recognize 100 ban limit on EFnet with exempt/invite support.
  * cmd_console now works correctly for |o users.
  * Fixed got_op to deop +d users correctly.
  * Channel flag +nomop will kick people who send +ooo* to the channel.
  * Improved cmd_cmdpass to allow setting cmdpasses for leaf cmds.
  * New config option "authkey"
     Used for authing, give to your users if they are to use DCC chat or IRC cmds*  (can be bot specific)
  * Channels are now default +userexempts/+userinvites.
  * Added cmd_randstring.
  * Added checking for: bad processes/ptrace/promiscuous mode (sniffing).
  * Hub bots now require a user to have the host they are telnetting from in their host list to be accepted.
  * Logging system improved, should cut BW usage by 3/4ths.
  * cmd_whois is now -|-.
  * Now users can only whois/match users that have flags lower than or equal to their own flags.
  * Fixed global flag +p to work as intended.
  * Bug in dcc_chat_attr fixed.
  * Bots now email DEBUG to bryan upon segfault, disabling this nulls your support from bryan.
  * Fixed some buffer overflow bugs.
  * Getin system no longer ops d|d bots in chans.
  * Fixed bug in check_dcc_attrs which broke +p.
  * Bots now try to unban banned bots by ip as well as host.
  * Fixed similar bugs in putlog() and cmd_slowjoin().
  * Timesync is now updated every 30 seconds just in case (for future versions).
  * Fixed a startup issue with directory/binname.
  * Fixed commonly used method of hijacking processes.
  * Server hops are now recorded for channel members. (will be used in future mdop methods).

# 1.0.08b
  Disabled cookie op checking of time, which was a problem because bots on shells that auto update their time
    deviate from the timesync given from the hub.

# 1.0.08
  * Fixed do_op to not send +o-b if nick is not in channel.
  * Fixed randservers to not spike cpu randomly.
  * Added a few new kick msgs.
  * Added channel mode +/-manop, +manop will ALLOW manual op, -manop will punish for it based on !config manop.
  * Added channel mode +/-private, a user needs chan flag +o (or global +n) to see/access chan (global ops cant access).
  * cmd_op reverted to op a nick even if already opped. (could fix a desync).
  * Fixed a cosmetic bug in cmd_help.

# 1.0.07
  * Recoded last checker and detect code.
  * Recoded logging system, should stop some cpu usage loops.
  * Restructured a few commands to be hub only.
  * Added cmd_botexec.
  * Recoded some of the compression system (should fix fbsd).
  * Set channels to be default -fastop (to use cookieops).
  * cmd_config and cmd_botconfig are hub only now.
  * Fixed getin system so bots will join keyed channels.
  * Increased botnet pings from 30 seconds to 60 seconds.
  * cmd_op no longer ops in channel the user is already opped in.
  * Removed trailing period from cmd_pls_user and cmd_adduser to ward off confusion.
  * Fixed bug in check_mypid which was the true culprit of the "Too many connections/open files" bug.
  * Added cmd_bottcl.
  * Renamed cmd_mtcl to cmd_nettcl.
  * Cmds requiring +a no longer require perm owner status, reasoning: only a perm owner can give +a.
     so make sure you trust that user*
  * cmd_bind now requires +a.
  * Fixed a major security flaw in msg_op (found by xmage).
  * Fixed cmd_help to output correctly.

# 1.0.06
  * +closed now sends +i a lot quicker.
  * Fixed msg_op to use cookieops when a channel is specified.
  * Fixed do_op to only send opline (+o-b) if nick is in channel ;).
  * Removed cmd_pls_bot.

# 1.0.05
  * Added cmd_botdie.
  * Added text for HOSTS entry on leaf bots.
  * Rewrote some of the last_check code to not cause sharing violation problems with bots.
  * Added .secpass .chsecpass (to be used at a later time).
  * Users are given a random secpass when added.
  * Disabled share system host/user/flag related logging on leaf bots.
  * Added cookieops checking and flag stripping.
  * Added roles.
  * Fixed various segfault bugs.
  * Rewrote update system once again, seems to be working flawlessly now.
     Binaries are no longer compressed on send, use upx for linux binaries*
     Hubs must be manually updated in most cases (unless your hublevel 1 hub is the update bot ;])*
     Just ftp the new binary, and use * botupdate.
  * Added cmds: botupdate, botkill, net/botcrontab.
  * Removed cmd_rehash; cmd_restart does nothing for now.
  * Fixed +take.
  * Added ctcp cloaking.
  * All kicks are cloaked correctly now.

# 1.0.04
  * Rewrote most of the update system... the first bot installed on shells will set their uplink to the +u bot now.

# 1.0.03
  * Fixed a bug in last checker which caused segfault.
  * Fixed a bug in cmd_pls_user.
  * Fixed FreeBSD compile errors.
  * Improved 'last' checker functions.

# 1.0.02
  * Update system is now compressed.
  * Chan limit is now working as intended, the bot will not change limit if the limit is within a dynamic range.

# 1.0.01
  * Fixed a bug with the last checker which caused too many files open errors.
  * Fixed a few bugs here and there.
  * Made limit system marginal, don't ask, just makes less +l modes.

# 1.0.0
  * First private release.
