#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <bio.h>

#define BUFMAX 2048
#define N_HDLS 32

char *dir, *user, query[256];
ulong hndlr;
long total;


typedef struct F {
	char *name;
	Qid qid;
	ulong perm;
} F;

F root = { "/", { 0, 0, QTDIR }, 0700|DMDIR };
F roottab[N_HDLS];

void
usage(void)
{
	fprint(2, "%s [-D] [-d dir] [-m mtpt] [-s srv]\n", argv0);
	threadexitsall("usage");
}

void
inithandler(Dir d, int index)
{
	F handler = {
		d.name,
		{ index + 1, 0, QTDIR },
		0700|DMDIR,
	};
	roottab[index] = handler;
}

F *
filebypath(uvlong path)
{
	int i;
	if(path == 0)
		return &root;
	for(i = 0; i < total; i++)
		if(path == roottab[i].qid.path)
			return &roottab[i];
	return nil;
}

void
fsattach(Req *r)
{
	r->fid->qid = filebypath(0)->qid;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

char *
fswalk1(Fid *fid, char *name, Qid *qid)
{
	if((strcmp("/", name) == 0) || (strcmp("..", name) == 0)) {
		*qid = root.qid;
		fid->qid = *qid;
		return nil;
	}
	int i;
	for(i = 0; i < total; i++)  {
		if(strcmp(name, roottab[i].name) == 0) {
			*qid = roottab[i].qid;
			fid->qid = *qid;
			hndlr=fid->qid.path;
			return nil;
		}
	}
	for(i = 0; i < sizeof name; i++)
		query[i] = name[i];
	query[sizeof name] = 0;
	qid->path = total + 1;
	qid->type = QTFILE;
	fid->qid = *qid;
	return nil;
}

void
fillstat(Dir *d, uvlong path)
{
	F *f;

	f = filebypath(path);
	d->length = 0;
	d->name = estrdup9p(f->name);
	d->qid = f->qid;
	d->mode = f->perm;
	d->uid = estrdup9p(user);
	d->gid = estrdup9p(user);
	d->muid = estrdup9p(user);
	d->atime = time(0);
	d->mtime = time(0);
}

void
fsstat(Req *r)
{
	fillstat(&r->d, r->fid->qid.path);
	respond(r, nil);
	return;
}

int
rootgen(int n, Dir *d, void *v)
{
	USED(v);
	if(n >= total)
		return -1;
	fillstat(d, roottab[n].qid.path);
	return 0;
}

long
runhandler(char *buf)
{
	F *f;
	char path[DIRMAX];
	int fd[2];
	if (pipe(fd) < 0) {
		fprint(2, "Failed to create pipe: %r\n");
		return 0;
	}
	f = filebypath(hndlr);
	switch(fork()) {
	case -1:
		fprint(2, "Failed to fork %r\n");
		return 0;
	case 0:
		close(fd[0]);
		dup(fd[1], 1);
		sprint(path, "%s/%s %s\n", dir, f->name, query);
		execl("/bin/rc", "rc", "-c", path, nil);
		fprint(2, "Command failed, please fix handler %r\n");
		return 0;
	default:
		break;
	}
	close(fd[1]);
	long n = 0;
	for(long nr = 0; n < BUFMAX; n += nr) {
		nr = read(fd[0], buf + n, BUFMAX);
		if (nr < 1) 
			break;
	}
	wait;
	buf[n] = -1;
	return n;
}

void
fsread(Req *r)
{
	int count;
	char buf[BUFMAX];
	uvlong path;
	path = r->fid->qid.path;
	if(path == 0){
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
	}
	count = runhandler(buf);
	readbuf(r, buf, count);
	respond(r, nil);
}

void
fswrite(Req *r)
{
	respond(r, nil);
}

Srv fs = {
.attach = fsattach,
.read = fsread,
.write = fswrite,
.stat = fsstat,
.walk1 = fswalk1,
};

void
threadmain(int argc, char *argv[])
{
	Dir *h;
	int dp;
	char *mtpt, *srv;
	mtpt = "/mnt/search";
	srv = nil;
	dir = "handlers";
	ARGBEGIN{
	case 'd':
		dir = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srv = EARGF(usage());
		break;
	case 'D':
		chatty9p++;
		break;
	default:
		usage();
	}ARGEND;

	user = getuser();
	dp = open(dir, OREAD);
	if (dp < 0)
		threadexitsall("Unable to open handlers directory %r\n");
	total = dirreadall(dp, &h);
	for(int i = 0;i < total; i++) {
		inithandler(h[i], i);
	}
	free(h);
	close(dp);

	threadpostmountsrv(&fs, srv, mtpt, MREPL|MCREATE);
	threadexits(nil);
}

