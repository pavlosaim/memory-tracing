#!/usr/local/bin/perl

#print "\nPerl GNUPLOT Script Generator for Gleipnir generated plots\n";
#print "\nAuthor: Tomislav Janjusic\n";

#error checking
if($#ARGV < 3){
	print "Usage: <ref/miss> <path/dir> <cache> <file_name>\n";
	exit (1);
}
$numargs = $#ARGV + 1;

$option = $ARGV[0];
if($option ne "ref" &&  $option ne "miss"){
	print "Usage: First argument not an option\n";
	exit(1);
}

$dir = $ARGV[1];
$cache = $ARGV[2];
$flname = $ARGV[3];

@structures;
for($i=4; $i<$numargs; $i++){
	push @structures, $ARGV[$i];
}

$file = $cache . "." . $flname . "." . "segments";
$fullpath = $dir . "/" . $file;
if(!(-e $fullpath)){
  print "File doesn't exist:\n\t$fullpath\n";
	exit(1);
}

#create directory for graphs
$graphsdir = $dir . "/" . $cache . ".graphs";
mkdir $graphsdir;

#print the dir/files we're looking at
print "Processing files in $dir\n";

$yrangem = 0;
$yrangerefs = 0;
$tics = 0;

#get $tics
	$tmp = $fullpath;
	open FD, "$tmp" or die "can't open $tmp for reading\n";
	$myline = <FD>;
	$myline = <FD>;
	chomp($myline);
	@line = split(' ', $myline);
$tics = $line[0];



#																									 #
# -- print data , build script, and run gnuplot -- #
# 
# -- make tmp.dat

$tmp = $fullpath;

# -- make tmp.gpi
$tmp = $graphsdir . "/tmp.gpi";
open FD, ">$tmp" or die "can't open tmp gpi for writting\n";

if($option eq "ref"){
	$gpout = $graphsdir. "/" . $file . "-ref" . ".eps";
}
else{
	$gpout = $graphsdir. "/" . $file . "-miss" . ".eps";
}

print FD "set term postscript eps color font \"Helvetica,8\" size 7.0, 3.0\n";	
print FD "set output '$gpout'\n\n";	

print FD "set border 0 front linetype -1 linewidth 0.00\n";
print FD "set boxwidth 0 absolute\n";
print FD "set style fill solid 1.00 noborder\n\n";

print FD "set key out center top horizontal noreverse enhanced autotitles columnhead box\n";
print FD "set key invert samplen 4 spacing 0.75 width 0 height 0\n";
print FD "set key font \",6\"\n\n";

print FD "set grid nopolar\n";
print FD "set grid layerdefault linetype 0 linewidth 1.00, linetype 0 linewidth 1.00\n\n";
print FD "set grid noxtics nomxtics ytics nomytics noztics nomztics \\\n";
print FD "\tnox2tics nomx2tics noytics nomy2tics nocbtics nomcbtics\n";
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
	print FD "set title \"Stack Heap Global (references)\"\n";
	print FD "set ylabel \"References\"\n";
}
else{
	print FD "set title \"Stack Heap Global (misses)\"\n";
	print FD "set ylabel \"Misses\"\n";
}
print FD "set title font \"Helvetica,9\"\n";
print FD "set ylabel font \"Helvetica,6\"\n";
print FD "set xlabel \"Tics per $tics Load, Store, Modifies\"\n";
print FD "set xlabel font \"Helvetica,6\"\n\n";

print FD "set style line 1 lt 1 lw 0 pt 0 linecolor rgb \"green\"\n";
print FD "set style line 2 lt 1 lw 0 pt 0 linecolor rgb \"blue\"\n";
print FD "set style line 3 lt 1 lw 0 pt 0 linecolor rgb \"red\"\n\n";
print FD "set style increment user\n";
print FD "mydata='$fullpath'\n\n";

if($option eq "ref"){
	print FD "plot mydata using 4:xtic(int(\$0)\%5==0\?stringcolumn(1):\"\") t column(4), \\\n";
	print FD "\tmydata using 3 title column(3), \\\n";
	print FD "\tmydata using 2 title column(2)\n";
}
else{
	print FD "plot mydata using 8:xtic(int(\$0)\%5==0\?stringcolumn(1):\"\") t column(8), \\\n";
	print FD "\tmydata using 7 title column(7), \\\n";
	print FD "\tmydata using 6 title column(6)\n";
}
close(FD);

print "Created .gpi in $graphsdir\n";

$tmp = $graphsdir . "/tmp.tex";
open FD, ">$tmp" or die "can not open $tmp for writing.\n";

#run gnuplot
$tmp = $graphsdir . "/tmp.gpi";
system("gnuplot", $tmp);	
unlink $tmp;

print "Finishing .tex\n";

$tmp = $graphsdir . "/tmp.tex";

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

exit(1);
