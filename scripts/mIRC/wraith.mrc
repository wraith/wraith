ON *:LOAD:{ echo -a Wraith authorization script, for mIRC 5.8 and up, by bryan loaded (MSG/DCC/Psybnc support). type /setauth }

alias helpauth {
  echo -a Type /setauth
  echo -a /auth -Auth string <botnick> <- That will return the md5 hash. (use for telnet possibly)
}

alias setauth {
  if (!$3) {
    echo -a usage: /setauth password secpass authkey <botnick>
    echo -a <botnick> is optional if the authkey specified is for that bot only...
  }
  else {
    if ($4) {
      set %w.pass. $+ $4 $1
      set %w.secpass. $+ $4 $2
      set %w.authkey. $+ $4 $3 
    }
    else {
      set %w.pass $1
      set %w.secpass $2
      set %w.authkey $3
    }
    echo -a set!
  }
}

ON *:CHAT:*:{
  var %c = %auth. [ $+ [ $nick ] ]
  if (($1 === -Auth || $1 === ÿû-Auth) && $len($2) == 50) {
    msg =$nick +Auth $wmd5($2 $+ $wsecpass($3) $+ $wauthkey($3))
  }
}
ON *:TEXT:auth*:?:{
  var %c = %auth. [ $+ [ $nick ] ]
  if (!$3) {
    ;if this is a MSG not psybnc DCC, and we arent cleared to auth with them, IGNORE.
    if ($left($nick,1) != $chr(40) && !%c) {
      return
    }
    if ($right($1,1) == . && %c) {
      msg $nick auth $wpass($2)
    } 
    elseif ($right($1,1) == ! && %c) {
      msg $nick auth $wpass($2) %myuser
    }
  }
}
ON *:TEXT:*:?:{ 
  var %c = %auth. [ $+ [ $nick ] ]
  if ($left($nick,1) != $chr(40) && !%c) { return }
  if (($1 === -Auth || $1 === ÿû-Auth) && $len($2) == 50) {
    msg $nick +Auth $wmd5($2 $+ $wsecpass($3) $+ $wauthkey($3))
  }
}
ALIAS -l wraith {
  if ($eval(% $+ $1 $+ . $+ $2,2)) {
    return $ifmatch
  }
  else {
    return $eval(% $+ $1,2)
  }
}

alias -l wauthkey { return $wraith(w.authkey,$1) }
alias -l wpass { return $wraith(w.pass,$1) }
alias -l wsecpass { return $wraith(w.secpass,$1) }

alias -l wmd5 {
  if ($version < 5.8) {
    echo 8 -a This script will only work for mIRC 5.8 and up.
  }
  if ($version >= 6.03) {
    return $md5($1)
  } 
  else {
    if (!$exists($nofile($script) $+ /md5.dll)) { 
      echo 4 -a You need to place md5.dll in $nofile($script) for this to work.
      halt
    }
    return $dll($nofile($script) $+ /md5.dll,md5,$1)
  }
}

ALIAS auth { 
  if ($1 != -Auth || !$2) { 
    echo 8 -a Usage: /auth -Auth string botname 
    echo 8 -a botname is optional. 
  }
  else {
    echo +Auth $md5($2 $+ $wsecpass($3) $+ $wauthkey($3))
  }
}
ALIAS msg { 
  if ($1 !ischan && $2 === auth? && $left($1,1) != $chr(40)) { set -u30 %auth. $+ $1 1 }
  msg $1-
}
ON *:INPUT:?:{ if ($1 === auth? && $left($target,1) != $chr(40)) { set -u30 %auth. $+ $target 1 } }
