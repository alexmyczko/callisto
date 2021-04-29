#!/usr/bin/perl
#
# Author: Christian Monstein
# Version: 2014-05-16
# Updated: 2014-06-13, 2017-01-10, 2017-04-06, 2017-07-02, 2018-12-10, 2019-06-03
# Copies FIT-files to the FTP-server at FHNW in Switzerland
# Move FIT files into a local backup device (local, USB, external,...)
# This script should be execute every 15 minutes by e.g. ssfree.exe

use strict;
use warnings;
use File::Path;
use File::Copy;
use Net::FTP;

{
  my @tnow  = localtime(time());
  my $today = sprintf('%04i%02i%02i', $tnow[5]+1900, $tnow[4]+1, $tnow[3]);
  my $SourcePath = '/var/www/callisto';
  my $DestinationPath = '/var/www/callisto-backup';
  opendir(ID, $SourcePath) || die("Couldn't find directory");

  my @listing = readdir(ID);

  closedir(ID);

  foreach my $listing(sort @listing) 
  {
     my $filename = $listing;
     if ($filename =~ /^POLAND-Grotniki_(\d\d\d\d)(\d\d)(\d\d)_.*\.fit?$/)
     {
	my ($year, $mon, $day) = ($1, $2, $3);
	if ( ! -d "$DestinationPath/$year/$mon/$day")
	{
	   mkpath("$DestinationPath/$year/$mon/$day");
	}
	my $old = "$SourcePath/$listing";
	print "\nSource: ",$old," -> FTP-server copied.  ";
	my $new = "$DestinationPath/$year/$mon/$day";
	my $ftp = Net::FTP->new("147.86.8.73", Timeout => 500, Passive => 1, Debug   => 0);
        $ftp->login("solarradio","SECRETPASSWORD") or die 'Cannot login ', $ftp->message;
        $ftp->binary();
	sleep(2);
        my $tmp = $filename . ".tmp"; # upload as .tmp to avoid blocking at server site
	$ftp->put($old,$tmp) or die $ftp->message;
	sleep(10);
        $ftp->rename($tmp,$filename) or die $ftp->message; # once uploaded, rename to original filename
	$ftp->quit;
	move($old,$new); # backup on a local device
     }
   }
   print "\n\nFTP finished...\n";
   sleep(1);
}
