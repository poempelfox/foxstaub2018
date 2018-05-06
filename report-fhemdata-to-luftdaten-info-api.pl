#!/usr/bin/perl -w

# As the handy name of this script suggests, this requests the current
# sensordata from FHEM and (if there IS current data) then reports it to
# the luftdaten.info API. You should probably execute this from cron every
# two to three minutes.

# Settings

# The unique ID of this sensor. Use at least 8 random digits.
# The sensor will need to be registered at luftdaten.info with the
# Chip-ID "foxstaub-THISNUMBER" (see their webpage on how to register sensors).
# Please note that for this to work you will absolutely need to set a UNIQUE ID
# below. And it really really needs to be unique, so don't use '42' or such
# nonsense, use a truly random number, 8 to 10 digits.
$sensorid = 'INVALID1';

# Where is FHEM?
$fhem = '/opt/fhem/fhem.pl localhost:7072';

# And what is the name of our sensor in FHEM?
$fhemsensorname = 'feinstaub';

# You should usually not touch anything below this line.

# --------------------------- this line ---------------------------------------

use POSIX qw(mktime);
use LWP::UserAgent;

$maxage = 5 * 60;
$timeout = 10; # Timeout for HTTP requests.
$debug = 1;
$ldiapiendpoint = 'https://api.luftdaten.info/v1/push-sensor-data/';
$madaviapiendpoint = 'https://api-rrd.madavi.de/data.php';

$pm2_5 = undef;
$pm10 = undef;
$humidity = undef;
$pressure = undef;
$temperature = undef;
$lastupdatets = 0;
if ($sensorid =~ m/INV/) {
  print(STDERR "ERROR: No unique sensor ID set. You need to set a UNIQUE SensorID\n");
  print(STDERR "       in the header of this script before it can be used.\n");
  exit(2);
}
unless (open($FHEMEXEC, '-|', $fhem . ' "list ' . $fhemsensorname. ' humidity pressure temperature pm10 pm2_5"')) {
  print(STDERR "Could not execute FHEM as '$fhem'.\n");
  exit(1);
}
while ($ll = <$FHEMEXEC>) {
  $ll =~ s/[\r\n]//g;
  if ($ll =~ m/\s+([0-9]{4})-([01][0-9])-([0-3][0-9])\s+([0-2][0-9]):([0-6][0-9]):([0-6][0-9])\s+(\w+)\s+([0-9.]+)$/m) {
    my $yr = $1; my $mo = $2; my $da = $3; my $hr = $4; my $mi = $5; my $se = $6;
    my $varname = $7; my $val = $8;
    my $vts = mktime($se, $mi, $hr, $da, $mo - 1, $yr - 1900);
    if ($lastupdatets == 0) { # Learn timestamp
      $lastupdatets = $vts;
    } else { # Not the first timestamp.
      # Normally all values should have the same timestamp, but in case that is
      # not so, assume the worst.
      if ($vts < $lastupdatets) {
        $lastupdatets = $vts;
      }
    }
    if (($varname eq 'pm2_5') || ($varname eq 'pm10') || ($varname eq 'humidity')
     || ($varname eq 'temperature') || ($varname eq 'pressure')) {
      $$varname = $val;
    }
  }
}
close($FHEMEXEC);
my $curts = time();
if ($curts > ($lastupdatets + $maxage)) {
  # Data too old.
  if ($debug > 0) {
    print("No current data available, so nothing to send.\n");
  }
  exit(0);
}
#print("$lastupdatets $pm2_5 $pm10 $humidity $temperature $pressure\n");
my $ua = LWP::UserAgent->new();
my $res;
$ua->agent("report-fhemdata-to-luftdaten-info-api.pl");
$ua->timeout($timeout);
$ua->requests_redirectable([]); # Do not automatically follow redirects
if (defined($pm2_5) && defined($pm10)) {
  $postcontent =  '{"software_version": "rftlia.pl 0.1",';
  $postcontent .= '"sensordatavalues":[';
  $postcontent .= '{"value_type":"P1","value":"' . $pm10 . '"},';
  $postcontent .= '{"value_type":"P2","value":"' . $pm2_5 .'"}]}';
  foreach $apiep ($ldiapiendpoint, $madaviapiendpoint) {
    $res = $ua->post($apiep,
                     'content_type' => "application/json",
                     'X-Pin' => "1",
                     'X-Sensor' => 'foxstaub-' . $sensorid,
                     'Content' => $postcontent
                    );
    if (!($res->is_success)) {
      print(STDERR "ERROR posting PM sensor data to $apiep: " . $res->status_line . "\n");
    }
  }
}
if (defined($temperature) && defined($pressure) && defined($humidity)) {
  $postcontent =  '{"software_version": "rftliapl-0.1",';
  $postcontent .= '"sensordatavalues":[';
  $postcontent .= '{"value_type":"temperature","value":"' . $temperature . '"},';
  $postcontent .= '{"value_type":"humidity","value":"' . $humidity .'"},';
  $postcontent .= '{"value_type":"pressure","value":"' . $pressure . '"}]}';
  foreach $apiep ($ldiapiendpoint, $madaviapiendpoint) {
    $res = $ua->post($apiep,
                     'content_type' => "application/json",
                     'X-Pin' => "11",
                     'X-Sensor' => 'foxstaub-' . $sensorid,
                     'Content' => $postcontent
                    );
    if (!($res->is_success)) {
      print(STDERR "ERROR posting temp/hum/press. data to $apiep: " . $res->status_line . "\n");
    }
  }
}
