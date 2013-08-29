#ifndef _http_h_
#define _http_h_

struct http_message
{
	struct
	{
		int code;
		int length;
	} header;
	char *content;
	int length;
	struct
	{
		int in_content;
		int chunk;
		char buf[4096];
		int size;
		char *offset;
		char *last;
		int free;
		int left;
		int total;
	} state;
};

/* low level methods */
struct http_url
{
	char *protocol;
	char *host;
	char *query;
};

struct http_url *http_parse_url( const char * );
int http_connect( struct http_url * );
int http_send( int, const char * );
int http_read( int, struct http_message * );

/* high level methods */
int http_request( const char * );
int http_response( int, struct http_message * );

#endif
