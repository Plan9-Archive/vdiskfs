#include "u.h"
#include <linux/unistd.h>
#include "libc.h"
#include "thread.h"
#include "threadimpl.h"

       _syscall0(pid_t,gettid)

int	_threaddebuglevel;

static	uint		threadnproc;
static	uint		threadnsysproc;
static	Lock		threadnproclock;
static	Ref		threadidref;

static	void		addthread(_Threadlist*, _Thread*);
static	void		delthread(_Threadlist*, _Thread*);
static	void		addthreadinproc(Proc*, _Thread*);
static	void		delthreadinproc(Proc*, _Thread*);
static	void		contextswitch(Context *from, Context *to);
static	void		scheduler(void*);

static _Thread*
getthreadnow(void)
{
	return proc()->thread;
}
_Thread	*(*threadnow)(void) = getthreadnow;

static Proc*
procalloc(void)
{
	Proc *p;

	p = malloc(sizeof *p);
	memset(p, 0, sizeof *p);
	lock(&threadnproclock);
	threadnproc++;
	unlock(&threadnproclock);
	return p;
}

static void
threadstart(void *v)
{
	_Thread *t;

	t = v;
	t->startfn(t->startarg);
	_threadexit();
}

static _Thread*
threadalloc(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;
	sigset_t zero;

	/* allocate the task and stack together */
	t = malloc(sizeof *t+stack);
	memset(t, 0, sizeof *t);
	t->stk = (uchar*)(t+1);
	t->stksize = stack;
	t->id = incref(&threadidref);
	t->startfn = fn;
	t->startarg = arg;

	/* do a reasonable initialization */
	memset(&t->context.uc, 0, sizeof t->context.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

	/* on Linux makecontext neglects floating point */
	getcontext(&t->context.uc);

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	t->context.uc.uc_stack.ss_sp = t->stk+8;
	t->context.uc.uc_stack.ss_size = t->stksize-16;
	makecontext(&t->context.uc, (void(*)())threadstart, 1, t);

	return t;
}

_Thread*
_threadcreate(Proc *p, void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	t = threadalloc(fn, arg, stack);
	t->proc = p;
	addthreadinproc(p, t);
	p->nthread++;
	_threadready(t);
	return t;
}

int
threadcreate(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	t = _threadcreate(proc(), fn, arg, stack);
	return t->id;
}

int
proccreate(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;
	Proc *p;

	p = procalloc();
//print("pa %p\n", p);
	t = _threadcreate(p, fn, arg, stack);
//print("ps %p\n", p);
	_procstart(p, scheduler);
	return t->id;
}

void
_threadswitch(void)
{
	Proc *p;

	p = proc();
	contextswitch(&p->thread->context, &p->schedcontext);
}

void
_threadready(_Thread *t)
{
	Proc *p;

	p = t->proc;
	lock(&p->lock);
	addthread(&p->runqueue, t);
	_procwakeup(&p->runrend);
	unlock(&p->lock);
}

void
threadyield(void)
{
	_threadready(proc()->thread);
	_threadswitch();
}

void
_threadexit(void)
{
	proc()->thread->exiting = 1;
	_threadswitch();
}

void
threadexits(char *msg)
{
/*
	Proc *p;

	p = proc();
	utfecpy(p->msg, p->msg+sizeof p->msg, msg);
*/
	_threadexit();
}

void
threadexitsall(char *msg)
{
	if(msg && msg[0])
		exit(1);
	exit(0);
}

static void
contextswitch(Context *from, Context *to)
{
	if(swapcontext(&from->uc, &to->uc) < 0){
		fprint(2, "swapcontext failed: %r\n");
		assert(0);
	}
}

static void
scheduler(void *v)
{
	_Thread *t;
	Proc *p;

	p = v;
	setproc(p);
	print("s %p %d\n", p, gettid());
	p->tid = pthread_self();
	pthread_detach(p->tid);
	lock(&p->lock);
	for(;;){
		while((t = p->runqueue.head) == nil){
			if(p->nthread == 0)
				goto Out;
			p->runrend.l = &p->lock;
			_procsleep(&p->runrend);
		}
		delthread(&p->runqueue, t);
		unlock(&p->lock);
		p->thread = t;
		// print("run %s %d\n", t->name, t->id);
		contextswitch(&p->schedcontext, &t->context);
		p->thread = nil;
		lock(&p->lock);
		if(t->exiting){
			delthreadinproc(p, t);
			p->nthread--;
			free(t);
		}
	}

Out:
	lock(&threadnproclock);
	if(p->sysproc)
		--threadnsysproc;
	if(--threadnproc == threadnsysproc)
		exit(0);
	unlock(&threadnproclock);
	unlock(&p->lock);
	free(p);
	setproc(0);
	print("e %p (tid %d)\n", p, gettid());
	pthread_exit(nil);
}

void
_threadsetsysproc(void)
{
	lock(&threadnproclock);
	if(++threadnsysproc == threadnproc)
		exit(0);
	unlock(&threadnproclock);
	proc()->sysproc = 1;
}

/*
 * debugging
 */
void
threadsetname(char *fmt, ...)
{
	va_list arg;
	_Thread *t;

	t = proc()->thread;
	va_start(arg, fmt);
	vsnprint(t->name, sizeof t->name, fmt, arg);
	va_end(arg);
}

void
threadsetstate(char *fmt, ...)
{
	va_list arg;
	_Thread *t;

	t = proc()->thread;
	va_start(arg, fmt);
	vsnprint(t->state, sizeof t->name, fmt, arg);
	va_end(arg);
}

/*
 * locking
 */
static int
threadqlock(QLock *l, int block, ulong pc)
{
	lock(&l->l);
	if(l->owner == nil){
		l->owner = (*threadnow)();
//print("qlock %p @%#x by %p\n", l, pc, l->owner);
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
//print("qsleep %p @%#x by %p\n", l, pc, (*threadnow)());
	addthread(&l->waiting, (*threadnow)());
	unlock(&l->l);

	_threadswitch();

	if(l->owner != (*threadnow)()){
		fprint(2, "qlock pc=0x%lux owner=%p self=%p oops\n", pc, l->owner, (*threadnow)());
		abort();
	}
//print("qlock wakeup %p @%#x by %p\n", l, pc, (*threadnow)());
	return 1;
}

static void
threadqunlock(QLock *l, ulong pc)
{
	lock(&l->l);
//print("qlock unlock %p @%#x by %p (owner %p)\n", l, pc, (*threadnow)(), l->owner);
	if(l->owner == nil){
		fprint(2, "qunlock pc=0x%lux owner=%p self=%p oops\n",
			pc, l->owner, (*threadnow)());
		abort();
	}
	if((l->owner = l->waiting.head) != nil){
		delthread(&l->waiting, l->owner);
		_threadready(l->owner);
	}
	unlock(&l->l);
}

static int
threadrlock(RWLock *l, int block, ulong pc)
{
	USED(pc);

	lock(&l->l);
	if(l->writer == nil && l->wwaiting.head == nil){
		l->readers++;
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
	addthread(&l->rwaiting, (*threadnow)());
	unlock(&l->l);
	_threadswitch();
	return 1;	
}

static int
threadwlock(RWLock *l, int block, ulong pc)
{
	USED(pc);

	lock(&l->l);
	if(l->writer == nil && l->readers == 0){
		l->writer = (*threadnow)();
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
	addthread(&l->wwaiting, (*threadnow)());
	unlock(&l->l);
	_threadswitch();
	return 1;
}

static void
threadrunlock(RWLock *l, ulong pc)
{
	_Thread *t;

	USED(pc);
	lock(&l->l);
	--l->readers;
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
		_threadready(t);
	}
	unlock(&l->l);
}

static void
threadwunlock(RWLock *l, ulong pc)
{
	_Thread *t;

	USED(pc);
	lock(&l->l);
	l->writer = nil;
	assert(l->readers == 0);
	while((t = l->rwaiting.head) != nil){
		delthread(&l->rwaiting, t);
		l->readers++;
		_threadready(t);
	}
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
		_threadready(t);
	}
	unlock(&l->l);
}

/*
 * sleep and wakeup
 */
static void
threadrsleep(Rendez *r, ulong pc)
{
	addthread(&r->waiting, proc()->thread);
	qunlock(r->l);
	_threadswitch();
	qlock(r->l);
}

static int
threadrwakeup(Rendez *r, int all, ulong pc)
{
	int i;
	_Thread *t;

	for(i=0;; i++){
		if(i==1 && !all)
			break;
		if((t = r->waiting.head) == nil)
			break;
		delthread(&r->waiting, t);
		_threadready(t);	
	}
	return i;
}

/*
 * hooray for linked lists
 */
static void
addthread(_Threadlist *l, _Thread *t)
{
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = nil;
	}
	l->tail = t;
	t->next = nil;
}

static void
delthread(_Threadlist *l, _Thread *t)
{
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

static void
addthreadinproc(Proc *p, _Thread *t)
{
	_Threadlist *l;

	l = &p->allthreads;
	if(l->tail){
		l->tail->allnext = t;
		t->allprev = l->tail;
	}else{
		l->head = t;
		t->allprev = nil;
	}
	l->tail = t;
	t->allnext = nil;
}

static void
delthreadinproc(Proc *p, _Thread *t)
{
	_Threadlist *l;

	l = &p->allthreads;
	if(t->allprev)
		t->allprev->allnext = t->allnext;
	else
		l->head = t->allnext;
	if(t->allnext)
		t->allnext->allprev = t->allprev;
	else
		l->tail = t->allprev;
}

void**
procdata(void)
{
	return &proc()->udata;
}

static int threadargc;
static char **threadargv;
int mainstacksize;

static void
threadmainstart(void *v)
{
	USED(v);
	threadmain(threadargc, threadargv);
}

int
main(int argc, char **argv)
{
	Proc *p;

	threadargc = argc;
	threadargv = argv;

	/*
	 * Install locking routines into C library.
	 */
	_lock = _threadlock;
	_unlock = _threadunlock;
	_qlock = threadqlock;
	_qunlock = threadqunlock;
	_rlock = threadrlock;
	_runlock = threadrunlock;
	_wlock = threadwlock;
	_wunlock = threadwunlock;
	_rsleep = threadrsleep;
	_rwakeup = threadrwakeup;

	pthreadinit();
	p = procalloc();
	if(mainstacksize == 0)
		mainstacksize = 65536;
	_threadcreate(p, threadmainstart, nil, mainstacksize);
	scheduler(p);
	return 0;	/* not reached */
}