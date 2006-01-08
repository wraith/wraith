#! /usr/bin/php
<?php
  $info = Array();
  exec("svn info", $info);
  $datel = explode(' ', $info[8]);
  $date = strtotime("{$datel[3]} {$datel[4]} {$datel[5]}");
  echo $date;
?>
