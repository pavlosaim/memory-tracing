#!/usr/bin/perl

use strict;
use warnings;

if($#ARGV < 1){
	print "Usage\$ perl <infile> <outfile>\n";
	exit(0);
}

my $infile = $ARGV[0];
my $outfile = $ARGV[1];

print "IN< " . $infile . "\nOUT> " . $outfile ."\n";

my @table = ();
my $inc = 0;
my $href = {};
my $shref = {};

#--- determine step size
open FIN, $infile or die "can't open FIN\n";
my $tmp = <FIN>; chomp($tmp);
my @atmp = split(' ', $tmp);

while(@atmp != 1){
	$tmp = <FIN>; chomp($tmp);
	@atmp = split(' ', $tmp);
}
close FIN;

my $step_size = $tmp;
print "step_size: " . $step_size . "\n";

#--- open files
open FIN, $infile or die "can't open FIN\n";
open FOUT,">",$outfile or die "can't open FOUT\n";

while(<FIN>){
	chomp($_);
	
	my @line;
	$_ =~ s/^\s+//g;
	@line = split('\s+', $_);

	if(@line == 2){
	  $href -> { $line[0] } = $line[1];
		if(exists $shref->{$line[0]}){
			$shref -> { $line[0] } += $line[1];
		}
		else{
			$shref -> { $line[0] } = $line[1];
		}
	}
	else{
		push @table, $href;
		$href = {};	
		$inc++;
	}
}

#--- sort href sort structs
#foreach my $value (sort keys $shref) {
#	print $value . " -> " . $shref->{$value} . "\n";
#}

#--- print struct stats
#print "\nREPRINT\n";
my $si = 0; 
for my $key (keys %$shref ){
#	print $key . " -> " . $shref->{$key} . "\n";
	$si++;
}
print "Identified Structs: $si\n";

#--- print
printf FOUT "%s ", "Instr";

for my $struct (sort { $shref->{$b} <=> $shref->{$a} } keys %$shref ) {
	printf FOUT "%s ", $struct;
}
printf FOUT "\n";

$tmp=$step_size;
for $href (@table) {
	printf FOUT "%d ", $tmp;
	$tmp+=$step_size;
	
	for my $key (sort { $shref->{$b} <=> $shref->{$a} } keys %$shref){
		if(exists $href->{$key}){
			printf FOUT "%d ", $href->{$key};
		}
		else{
			printf FOUT "%d ", 0;
		}
	}
	printf FOUT "\n";
}

print "\nIdentified structs\n\n";
for my $struct (sort { $shref->{$b} <=> $shref->{$a} } keys $shref) {
	print $struct . ":" . $shref->{$struct} . "\n";
}



