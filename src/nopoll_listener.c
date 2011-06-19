/*
 *  LibNoPoll: A websocket library
 *  Copyright (C) 2011 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build Websocket enabled solutions
 *  contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         Edificio Alius A, Oficina 102,
 *         C/ Antonio Suarez Nº 10,
 *         Alcalá de Henares 28802 Madrid
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/nopoll
 */
#include <nopoll_listener.h>
#include <nopoll_private.h>

/** 
 * @brief Creates a listener socket on the provided port.
 */
NOPOLL_SOCKET     nopoll_listener_sock_listen      (noPollCtx   * ctx,
						    const char  * host,
						    const char  * port)
{
	struct hostent     * he;
	struct in_addr     * haddr;
	struct sockaddr_in   saddr;
	struct sockaddr_in   sin;
	NOPOLL_SOCKET        fd;

#if defined(NOPOLL_OS_WIN32)
	int                  sin_size  = sizeof (sin);
#else    	
	int                  unit      = 1; 
	socklen_t            sin_size  = sizeof (sin);
#endif	
	uint16_t             int_port;
	int                  bind_res;

	nopoll_return_val_if_fail (ctx, ctx,  -2);
	nopoll_return_val_if_fail (ctx, host, -2);
	nopoll_return_val_if_fail (ctx, port || strlen (port) == 0, -2);

	/* resolve hostname */
	he = gethostbyname (host);
        if (he == NULL) {
		nopoll_log (ctx, NOPOLL_LEVEL_CRITICAL, "unable to get hostname by calling gethostbyname");
		return -1;
	} /* end if */

	haddr = ((struct in_addr *) (he->h_addr_list)[0]);
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) <= 2) {
		/* do not allow creating sockets reusing stdin (0),
		   stdout (1), stderr (2) */
		nopoll_log (ctx, NOPOLL_LEVEL_DEBUG, "failed to create listener socket: %d (errno=%d)", fd, errno);
		return -1;
        } /* end if */

#if defined(NOPOLL_OS_WIN32)
	/* Do not issue a reuse addr which causes on windows to reuse
	 * the same address:port for the same process. Under linux,
	 * reusing the address means that consecutive process can
	 * reuse the address without being blocked by a wait
	 * state.  */
	/* setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char  *)&unit, sizeof(BOOL)); */
#else
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &unit, sizeof (unit));
#endif 

	/* get integer port */
	int_port  = (uint16_t) atoi (port);

	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family          = AF_INET;
	saddr.sin_port            = htons(int_port);
	memcpy(&saddr.sin_addr, haddr, sizeof(struct in_addr));

	/* call to bind */
	bind_res = bind(fd, (struct sockaddr *)&saddr,  sizeof (struct sockaddr_in));
	nopoll_log (ctx, NOPOLL_LEVEL_DEBUG, "bind(2) call returned: %d", bind_res);
	if (bind_res == NOPOLL_SOCKET_ERROR) {
		nopoll_log (ctx, NOPOLL_LEVEL_DEBUG, "unable to bind address (port:%u already in use or insufficient permissions). Closing socket: %d", int_port, fd);
		nopoll_close_socket (fd);
		return -1;
	}
	
	if (listen(fd, ctx->backlog) == NOPOLL_SOCKET_ERROR) {
		nopoll_log (ctx, NOPOLL_LEVEL_CRITICAL, "an error have occur while executing listen");
		return -1;
        } /* end if */

	/* notify listener */
	if (getsockname (fd, (struct sockaddr *) &sin, &sin_size) < -1) {
		return -1;
	} /* end if */

	/* report and return fd */
	nopoll_log  (ctx, NOPOLL_LEVEL_DEBUG, "running listener at %s:%d (socket: %d)", inet_ntoa(sin.sin_addr), ntohs (sin.sin_port), fd);
	return fd;
}

/** 
 * @brief Creates a new websocket server listener on the provided host
 * name and port. 
 *
 * @param host The hostname or address interface to bind on.
 *
 * @param port The port where to listen, or NULL to use default port: 80.
 *
 * @return A reference to a \ref noPollConn object representing the
 * listener or NULL if it fails.
 */
noPollConn      * nopoll_listener_new (noPollCtx  * ctx,
				       const char * host,
				       const char * port)
{
	NOPOLL_SOCKET   session;
	noPollConn    * listener;

	nopoll_return_val_if_fail (ctx, ctx && host, NULL);

	/* call to create the socket */
	session = nopoll_listener_sock_listen (ctx, host, port);
	if (session == -1) {
		nopoll_log (ctx, NOPOLL_LEVEL_CRITICAL, "Failed to start listener error was: %d", errno);
		return NULL;
	} /* end if */

	/* create noPollConn ection object */
	listener          = nopoll_new (noPollConn, 1);
	listener->refs    = 1;
	listener->session = session;
	listener->ctx     = ctx;
	listener->role    = NOPOLL_ROLE_MAIN_LISTENER;

	/* record host and port */
	listener->host    = strdup (host);
	listener->port    = strdup (port);

	/* register connection into context */
	nopoll_ctx_register_conn (ctx, listener);

	/* configure default handlers */
	listener->receive = nopoll_conn_default_receive;
	listener->send    = nopoll_conn_default_send;

	return listener;
}

/** 
 * @brief Creates a websocket listener from the socket provided.
 *
 * @param ctx The context where the listener will be associated.
 *
 * @param session The session to associate to the listener.
 *
 * @return A reference to a listener connection object or NULL if it
 * fails.
 */
noPollConn   * nopoll_listener_from_socket (noPollCtx      * ctx,
					    NOPOLL_SOCKET    session)
{
	noPollConn * listener;

	struct sockaddr_in   sin;
#if defined(NOPOLL_OS_WIN32)
	/* windows flavors */
	int                  sin_size = sizeof (sin);
#else
	/* unix flavors */
	socklen_t            sin_size = sizeof (sin);
#endif

	nopoll_return_val_if_fail (ctx, ctx && session > 0, NULL);
	
	/* create noPollConn ection object */
	listener          = nopoll_new (noPollConn, 1);
	listener->refs    = 1;
	listener->session = session;
	listener->ctx     = ctx;
	listener->role    = NOPOLL_ROLE_LISTENER;

	/* get peer value */
	if (getpeername (session, (struct sockaddr *) &sin, &sin_size) < -1) {
		nopoll_log (ctx, NOPOLL_LEVEL_CRITICAL, "unable to get remote hostname and port");
		return nopoll_false;
	} /* end if */

	/* record host and port */
	/* lock mutex here to protect inet_ntoa */
	listener->host    = strdup (inet_ntoa (sin.sin_addr));
	/* release mutex here to protect inet_ntoa */
	listener->port    = nopoll_strdup_printf ("%d", ntohs (sin.sin_port));

	/* register connection into context */
	nopoll_ctx_register_conn (ctx, listener);

	/* configure default handlers */
	listener->receive = nopoll_conn_default_receive;
	listener->send    = nopoll_conn_default_send;

	return listener;
}

/** 
 * @brief Public function that performs a TCP listener accept.
 *
 * @param server_socket The listener socket where the accept()
 * operation will be called.
 *
 * @return Returns a connected socket descriptor or -1 if it fails.
 */
NOPOLL_SOCKET nopoll_listener_accept (NOPOLL_SOCKET server_socket)
{
	struct sockaddr_in inet_addr;
#if defined(AXL_OS_WIN32)
	int               addrlen;
#else
	socklen_t         addrlen;
#endif
	addrlen       = sizeof(struct sockaddr_in);

	/* accept the connection new connection */
	return accept (server_socket, (struct sockaddr *)&inet_addr, &addrlen);
}
