#include <u.h>
#include <errno.h>
#include "threadimpl.h"

static int multi;
static Proc *theproc;
static pthread_key_t key;

/*
 * Called before we go multiprocess.
 */
void
_threadmultiproc(void)
{
	if(multi == 0){
		multi = 1;
		pthread_key_create(&key, 0);
		_threadsetproc(theproc);
	}
}

/*
 * Set the proc for the current pthread.
 */
void
_threadsetproc(Proc *p)
{
	if(!multi){
		theproc = p;
		return;
	}
	pthread_setspecific(key, p);
}

/* 
 * Get the proc for the current pthread.
 */
Proc*
_threadgetproc(void)
{
	if(!multi)
		return theproc;

	return pthread_getspecific(key);
}

/*
 * Called to associate p with the current pthread.
 */
void
_threadinitproc(Proc *p)
{
	p->pthreadid = pthread_self();
	_threadsetproc(p);
}

/*
 * Called to exit the current pthread.
 */
void
_threadexitproc(char *exitstr)
{
	pthread_exit(nil);
}

/*
 * Called to exit all pthreads.
 */
void
_threadexitallproc(char *exitstr)
{
	exits(0);
}

/*
 * Called to poll for any kids of this pthread.
 * Wait messages aren't restricted to a particular
 * pthread, so we have a separate proc responsible
 * for them.  So this is a no-op.
 */
void
_threadwaitkids(Proc *p)
{
}

/*
 * Separate process to wait for child messages.
 */
Channel *_threadexecchan;
void
_threadwaitproc(void *v)
{
	Channel *c;
	Waitmsg *w;
	sigset_t none;

	sigemptyset(&none);
	pthread_sigmask(SIG_SETMASK, &none, 0);

	USED(v);
	
	for(;;){
		w = wait();
		if(w == nil){
			if(errno == ECHILD)
				recvul(_threadexecchan);
			continue;
		}
		if((c = _threadwaitchan) != nil)
			sendp(c, w);
		else
			free(w);
	}
}

/* 
 * Called before the first exec.
 */
void
_threadfirstexec(void)
{
}

void
_threadmaininit(void)
{
	_threadexecchan = chancreate(sizeof(ulong), 1);
	proccreate(_threadwaitproc, nil, 32*1024);

	/*
	 * Sleazy: decrement threadnprocs so that 
	 * the existence of the _threadwaitproc proc
	 * doesn't keep us from exiting.
	 */
	lock(&_threadpq.lock);
	--_threadnprocs;
	/* print("change %d -> %d\n", _threadnprocs+1, _threadnprocs); */
	unlock(&_threadpq.lock);
}

void
_threadafterexec(void)
{
	nbsendul(_threadexecchan, 1);
}

/*
 * Called to start a new proc.
 */
void
_threadstartproc(Proc *p)
{
	Proc *np;
	pthread_t tid;
	sigset_t all;

	np = p->newproc;
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, nil);
	if(pthread_create(&tid, nil, (void*(*)(void*))_threadscheduler, 
			np) < 0)
		sysfatal("pthread_create: %r");
	np->pthreadid = tid;
}