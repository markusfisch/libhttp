#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <http.h>

/**
 * Make a GET request and dump the response to stdout
 *
 * @param argc - number of arguments
 * @param argv - arguments
 */
int main( int argc, char **argv )
{
#ifdef WIN32
	{
		WSADATA wsaData = { 0 };
		int result;

		if( (result = WSAStartup( MAKEWORD( 2, 2 ), &wsaData )) )
		{
			printf( "WSAStartup failed: %d\n", result );
			return 1;
		}
	}
#endif

	while( --argc )
	{
		int sd;
		struct http_message msg;

		if( (sd = http_request( *++argv )) < 1 )
		{
			perror( "http_request" );
			continue;
		}

		memset( &msg, 0, sizeof( msg ) );

		while( http_response( sd, &msg ) > 0 )
			if( msg.content )
				write( 1, msg.content, msg.length );

		close( sd );

		if( msg.header.code != 200 )
			fprintf(
				stderr,
				"error: returned HTTP code %d\n",
				msg.header.code );
	}

#ifdef WIN32
	WSACleanup();
#endif

	return 0;
}
