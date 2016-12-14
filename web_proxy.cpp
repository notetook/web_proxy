#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winSock2.h>

const int max_thread = 500;
const int max_buff = 300000;
const int max_buff2 = 1000;

int thread_cnt;
short port;

char before[max_buff], after[max_buff];

typedef struct plist {
	SOCKET hsd;
} PLIST;

void error_handling ( char *msg )
{
	printf("%s\n", msg);
	exit ( 1 );
}

DWORD WINAPI forward ( LPVOID param )
{
	PLIST *P = (PLIST *) param;
	SOCKET hsd;

	int sport = 80;
	int i, j, k;

	hsd = P->hsd;

	char buf[max_buff], t1[max_buff], t2[max_buff2], t3[max_buff2], t4[max_buff2], t5[max_buff2];
	char tmp[max_buff];
	HOSTENT *server;
	SOCKADDR_IN server_addr;
	SOCKET ssd;
	int n, m, body, header;
	int modi_n, modi_m;
		
	memset ( buf, 0, max_buff );
	n = recv ( hsd, buf, max_buff, 0 );

	sscanf(buf, "%s%s%s%s%s", t1, t2, t3, t4, t5);

	if ( n > 0 && strncmp(t1, "GET", 3) == 0 && (strncmp(t3, "HTTP/1.1", 8) || strncmp(t3, "HTTP/1.0", 8)) ){
		strcpy ( tmp, t5 );

		memset ( &server, 0, sizeof(server) );
		server = gethostbyname ( tmp );
		if ( server == NULL ){
			printf("gethostbyname is fucked\n");
			closesocket ( hsd );
			return 1;
		}
			
		memcpy ( tmp, server->h_addr_list, server->h_length );

		ssd = socket ( PF_INET, SOCK_STREAM, 0 );
		if ( ssd == INVALID_SOCKET )
			error_handling ( "ssd socket error!" );
		setsockopt ( ssd, IPPROTO_TCP, TCP_NODELAY, (char *)&max_buff, sizeof(max_buff) );

		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		memcpy ( (char*)&server_addr.sin_addr.s_addr, (char*)server->h_addr_list[0], server->h_length );
		printf("try to connect with addr = %s\n", inet_ntoa(server_addr.sin_addr));
		server_addr.sin_port = htons(sport);

		printf("Success %s with addr = %s\n", t5, inet_ntoa(server_addr.sin_addr));

		if ( connect ( ssd, (SOCKADDR *)&server_addr, sizeof(server_addr) ) == SOCKET_ERROR ){
			int err_no = 0;
			err_no = WSAGetLastError();
			printf("error_code = %d %d\n", err_no, tmp);
			error_handling ( "forward connect error!" );
		}

		for(i=0; i<strlen(buf)-4; i++){
			if ( strncmp ( (const char*)buf+i, "gzip", 4 ) == 0 ){
				memset ( (unsigned char*)buf+i, 0x20, 4 );
			}
		}

		if ( send ( ssd, buf, strlen(buf), 0 ) != strlen(buf) )
			error_handling ( "forward send error!" );
		
		m = 0;
	
		for(;;){
			n = recv ( ssd, buf+m, max_buff-m, 0 );
			if ( n == SOCKET_ERROR ){
				i = WSAGetLastError ();
				printf("recv error : %d\n", i);
				return 1;
			} m += n;
			if ( n <= 0 ) break;
		} n = m;
			
		if ( n>0 ){
			char disti[5];
			disti[0] = 0x0d, disti[1] = 0x0a, disti[2] = 0x0d, disti[3] = 0x0a; // \r\n\r\n
			for(i=0; i<n-4; i++){
				if ( memcmp ( buf+i, disti, 4 ) == 0 ){
					body = i+4;
					break;
				}
			}

			for(i=0; i<body; i++){
				if ( memcmp ( buf+i, "Content-Length: ", 16 ) == 0 ){
					break;
				}
			}

			if ( i != body ){
				for(i=body, j=0; i<n; i++, j++){
					if ( memcmp ( buf+i, before, strlen(before) ) == 0 ){
						memcpy ( tmp+j, after, strlen(after) );
						i += strlen(before)-1;
						j += strlen(after)-1;
					} else {
						tmp[j] = buf[i];
					}
				} modi_n = j;

				for(i=0; i<body; i++){
					if ( memcmp ( buf+i, "Content-Length: ", 16 ) == 0 ){
						for(j=0; j<16; j++) t1[i+j] = buf[i+j];
						sprintf(t1+i+16, "%d", modi_n);

						sscanf(buf+i+16, "%d", &m);
						sprintf(t2, "%d\n", m);
						sprintf(t3, "%d\n", modi_n);
						k = strlen(t3) - strlen(t2);
				
						for(j=i+16+strlen(t2); j<body; j++)
							t1[j+k] = buf[j];
						header = j+k;
				
						break;
					} else
						t1[i] = buf[i];
				}

				memcpy ( t1+header, tmp, modi_n );
				send ( hsd, t1, header+modi_n, 0 );
			} else {
				memcpy ( t1, buf, body );

				modi_n = body;
				for(i=body; i<n; i++){
					for(;;i++){
						if ( buf[i] != 0x0a && buf[i] != 0x0d ){
							break;
						}
					}

					sscanf(buf+i, "%x", &m);
						
					for(;;i++){
						if ( buf[i] == 0x0a ){
							i ++;
							break;
						}
					}

					modi_m = m;
					for(j=0, k=0; j<m; j++, k++){
						if ( memcmp ( buf+i+j, before, strlen(before) ) == 0 ){
							memcpy ( tmp+k, after, strlen(after) );
							j += strlen(before)-1;
							k += strlen(after)-1;
							modi_m += strlen(after)-strlen(before);
						} else
							tmp[k] = buf[i+j];
					}

					sprintf(t1+modi_n, "%x\r\n", modi_m);
					sprintf(t2, "%x\n", modi_m);
					modi_n += strlen(t2)+2;

					memcpy ( t1+modi_n, tmp, modi_m );
					modi_n += modi_m;

					i += m-1;
				}

				send ( hsd, t1, modi_n, 0 );
//				send ( hsd, buf, n, 0 );
			}
		}
		
		closesocket ( ssd );
	}

	closesocket ( hsd );

	return 0;
}

int main ()
{
	// input
	printf("Welcome to proxy server!!!\n\n");

	printf("port number : ");
	scanf("%d", &port);
	printf("data change\nbefore : ");
	scanf("%s", before);
	printf("after : ");
	scanf("%s", after);
	//port = 9090;

	// initializing
	WSADATA wsadata;
	if ( WSAStartup ( MAKEWORD(2,2), &wsadata ) != 0 )
		error_handling ( "WSAStartup error!" );

	// create and bind socket
	SOCKET psd;
	psd = socket ( PF_INET, SOCK_STREAM, 0 );
	if ( psd == INVALID_SOCKET )
		error_handling ( "socket error!" );

	SOCKADDR_IN proxy_addr;
	memset(&proxy_addr, 0, sizeof(proxy_addr));
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_addr.sin_port = htons(port);

	if ( bind ( psd, (SOCKADDR *) &proxy_addr, sizeof(proxy_addr) ) == SOCKET_ERROR )
		error_handling ( "bind error!" );

	// listening
	if ( listen ( psd, 10 ) == SOCKET_ERROR )
		error_handling ( "listen error!" );


	SOCKET hsd;
	SOCKADDR_IN host_addr;
	int host_addr_len = sizeof(host_addr);

	HANDLE th[max_thread];
	DWORD tid[max_thread];
	PLIST *P[max_thread];

	for(;;)
	{
		// accepting
		memset(&host_addr, 0, sizeof(host_addr));
		hsd = accept ( psd, (SOCKADDR *) &host_addr, &host_addr_len );
		int err_no = 0;
		err_no = WSAGetLastError();
		if ( hsd == INVALID_SOCKET ){
			error_handling ( "accept error!" );
		}
		
		// create thread
		thread_cnt ++;
		printf("Create Thread #%d\n", thread_cnt);
		P[thread_cnt] = (PLIST *) HeapAlloc ( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PLIST) );
		P[thread_cnt]->hsd = hsd;
		th[thread_cnt] = CreateThread ( NULL, 0, forward, P[thread_cnt], 0, &tid[thread_cnt] );
		if ( th[thread_cnt] == NULL )
			error_handling ( "create_thread error!" );
	}

	WSACleanup();

	return 0;	
}