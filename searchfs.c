#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

char *user;

typedef struct F {
	char *name;
	Qid qid;
	ulong perm;
} F;

enum {
	Qroot,
	Qrand,
	Qlrand,
	Qfrand,
};

F root = { "/", {Qroot, 0, QTDIR}, 0555|DMDIR };
F roottab[] = {
	"rand", {Qrand, 0, QTFILE}, 0444,
	"lrand", {Qlrand, 0, QTFILE}, 0444,
	"frand", {Qfrand, 0, QTFILE}, 0444,
};

Cmdtab cmd[] = {
	0, "range", 3,
};

void
usage(void)
{
	fprint(2, "%s [-D] [-m mtpt] [-s srv]\n", argv0);
	threadexitsall("usage");
}

F *
filebypath(uvlong path)
{
	int i;

	if(path == Qroot)
		return &root;
	for(i = 0; i < nelem(roottab); i++)
		if(path == roottab[i].qid.path)
			return &roottab[i];
	return nil;
}

void
fsattach(Req *r)
{
	r->fid->qid = filebypath(Qroot)->qid;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

/* If this file doesn't exist, we use the path element of the request to deduce the handler we want. If the path doesn't exist, return "No such handler: %d" */
char *
fswalk1(Fid *fid, char *name, Qid *qid)
{
	int i;
	
	switch(fid->qid.path){
	/* There are no files on our root, only dirs
	if the walk is to an eventual file, we call the handler
	if the walk is to one of the dirs in our roottab, return that qid
	*/
	case Qroot:
		for(i = 0; i < nelem(roottab); i++){
			if(strcmp(roottab[i].name, name) == 0){
				*qid = roottab[i].qid;
				fid->qid = *qid;
				return nil;
			}
		}
		if(strcmp("..", name) == 0){
			*qid = root.qid;
			fid->qid = *qid;
			return nil;
		}
		break;
	}
	return "Directory not found"; 
}

void
fillstat(Dir *d, uvlong path)
{
	F *f;

	f = filebypath(path);
	d->name = estrdup9p(f->name);
	d->qid = f->qid;
	d->mode = f->perm;
	d->uid = estrdup9p(user);
	d->gid = estrdup9p(user);
	d->muid = estrdup9p(user);
	d->length = 0;
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
	if(n >= nelem(roottab))
		return -1;
	fillstat(d, roottab[n].qid.path);
	return 0;
}

void
fsread(Req *r)
{
	char buf[128];
	int count;
	uvlong path;

	path = r->fid->qid.path;
	if(path == Qroot){
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
	}

	switch(path){
	case Qrand:
		snprint(buf, sizeof buf, "%d\n", rand());
		break;
	case Qlrand:
		snprint(buf, sizeof buf, "%ld\n", lrand());
		break;
	case Qfrand:
		snprint(buf, sizeof buf, "%f\n", frand());
		break;
	}

	count = strlen(buf);
	if(r->ifcall.count < count)
		count = r->ifcall.count;
	r->ofcall.count = count;
	memmove(r->ofcall.data, buf, count);
	respond(r, nil);
}

void
fswrite(Req *r)
{
	uvlong path;
	Cmdbuf *cb;
	Cmdtab *cp;
	
	cb = parsecmd(r->ifcall.data, r->ifcall.count);
	cp = lookupcmd(cb, cmd, nelem(cmd));
	if(cp == nil){
		respondcmderror(r, cb, "%r");
		return;
	}

	path = r->fid->qid.path;
	switch(path){
	case Qrand:
		print("%s %s\n", cb->f[0], cb->f[1]);
		break;
	}
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
	char *mtpt, *srv;

	mtpt = "/mnt/rng";
	srv = nil;
	ARGBEGIN{
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
	//rootgen = genRoot();
	threadpostmountsrv(&fs, srv, mtpt, MREPL);
	
	threadexitsall(nil);
}

