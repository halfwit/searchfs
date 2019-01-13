#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <bio.h>
#include <ndb.h>

char *user;

typedef struct F {
	char *name;
	char *path;
	Qid qid;
	ulong perm;
} F;

F root = { "/", nil, {0, 0, QTDIR}, 0700|DMDIR };
F roottab[32];
ulong total;

void
usage(void)
{
	fprint(2, "%s [-D] [-c cfg] [-m mtpt] [-s srv]\n", argv0);
	threadexitsall("usage");
}

F *
filebypath(uvlong path)
{
	int i;

	if(path == 0)
		return &root;
	for(i = 0; i < nelem(roottab); i++)
		if(path == roottab[i].qid.path)
			return &roottab[i];
	return nil;
}

char *
findattr(Ndbtuple *t, char * name) {
	Ndbtuple *nt;
	nt = ndbfindattr(t->entry, t->line, name);
	if (nt == 0)
		return 0;
	return nt->val;
}

ulong
parsecfg(Ndb* db)
{
	Ndbs s;
	Ndbtuple *t, *nt;
	ulong n = 0;
	t = ndbsearch(db, &s, "type", "search");
	for(nt = t; nt; nt = ndbsnext(&s, "type", "search") ) {
		F handler = {
			findattr(nt, "name"),
			findattr(nt, "path"),
			{ n + 1, 0, QTDIR }, /* qid 0 is our root dir */
			0700|DMDIR,
		};
		if(handler.name == 0 || handler.path == 0)
			continue;
		roottab[n] = handler;
		n++;
	}
	free(t);
	return n;
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
	int i;
	if ((fid->qid.path == 0) || (strcmp("..", name) == 0)) {
		*qid = root.qid;
		fid->qid = *qid;
		return nil;
	}
	for(i = 0; i < total; i++) {
		if(strcmp(roottab[i].name, name) == 0) {
			*qid = roottab[i].qid;
			fid->qid = *qid;
			return nil;
		}

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
	if(n >= total)
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
	if(path == 0){
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
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
	Ndb* db;
	char *mtpt, *srv, *cfg;
	mtpt = "/mnt/search";
	cfg = nil;
	srv = nil;
	ARGBEGIN{
	case 'c':
		cfg = EARGF(usage());
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
	db = ndbopen(cfg);

	if (db == nil)
		threadexitsall("Unable to open config file.\n");

	total = parsecfg(db);
	if (total == 0)
		threadexitsall("Unable to set up handlers\n");
	threadpostmountsrv(&fs, srv, mtpt, MREPL|MCREATE);

	threadexits(nil);
}

