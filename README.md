libhttp
=======

Light-weight C library for [HTTP/1.1][1] requests.
Meant for embedded environments or whereever [libcurl][3] would be just
too much.

Fitness
-------

If there's any chance you will ever need to make [HTTPS][2] requests or do
anything else than just plain simple HTTP/1.1 requests, use [libcurl][3].

This library aims to remain as simple and portable as possible to offer
a nice and small alternative.

Sample
------

### Simple GET request

For a simple GET request you just need to call http_request() and
http_response():

	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>

	#include "http.h"

	int main(int argc, char **argv) {
		int sd;
		struct http_message msg;

		if (!--argc || (sd = http_request(*++argv)) < 1) {
			perror("http_request");
			return -1;
		}

		memset(&msg, 0, sizeof(msg));

		while (http_response(sd, &msg) > 0) {
			if (msg.content) {
				write(1, msg.content, msg.length);
			}
		}

		close(sd);

		if (msg.header.code != 200) {
			fprintf(
				stderr,
				"error: returned HTTP code %d\n",
				msg.header.code);
		}

		return 0;
	}

### Custom request

If you want to add/tweak some header values or do a POST request, use the low level functions http_parse_url(), http_connect() and http_send() to compose your request like this:

	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <unistd.h>

	#include "http.h"

	int post(int sd, struct http_url *url) {
		char buf[1024];

		snprintf(
			buf,
			sizeof(buf),
			"\
	POST /%s HTTP/1.1\r\n\
	User-Agent: Mozilla/4.0 (Linux)\r\n\
	Host: %s\r\n\
	Accept: */*\r\n\
	Content-Length: 13\r\n\
	Connection: close\r\n\
	\r\n\
	q=Test&btn=Go\r\n\
	\r\n",
			url->query,
			url->host);

		if (http_send(sd, buf)) {
			perror("http_send");
			return -1;
		}

		return 0;
	}

	int main(int argc, char **argv) {
		struct http_url *url;
		struct http_message msg;
		int sd;

		if (!(url = http_parse_url(*++argv)) ||
				!(sd = http_connect(url))) {
			free(url);
			perror("http_connect");
			return -1;
		}

		memset(&msg, 0, sizeof(msg));

		if (!post(sd, url)) {
			while (http_response(sd, &msg) > 0) {
				if (msg.content) {
					write(1, msg.content, msg.length);
				}
			}
		}

		free(url);
		close(sd);

		if (msg.header.code != 200) {
			fprintf(
				stderr,
				"error: returned HTTP code %d\n",
				msg.header.code);
		}

		return 0;
	}

Testing
-------

There's a bash script to test any URL list against libhttp:

	$ cd test && bash test.sh [FILE-WITH-URLS]

If you find bugs, please report them.

[1]: https://en.wikipedia.org/wiki/HTTP
[2]: https://en.wikipedia.org/wiki/HTTP_Secure
[3]: https://github.com/bagder/curl
