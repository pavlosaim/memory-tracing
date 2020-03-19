#!/usr/bin/perl

#error checking
if($#ARGV < 1) {
	print "usage: <infile> <outfile> <granularity (int)> \n";
    exit(0);
}

$inf = $ARGV[0];
$outf = $ARGV[1];
$gran = $ARGV[2];

if($gran < 1){
	print "Granularity must be >= 1\n";
	exit(0);
}

$cnt = 0;
$tic = 0;
$stack = 0;
$heap = 0;
$global = 0;
$total = 0;
$memfp = 0;

#-----###----
open IN, $inf or die "error infile\n" ;
open OUT, ">$outf" or die "error outfile\n";

$line = <IN>;
chomp($line);
@_line = split(' ', $line);
print OUT "$_line[0] $_line[1] $_line[2] $_line[3] $_line[4] $_line[5]\n";

#strip the log file into a separate files, load hash on the go.
while(<IN>) {
	chomp($_);
	@aline = split(' ', $_);
	
	$cnt++;
	$tic     = @aline[0];
	$stack	+= @aline[1];
	$heap		+= @aline[2];
	$global	+= @aline[3];
	$total	+= @aline[4];
	$memfp	+= @aline[5];
	
	if($cnt == $gran){
		print OUT "$tic $stack $heap $global $total $memfp\n";
		$stack 	= 0;
		$heap 	= 0;
		$global = 0;
    $total = 0;
		$memfp 	= 0;
		$cnt 		= 0;
	}
}

if($stack != 0 || $heap != 0 || $global != 0){
	print OUT "$tic $stack $heap $global $total $memfp\n";
}

close IN;
close OUT;
