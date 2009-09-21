/* 
  this is a quick hack based on sha1sum which takes a list of 
  files from stdin and produces several lists of sha1sums 
  representing the digest of the entire file as well as block 
  components of the files at different granularities.

This version ignores zero buffers for the purposes of the block outputs

*/

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <stdio.h>
#include <libsec.h>

#define NUMLOGS 4

int lfd[NUMLOGS];

struct {
	char *name;
	int blksz;
} nfd[] = {
	"/tmp/512.score", 512,
	"/tmp/1024.score", 1024,
	"/tmp/4096.score", 4096,
	"/tmp/8192.score", 8192,
};
	

static int
digestfmt(Fmt *fmt)
{
	char buf[SHA1dlen*2+1];
	uchar *p;
	int i;

	p = va_arg(fmt->args, uchar*);
	for(i=0; i<SHA1dlen; i++)
		sprint(buf+2*i, "%.2ux", p[i]);
	return fmtstrcpy(fmt, buf);
}

static void
sum(int fd, char *name)
{
	int n;
	int totalbytes;
	uchar buf[8192], zerobuf[8192], digest[SHA1dlen];
	DigestState *s;
	DigestState *ns[NUMLOGS];
	int count, block, left;

	memset(zerobuf, 0, 8192);

	s = sha1(nil, 0, nil, nil);
	for(count = 0; count < NUMLOGS; count++) {
		ns[count] = sha1(nil, 0, nil, nil);
	}

	totalbytes = 0;	
	while((n = read(fd, buf, sizeof buf)) > 0) {
		sha1(buf, n, nil, s);
		if( memcmp(buf, zerobuf, 8192)==0 )
			continue;	
		for(count = 0; count < NUMLOGS; count++) {
			for(block = 0; block < n; block += nfd[count].blksz) {
				left = n - block;				
				if (left > nfd[count].blksz)
					left = nfd[count].blksz;
				sha1(buf+block, left, nil, ns[count]);
				sha1(nil, 0, digest, ns[count]);
				ns[count] = sha1(nil, 0, nil, nil);
				if(name == nil) {
					fprint(lfd[count], "%M\n", digest);
				} else {
					fprint(lfd[count], "%M\t%s\n", digest, name);
				}			
			}

		}
		totalbytes += n;
	}
	sha1(nil, 0, digest, s);
	if(name == nil)
		print("%M\n", digest);
	else
		print("%M\t%d\t%s\n", digest, totalbytes, name);
}

/* start with just file based output */

void
main(int argc, char *argv[])
{
	int fd;
	char fname[255];
	int count;

	ARGBEGIN{
	default:
		fprint(2, "usage: fsscan\n");
		exits("usage");
	}ARGEND

	fmtinstall('M', digestfmt);

	for(count=0;count < NUMLOGS;count++) {
		lfd[count] = create( nfd[count].name, OWRITE, 0666);
		if(lfd[count] < 0){
			fprint(2, "fsscan: can't create %s: %r\n", nfd[count].name);
			exits(nil);
		}
	}

	while(1) {
		if(fgets(fname, 255, stdin) == 0)
			break;

		*(fname+strlen(fname)-1) = '\0';

		fd = open(fname, OREAD);
		if(fd < 0){
			fprint(2, "fsscan: can't open %s: %r\n", fname);
			continue;
		}
		sum(fd, fname);
		close(fd);
	};

	for(count=0;count < NUMLOGS;count++)
		close(lfd[count]);

	exits(nil);
}
