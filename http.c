#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http.h"

#ifndef HTTP_USER_AGENT
#define HTTP_USER_AGENT "Mozilla/4.0 (Linux)"
#endif

#ifndef HTTP_TIME_OUT
#define HTTP_TIME_OUT 360
#endif

/**
 * Parse URL into protocol, hostname and query part; the returned
 * structure needs to be freed after use
 *
 * @param url - URL
 */
struct http_url *http_parse_url(const char *url) {
	struct http_url *hu;
	size_t len;
	char *buf;
	char *p;

	if (!url ||
			(len = strlen(url)) < 1 ||
			!(hu = calloc(1, sizeof(struct http_url) + len + 1))) {
		return NULL;
	}

	buf = (char *) hu + sizeof(struct http_url);
	memcpy(buf, url, len);

	if ((p = strstr(buf, "://"))) {
		*p = 0;
		hu->protocol = buf;
		buf = p + 3;
	}

	hu->host = buf;

	if ((p = strchr(buf, '/'))) {
		*p = 0;
		hu->query = ++p;

		if ((p = strchr(p, '#'))) {
			*p = 0;
		}
	} else {
		hu->query = "";
	}

	if ((p = strchr(hu->host, ':'))) {
		hu->protocol = ++p;
	} else {
		hu->protocol = "http";
	}

	return hu;
}

/**
 * Resolve URL and try to connect
 *
 * @param hu - URL structure
 */
int http_connect(struct http_url *hu) {
	struct addrinfo hints, *si, *p;
	int sd = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* cut off optional colon */
	{
		char buf[256];
		char *host = hu->host;
		char *p;

		if ((p = strchr(host, ':'))) {
			size_t l = p - host;

			if (l > sizeof(buf) - 1) {
				return -1;
			}

			*buf = 0;
			host = strncat(buf, host, l);
		}

		if (getaddrinfo(host, hu->protocol, &hints, &si)) {
			return -1;
		}
	}

	/* loop through all results until connect is successful */
	for (p = si; p; p = p->ai_next) {
		if ((sd = socket(
				p->ai_family,
				p->ai_socktype,
				p->ai_protocol)) < 0) {
			continue;
		}

		if (connect(sd, p->ai_addr, p->ai_addrlen) < 0) {
#ifdef WIN32
			closesocket(sd);
#else
			close(sd);
#endif
			continue;
		}

		break;
	}

	if (!p && sd > -1) {
#ifdef WIN32
		closesocket(sd);
#else
		close(sd);
#endif
		sd = -1;
	}

	freeaddrinfo(si);

	return sd;
}

/**
 * Send HTTP request
 *
 * @param sd - socket
 * @param rq - request
 */
int http_send(int sd, const char *rq) {
	size_t l;

	if (!rq) {
		return -1;
	}

	for (l = strlen(rq); l > 0;) {
		int bytes = send(sd, rq, l, 0);

		if (bytes < 0) {
			return -1;
		}

		rq += bytes;
		l -= bytes;
	}

	return 0;
}

/**
 * Cut off trailing CRLF
 *
 * @param msg - message struct
 */
static void http_cut_trailing_crlf(struct http_message *msg) {
	if (msg->state.chunk == 1 && msg->length > 0) {
		/* cut of just CR */
		msg->content[--msg->length] = 0;
	} else if (!msg->state.chunk && msg->length > 1) {
		/* cut of CR/LF */
		msg->length -= 2;
		msg->content[msg->length] = 0;
	}
}

/**
 * Parse message content
 *
 * @param msg - message struct
 * @param bod - begin of data, points to the first character
 * @param eod - end of data, points to the terminating NULL character
 */
static char *http_parse_content(
		struct http_message *msg,
		char *bod,
		char *eod) {
	int len = eod - bod;

	/* check if message is chunked */
	if (msg->state.chunk > -1) {
		/* check if chunk stops in this segment */
		if (msg->state.chunk < len) {
			char *eoc;

			if (msg->state.chunk > 0) {
				if (msg->state.chunk == 1) {
					/* skip trailing LF from previous
					 * chunk header */
					++bod;
					msg->length = 0;
					msg->state.chunk = 0;
				} else {
					/* return data before next chunk
					 * header first */
					msg->length = msg->state.chunk;
					msg->content = bod;

					bod += msg->state.chunk;

					msg->state.chunk = 0;
					http_cut_trailing_crlf(msg);
				}

				return bod;
			}

			/* chunk header is always at the begin of
			 * data here */
			if (!(eoc = strchr(bod, '\n'))) {
				/* chunk header is incomplete */
				return bod;
			}

			*eoc = 0;
			sscanf(bod, "%x", &msg->state.chunk);

			/* add trailing CR/LF to chunk size */
			msg->state.chunk += 2;

			msg->content = ++eoc;
			len = eod - msg->content;

			if (msg->state.chunk < len) {
				/* chunk ends in this segment and a new
				 * one starts */
				msg->length = msg->state.chunk;
				bod = msg->content + msg->state.chunk;

				/* CR/LF must be together here */
				msg->state.chunk = 0;
				http_cut_trailing_crlf(msg);

				return bod;
			} else {
				/* next chunk header is beyond this segment */
				msg->length = len;

				msg->state.chunk -= len;
				http_cut_trailing_crlf(msg);

				return eod;
			}
		} else {
			/* next chunk header is beyond this segment;
			 * fall through to default behaviour */
			msg->state.chunk -= len;
		}
	}

	msg->content = bod;
	msg->length = len;

	http_cut_trailing_crlf(msg);

	return eod;
}

/**
 * Parse HTTP message
 *
 * @param msg - message struct
 * @param bod - begin of data, points to the first character
 * @param eod - end of data, points to the terminating NULL character
 */
static char *http_parse_message(
		struct http_message *msg,
		char *bod,
		char *eod) {
	char *lf;

	if (!bod) {
		return bod;
	}

	if (msg->state.in_content) {
		return http_parse_content(msg, bod, eod);
	}

	/* header line is incomplete so fetch more data */
	if (!(lf = strchr(bod, '\n'))) {
		return bod;
	}

	/* parse status code */
	if (!msg->header.code) {
		/* accept HTTP/1.[01] only */
		if (strncmp(bod, "HTTP/1.", 7) ||
				!strchr("01", *(bod + 7))) {
			return NULL;
		}

		bod += 8;
		msg->header.code = atoi(bod);

		return http_parse_message(msg, ++lf, eod);
	}

	/* parse header line by line */
	for (; lf; bod = ++lf, lf = strchr(bod, '\n')) {
#define WHITESPACE " \t\r\n"
		char *value;

		*lf = 0;

		/* check for end of header */
		if (!*bod || !strcmp(bod, "\r")) {
			if (!*bod) {
				++bod;
			} else {
				bod += 2;
			}

			msg->state.in_content = 1;

			return http_parse_message(msg, bod, eod);
		}

		if (!(value = strchr(bod, ':'))) {
			continue;
		}

		*value = 0;
		++value;

		bod += strspn(bod, WHITESPACE);
		strtok(bod, WHITESPACE);

		value += strspn(value, WHITESPACE);
		strtok(value, WHITESPACE);

		if (!strcasecmp(bod, "Transfer-Encoding") &&
				!strcasecmp(value, "chunked")) {
			/* means 0 bytes until next chunk header */
			msg->state.chunk = 0;
		} else if (!strcasecmp(bod, "Content-Length")) {
			msg->header.length = atoi(value);
		}
	}

	return bod;
}

/**
 * Read next part of the response; returns 0 when message is complete;
 * this function blocks until data is available
 *
 * @param sd - socket
 * @param msg - message struct that gets filled with data, must be
 *              all 0 for the very first call
 */
int http_read(int sd, struct http_message *msg) {
	if (!msg) {
		return -1;
	}

	/* first-time initialization */
	if (!msg->state.offset) {
		/* initialize size of read buffer;
		 * reserve a byte for the terminating NULL
		 * character */
		msg->state.size = sizeof(msg->state.buf) - 1;
		msg->state.offset = msg->state.buf;

		/* initialize remaining length of chunk;
		 * -1 means content is not chunked */
		msg->state.chunk = -1;

		/* initialize content length;
		 * -1 means not given */
		msg->header.length = -1;
	}

	if (msg->state.total == msg->header.length) {
		/* return 0 for keep-alive connections */
		return 0;
	}

	for (;;) {
		/* try to parse buffer first if there's still data in it */
		if (msg->state.left > 0) {
			char *parsed_until;

			msg->length = 0;

			parsed_until = http_parse_message(
				msg,
				msg->state.offset,
				msg->state.offset + msg->state.left);

			if (parsed_until > msg->state.offset) {
				msg->state.left -= parsed_until - msg->state.offset;
				msg->state.offset = parsed_until;
				msg->state.total += msg->length;
				return 1;
			}
		}

		/* make offset begin at the front of the buffer */
		if (msg->state.offset > msg->state.buf) {
			if (msg->state.left > 0)
				memmove(
					msg->state.buf,
					msg->state.offset,
					msg->state.left);

			msg->state.offset = msg->state.buf;
		}

		/* wait for and read new data from the network */
		{
			char *append = msg->state.offset + msg->state.left;
			int bytes,
				size = (msg->state.buf + msg->state.size)-append;

			/* drop half of the buffer if there's no space left */
			if (size < 1) {
				int half = msg->state.size >> 1;

				memcpy(
					msg->state.buf,
					msg->state.buf + half,
					half);

				msg->state.offset = msg->state.buf;
				msg->state.left = half;

				append = msg->state.offset + msg->state.left;
				size = msg->state.size - msg->state.left;
			}

			if ((bytes = recv(
					sd,
					append,
					size,
					0)) < 1) {
				/* bytes == 0 means remote socket was closed */
				return 0;
			}

			append[bytes] = 0;
			msg->state.left += bytes;
		}
	}

	return 0;
}

/**
 * Send HTTP request
 *
 * @param url - URL
 */
int http_request(const char *url) {
	struct http_url *hu;
	int sd;

	if (!(hu = http_parse_url(url)) ||
			(sd = http_connect(hu)) < 0) {
		/* it's save to free NULL */
		free(hu);
		return -1;
	}

	/* this way even very very long query strings won't be a problem */
	if (http_send(sd, "GET /") ||
			http_send(sd, hu->query) ||
			http_send(sd, " HTTP/1.1\r\n\
User-Agent: "HTTP_USER_AGENT"\r\n\
Host: ") ||
			http_send(sd, hu->host) ||
			http_send(sd, "\r\n\
Accept: */*\r\n\
Connection: close\r\n\
\r\n")) {
#ifdef WIN32
		closesocket(sd);
#else
		close(sd);
#endif
		return -1;
	}

	free(hu);

	return sd;
}

/**
 * Read next part of the response; returns 0 when message is complete;
 * this function blocks until data is available
 *
 * @param sd - socket
 * @param msg - message struct that gets filled with data, must be
 *              all 0 for the very first call
 */
int http_response(int sd, struct http_message *msg) {
	fd_set r;
	struct timeval tv;

	tv.tv_sec = HTTP_TIME_OUT;
	tv.tv_usec = 0;

	FD_ZERO(&r);
	FD_SET(sd, &r);

	if (select(sd + 1, &r, NULL, NULL, &tv) < 1 ||
			!FD_ISSET(sd, &r)) {
		return -1;
	}

	return http_read(sd, msg);
}
