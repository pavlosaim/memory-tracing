#!/usr/local/bin/perl

#print "\nPerl GNUPLOT Script Generator for Gleipnir generated plots\n";
#print "\nAuthor: Tomislav Janjusic\n";

#error checking
if($#ARGV < 4){
	print "Usage: <ref/miss> <path/dir> <cache> <file_name> <structures>\n";
	exit (1);
}
$numargs = $#ARGV + 1;

$option = $ARGV[0];
if($option ne "ref" && $option ne "miss"){
	print "Usage: First argument not an option\n";
	print "Usage: <ref/miss> <path/dir> <cache> <file_name> <structures>\n";
	exit(1);
}

$dir = $ARGV[1];
$cache = $ARGV[2];
$flname = $ARGV[3];
@structures;

$i = 0;
$file = $cache . "." . $flname . "." . "struct.snapshot";
$fullpath = $dir . "/" . $file . "." . $i;
print " $fullpath\n";
if(!(-e $fullpath)){
	print "\tPath doesn't exist\n";
	exit(1);
}

print "Adding structures:\n";
for($i=4; $i < $numargs; $i++){
	#expand this structure?
	if(index($ARGV[$i], '+') >= 0){
		print "$ARGV[$i] (expanding)\n";
		$ARGV[$i] =~ s/\+//g;	
	
		# add to %expand hash
		$expand{$ARGV[$i]} = 1;
		for($j=0; $j<7; $j++){
			$hname = $ARGV[$i] . "-" . $j;
			print "\t\t$hname\n";
			$structures{$hname} = 0;
		}
	}
	else{	
		print "\t$ARGV[$i]\n";
		$structures{$ARGV[$i]} = 0;
	}
	
	push @structures, $ARGV[$i];
}

print "\tOTHER\n";
push @structures, "_OTHER_";
$structures{"OTHER"} = 0;

print "\nDone!\n\n";

#create directory for graphs
$graphsdir = $dir . "/" . $cache . ".seggraphs";
mkdir $graphsdir;

#
# --- create .dat file and print header data ---
#

$tmp = $graphsdir . "/tmp_struct.dat";
open(FDdat, ">$tmp") or die "Can't open tmp_struct.dat for writing\n";
print FDdat "Refs";
for $struct (keys %structures) {
	print FDdat " $struct";
}
print FDdat "\n";

# process file
$tics = 0;
$fcnt = 0;
$lc = 0;
print "Processing files in $dir\n";
while((-e $fullpath)){
	$fullpath = $dir . "/" . $file . "." . $fcnt;

	#for every line in file, look at structure
	#add to list
	if(!open(FD, $fullpath)){
			goto DONE;
	}
	print "Processing: \n\t$fullpath\n";

	$_ = <FD>;
	chomp($_);
	@line = split(' ', $_);
	if($line[0] ne "Refcount"){
		print "Something is wrong at refcount\n";
		print "line[0] = $line[0]\n";
		exit(1);
	}
	else{
		$refcount = $line[1];
		if($fcnt == 0){
			$tics = $refcount;
		}
	}

	#look through file structures
	while(<FD>){
		chomp($_);
		@line = split (' ', $_);

		if($line[0] eq "structure:"){
			$structname = $line[1];
			$ref = $line[4]; $miss = $line[6];
		
			#option dependent ref or miss
			if($option eq "ref"){
				$val = $ref;
			}
			else{
				$val = $miss;
			}
			
			# other or struct	
			if((!exists $structures{$structname}) && (!exists $expand{$structname})){
				$structures{"OTHER"} += $val;
				if($val > 0){
					$touched{"OTHER"} = 1;
				}
			}
			else{
				if(!exists $expand{$structname} ){
					$structures{$structname} = $val;
					
					if($val > 0){
						$touched{$structname} = 1;
					}
				}	
				else{
					print "Expanding $structname\n";
					#add all H-nums
					while(<FD>){
						chomp($_);
						@line = split (' ', $_);
						
						if($line[0] eq "H"){
							$hname = $structname . "-" . $line[1];
							print "\t $hname\n";
								
							$val = ($option eq "ref") ? $line[2] : $line[4];
							$structures{$hname} = $val;
							
							if($val > 0){
								$touched{$hname} = 1;
							}
						}
						elsif($line[0] eq "structure:"){
							seek(FD, -(length($_)+1), 1);
							last;
						}
					} #while _ expand
				}
			}
		}
	}

	#write results to .dat file
	print FDdat $refcount;
	for $struct (keys %structures) {
		print FDdat " " . $structures{$struct};
		$structures{$struct} = 0;
	}
	print FDdat "\n";

	$fcnt++;
}

DONE:
close(FD);
close(FDdat);

print "Finished processing files\n";
#
# --- make tmp.gpi
#
$tmp = $graphsdir . "/tmp.gpi";
open FD, ">$tmp" or die "can't open tmp gpi for writing\n";

if($option eq "ref"){
	$gpout = $graphsdir . "/" . $cache . "." . $flname . ".segref" . ".eps";
}
else{
	$gpout = $graphsdir . "/" . $cache . "." . $flname . ".segmiss" . ".eps";
}

print FD "set term postscript eps color font \"Helvetica,8\" size 7.0, 3.0\n";	
print FD "set output '$gpout'\n\n";	

print FD "set border 0 front linetype -1 linewidth 0.00\n";
print FD "set boxwidth 0 absolute\n";
print FD "set style fill solid 1.00 noborder\n\n";

print FD "set key out center top horizontal noreverse enhanced autotitles columnhead box\n";
print FD "set key invert samplen 4 spacing 1.00 width 0 height 0\n";
print FD "set key font \",6\"\n\n";

print FD "set grid nopolar\n";
print FD "set grid layerdefault linetype 0 linewidth 1.00, linetype 0 linewidth 1.00\n\n";
print FD "set grid noxtics nomxtics ytics nomytics noztics nomztics \\\n";
print FD "\tnox2tics nomx2tics noytics nomy2tics nocbtics\n";
print FD "set grid ytics mxtics\n";

print FD "set style histogram rowstacked title offset character 0, 0, 0\n";
print FD "set datafile missing '-'\n\n";

print FD "set style data histogram\n";
print FD "set style fill solid border -1\n\n";

print FD "set xtics border in scale 0,0 nomirror rotate by -45 offset 0\n";
print FD "set xtics norangelimit font \",8\"\n\n";

print FD "set xtics ()\n";
print FD "set ytics ()\n\n";
if($option eq "ref"){
	print FD "set title \"Structure (references)\"\n";
	print FD "set ylabel \"References\"\n";
}
else{
	print FD "set title \"Structure (misses)\"\n";
	print FD "set ylabel \"Misses\"\n";
}
print FD "set title font \"Helvetica,9\"\n";
print FD "set ylabel font \"Helvetica,6\"\n";
if($option eq "ref"){
	print FD "set xlabel \"References per $tics Load, Store, Modifies\"\n";
}
else{
	print FD "set xlabel \"Misses per $tics Load, Store, Modifies\"\n";
}

print FD "set xlabel font \"Helvetica,6\"\n\n";

$tmp = $graphsdir . "/tmp_struct.dat";
print FD "mydata='$tmp'\n\n";
$hsize = keys(%structures);
$hsize++;

#
# --- print plots but only for "touched" variables
#
# Ugh, this is a bit of a tricky way of doing it, but I have to open the dat file and read
# the column headers. Some are empty and I have no clue how to tell gnuplot not to read them
# aside from simply ignoring the plots below. So, read them and compared them to "touched" values

$tmpdat = $graphsdir . "/tmp_struct.dat";
open FDdat, "$tmpdat" or die "can't open $tmpdat for writing.\n";
$tmpline = <FDdat>;
close(FDdat);

@line = split(' ', $tmpline);
shift(@line);
$structs = @line;
$i = 2;
$lc = 0;

&makepalette();

print "Adding plots for:\n";
print FD "plot ";
foreach $tmp (@line){
	if(exists $touched{$tmp}){
		print "\t\t$tmp\n";
		if($i == 2){
			$lcstring = getcolor(($lc % 36));
			print FD "mydata using $i:xtic(int(\$0)\%5==0\?stringcolumn(1):\"\") lt 1 lc rgb \"$lcstring\" title '$tmp', \\\n";
		}
		else{
			$lcstring = getcolor(($lc % 36));
			print FD "\tmydata using $i lt 1 lc rgb \"$lcstring\" title '$tmp', \\\n";
		}
	}
	$i++; $lc++;
}
seek(FD, -4, 1);
print FD "\n\n\n\n\n";
close(FD);
print "\nCreated .gpi in $graphsdir\n";

#run gnuplot
$tmp = $graphsdir . "/tmp.gpi";
system("gnuplot", $tmp);
#unlink $tmp;

print "Finishing .tex\n";

$tmp = $graphsdir . "/tmp.tex";
open FD, ">$tmp" or die "can't open $tmp for writing.\n";

print FD "\\documentclass\{beamer\}\n";
print FD "\\usepackage\{graphicx\}\n";
print FD "\n";
print FD "\\setbeamersize\{text margin left=.1cm,text margin right=.1cm\}\n";
print FD "\n";
print FD "\\begin\{document\}\n";
print FD "\n";
print FD "\\frame\{\n";
print FD "\\includegraphics[width=1.0\\textwidth]\{$gpout\}\n";
print FD "\}\n";
print FD "\n";
print FD "\\end\{document\}\n";
close(FD);

$cmd = "latex";
push @args, "--output-directory=$graphsdir/";
push @args, "$graphsdir/tmp.tex";
system $cmd, @args;
@args = ();

$cmd = "dvips";
push @args, "$graphsdir/tmp.dvi"; 
system $cmd, @args;
@args = ();

$cmd = "ps2pdf";
push @args, "tmp.ps";
push @args, "$graphsdir/tmp.pdf";
system $cmd, @args;
@args = ();

if($option eq "ref"){
	$cmd = "mv";
	push @args, "$graphsdir/tmp.pdf";
	push @args, "$graphsdir/tmp-ref.pdf";
	system $cmd, @args;
}
else{
	$cmd = "mv";
	push @args, "$graphsdir/tmp.pdf";
	push @args, "$graphsdir/tmp-miss.pdf";
	system $cmd, @args;
}
@args = ();

unlink "tmp.ps";
$tmp = $graphsdir . "/tmp.aux";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.dvi";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.log";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.nav";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.out";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.snm";
unlink $tmp; 
$tmp = $graphsdir . "/tmp.toc";
unlink $tmp; 

print "I'm all done here...enjoy\n";
exit(1);

# subroutines and stuff
sub getcolor {
	
	($index) = @_;
	$colorhexstring = $colorpalette[$index];

	return $colorhexstring;
}

sub makepalette {

	#set standard palette -16
	push @colorpalette, "#0000ff";#blue
	push @colorpalette, "#ff0000";#red
	push @colorpalette, "#008000";#green
	push @colorpalette, "#00ff00";#lime
	push @colorpalette, "#ffff00";#yellow
	push @colorpalette, "#000080";#navy
	push @colorpalette, "#008080";#teal
	push @colorpalette, "#00ffff";#aqua
	push @colorpalette, "#800000";#maroon
	push @colorpalette, "#800080";#purple
	push @colorpalette, "#808000";#olive
	push @colorpalette, "#ffffff";#white
	push @colorpalette, "#808080";#gray
	push @colorpalette, "#cd5c5c";#indian red
	push @colorpalette, "#c0c0c0";#silver
	push @colorpalette, "#ff00ff";#fuchsia

	#additional colors -20
	push @colorpalette, "#a9bde6";#blue
	push @colorpalette, "#a6ebb5";#green
	push @colorpalette, "#f9b7b0";#red
	push @colorpalette, "#f9e0b0";#brown
	
	push @colorpalette, "#7297e6";#blue
	push @colorpalette, "#67eb84";#green
	push @colorpalette, "#f97a6d";#red
	push @colorpalette, "#f9c96d";#brown
	
	push @colorpalette, "#1d4599";#blue
	push @colorpalette, "#11ad34";#green
	push @colorpalette, "#e62b17";#red
	push @colorpalette, "#e69f17";#brown
	
	push @colorpalette, "#2f3f60";#blue
	push @colorpalette, "#2f6c3d";#green
	push @colorpalette, "#8f463f";#red
	push @colorpalette, "#8f743f";#brown

	push @colorpalette, "#031a49";#blue
	push @colorpalette, "#025214";#green
	push @colorpalette, "#6d0d03";#red
	push @colorpalette, "#6d4903";#brown
}



