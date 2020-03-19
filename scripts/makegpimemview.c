/* virtual memory view app
 * author: Tomislav Janjusic
 * inst:   UNT / ORNL
 * date modified: aug. 19th, 2013
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN 0
#define MAX ((1UL << 63)-1)

#define ALIGN(size) \
	( ( (size + (1024 - 1) ) >> 10 ) \
                           << 10 )
typedef unsigned long ULong;
typedef long Long;

ULong stackmin = MAX;
ULong stackmax = MIN;
ULong stacksz = 0;

ULong heapmin = MAX;
ULong heapmax = MIN;
ULong heapsz = 0;

ULong globalmin = MAX;
ULong globalmax = MIN;
ULong globalsz = 0;

ULong physmin = MAX;
ULong physmax = MIN;
ULong physsz = 0;

typedef struct _type{
	unsigned int size;
	ULong vaddr;
	ULong paddr;
	char segment;
} lstruct;

lstruct values;
ULong linenumber = 0;

char* stackview = NULL;
char* heapview = NULL;
char* globalview = NULL;

void updatememview(char *linebuffer);
void processline(char* linebuffer);
void updatememsize(void);

int main(int argc, char* argv[])
{
	char c;
	char filepath[128];
	char linebuffer[1024];
	int i = 0;
	FILE *fd;

	if(argc < 2){
		printf("Error: need at least 1 argument (usually file path)\n");
		exit(0);
	}

	strcpy(filepath, argv[1]);

	printf("stackmin = %lu\n", stackmin);
	printf("stackmax = %lu\n", stackmax);

	printf("\n");
	printf("heapmin = %lu\n", heapmin);
	printf("heapmax = %lu\n", heapmax);

	printf("\n");
	printf("globalmin = %lu\n", globalmin);
	printf("globalmax = %lu\n", globalmax);

	printf("\n");
	printf("physmin = %lu\n", physmin);
	printf("physmax = %lu\n", physmax);



	printf("\n");
	printf("filepath = \"%s\"\n", filepath);

	if(!(fd = fopen(filepath, "r"))){
		printf("Error: cannot open path \"%s\"\n", filepath);
	}

	/* process in 2 phases */
	/* phase 1*/
	printf("\nPhase 1...");
	fflush(stdout);

	/* skip first line */
	c = getc(fd);
	do{
		c = getc(fd);
	}
	while(c != '\n');
	linenumber++;

	/* start processing */	
	while(c != EOF){
		c = getc(fd);
	
		/* skip line? */
		if(c == 'X'){
			do{
				c = getc(fd);
			}
			while(c != '\n');
			linenumber++;
			continue;
		}
		
		/* process ? */
		if(c == '\n'){
			linebuffer[i] = '\0';
			linenumber++;
			processline(linebuffer);
			i = 0;
			continue;
		}
		else	
		{
			linebuffer[i] = c;
			i++;
		}
	}

	updatememsize();

	printf("done\n");
	
	printf("\nstackmin = %lx\n", stackmin);
	printf("stackmax = %lx\n", stackmax);
	printf("stacksz = %lu\n", stacksz);

	printf("\n");
	printf("heapmin = %lx\n", heapmin);
	printf("heapmax = %lx\n", heapmax);
	printf("heapsz = %lu\n", heapsz);

	printf("\n");
	printf("globalmin = %lx\n", globalmin);
	printf("globalmax = %lx\n", globalmax);
	printf("globalsz = %lu\n\n", globalsz);

	printf("\n");
	printf("physmin = %lx\n", physmin);
	printf("physmax = %lx\n", physmax);
	printf("physsz = %lu\n\n", physsz);


	/* make views */
	stackview = calloc(stacksz, sizeof(char));
	if(stackview == NULL){
		printf("Error, calloc return 0\n");
	}

	heapview = calloc(heapsz, sizeof(char));
	if(heapview == NULL){
		printf("Error, calloc return 0\n");
	}

	globalview = calloc(globalsz, sizeof(char));
	if(globalview == NULL){
		printf("Error, calloc return 0\n");
	}

	/* phase 2*/	
	fseek(fd, 0, SEEK_SET);
	
	printf("\nPhase 2...\n");

	linenumber = 0;
	/* skip first line */
	c = getc(fd);
	do{
		c = getc(fd);
	}
	while(c != '\n');
	linenumber++;

	/* start processing phase 2*/
	i = 0;
	while(c != EOF){
		c = getc(fd);
	
		/* skip line? */
		if(c == 'X'){
			do{
				c = getc(fd);
			}
			while(c != '\n');
			linenumber++;
			continue;
		}
		
		/* process ? */
		if(c == '\n'){
			linenumber++;
			linebuffer[i] = '\0';
			updatememview(linebuffer);
			i = 0;
			continue;
		}
		else	
		{
			linebuffer[i] = c;
			i++;
		}
	}

	printf("done\n");
	fclose(fd);

	/* process stack, heap, and global */
	printf("Creating 'memview.xxx' datafiles\n");
	
	/* stack */
	fd = fopen("memview.stack", "w+");
	Long szcnt = (Long) stacksz-1;

	Long X = (ALIGN(stacksz)/1024) + 
					 (ALIGN(heapsz)/1024) +
					 (ALIGN(globalsz)/1014);
	Long Y;
	
	Long xrange1 = X;
	Long xrange2 = X;

	printf("\nStack X at: %lu\n", X);
	printf("Stack size: %lu\n", szcnt);

	while(szcnt >= 0){
		for(Y = 1023; Y >= 0 && szcnt>=0; Y--){
			if(stackview[szcnt] == 1){
				fprintf(fd, "%lu %lu\n", X, Y);
			}
			szcnt--;
		}
		X--;
	}
	fclose(fd);

	/* heap */
	fd = fopen("memview.heap", "w+");
	szcnt = (Long) heapsz-1;

	X = (ALIGN(heapsz)/1024) +
		  (ALIGN(globalsz)/1024);
	
	printf("Heap X at: %lu\n", X);
	printf("Heap size: %lu\n", szcnt);

	while(szcnt >= 0){
		for(Y = 1023; Y >= 0 && szcnt >= 0; Y--){
			if(heapview[szcnt] == 1){
				fprintf(fd, "%lu %lu\n", X, Y);
			}
			szcnt--;
		}
		X--;
	}
	fclose(fd);

	/* global */
	fd = fopen("memview.global", "w+");
	szcnt = (Long) heapsz-1;

	X = (ALIGN(globalsz)/1024);
	
	printf("Global X at: %lu\n", X);
	printf("Global size: %lu\n", szcnt);

	while(szcnt >= 0){
		for(Y = 1023; Y >= 0 && szcnt >= 0; Y--){
			if(globalview[szcnt] == 1){
				fprintf(fd, "%lu %lu\n", X, Y);
				if(xrange1 > X){
					xrange1 = X;
				}
			}
			szcnt--;
		}
		X--;
	}
	fclose(fd);

	printf("Data files ready.\n");

	
	printf("\nCreating .gpi files.\n");
	fd = fopen("memview.gpi", "w+");


	fprintf(fd,
"set term postscript eps color font \"Helvetica,8\" size 7.0, 3.0\n"
"set output 'memview.eps'\n\n"
"set border 0 front linetype -1 linewidth 0.0\n\n"

"set key out center top horizontal noreverse enhanced autotitles columnhead box\n"
"set key invert samplen 4 spacing 1.00 width 0 height 0\n"
"set key font \",6\"\n\n"

"set grid nopolar\n"
"set grid layerdefault linetype 0 linewidth 1.00, linetype 0 linewidth 1.00\n\n"

"set title \"Virtual Memory View\"\n"
"set xlabel \"Virtual Memory Space\"\n"
"set xtics 64 border in scale 1,0.5 nomirror rotate by -45 offset 0,-0.5\n"
"set ytics 64\n\n"
"set xrange [%ld : %ld]\n"
"set yrange [0 : 1024]\n\n"
"plot 'memview.stack' using 1:2 pt 5 ps 0.2 lc rgb \"red\" t 'Stack', \\\n"
"     'memview.heap' using 1:2 pt 5 ps 0.2 lc rgb \"blue\" t 'Heap', \\\n"
"     'memview.global' using 1:2 pt 5 ps 0.2 lc rgb \"green\" t 'Global' \n", xrange1-1, xrange2+1);

	fclose(fd);

	printf("Finished creating .gpi files.\n\nI'm all done here...\n");

return 0;
}

void updatememsize(void)
{
	stacksz = stackmax - stackmin;
	globalsz = globalmax - globalmin;
	heapsz = heapmax - heapmin;
	physsz = physmax - physmin;

	return;
}

void updatememview(char *linebuffer)
{
	char c;
	int i=0;
	
	/* process vaddr */
	i = 2;
	c = linebuffer[i];
	values.vaddr = c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.vaddr *= 16;
		values.vaddr += c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
		i++;
	}
	i++;
	
	/* process paddr */
	c = linebuffer[i];
	values.paddr = c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.paddr *= 16;
		values.paddr += c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
		i++;
	}
	i++;

	/* process size */
	c = linebuffer[i];
	values.size = (int) linebuffer[i] - 48;
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.size *= 10;
		values.size += (int) linebuffer[i] - 48;
		i++;
	}
	i++;

	/* process segment */
	values.segment = linebuffer[i+2];

	/* update memview */
	switch(values.segment){
	case ('S'):
		for(i=0; i<values.size; i++){
			stackview[(values.vaddr - stackmin) + i]=1;
		}
		break;
	case('H'):
		for(i=0; i<values.size; i++){
			heapview[(values.vaddr - heapmin) + i]=1;
		}
		break;
	case('G'):
		for(i=0; i<values.size; i++){
			globalview[(values.vaddr - globalmin) + i] = 1;
		}
		break;
	default:
		printf("Error: Segment info = \"%c\" at line %lu\n", values.segment, linenumber);
		exit(1);
		break;
	};

	return;
}

void processline(char* linebuffer)
{
	char c;
	int i=0;

	/* process vaddr */
	i = 2;
	c = linebuffer[i];
	values.vaddr = c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.vaddr *= 16;
		values.vaddr += c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
		i++;
	}
	i++;
	
	/* process paddr */
	c = linebuffer[i];
	values.paddr = c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.paddr *= 16;
		values.paddr += c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 10));
		i++;
	}
	i++;

	/* process size */
	c = linebuffer[i];
	values.size = (int) linebuffer[i] - 48;
	i++;
	while(linebuffer[i] != ' '){
		c = linebuffer[i];
		values.size *= 10;
		values.size += (int) linebuffer[i] - 48;
		i++;
	}
	i++;

	/* process segment */
	values.segment = linebuffer[i+2];

	/* update globals */
	switch(values.segment){
		case 'S':
			if(values.vaddr < stackmin){
				stackmin = values.vaddr;
			}
			if((values.vaddr + values.size) > stackmax){
				stackmax = values.vaddr + values.size;
			}
			break;
		case 'G':
			if(values.vaddr < globalmin){
				globalmin = values.vaddr;
			}
			if((values.vaddr + values.size) > globalmax){
				globalmax = values.vaddr + values.size;
			}
			break;
		case 'H':
			if(values.vaddr < heapmin){
				heapmin = values.vaddr;
			}
			if((values.vaddr + values.size) > heapmax){
				heapmax = values.vaddr + values.size;
			}
			break;
		default:
			printf("Error: Segment info = \"%c\" at line %lu\n", values.segment, linenumber);
			exit(1);
			break;
	}

	if(values.paddr < physmin){
		physmin = values.paddr;
		if(physmin == 0){
			printf("\nPhysical address 0 at line %lu\n", linenumber);
			printf(":%s:\n", linebuffer);
		}
	}
	if((values.paddr + values.size) > physmax){
		physmax = values.paddr + values.size;
	}
	
	return;
}


