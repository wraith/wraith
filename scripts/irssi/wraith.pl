

use Irssi 20020121.2020 ();

$VERSION = "1.01";
%IRSSI = (
          authors     => 'bryan P38',
          contact     => 'bryan\@shatow.net, bryan on EFnet',
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
  Irssi::print auth($data);
}


sub auth($$$) {
  my($data,$server,$witem) = @_;
  my ($secpass,$authkey,$password,$botdump);
  $secpass = Irssi::settings_get_str('auth_secpass');
  $authkey = Irssi::settings_get_str('auth_authkey');
  $password = Irssi::settings_get_str('auth_password');
  $botdump = $data . $secpass . $authkey ;
  return md5_hex(encode_utf8($botdump));
}



Irssi::settings_add_str('auth', 'auth_password', '');
Irssi::settings_add_str('auth', 'auth_secpass', '');
Irssi::settings_add_str('auth', 'auth_authkey', '');

Irssi::command_bind("auth", "cmd_auth");

=comment
#Irssi::signal_add_priority("message private", \&message, Irssi::SIGNAL_PRIORITY_LOW + 1);
#Irssi::signal_add_priority("dcc request", \&dcc_request, Irssi::SIGNAL_PRIORITY_LOW + 1);
=cut
