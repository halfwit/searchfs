#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <bio.h>

#define BUFMAX 24786
#define N_HDLS 32

char *dir, *user;
long total;

typedef struct F {
	char *name;
	Qid qid;
	ulong perm;
} F;

typedef struct Q {
	char *cmd;
	char *args;
	int nargs;
} Q;

F root = { "/", { 0, 0, QTDIR }, 0700|DMDIR };
F roottab[N_HDLS];

void
usage(void){
	fprint(2, "%s [-D] [-d dir] [-m mtpt] [-s srv]\n", argv0);
	threadexitsall("usage");
}

void
inithandler(Dir d, int index){
	F handler = {
		d.name,
		{ index + 1, 0, QTDIR },
		0700|DMDIR,
	};
	roottab[index] = handler;
}

F *
filebypath(uvlong path){
	int i;
	if(path == 0)
		return &root;
	for(i = 0; i < total; i++)
		if(path == roottab[i].qid.path)
			return &roottab[i];
	return nil;
}

void
fsattach(Req *r){
	r->fid->qid = root.qid;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

char *
fswalk1(Fid *fid, char *name, Qid *qid){
	// Just return the root
	if((strcmp("/", name) == 0) || (strcmp("..", name) == 0)) {
		*qid = root.qid;
		fid->qid = *qid;
		return nil;
	}
	Q *q;
	int i;
	for(i = 0; i < total; i++)  {
		if(strcmp(name, roottab[i].name) == 0) {
			q = malloc(sizeof(Q));
			q->cmd = smprint("%s/%s", dir, name);
			q->args = "";
			q->nargs = 0;
			fid->aux = q;
			*qid = roottab[i].qid;
			fid->qid = *qid;
			return nil;
		}
	}
	// Research what not using a unique qid->path would do
	// We may want to instead create a type to hold a current
	// path, and check it every time that we don't overflow
	// being sure to reset to total + 1 if we are going to
	qid->path = total + 1;
	qid->type = QTFILE;
	fid->qid = *qid;
	q = fid->aux;
	q->args = smprint("%s/%s", q->args, name);
	q->nargs++;
	fid->aux = q;
	return nil;
}

char *
parseargs(Q *q){
	char *argv[N_HDLS], *final;
	int n, i;
	final = q->cmd;
	n = getfields(q->args, argv, N_HDLS, 1, "/");
	for(i = 0; i < n-1; i++){
		final = smprint("%s %s", final, argv[i]);
	}
	final = smprint("%s -- %s", final, argv[n-1]);
	return final;
}

void
fillstat(Dir *d, uvlong path){
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
fsstat(Req *r){
	fillstat(&r->d, r->fid->qid.path);
	respond(r, nil);
	return;
}

int
rootgen(int n, Dir *d, void *v){
	USED(v);
	if(n >= total)
		return -1;
	fillstat(d, roottab[n].qid.path);
	return 0;
}

long
runhandler(char *buf, char *cmd){
	int fd[2];
	if (pipe(fd) < 0) {
		fprint(2, "Failed to create pipe: %r\n");
		return 0;
	}
	switch(fork()) {
	case -1:
		fprint(2, "Failed to fork %r\n");
		return 0;
	case 0:
		close(fd[0]);
		dup(fd[1], 1);
		execl("/bin/rc", "rc", "-c", cmd, nil);
		fprint(2, "Command failed, please fix handler %r\n");
		return 0;
	default:
		break;
	}
	sleep(500);
	close(fd[1]);
	long n = 0, nr;
	for(; n < BUFMAX; n += nr) {
		nr = read(fd[0], buf + n, BUFMAX);
		if (nr < 1) 
			break;
	}
	buf[n] = -1;
	return n;
}

void
fsread(Req *r){
	Q *q = r->fid->aux;
	int n;
	char buf[BUFMAX], *cmd;
	if(r->fid->qid.path == 0){
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
	}
	cmd = parseargs(q);
	n = runhandler(buf, cmd);
	readbuf(r, buf, n);
	respond(r, nil);
}


void
fswrite(Req *r){
	respond(r, nil);
}

void
fsdestroy(Fid *f){
	Q *q;

	q = f->aux;
	free(q);
	f->aux = nil;
}

Srv fs = {
.attach = fsattach,
.destroyfid = fsdestroy,
.read   = fsread,
.write  = fswrite,
.stat   = fsstat,
.walk1  = fswalk1,

};

void
threadmain(int argc, char *argv[]){
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
