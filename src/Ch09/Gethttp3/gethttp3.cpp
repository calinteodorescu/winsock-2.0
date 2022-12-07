//
// GetHTTP3.cpp --    Retrieve a file from a HTTP server
//
//                    This version uses overlapped I/O
//                    with a completion function.
//
// Compile and link with ws2_32.lib
//

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // ws2_32 is the newer version, wsock32.lib is the obsolete version

void GetHTTP( LPCSTR lpServerName,
              LPCSTR lpFileName
            );

//
// Overlapped I/O completion function
//
void CALLBACK RecvComplete( DWORD           dwError, 
                            DWORD           cbTransferred, 
                            LPWSAOVERLAPPED lpOverlapped, 
                            DWORD           dwFlags
                          );

// Helper macro for displaying errors
#define PRINTERROR(s)    \
        fprintf(stderr,"\n%s %d\n", s, WSAGetLastError())

#define BUFFER_SIZE 1024

//
// Structure used to pass
// additional info to the 
// completion function
//
typedef struct tagIOREQUEST
{
    WSAOVERLAPPED over; // Must be first
    SOCKET        Socket;
    BOOL          fFinished;
    LPBYTE        pBuffer;
} IOREQUEST, *LPIOREQUEST;


int main( int argc, const char** argv )
{
    WORD    wVersionRequested = WINSOCK_VERSION;
    WSADATA wsaData           = {};
    int     nRet              = -1;

    //
    // Check arguments
    //
    if ( argc != 3 )
    {
        fprintf( stderr, "\nSyntax: GetHTTP ServerName FullPathName\n" );

        return 1;
    }

    //
    // Initialize WinSock.dll
    //
    nRet = ::WSAStartup( wVersionRequested,
                         & wsaData
                       );
    if ( nRet != 0 )
    {
        fprintf(stderr, "\nWSAStartup() error (%d)\n", nRet);
        
        ::WSACleanup();

        return 1;
    }

    //
    // Check WinSock version
    //
    if ( wsaData.wVersion != wVersionRequested )
    {
        fprintf(stderr,"\nWinSock version not supported\n");

        ::WSACleanup();

        return 1;
    }

    //
    // Set "stdout" to binary mode
    // so that redirection will work
    // for binary files (.gif, .jpg, .exe, etc.)
    //
    _setmode(_fileno(stdout), _O_BINARY);

    //
    // Call GetHTTP() to do all the work
    //

    const char* serverName = argv[ 1 ];
    const char* fileName   = argv[ 2 ];

    GetHTTP( serverName,
             fileName             
           );

    ::WSACleanup();

    return 0;
}


void GetHTTP( LPCSTR lpServerName,
              LPCSTR lpFileName
            )
{
    LPHOSTENT    lpHostEntry = nullptr;
    SOCKADDR_IN  saServer    = {};
    SOCKET       Socket      = INVALID_SOCKET;
    int          nRet        = SOCKET_ERROR;

    //
    // Lookup the host address
    //
    lpHostEntry = ::gethostbyname( lpServerName );
    if ( lpHostEntry == NULL )
    {
        PRINTERROR("socket()");

        return;
    }
    
    // Create a TCP/IP stream socket
    Socket = ::socket( AF_INET,
                       SOCK_STREAM,
                       IPPROTO_TCP
                     );
    if ( Socket == INVALID_SOCKET )
    {
        PRINTERROR("socket()");

        return;
    }

    //
    // Fill in the rest of the server address structure
    //
    saServer.sin_family = AF_INET;
    saServer.sin_addr   = * ( ( LPIN_ADDR ) * lpHostEntry->h_addr_list );
    saServer.sin_port   = htons( 23456 );

    //
    // Connect the socket
    //
    nRet = ::connect( Socket,
                      ( LPSOCKADDR ) & saServer,
                      sizeof( SOCKADDR_IN )
                    );
    if ( nRet == SOCKET_ERROR )
    {
        PRINTERROR("connect()");

        ::closesocket( Socket );  

        return;
    }
    
    //
    // Format the HTTP request
    // and send it
    //
    char szBuffer[ BUFFER_SIZE ] = {};

    sprintf( szBuffer, "GET %s\n", lpFileName );

    nRet = ::send( Socket,
                   szBuffer,
                   strlen( szBuffer ),
                   0
                 );

    if ( nRet == SOCKET_ERROR )
    {
        PRINTERROR("send()");

        ::closesocket(Socket);    

        return;
    }

    //
    // We're connected, so we can now
    // post a receive buffer
    //

    // 
    // We use a little trick here to pass additional
    // information to out completion function.
    // We append any additional info onto the end
    // of a WSAOVERLAPPED structure and pass it
    // as the lpOverlapped parameter to WSARecv()
    // When the completion function is called,
    // this additional info is then available.
    //
    BYTE      aBuffer[ BUFFER_SIZE ] = {};

    IOREQUEST ioRequest              = {};    
    memset( & ioRequest.over, 0, sizeof( ioRequest.over ) );
        ioRequest.Socket    = Socket;
        ioRequest.fFinished = FALSE;
        ioRequest.pBuffer   = aBuffer;

    WSABUF wsabuf;
        wsabuf.buf = ( char* ) aBuffer;
        wsabuf.len = sizeof( aBuffer );

    DWORD dwRecv  = 0;
    DWORD dwFlags = 0;

    nRet = ::WSARecv( Socket, 
                      & wsabuf,
                      1,
                      & dwRecv,
                      & dwFlags,
                      ( LPWSAOVERLAPPED ) & ioRequest,
                      RecvComplete
                    );

    if ( nRet == SOCKET_ERROR )
    {
        auto error = ::WSAGetLastError( );
        switch ( error )
        {
            case WSA_IO_PENDING:
            {
                goto keepGoing;
            }
            default:
            {
                PRINTERROR("WSARecv()");

                closesocket(Socket);

                return;
            }
        }
    }

keepGoing:

    // Receive the file contents and print to stdout
    for( ;; )
    {
        //
        // We could do other processing here
        //

        //
        // Use the SleepEx() function to signal
        // that we are in an altertable wait state
        //
        ::SleepEx( 0, TRUE );

        //
        // If the completion function says we're finished
        //
        if ( ioRequest.fFinished )
        {
            break;
        }
    }

    closesocket( Socket );
}

void CALLBACK RecvComplete( DWORD           dwError, 
                            DWORD           cbRecv, 
                            LPWSAOVERLAPPED lpOver, 
                            DWORD           dwFlags
                          )
{
    //
    // Check for errors
    //
    if ( dwError != 0 )
    {
        fprintf(stderr,"\nRecvComplete() error: %ld", dwError );

        return;
    }

    LPIOREQUEST pReq = ( LPIOREQUEST ) lpOver;

    //
    // If no error and no data returned,
    // then the connection has been closed.
    //    
    if ( cbRecv == 0 )
    {
        pReq->fFinished = TRUE;

        return;
    }

    fprintf(stderr,"\nRecvComplete(): %ld bytes received", cbRecv);
    //
    // Write the received data to stdout
    //
    fwrite(pReq->pBuffer, cbRecv, 1, stdout);

    // 
    // And then post the buffer to receive again
    //
    WSABUF wsabuf;
        wsabuf.buf = ( char* ) pReq->pBuffer;
        wsabuf.len = BUFFER_SIZE;

    DWORD dwRecv = 0;
    int   nRet   = SOCKET_ERROR;
    
    dwFlags = 0;

    nRet = ::WSARecv( pReq->Socket, 
                      & wsabuf,
                      1,
                      & dwRecv,
                      & dwFlags,
                      lpOver,
                      RecvComplete
                    );

    auto error = ::WSAGetLastError();
    switch ( error )
    {
        case WSA_IO_PENDING:
        {
            break;
        }
        default:
        {
            PRINTERROR("RePost with WSARecv()");

            pReq->fFinished = TRUE;

            break;
        }
    }
}

