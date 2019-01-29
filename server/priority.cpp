/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

#define QUEUESIZE 6
#define DELAY 1500

pthread_t hpt; //High Priority		
pthread_t mpt;  // Medium Priority
pthread_t lpt;  //Low Priority

pthread_barrier_t mybarrier;
void echo(int connfd);
void *thread(void *);

//define queue structure
typedef struct {
	int buf[QUEUESIZE];
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} queue;

//declare queue functins
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, int in);
void queueDel (queue *q, int *out);
void millisleep(int milliseconds);
void *consumer(void *);


int main(int argc, char **argv) 
{
    int listenfd, connfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;
    int bytesReceived = 0;
    char recvBuff[256];
    pthread_t t1;
    if (argc != 2) 
    {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }
    port = atoi(argv[1]);
    queue *fifo;
    fifo = queueInit ();
	if (fifo ==  NULL) {
		fprintf (stderr, "main: Queue Init failed.\n");
		exit (1);
	}
	struct sched_param my_param; // struct sched_param is used to store the scheduling priority
	pthread_attr_t lp_attr;
	pthread_attr_t mp_attr;
	pthread_attr_t hp_attr;
	int i, min_priority=0, policy;
	
	/* MAIN-THREAD WITH HIGH PRIORITY */
	my_param.sched_priority = sched_get_priority_min(SCHED_FIFO); // Set the priority to the minimum.
	min_priority=my_param.sched_priority;
	my_param.sched_priority=my_param.sched_priority+1;

	pthread_setschedparam(pthread_self(), SCHED_RR, &my_param); //set/get scheduling policy and parameters of a thread
	
	//  The pthread_getschedparam() function returns the scheduling policy and parameters of the thread thread, in the buffers pointed to by policy and param, respectively.
	
	pthread_getschedparam (pthread_self(), &policy, &my_param);  
	
	printf("Manager Priority=%d\n",my_param.sched_priority);


	/* SCHEDULING POLICY AND PRIORITY FOR OTHER THREADS */
	pthread_attr_init(&lp_attr);
	pthread_attr_init(&lp_attr);
	pthread_attr_init(&lp_attr);
	pthread_attr_setinheritsched(&lp_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&lp_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&lp_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&lp_attr, SCHED_FIFO);
	pthread_attr_setschedpolicy(&lp_attr, SCHED_FIFO);
	pthread_attr_setschedpolicy(&lp_attr, SCHED_FIFO);

	my_param.sched_priority = min_priority;
	pthread_attr_setschedparam(&lp_attr, &my_param);
	my_param.sched_priority = min_priority;
	pthread_attr_setschedparam(&lp_attr, &my_param);
	my_param.sched_priority = min_priority;
	 //the pthread_setschedparam call sets both policy and priority at the same time. Conversely, the pthread_getschedparam function returns the current policy and priority for a specified thread
     	pthread_attr_setschedparam(&lp_attr, &my_param);
     	
	Pthread_create (&lpt, &lp_attr, consumer, fifo);
	Pthread_create (&mpt, &lp_attr, consumer, fifo);
	Pthread_create (&hpt, &lp_attr, consumer, fifo);
	
	listenfd = Open_listenfd(port);
	clientlen = sizeof(clientaddr);
	while (1) 
	{
		
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		printf("New Client Arrived ");
		pthread_mutex_lock (fifo->mut);
		queueAdd (fifo, connfd);
		while (fifo->full) {
			printf ("producer: queue FULL.\n");
			pthread_cond_wait (fifo->notFull, fifo->mut);
		}
		
		printf("producer: added %d\n",connfd-3);
		pthread_mutex_unlock (fifo->mut);
		pthread_cond_signal (fifo->notEmpty);
    }
}
/* $end echoserverimain */

void *consumer(void *fileDescriptor){
	queue *fifo;
	fifo=(queue *)fileDescriptor;
	int fd,d;
	pthread_t thread_id = pthread_self();
	struct sched_param param;
	int priority, policy, ret;
	ret = pthread_getschedparam (thread_id, &policy, &param);
	 priority = param.sched_priority;	
	printf("Worker Priority=%d \n", priority);
	while(1){
		pthread_mutex_lock (fifo->mut);
		while (fifo->empty) {
			//printf ("Worker queue EMPTY.\n");
			pthread_cond_wait (fifo->notEmpty, fifo->mut);
		}
		queueDel (fifo, &fd);
		pthread_mutex_unlock (fifo->mut);
		pthread_cond_signal (fifo->notFull);
		printf ("consumer: received %d.\n", fd-3);
		//priority = param.sched_priority;	
		//printf("%d \n", priority);
		for(d=0;d<DELAY;d++){
			millisleep(10);
		}
		echo(fd);
		printf("consumer: served %d \n",fd-3);
		Close(fd);
		millisleep(100);
	}

}

//queue function definitions
queue *queueInit (void)
{
	queue *q;

	q = (queue *)malloc (sizeof (queue));
	if (q == NULL) return (NULL);

	q->empty = 1;
	q->full = 0;
	q->head = 0;
	q->tail = 0;
	q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
	pthread_mutex_init (q->mut, NULL);
	q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notFull, NULL);
	q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	pthread_cond_init (q->notEmpty, NULL);
	
	return (q);
}

void queueDelete (queue *q)
{
	pthread_mutex_destroy (q->mut);
	free (q->mut);	
	pthread_cond_destroy (q->notFull);
	free (q->notFull);
	pthread_cond_destroy (q->notEmpty);
	free (q->notEmpty);
	free (q);
}

void queueAdd (queue *q, int in)
{
	q->buf[q->tail] = in;
	q->tail++;
	if (q->tail == QUEUESIZE)
		q->tail = 0;
	if (q->tail == q->head)
		q->full = 1;
	q->empty = 0;

	return;
}

void queueDel (queue *q, int *out)
{
	*out = q->buf[q->head];

	q->head++;
	if (q->head == QUEUESIZE)
		q->head = 0;
	if (q->head == q->tail)
		q->empty = 1;
	q->full = 0;

	return;
}

void millisleep(int milliseconds)
{
      usleep(milliseconds * 1000);
}

