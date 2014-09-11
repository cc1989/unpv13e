#include	"unp.h"
#include <sys/epoll.h>

#define MAX_EVENTS 10000
void
my_lock_init(char *pathname);
int my_lock_wait();
void my_lock_release();

pid_t
child_make(int i, int listenfd, int addrlen)
{
	pid_t	pid;
	void	child_main(int, int, int);

	if ( (pid = Fork()) > 0)
		return(pid);		/* parent */

	child_main(i, listenfd, addrlen);	/* never returns */
}

void setnonblocking(int connfd)
{
	int val; 

	if (-1 == (val = fcntl(connfd, F_GETFL)))
		err_sys("fcntl");
	val |= O_NONBLOCK;
	if (-1 == (val = fcntl(connfd, F_SETFL, val)))
		err_sys("fcntl");
}
void			web_child(int);
void *child_thread(void *arg)
{
	struct epoll_event events[MAX_EVENTS];
	int	 nfds, epollfd, n;
	
	epollfd = *(int *)arg;

	for ( ; ; ) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		printf("nfds = %d\n", nfds);
		if (-1 == nfds)
			err_sys("epoll_wait");
		for (n = 0; n < nfds; n++)
		{
			web_child(events[n].data.fd);		/* process the request */
			/*Close(events[n].data.fd);*/
		}

	}

}
void
child_main(int i, int listenfd, int addrlen)
{
	int connfd, epollfd, n, nfds, nconn = 1;
	socklen_t		clilen;
	struct sockaddr	*cliaddr;
	struct epoll_event ev, events[MAX_EVENTS];
	pthread_t tid;

	cliaddr = Malloc(addrlen);

	setnonblocking(listenfd);
	epollfd = epoll_create(MAX_EVENTS);
	if (-1 == epollfd)
		err_sys("epoll_create");

	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev))
		err_sys("epoll_ctr");
	printf("child %ld starting\n", (long) getpid());
	clilen = addrlen;
	/*if (0 != pthread_create(&tid, NULL, child_thread, &epollfd))
		err_sys("pthread_create");*/
	for ( ; ; ) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		printf("nfds = %d, connections = %d\n", nfds, nconn);
		if (-1 == nfds)
			err_sys("epoll_wait");
		for (n = 0; n < nfds; n++)
		{
			if (events[n].data.fd == listenfd && nconn <= MAX_EVENTS)
			{
				if (my_lock_wait())
					continue;
				connfd = accept(listenfd, cliaddr, &clilen);
				if (connfd == -1)
				{
					my_lock_release();
					continue;
				}
				my_lock_release();
				printf("%d:recevice one request\n", getpid());
				nconn++;
				setnonblocking(connfd);
				ev.events = EPOLLIN;
				ev.data.fd = connfd;
				if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev))
					err_sys("epoll_ctl");	
			}
			else
			{
				if (events[n].data.fd < 0)	
					continue;
				web_child(events[n].data.fd);
				if (-1 == epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL))
					err_sys("epoll_ctl");
				Close(events[n].data.fd);
				nconn--;
			}
		}
		
	}
}
