/******************************************************************************
***  n02 v0.3 winnt                                                         ***
**   Open Kaillera Client Core                                               **
***  For latest sources, visit http://sf.net/projects/okai                  ***
******************************************************************************/
#define nPThreadB
#include "nThread.h"
#include "../errr.h"

#ifdef __GNUC__
#pragma GCC visibility push(internal)
#endif /* __GNUC__ */


slist<nPThread*, NTHREAD_MAX> nPThread::list;


void* nPThreadProc (nPThread* thread) {

	//TRACE(nPThread, nPThreadProc, __LINE__);

	nPThread::list.add(thread);

	n02_TRACE();//TRACE(nPThread, runIN, __LINE__);
	__try {
		thread->run();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		nPThread::list.remove(thread);
		return 0;
	}
	n02_TRACE();//TRACE(nPThread, runOUT, __LINE__);

	nPThread::list.remove(thread);

	return 0;
}


int nPThread::create(){
	//TRACE(nPThread, create, this);
	n02_TRACE();
	// The HANDLE from the previous run of this object was never closed, so
	// every create() (e.g. each press of "Refresh") leaked one kernel handle.
	if (owned && t.handlep != 0)
		CloseHandle(t.handlep);
	t.handlep = 0;
	owned = false;

	pthread_t h = 0;
	if (0==pthread_create(&h, NULL, (void* (*)(void*))nPThreadProc, this)) {
		t.handlep = h;
		owned = true;
		return -1;
	}
	return 0;
}

int nPThread::capture(){
	//TRACE(nPThread, capture, this);
	list.add(this);
	t.handlep = pthread_self(); // pseudo handle, must NOT be closed
	owned = false;
	return 0;
}

void nPThread::release(){
	list.remove(this);
	if (!owned)
		t.handlep = 0;
}

int nPThread::destroy(){
	//TRACE(nPThread, destroy, this);
	list.remove(this);
	// Only ever terminate a thread we created ourselves. For a capture()d
	// thread the handle is the pseudo handle of the *calling* thread, and
	// TerminateThread() on it would kill the caller.
	if (!owned || t.handlep == 0)
		return 0;
	int ok = (pthread_kill(t.handlep, 0)==0)?1:0;
	CloseHandle(t.handlep);
	t.handlep = 0;
	owned = false;
	return ok;
}

int nPThread::prioritize(int priority){
	return 0;
}

int nPThread::status(){
	return 0;
}

void nPThread::yield(){
	//TRACE(nPThread, yield, this);
	pthread_yield();
}

void nPThread::sleep(int seconds){
	//TRACE(nPThread, sleep, this);
	Sleep(seconds * 1000);
}

#ifdef __GNUC__
#pragma GCC visibility pop
#endif /* __GNUC__ */
