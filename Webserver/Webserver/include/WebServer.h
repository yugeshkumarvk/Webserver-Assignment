#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include "stdafx.h"
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <ctime>
#include <fstream>
#include <iostream>

#include <winsock2.h>

using namespace std;

//Disable deprecation warnings
#pragma warning(disable: 4996)

//Buffer Length 
#define MAX_BUFFER_LEN 1024

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

class CClientContext  //To store and manage client related information
{
private:

	bool              m_bWriteSet;
	SOCKET            m_Socket;  //accepted socket
	CClientContext   *m_pNext; //this will be a singly linked list

public:

	struct http_request
	{
		http_request() : authentication_given_(false) {}

		std::string                        method_;
		std::string                        path_;
		std::map<std::string, std::string> params_;

		std::string                        accept_;
		std::string                        accept_language_;
		std::string                        accept_encoding_;
		std::string                        user_agent_;

		/* status_: used to transmit server's error status, such as
		o  202 OK
		o  404 Not Found 
		and so on */
		std::string                        status_;

		/* auth_realm_: allows to set the basic realm for an authentication,
		no need to additionally set status_ if set */
		std::string                        auth_realm_;

		std::string                        answer_;

		/*   authentication_given_ is true when the user has entered a username and password.
		These can then be read from username_ and password_ */
		bool authentication_given_;
		std::string username_;
		std::string password_;
	};
	struct http_request req;

	//Get/Set calls
	void SetWrite(bool n)
    {
         m_bWriteSet = n;
    }

    bool GetWriteSet()
    {
         return m_bWriteSet;
    }

	void SetSocket(SOCKET s)
	{
		m_Socket = s;
	}

	SOCKET GetSocket()
	{
		return m_Socket;
	}

	CClientContext* GetNext()
	{
		return m_pNext;
	}

	void SetNext(CClientContext *pNext)
	{
		m_pNext = pNext;
	}

	//Constructor
	CClientContext()
	{
		m_Socket =  SOCKET_ERROR;
		m_bWriteSet = false;
		m_pNext = NULL;
	}

	//destructor
	~CClientContext()
	{
		closesocket(m_Socket);
	}
};

//Head of the client context singly linked list
CClientContext    *g_pClientContextHead = NULL;

//Global FD Sets
fd_set g_ReadSet, g_WriteSet, g_ExceptSet;

//global functions
void AcceptConnections(SOCKET ListenSocket);
void InitSets(SOCKET ListenSocket);
int GetSocketSpecificError(SOCKET Socket);
CClientContext* GetClientContextHead();
void AddClientContextToList(CClientContext *pClientContext);
CClientContext* DeleteClientContext(CClientContext *pClientContext);
bool ReceiveBytes(std::string &content, CClientContext *pClientContext);
bool SendBytes(const std::string &content, CClientContext *pClientContext);
void SplitGetReq(std::string get_req, std::string& path, std::map<std::string, std::string>& params);
std::string base64_decode(std::string const& s);
std::vector<std::string> Tokenize(const std::string& str,const std::string& delimiters);
std::string trim_right( const std::string & source,const std::string & t = " " );
std::string trim_left ( const std::string & source, const std::string & t  = " " );
std::string trim( const std::string & source, const std::string & t = " " );

#endif // _WEBSERVER_H_