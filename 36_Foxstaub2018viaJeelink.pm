
# $Id: 36_Foxstaub2018viaJeelink.pm $
# This is mostly a modified version of 36_LaCrosse.pm from FHEM.
# You need to put this into /opt/fhem/FHEM/ and to make it work, you'll
# also need to modify 36_JeeLink.pm: to the string "clientsJeeLink" append
# ":Foxstaub2018viaJeelink".

package main;

use strict;
use warnings;
use SetExtensions;

sub Foxstaub2018viaJeelink_Parse($$);


sub Foxstaub2018viaJeelink_Initialize($) {
  my ($hash) = @_;
                       # OK CC 71 245 1 128 155 192 48 46 234 0 0 16 0 17
  $hash->{'Match'}     = '^\S+\s+CC\s+\d+\s+245\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s*$';  # FIXME
  $hash->{'SetFn'}     = "Foxstaub2018viaJeelink_Set";
  ###$hash->{'GetFn'}     = "Foxstaub2018viaJeelink_Get";
  $hash->{'DefFn'}     = "Foxstaub2018viaJeelink_Define";
  $hash->{'UndefFn'}   = "Foxstaub2018viaJeelink_Undef";
  $hash->{'FingerprintFn'}   = "Foxstaub2018viaJeelink_Fingerprint";
  $hash->{'ParseFn'}   = "Foxstaub2018viaJeelink_Parse";
  ###$hash->{'AttrFn'}    = "Foxstaub2018viaJeelink_Attr";
  $hash->{'AttrList'}  = "IODev"
    ." ignore:1,0"
    ." $readingFnAttributes";

  $hash->{'AutoCreate'} = { "Foxstaub2018viaJeelink.*" => { autocreateThreshold => "2:120", FILTER => "%NAME" }};

}

sub Foxstaub2018viaJeelink_Define($$) {
  my ($hash, $def) = @_;
  my @a = split("[ \t][ \t]*", $def);

  if ((int(@a) < 3) || (int(@a) > 3)) {
    my $msg = "wrong syntax: define <name> Foxstaub2018viaJeelink <addr>";
    Log3 undef, 2, $msg;
    return $msg;
  }

  $a[2] =~ m/^(\d{1,3}|0x[\da-f]{2})$/i;
  unless (defined($1)) {
    return "$a[2] is not a valid Foxstaub2018 address";
  }

  my $name = $a[0];
  my $addr;
  if ($a[2] =~ m/0x(.+)/) {
    $addr = $1;
  } else {
    $addr = sprintf("%02x", $a[2]);
  }

  return "Foxstaub2018viaJeelink device $addr already used for $modules{Foxstaub2018viaJeelink}{defptr}{$addr}->{NAME}." if( $modules{Foxstaub2018viaJeelink}{defptr}{$addr} && $modules{Foxstaub2018viaJeelink}{defptr}{$addr}->{NAME} ne $name );

  $hash->{addr} = $addr;

  $modules{Foxstaub2018viaJeelink}{defptr}{$addr} = $hash;

  AssignIoPort($hash);
  if (defined($hash->{IODev}->{NAME})) {
    Log3 $name, 3, "$name: I/O device is " . $hash->{IODev}->{NAME};
  } else {
    Log3 $name, 1, "$name: no I/O device";
  }

  return undef;
}


#-----------------------------------#
sub Foxstaub2018viaJeelink_Undef($$) {
  my ($hash, $arg) = @_;
  my $name = $hash->{NAME};
  my $addr = $hash->{addr};

  delete( $modules{Foxstaub2018viaJeelink}{defptr}{$addr} );

  return undef;
}


#-----------------------------------#
sub Foxstaub2018viaJeelink_Get($@) {
  my ($hash, $name, $cmd, @args) = @_;

  return "\"get $name\" needs at least one parameter" if(@_ < 3);

  my $list = "";

  return "Unknown argument $cmd, choose one of $list";
}

#-----------------------------------#
sub Foxstaub2018viaJeelink_Attr(@) {
  my ($cmd, $name, $attrName, $attrVal) = @_;

  return undef;
}


#-----------------------------------#
sub Foxstaub2018viaJeelink_Fingerprint($$) {
  my ($name, $msg) = @_;

  return ( "", $msg );
}


#-----------------------------------#
sub Foxstaub2018viaJeelink_Set($@) {
  my ($hash, $name, $cmd, $arg, $arg2) = @_;

  my $list = "";

  if( $cmd eq "nothingToSetHereYet" ) {
    #
  } else {
    return "Unknown argument $cmd, choose one of ".$list;
  }

  return undef;
}

#-----------------------------------#
sub Foxstaub2018viaJeelink_Parse($$) {
  my ($hash, $msg) = @_;
  my $name = $hash->{NAME};

  my ( @bytes, $addr );
  my $pressure = 0;
  my $temperature = -101.0;
  my $relhum = -1.0;
  my $pm2_5 = -1.0;
  my $pm10 = -1.0;
  my $batvolt = -1.0;

  if ($msg =~ m/^OK CC /) {
    # OK CC 71 245 1 128 155 192 48 46 234 0 0 16 0 17
    # c+p from main.c. Warning: All Perl offsets are off by 2!
    # Byte  2: Sensor-ID (0 - 255/0xff)
    # Byte  3: Sensortype (=0xf5 for Foxstaub)
    # Byte  4: Pressure, MSB
    # Byte  5: Pressure       Pressure is in 1/256th Pascal.
    # Byte  6: Pressure
    # Byte  7: Pressure, LSB
    # Byte  8: Temperature, MSB   The temperature is in 1/100th degrees C with an
    # Byte  9: Temperature, LSB   offset of +100.00, so e.g. 12155 = 21.55 degC
    # Byte 10: rel.Humidity, MSB     humidity is in 1/512th percent.
    # Byte 11: rel.Humidity, LSB
    # Byte 12: PM2.5, MSB       particulate matter 2.5 is in 1/10th ug/m^3
    # Byte 13: PM2.5, LSB
    # Byte 14: PM10, MSB
    # Byte 15: PM10, LSB
    @bytes = split( ' ', substr($msg, 6) );

    if (int(@bytes) != 14) {
      DoTrigger($name, "UNKNOWNCODE $msg");
      return "";
    }
    if ($bytes[1] != 0xF5) {
      DoTrigger($name, "UNKNOWNCODE $msg");
      return "";
    }

    #Log3 $name, 3, "$name: $msg cnt ".int(@bytes)." addr ".$bytes[0];

    $addr = sprintf( "%02x", $bytes[0] );
    my $pressraw = (($bytes[2] << 16)
                  | ($bytes[3] <<  8) | ($bytes[4] <<  0));
    if ($pressraw != 0xffffff) {
      $pressure = sprintf("%.3f", $pressraw / 4096.0); # in hPa!
    }
    my $tempraw = (($bytes[5] << 8) | ($bytes[6] << 0));
    if ($tempraw != 0xffff) {
      $temperature = sprintf("%.2f", (-45.00 + 175.0 * ($tempraw / 65535.0)));
    }
    my $humraw = (($bytes[7] << 8) | ($bytes[8] << 0));
    if ($humraw != 0xffff) {
      $relhum = sprintf("%.1f", (100.0 * ($humraw / 65535.)));
    }
    $pm2_5 = sprintf("%.1f", (($bytes[9] << 8) | ($bytes[10] << 0)) / 10.0);
    $pm10 = sprintf("%.1f", (($bytes[11] << 8) | ($bytes[12] << 0)) / 10.0);
    $batvolt = ($bytes[13] / 100.0) * 11.0;
  } else {
    DoTrigger($name, "UNKNOWNCODE $msg");
    return "";
  }

  my $raddr = $addr;
  my $rhash = $modules{Foxstaub2018viaJeelink}{defptr}{$raddr};
  my $rname = $rhash ? $rhash->{NAME} : $raddr;

  return "" if( IsIgnored($rname) );

  if ( !$modules{Foxstaub2018viaJeelink}{defptr}{$raddr} ) {
    # get info about autocreate
    my $autoCreateState = 0;
    foreach my $d (keys %defs) {
      next if ($defs{$d}{TYPE} ne "autocreate");
      $autoCreateState = 1;
      $autoCreateState = 2 if(!AttrVal($defs{$d}{NAME}, "disable", undef));
    }

    # decide how to log
    my $loglevel = 4;
    if ($autoCreateState < 2) {
      $loglevel = 3;
    }

    Log3 $name, $loglevel, "Foxstaub2018viaJeelink: Unknown device $rname, please define it";

    return "";
  }

  my @list;
  push(@list, $rname);

  $rhash->{"Foxstaub2018viaJeelink_lastRcv"} = TimeNow();
  $rhash->{"sensorType"} = "Foxstaub2018viaJeelink";

  readingsBeginUpdate($rhash);

  # What is it good for? I haven't got the slightest clue, and the FHEM docu
  # about it could just as well be in Russian, it's absolutely not understandable
  # (at least for non-seasoned FHEM developers) what this is actually used for.
  readingsBulkUpdate($rhash, "state", "Initialized");
  # Round and write temperature and humidity
  if ($pressure > 100.0) { # Could be valid
    readingsBulkUpdate($rhash, "pressure", $pressure);
  }
  if (($temperature > -100.0) && ($temperature < 100.0)) { # Could be valid
    readingsBulkUpdate($rhash, "temperature", $temperature);
  }
  if (($relhum >= 0.0) && ($relhum <= 100.0)) { # Could be valid
    readingsBulkUpdate($rhash, "humidity", $relhum);
  }
  if (($pm2_5 >= 0) && ($pm2_5 < 1000)) { # This is the sensors range
    readingsBulkUpdate($rhash, "pm2_5", $pm2_5);
  }
  if (($pm10 >= 0) && ($pm10 < 1000)) { # This is the sensors range
    readingsBulkUpdate($rhash, "pm10", $pm10);
  }
  if (($batvolt > 0.0) && ($batvolt < 25.0)) { # Could be valid
    readingsBulkUpdate($rhash, "batvolt", $batvolt);
  }

  readingsEndUpdate($rhash,1);

  return @list;
}


1;

=pod
=begin html

<a name="Foxstaub2018viaJeelink"></a>
<h3>Foxstaub2018viaJeelink</h3>
<ul>

  <tr><td>
  FHEM module for the Foxstaub2018-device.<br><br>

  It can be integrated into FHEM via a <a href="#JeeLink">JeeLink</a> as the IODevice.<br><br>

  On the JeeLink, you'll need to run a slightly modified version of the firmware
  for reading LaCrosse (found in the FHEM repository as
  /contrib/36_LaCrosse-LaCrosseITPlusReader.zip):
  It has support for a sensor type called "CustomSensor", but that is usually
  not compiled in. There is a line<br>
   <code>CustomSensor::AnalyzeFrame(payload);</code><br>
  commented out in <code>LaCrosseITPlusReader10.ino</code> -
  you need to remove the `////` to enable it, then recompile the firmware
  and flash the JeeLink.<br><br>

  You need to put this into /opt/fhem/FHEM/ and to make it work, you'll
  also need to modify 36_JeeLink.pm: to the string "clientsJeeLink" append
  ":Foxstaub2018viaJeelink".<br><br>

  <a name="Foxstaub2018viaJeelink_Define"></a>
  <b>Define</b>
  <ul>
    <code>define &lt;name&gt; Foxstaub2018viaJeelink &lt;addr&gt;</code> <br>
    <br>
    addr is the ID of the sensor, either in decimal or (prefixed with 0x) in hexadecimal notation.<br>
  </ul>
  <br>

  <a name="Foxstaub2018viaJeelink_Set"></a>
  <b>Set</b>
  <ul>
    <li>there is nothing to set here.</li>
  </ul><br>

  <a name="Foxstaub2018viaJeelink_Get"></a>
  <b>Get</b>
  <ul>
  </ul><br>

  <a name="Foxstaub2018viaJeelink_Readings"></a>
  <b>Readings</b>
  <ul>
    <li>temperature</li>
    <li>humidity</li>
    <li>pressure (hPa)<br>
      the athmospheric pressure in hecto-Pascal</li>
    <li>PM 2.5<br>
      Particulate Matter 2.5 micro-meter value from the SDS011 dust sensor</li>
    <li>PM 10<br>
      Particulate Matter 2.5 micro-meter value from the SDS011 dust sensor</li>
    <li>batvolt (V)<br>
      the battery voltage of the battery in volts.</li>
  </ul><br>

  <a name="Foxstaub2018viaJeelink_Attr"></a>
  <b>Attributes</b>
  <ul>
    <li>ignore<br>
    1 -> ignore this device.</li>
  </ul><br>

  <b>Logging and autocreate</b><br>
  <ul>
  <li>If autocreate is not active (not defined or disabled) and LaCrosse is not contained in the ignoreTypes attribute of autocreate then
  the <i>Unknown device xx, please define it</i> messages will be logged with loglevel 3. In all other cases they will be logged with loglevel 4. </li>
  <li>The autocreateThreshold attribute of the autocreate module (see <a href="#autocreate">autocreate</a>) is respected. The default is 2:120, means, that
  autocreate will create a device for a sensor only, if the sensor was received at least two times within two minutes.</li>
  </ul>

</ul>

=end html
=cut
