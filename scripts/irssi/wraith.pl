

use Irssi 20020121.2020 ();

$VERSION = "1.01";
%IRSSI = (
          authors     => 'bryan P38',
          contact     => 'bryan@shatow.net, bryan on EFnet',
          name        => 'wraith-auth',
          description => 'private auth script for botpack wraith',
          license     => 'ALL?',
          url         => 'http://wraith.botpack.net/wiki/AuthSystem',
          changed     => '$Date$ ',
);

use Irssi::Irc;                 # for DCC object
use Digest::MD5 qw(md5 md5_hex md5_base64);
use Encode qw(encode_utf8);

sub cmd_auth {
  my($data,$server,$witem) = @_;
  if (!$data) {
    Irssi:print "Usage: /auth hash\nbefore you can do that, type /set auth, then /set all of the variables that show.\n";
  } else {
    Irssi::print auth($data);
  }
}


sub auth($) {
  my($data,$server,$witem) = @_;
  my ($secpass,$authkey,$botdump,$hash);
  #lets use the right word for the hash.
  my @words = split " ", $data;
  if ($data =~ /^\-Auth/) {
    $hash = $words[1];
  } else {
    $hash = $words[0];
  }

#  Irssi::print "Authing: $hash";

  $secpass = Irssi::settings_get_str('auth_secpass');
  $authkey = Irssi::settings_get_str('auth_authkey');
  $botdump = $hash . $secpass . $authkey ;
  return md5_hex(encode_utf8($botdump));
}

#this must handle both auth. and -Auth
Irssi::signal_add "message private", sub {
    my ($server, $text, $nick, $address, $target) = @_;
    my ($msg, $data, $password);
    my (@servers) = Irssi::servers();

    if ($msg =~ /^\255\251\001/) {
      $msg = substr($text,3);
    } else {
      $msg = $text;
    }

#    if ($nick =~ /^\(/) { #this is a psybnc dcc chat.
#    } else { #normal msg
#      $password = Irssi::settings_get_str('auth_password');
#      if ($msg =~ /^auth\./) { #msg back password
#        send auth $password;
#      }
#    }

    if ($msg =~ /^\-Auth/) {
      $server = $servers[0];
      my $cmd = "/MSG $nick +Auth " . auth($msg);
      $server->command("$cmd");
    }  
};

#this must handle -Auth
Irssi::signal_add "dcc chat message", sub {
  my ($dcc, $text) = @_;
  my ($msg,$data);
    my (@servers) = Irssi::servers();
  if ($msg =~ /^\255\251\001/) {
    $msg = substr($text,3);
  } else {
    $msg = $text;
  }

  if ($msg =~ /^\-Auth/) {
#    my $cmd = "/MSG =". $dcc->{nick} . " +Auth ". auth($msg);
    $server = $servers[0];
    my $cmd = "+Auth ". auth($msg);
    $server->command("/MSG =". $dcc->{nick} . " ". $cmd);
#    $dcc->send("$cmd");
#    send +Auth auth($msg);
  }
  
};

#Irssi::settings_add_str('auth', 'auth_password', '');
Irssi::settings_add_str('auth', 'auth_secpass', '');
Irssi::settings_add_str('auth', 'auth_authkey', '');

Irssi::command_bind("auth", "cmd_auth");
Irssi::print "Wraith authorization script loaded.";
