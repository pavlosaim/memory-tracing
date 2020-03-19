#!/usr/local/bin/perl

#print "\nPerl GNUPLOT Script Generator for Gleipnir generated plots\n";
#print "\nAuthor: Tomislav Janjusic\n";

#error checking
if($#ARGV < 3){
	print "Usage: <path/dir> <cache> <file_name> <functions>\n";
	exit (0);
}
$numargs = $#ARGV + 1;

#set prelims
$numsets = 64;
#$numsets = 128;
#$numsets = 1024;
$xtics = 16;
$mxtics = 8;

$dir = $ARGV[0];
$cache = $ARGV[1];
$flname = $ARGV[2];
@functions;

for($i=3; $i<$numargs; $i++){
	push @functions, $ARGV[$i];
}

#print the dir/files we're looking at
print "Processing files in $dir\n";

$i = 0;
$file = $cache . "." . $flname . "." . "function.snapshot";
$fullpath = $dir . "/" . $file . "." . $i;
if(!(-e $fullpath)){
	print "Path doesn't exist\n";
	exit(1);
}

#print $fullpath . "\n";


#create directory for graphs
$graphsdir = $dir . "/" . $cache . ".graphs";
mkdir $graphsdir;

#if file exists, process for each function
$fcnt = 0;
$lc = 0;
while((-e $fullpath)){
	$fullpath = $dir . "/" . $file . "." . $fcnt;

	$yrangem = 0;
	$yrangerefs = 0;

	# for each function_name argument get the data
	foreach $function (@functions){
		#open the file; break loop if !open()	
		if(!open(FD, $fullpath)){
			goto DONE;
		}	
		print "Processing $function in: \n\t$fullpath\n";

		# look through file for $function
		while(<FD>){
			chomp($_);
			@line = split(' ', $_);

			#reset i,j
			$i = 0;
			$j = 0;
	 
			if($line[1] eq $function){
				if (! exists $colorhash {$function} ){
					$colorhash {$function} = $lc;
					$lc++;
				}
			
				$vars = $line[5];
				push @flist, $function;
				printf "\t Processing $vars variables\n";

				$_ = <FD>;
				chomp($_);
				@line = split(' ', $_);
			
				# check for odd case EOF or, empty function? 
				if(!defined($line[0])){
					for( $i = 0; $i < $numsets; $i++){
						push @{$refslist[$i]}, 0;
						push @{$misseslist[$i]}, 0;
					}
					last;
				}
				elsif($line[0] eq "function:" or $line[0] eq "variable:"){
						for( $i = 0; $i < $numsets; $i++){
						push @{$refslist[$i]}, 0;
						push @{$misseslist[$i]}, 0;
					}
					last;
				}
				else{
					$i = 0;
					while($line[0] ne "function:" and $line[0] ne "variable:" and defined($line[0])){
						# must be set number, push to array
						if($line[0]>$i){
							for( $i; $i < $line[0]; $i++){
								push @{$refslist[$i]}, 0;
								push @{$misseslist[$i]}, 0;
							}
							if($yrangem < $line[3]){
								$yrangem = $line[3];
							}
							if($yrangerefs < $line[1]){
								$yrangerefs = $line[1];
							}
							push @{$refslist[$line[0]]}, $line[2];
							push @{$misseslist[$line[0]]}, $line[3];
							$i++;
						}
						else{
							if($yrangem < $line[3]){
								$yrangem = $line[3];
							}
							if($yrangerefs < $line[1]){
								$yrangerefs = $line[1];
							}
							push @{$refslist[$i]}, $line[2];
							push @{$misseslist[$i]}, $line[3];
							$i++;
						}
						
						$_ = <FD>;
						chomp($_);
						@line = split(' ', $_);
					} #--- while
					if($i<$numsets){
						for($i; $i<$numsets; $i++){
							push @{$refslist[$i]}, 0;
							push @{$misseslist[$i]}, 0;
						}
					}
				} #-- else (! empty function)

				# get variables
				#next;
				for($cvars = 0; $cvars < $vars; $cvars++){
					$tmp = $function . "-" . $line[1];	
					push @flist, $tmp;

          if (! exists $colorhash {$tmp} ){
            $colorhash {$tmp} = $lc;
            $lc++;
          }
				
					$_ = <FD>;
					chomp($_);
					@line = split(' ', $_);

					$i = 0;
					while($line[0] ne "function:" and $line[0] ne "variable:" and defined($line[0])){
						# must be set number, push to array
						if($line[0]>$i){
							for( $i; $i < $line[0]; $i++){
								push @{$refslist[$i]}, 0;
								push @{$misseslist[$i]}, 0;
							}
							if($yrangem < $line[3]){
								$yrangem = $line[3];
							}
							if($yrangerefs < $line[1]){
								$yrangerefs = $line[1];
							}
							push @{$refslist[$line[0]]}, $line[1];
							push @{$misseslist[$line[0]]}, $line[3];
							$i++;
						}
						else{
							if($yrangem < $line[3]){
								$yrangem = $line[3];
							}
							if($yrangerefs < $line[1]){
								$yrangerefs = $line[1];
							}
							push @{$refslist[$i]}, $line[1];
							push @{$misseslist[$i]}, $line[3];
							$i++;
						}
						
						$_ = <FD>;
						chomp($_);
						@line = split(' ', $_);
					} #--- while
					if($i<$numsets){
						for($i; $i<$numsets; $i++){
							push @{$refslist[$i]}, 0;
							push @{$misseslist[$i]}, 0;
						}
					}
				} #-- variables
				last;
			} #-- function
		} #-- while<FD>
			
		close(FD);
	} #-- for each $function

	if(scalar(@flist) == 0){
    $tmp = "nothing";
		push @flist, $tmp;

    if (! exists $colorhash {$tmp} ){
			$colorhash {$tmp} = $lc;
			$lc++;
    } 
    for($i=0; $i<$numsets; $i++){
			push @{$refslist[$i]}, 110;
			push @{$misseslist[$i]}, 11;
		}
	}
			
#																									 #
# -- print data , build script, and run gnuplot -- #
# 
# -- make tmp.dat
	$tmp = $graphsdir . "/tmprefs.dat";
	open(FDh, ">$tmp") or die "can't open tmp refs for writting\n";
	$tmp = $graphsdir . "/tmpmisses.dat";
	open(FDm, ">$tmp") or die "can't open tmp misses for writting\n";

	print FDh "Sets ";
	print FDm "Sets ";
	foreach $tmp (@flist){
		print FDh $tmp . " ";
		print FDm $tmp . " ";
	}

	print FDh "\n";
	print FDm "\n";
	for($i = 0; $i < $numsets; $i++){
		print FDh $i . " ";
		print FDm $i . " ";
		$j = 0;
		foreach $tmp (@flist) {
			print FDh $refslist[$i][$j] . " ";	
			print FDm $misseslist[$i][$j] . " ";	
			$j++;
		}
		print FDh "\n";
		print FDm "\n";
	}

	close(FDh);
	close(FDm);

# -- make tmp.gpi
	$tmp = $graphsdir . "/tmp.gpi";
	open FD, ">$tmp" or die "can't open tmp gpi for writting\n";

	$gpout = $graphsdir. "/" . $file . "." . $fcnt . ".eps";
	print FD "set grid\n";	
	print FD "set nokey\n";	

	print FD "set term postscript eps color font \"Helvetica,8\" size 5.0, 3.5\n";	
	print FD "set output '$gpout'\n\n";	
	
#set xtics border in scale 0,0 nomirror rotate by -45 offset 0 #character 0, 0, 0 autojustify
#set xtics norangelimit font ",8"

	print FD "set xrange [-0.5:$numsets-0.5]\n";	
	print FD "set xtics $xtics border in scale 0.75,0.5 nomirror rotate by -45 offset 0,0.5\n";	
	print FD "set xtics norangelimit font \",10\"\n";
	print FD "set mxtics $mxtics\n";	
	print FD "set xlabel \"cache sets\"\n";	
	print FD "set multiplot\n";
  print FD "set pointsize 0.5\n";

	if($yrangem == 0){
    $yrangem++;
  }
  else{
    $yrangem*=2;
  }

	print FD "set logscale y 10\n";
	print FD "set yrange [0.1:$yrangem]\n";

	print FD "set ylabel \"misses\"\n";	
	print FD "set title \"\"\n";	
	print FD "set size 1,0.4\n";	
	print FD "set origin 0,0\n";	
	print FD "set lmargin 13\n";	
	print FD "set bmargin 3\n";	
	print FD "set tmargin 0\n\n";	

	print FD "drefs='$graphsdir/tmprefs.dat'\n";
	print FD "dmisses='$graphsdir/tmpmisses.dat'\n\n";
	
	&makepalette();
	
	#deal with func_no weirdness
  $flistsz = scalar(@flist);
	if($flistsz>2){
    $tmp = shift(@flist);
    $lncolor = $colorhash {$tmp};
		$lcstring = &getcolor($lncolor);	
    push(@flist_, $tmp);
    $i = 3;
		print FD "plot dmisses using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'";
		
		for($i; $i < $flistsz+2; $i++) {
      $tmp = shift(@flist);
      $lncolor = $colorhash {$tmp};
			$lcstring = &getcolor($lncolor);	
      push(@flist_, $tmp);
      print FD ", \\\n";
			print FD "\t dmisses using 1:$i lt 1 lc rgb \"$lcstring\" w lp t '$tmp'";	
    }
    print FD "\n";
	}
	elsif(scalar(@flist == 2)){
    $tmp = shift(@flist);
    $lncolor = $colorhash {$tmp};
		$lcstring = &getcolor($lncolor);	
    push (@flist_, $tmp);
		print FD "plot dmisses using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp', \\\n";
		
		$tmp = shift(@flist);
    $lncolor = $colorhash {$tmp};
		$lcstring = &getcolor($lncolor);	
    push (@flist_, $tmp);
    print FD "\tdmisses using 1:3 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'\n\n"; 
	}
	else{
    $tmp = shift(@flist);
    $lncolor = $colorhash {$tmp};
		$lcstring = &getcolor($lncolor);	
    push(@flist_, $tmp);
		print FD "plot dmisses using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'\n\n";
	}
  print FD "\n";
	print FD "set title \"$file.$fcnt\"\n";	
	print FD "set key out center top horizontal enhanced autotitles columnhead box\n";
	print FD "set key samplen 4 spacing 1 width 0 height 0\n";
	print FD "set key font \",8\"\n";
	print FD "set ylabel \"refs\"\n\n";

  if($yrangerefs == 0){
    $yrangerefs+=100;
  }
  else{
    $yrangerefs *= 10;
  }

	print FD "set yrange [0.1:$yrangerefs]\n";

	print FD "set size 1,0.5\n";
	print FD "set origin 0,0.4\n";
	print FD "set bmargin 1\n";
	print FD "set tmargin 0\n";
	print FD "set xtics $xtics border in scale 1,0.5 nomirror rotate by -45 offset 0,-0.5\n";
	print FD "set mxtics $mxtics\n";
	print FD "set xlabel \"\"\n\n";

  $flistsz = scalar(@flist_);
	if($flistsz > 2){
    $i = 3;
    $tmp = shift(@flist_);
    $lncolor = $colorhash  {$tmp};
   	$lcstring = &getcolor($lncolor); 
		push (@flist_, $tmp);
    print FD "plot drefs using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'";
    
		for($i; $i < $flistsz+2; $i++) {
      $tmp = shift(@flist_);
      $lncolor = $colorhash {$tmp};
   		$lcstring = &getcolor($lncolor); 
      print FD ", \\\n";
      print FD "\t drefs using 1:$i	lt 1 lc rgb \"$lcstring\" w lp t '$tmp'";
    }
	}
	elsif(scalar(@flist_) == 2){
    $tmp = shift(@flist_);
    $lncolor = $colorhash {$tmp};
   	$lcstring = &getcolor($lncolor); 
		print FD "plot drefs using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp', \\\n";
    
		$tmp = shift(@flist_);
    $lncolor = $colorhash {$tmp};
   	$lcstring = &getcolor($lncolor); 
		print FD "\t dmisses using 1:3 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'\n\n";
	}
	else{
    $tmp = shift(@flist_);
    $lncolor = $colorhash {$tmp};
   	$lcstring = &getcolor($lncolor); 
		print FD "plot drefs using 1:2 lt 1 lc rgb \"$lcstring\" w lp t '$tmp'\n\n";
	}

	close(FD);
	
	$tmp = $graphsdir . "/tmp.tex";
	open FD, ">>$tmp" or die "can not append to \n$tmp\n";

	#run gnuplot
	$tmp = $graphsdir . "/tmp.gpi";
	system("gnuplot", $tmp);	

	$tmp = $graphsdir . "/tmprefs.dat";
	unlink $tmp;

	$tmp = $graphsdir . "/tmpmisses.dat";
	unlink $tmp;

	$tmp = $graphsdir . "/tmp.gpi";
	unlink $tmp;

	@flist = ();
  @flist_ = ();
	@refslist = ();
	@misseslist = ();
	
	$fcnt++;
} #-- (-e file)

DONE:

print "No more files to process\n";
print "Finishing .tex\n";

$tmp = $graphsdir . "/tmp.tex";
open FD, ">$tmp" or die "can't create tex file";

print FD "\\documentclass\{beamer\}\n";
print FD "\\usepackage\{graphicx\}\n";
print FD "\n";
print FD "\\setbeamersize\{text margin left=.1cm,text margin right=.1cm\}\n";
print FD "\n";
print FD "\\begin\{document\}\n";
print FD "\n";
for($i=0; $i<$fcnt; $i++){
	print FD "\\frame\{\n";
	print FD "\\includegraphics[width=1.0\\textwidth]\{$graphsdir/$file.$i.eps\}\n";
	print FD "\}\n";
	print FD "\n";
}
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


