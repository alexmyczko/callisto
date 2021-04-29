#include <config.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "log.h"
#include "callisto.h"
#include "util.h"
#include "conf.h"

static int listen_fd = -1;

#define MAXLINE 128

static void *handle_client(void *arg) {
    int fd = (int)((long)arg);
    int read_fd;
    FILE *f, *read_f;
    char buf[MAXLINE];

    read_fd = dup(fd);
    if (read_fd < 0) {
        logprintf(LOG_ERR, "handle_client:dup() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }

    if ((read_f = fdopen(read_fd, "r")) == NULL) {
        logprintf(LOG_ERR, "handle_client:fdopen() failed: %s",
                  strerror(errno));
        close(fd);
        close(read_fd);
        return NULL;
    }
    if ((f = fdopen(fd, "w")) == NULL) {
        logprintf(LOG_ERR, "handle_client:fdopen() failed: %s",
                  strerror(errno));
        close(fd);
        fclose(read_f);
        return NULL;
    }

    fputs("e-Callisto for Unix " PACKAGE_VERSION "\n", f);
    fflush(f);

    while ((fgets(buf, MAXLINE, read_f)) != NULL) {
        int l = strlen(buf);

        if (l == 0) break;
        if (buf[l-1] != '\n') {
            logprintf(LOG_ERR, "handle_client(): Line too long");
            fputs("ERROR line too long, closing connection\n", f);
            fflush(f);
            break;
        }

        /* clear LF and possible CR */
        buf[l-1] = 0; l--;
        if (buf[l-1] == '\r') buf[l-1] = 0;

        /* convert to lowercase */
        l--;
        while (l >= 0) {
            if (buf[l] >= 'A' && buf[l] <= 'Z')
                buf[l] += ('a' - 'A');
            l--;
        }

        /* parse commands */
        if (!buf[0]) { /* empty command */
            fputs("OK\n\n", f);
            fflush(f);

            
        } else if (!strcmp(buf, "quit")) {
            fputs("OK closing connection\n\n", f);
            fflush(f);
            break;


        } else if (!strcmp(buf, "start")) {
	    start_command = 1;
	    logprintf(LOG_NOTICE, "Recording (re)started by command server");
            fputs("OK starting new FITS file\n\n", f);
            fflush(f);


        } else if (!strcmp(buf, "stop")) {
	    stop_command = 1;
	    logprintf(LOG_NOTICE, "Recording stopped by command server");
            fputs("OK stopping\n\n", f);
            fflush(f);


        } else if (!strcmp(buf, "overview")) {
	    overview_command = 1;
	    logprintf(LOG_NOTICE, "Overview started by command server");
            fputs("OK starting spectral overview\n\n", f);
            fflush(f);


        } else if (!strcmp(buf, "get")) {
	    int mybuf = active_buffer;
	    buffer_t databuf = buffer[mybuf];
	    int i;
	    usec_t sweeplen = 1000000 * (usec_t)config.nchannels
		/ (usec_t)config.samplerate;
	    
	    databuf.size -= databuf.size % config.nchannels;
	    if (!databuf.size) { /* empty buffer, try the other one */
		databuf = buffer[1-mybuf];
		databuf.size -= databuf.size % config.nchannels;
	    }
	    if (databuf.size) {
		usec_t sweep = databuf.size / config.nchannels - 1;
		databuf.timestamp += sweep * sweeplen;
		fprintf(f, "OK\nt=%llu.%.6llu\n",
			databuf.timestamp/1000000, databuf.timestamp%1000000);
		for (i = 0; i < config.nchannels; i++)
		    fprintf(f, "ch%.3i=%.3f:%i\n", i+1, channels[i].f,
			    databuf.data[databuf.size - config.nchannels + i]);
		fputs("\n", f);
	    } else
		fputs("ERROR no data (yet)\n\n", f);
	    fflush(f);


	} else if (!strncmp(buf, "get", 3)
		   || !strncmp(buf, "put", 3)
		   || !strncmp(buf, "post", 4)
		   || !strncmp(buf, "head", 4)
		   || !strncmp(buf, "connect", 7)
		   || !strncmp(buf, "trace", 5)
		   || !strncmp(buf, "options", 7)
		   || !strncmp(buf, "delete", 6)
		   || strchr(buf, ':')) {
	    fputs("ERROR No HTTP allowed\n\n", f);
	    fflush(f);
	    break;


	} else {
            fprintf(f, "ERROR unrecognized command (%s)\n\n", buf);
            fflush(f);
	}
	
        msleep(1); /* avoid hogging the cpu */	
    }

    shutdown(fd, SHUT_RDWR);
    fclose(f);
    fclose(read_f);
    return NULL;
}

static void *server_loop(void *dummy) {
    int client_fd;

    (void)dummy;

    int errorcount = 0;

    while (1) {
        struct sockaddr_in addr;
        socklen_t addr_sz;
        pthread_t handler_thread;
	pthread_attr_t attr;

        addr_sz = sizeof(addr);
        client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_sz);

        if (client_fd < 0) {
            if (errno == EINTR) continue;
            logprintf(LOG_ERR, "accept() failed: %s", strerror(errno));
            errorcount++;
            if (errorcount > 10) { /* over ten consecutive errors */
                logprintf(LOG_ERR, "Too many consecutive accept() errors");
                logprintf(LOG_CRIT, "Shutting down");
                terminate(0);
            }
            continue;
        }
        errorcount = 0;
        
        if (pthread_attr_init(&attr) != 0
	    || pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0
	    || pthread_create(&handler_thread, &attr, handle_client,
			      (void*)((long)client_fd)) != 0
	    || pthread_attr_destroy(&attr) != 0) {
            char msg[] = "ERROR start_thread()\n";
            int e;
            logprintf(LOG_ERR, "Thread creation failed: %s", strerror(errno));
            e = write(client_fd, msg, strlen(msg));
            close(client_fd);
        }

        microsleep(1); /* sleep the rest of this timeslice */
    }

    return NULL;
}


int server_init(uint16_t port, int ipv4, int ipv6) {
    struct sockaddr_storage addr;
    struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
    int opt_true = 1;
    
    memset(&addr, 0, sizeof(addr));
    if (!ipv6) {
	addr4->sin_family = AF_INET;
	addr4->sin_port = htons(port);
	addr4->sin_addr.s_addr = INADDR_ANY;
    } else {
	addr6->sin6_family = AF_INET6;
	addr6->sin6_port = htons(port);
	memcpy(&addr6->sin6_addr, &in6addr_any, sizeof(in6addr_any));
    }
    
    if ((listen_fd = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_STREAM, 0)) == -1) {
	fprintf(stderr, "ERROR: Cannot create socket: %s\n", strerror(errno));
	return 0;
    }
    if(setsockopt(listen_fd,
		  SOL_SOCKET,
		  SO_REUSEADDR,
		  &opt_true,
		  sizeof(opt_true))
       < 0) {
	fprintf(stderr, "ERROR: Cannot set socket options: %s\n",
		strerror(errno));
	return 0;
    }
    if(!ipv4 && ipv6 && setsockopt(listen_fd,
				   IPPROTO_IPV6,
				   IPV6_V6ONLY,
				   &opt_true,
				   sizeof(opt_true))
       < 0) {
	fprintf(stderr, "ERROR: Cannot set socket options: %s\n",
		strerror(errno));
	return 0;
    }
    if (bind(listen_fd, (struct sockaddr *)(&addr),
	     !ipv6 ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) < 0) {
	fprintf(stderr, "ERROR: Cannot bind to port %i: %s\n",
		port, strerror(errno));
	return EXIT_FAILURE;
    }
    
    if (listen(listen_fd, 0) < 0) {
	fprintf(stderr, "ERROR: Cannot listen(): %s\n", strerror(errno));
	return 0;
    }

    return 1;
}

void server_start() {
    pthread_t thread_id;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0
        || pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0
        || pthread_create(&thread_id, &attr, server_loop, NULL) != 0
        || pthread_attr_destroy(&attr) != 0) {
	logprintf(LOG_CRIT, "Cannot create server thread, terminating: %s",
		  strerror(errno));
        terminate(-1);
    }
}
