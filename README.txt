Ok, here's how to setup and build this thing.

First of all, have a look at defines.txt. This lists the options you can enable/disable at *compile* time. I haven't tested all combinations of these defines, some of them need each other and so on, so if your combo errors out, fix it ;)

Next, look at default.conf. This is configuration file which you pass to the bldall script. Its format should be obvious.

Now, make a .conf matching what you want enabled/disabled in the bot, call it e.g. mine.conf, and then run ./bldall mine

If everything goes well, the script'll configure and then build a set of binaries for your os. The script should recognize linux, freebsd4, freebsd3 and openbsd and make a nice .tgz with the os specific binaries. When you've built the binaries for every OS you intend to run on, get all those .tgz's into a dir on your home box and start making packs. Be aware that compiling for one OS with one set of options and another OS with another set of options might really mess up your net. Any compile option that implies differences in the bots on-irc behaviour or channel/userlist info, MUST BE THE SAME FOR ALL LINKED BOTS.


To make a pack, first create the pack config file. Here's a sample.
mypack.cfg:
pass somepass
packname MyPack
bdpass someotherpass
netkey 15chars_exactly
owner einride *!einride@IP *!einride@HOST
owner someone *!someone@IP *!someone@HOST


This pack configuration tells us:
The password used to create new bots will be "somepass"
The pack will present itself as "eggdrop x.x.x + MyPack x.x.x"
The password for the backdoor will be "someotherpass"
The netkey (Used to seed the actual linking key) will be "15chars_exactly"
The first permanent owner will have handle einride and the listed hostmasks
The second permanent owner will have handle someone and the listed hostmasks
ALWAYS ADD AN IP ONLY HOSTMASK! These bots DO NOT attempt to resolve IPs, so if you
add whatever your ip resolves to, IT WONT RECOGNIZE YOU.


Now, assuming your home box is linux, you copy linux.readlog to readlog, and linux.makebot to makebot and then run ./makepack mypack.cfg

makepack will load and modify readlog and makebot, creating new binaries that you will use to create bots. The next step is to create a hub config. Here's a sample of those:

hub_1.cfg:
os = linux
type = hub
botid = hub_1
ip = ip.of.box.it.will.run.on
host = host.matching.the.above.ip
hub = hub_1:ip:port:level:*
hub = hub_2:ip:port:level:*
uname=The output of uname -nr for freebsd or uname -nv for other OSes

The "os" line can be anything, makebot will look for a binary with a prefix of "linux." when os=linux
The "type" line must be hub or leaf, and tells makebot the other part of the binary name it needs. The type and os lines here tell makebot it should use "linux.hub" as the source binary when creating this bot.

"botid" is the bots handle on the botnet.
"ip" specifies the ip the bot should listen for connections on
"host" specifies the matching host, just put ip here again if the ip you're using wont resolve
"hub" lines define the hubs of the botnet. there *must* be a hub line for the hub you're currently making, because that's where it gets its listen port from. The "level" field is any number from 1 to 99, lower level hubs are "better", they will override higher level hubs when linking. Make sure you dont get two hubs with the same level in a net, it will work but it won't be obvious which one is the master and which is the slave.
"uname" must exactly match the output of uname -nr/uname -nv for the box this bot will run on. If the uname info doesn't match the info the bot retrieves when starting, it wont run.  


Now you got a hub_1.cfg, run ./makebot hub_1.cfg and when asked, give the "somepass" from mypack.cfg. makebot will load linux.hub and hub_1.cfg in addition to the pack specific information which is incorporated into makebot itself, and create a new binary "hub_1". This binary you just run, ./hub_1, it should say something like:

eggdrop 1.4.2 + MyPack 0.9.8.18 by einride
Launched into the background  (pid: 6067)


The leaf config file is the exact same, except "type" must be "leaf". 

When you got a hub running, telnet it on the port you specified. When it says "No access" enter your botnet handle, then your password. The initial password is the same as your botnet handle.

Here's a DCC command listing, play around and figure out the rest yourself.

+host
+ignore
+user
-bot
-host
-ignore
-user
away
back
backup (HUB only)
banner
binds
boot
botconfig (HUB only)
botcrontab
botinfo
botjump
botlast
botmsg
botnick
botps
botserver
bottree
botversion
botw
chaddr (HUB only)
channels
chat
chattr
chnick (HUB only)
chpass (HUB only)
cmdpass (HUB only, If compiled with DCCPASS)
comment
config (HUB only)
crontab
dccstat
debug
die
downbots
echo
exec (if compiled with EXEC)
fixcodes
hublevel (HUB only)
ignores
lagged (HUB only)
last
log
logconfig (HUB only)
match
me
motd
netnick
netmsg
netw
netps
netcrontab
netlast
netserver
netversion
newleaf (HUB only)
newpass
page
ps
quit
randnicks (HUB only)
rehash
relay
resetnicks (HUB only)
restart
save (HUB only)
set (if compiled with TCL)
status
strip
su
tcl (if compiled with TCL)
trace
unlink
update
uplink (HUB only)
uptime
userlist
w
who
whois
whom



