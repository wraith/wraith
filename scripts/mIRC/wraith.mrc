on *:LOAD:{
  echo -a Wraith Auth by ducch (based on lordares's code) loaded.
  helpauth 
}
on *:START:echo -a Wraith Auth by ducch (based on lordares's code) loaded.

alias helpauth {
  echo -a Set the auth parameters (blank botnick will set default):
  echo -a /setauth <password> <secpass> <authkey> [botnick]
  echo -a Auth to a bot:
  echo -a /auth <botnick>
  echo -a Echo the +Auth value:
  echo -a /auth -Auth <string>
  echo -a Display the current auth parameters (blank botnick will show the default):
  echo -a /showauth [botnick]
  echo -a NOTE: Automatic auth'ing will only be followed after /auth!
}

alias setauth {
  if (!$3) {
    echo -a Usage: /setauth <password> <secpass> <authkey> [botnick]
    echo -a Note: using a blank botnick will set a default.
  }
  elseif ($4) {
    set %w.pass. [ $+ [ $4 ] $1
    set %w.secpass. [ $+ [ $4 ] $2
    set %w.authkey. [ $+ [ $4 ] $3
  }
  else {
    set %w.pass $1
    set %w.secpass $2
    set %w.authkey $3
  }
  echo -a Parameters were set successfully.
}

alias showauth {
  echo -a $iif($1,$1,Default) $+ : PASS: $wpass($1) :: SECPASS: $wsecpass($1) :: AUTHKEY: $wauthkey($1)
}

ON *:TEXT:*:?:{
  if (%auth. [ $+ [ $nick ] ] != 1) { return }
  elseif ($1 == auth.) { msg $nick auth $wpass($3) }
  elseif ($1 == -Auth && $len($2) == 50) { msg $nick +Auth $md5($2 $+ $wsecpass($3) $+ $wauthkey($3)) }
}

alias -l wraith {
  if ($eval(% $+ $1 $+ . $+ $2,2) || $var($eval(% $+ $1 $+ . $+ $2,1))) {
    return $eval(% $+ $1 $+ . $+ $2,2)
  }
  else {
    return $eval(% $+ $1,2)
  }
}

alias wauthkey { return $wraith(w.authkey,$1) }
alias wpass { return $wraith(w.pass,$1) }
alias wsecpass { return $wraith(w.secpass,$1) }

alias auth {
  if ($1 == -Auth) {
    if (!$2) {
      echo -a Usage: /auth -Auth string botname 
      echo -a botname is optional. 
    }
    else {
      echo +Auth $md5($2 $+ $wsecpass($3) $+ $wauthkey($3))
    }
  }
  else {
    if ($1 !ischan) {
      set -u15 %auth. [ $+ [ $1 ] ] 1
      msg $1 auth?
    }
  }
}

