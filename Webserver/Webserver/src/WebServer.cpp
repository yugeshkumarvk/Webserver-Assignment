// WebServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "WebServer.h"


int main(int argc, char* argv[])
{
	//Validate the input
	if (argc < 2)
	{
		printf("Usage: %s <port>\n", argv[0]);
		return -1;
	}

	// Initialize Winsock
	WSADATA wsaData;

	int nResult = 0;

	nResult = WSAStartup(MAKEWORD(2,2), &wsaData);

	if (NO_ERROR != nResult)
	{
		printf("Error occurred while executing WSAStartup()\n");
		return -1; //error
	}
	else
	{
		printf("WSAStartup() successful\n");
	}

	SOCKET ListenSocket;
	int    nPortNo;

	struct sockaddr_in ServerAddress;

	//Create a socket
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (INVALID_SOCKET == ListenSocket)
	{
		printf("Error occurred while opening socket: %d\n", WSAGetLastError());

		// Cleanup Winsock
		WSACleanup();
		return -1;
	}
	else
	{
		printf("socket() successful\n");
	}

	//Cleanup and Init with 0 the ServerAddress
	ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));

	//Port number will be supplied as a commandline argument
	nPortNo = atoi(argv[1]);

	//Fill up the address structure
	ServerAddress.sin_family = AF_INET;
	ServerAddress.sin_addr.s_addr = INADDR_ANY; //WinSock will supply address
	ServerAddress.sin_port = htons(nPortNo);    //comes from commandline

	//Assign local address and port number
	if (SOCKET_ERROR == bind(ListenSocket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)))
	{
		closesocket(ListenSocket);
		printf("Error occurred while binding\n");

		// Cleanup Winsock
		WSACleanup();
		return -1;
	}
	else
	{
		printf("bind() successful\n");
	}

	//Make the socket a listening socket
	if (SOCKET_ERROR == listen(ListenSocket,SOMAXCONN))
	{
		closesocket(ListenSocket);
		printf("Error occurred while listening\n");

		// Cleanup Winsock
		WSACleanup();
		return -1;
	}
	else
	{
		printf("listen() successful\n");
	}

	//This function will take care of multiple clients using select()/accept()
	AcceptConnections(ListenSocket);

	//Close open sockets
	closesocket(ListenSocket);

	//Cleanup Winsock
	WSACleanup();
	return 0; //success
}


//Initialize the Sets
void InitSets(SOCKET ListenSocket) 
{
     //Initialize
     FD_ZERO(&g_ReadSet);
     FD_ZERO(&g_WriteSet);
     FD_ZERO(&g_ExceptSet);

     //Assign the ListenSocket to Sets
     FD_SET(ListenSocket, &g_ReadSet);
     FD_SET(ListenSocket, &g_ExceptSet);

     //Iterate the client context list and assign the sockets to Sets
     CClientContext   *pClientContext  = GetClientContextHead();

     while(pClientContext)
     {
		 //if(pClientContext->GetSentBytes() < pClientContext->GetTotalBytes())
		  if ( pClientContext->GetWriteSet() == true )
          {
               //We have data to send
               FD_SET(pClientContext->GetSocket(), &g_WriteSet);
          }
          else
          {
               //We can read on this socket
               FD_SET(pClientContext->GetSocket(), &g_ReadSet);
          }

          //Add it to Exception Set
          FD_SET(pClientContext->GetSocket(), &g_ExceptSet); 

          //Move to next node on the list
          pClientContext = pClientContext->GetNext();
     }
}


//This function will loop on while it will manage multiple clients using select()
void AcceptConnections(SOCKET ListenSocket)
{
     while (true)
     {
          InitSets(ListenSocket);

          if (select(0, &g_ReadSet, &g_WriteSet, &g_ExceptSet, 0) > 0) 
          {
               //One of the socket changed state, let's process it.

               //ListenSocket?  Accept the new connection
               if (FD_ISSET(ListenSocket, &g_ReadSet)) 
               {
                    sockaddr_in ClientAddress;
                    int nClientLength = sizeof(ClientAddress);

                    //Accept remote connection attempt from the client
                    SOCKET Socket = accept(ListenSocket, (sockaddr*)&ClientAddress, &nClientLength);

                    if (INVALID_SOCKET == Socket)
                    {
						printf("Error occurred while accepting socket: %d\n", GetSocketSpecificError(ListenSocket));
                    }

                    //Display Client's IP
					printf("Client connected from: %s\n", inet_ntoa(ClientAddress.sin_addr));

                    //Making it a non blocking socket
                    u_long nNoBlock = 1;
                    ioctlsocket(Socket, FIONBIO, &nNoBlock);

                    CClientContext   *pClientContext  = new CClientContext;
                    pClientContext->SetSocket(Socket);

                    //Add the client context to the list
                    AddClientContextToList(pClientContext);
               }

               //Error occured for ListenSocket?
               if (FD_ISSET(ListenSocket, &g_ExceptSet)) 
               {
				   printf("Error occurred while accepting socket: %d\n", GetSocketSpecificError(ListenSocket));
				   continue;
               }

               //Iterate the client context list to see if any of the socket there has changed its state
               CClientContext   *pClientContext  = GetClientContextHead();

               while (pClientContext)
               {
                    //Check in Read Set
                    if (FD_ISSET(pClientContext->GetSocket(), &g_ReadSet))
                    {
						char chBuff[MAX_BUFFER_LEN] = "";
						std::string content, line, delimiter, path;
						std::map<std::string, std::string> params;
						std::vector<std::string> lines;
						std::vector<std::string>::iterator itr;
						size_t posStartPath;

						int nBytes = recv(pClientContext->GetSocket(), chBuff, MAX_BUFFER_LEN, 0);

                         if ((0 == nBytes) || (SOCKET_ERROR == nBytes))
                         {
                              if (0 != nBytes) //Some error occured, client didn't close the connection
                              {
                                   printf("\nError occurred while recieving on the socket: %d.", GetSocketSpecificError(pClientContext->GetSocket()));
                              }

                              //In either case remove the client from list
                              pClientContext = DeleteClientContext(pClientContext);
                              continue;
                         }

						content.assign(chBuff);
						delimiter.assign("\n");
						lines = Tokenize(content, delimiter);
						line.assign(lines[0]);

						if ( line.find("GET") == 0 )
						{
							pClientContext->req.method_="GET";
						}
						else if ( line.find("POST") == 0 )
						{
							pClientContext->req.method_="POST";
						}

						posStartPath = line.find_first_not_of(" ", 3);
						SplitGetReq(line.substr(posStartPath), path, params);

						pClientContext->req.status_ = "202 OK";
						pClientContext->req.path_   = path;
						pClientContext->req.params_ = params;

						static const std::string authorization   = "Authorization: Basic ";
						static const std::string accept          = "Accept: "             ;
						static const std::string accept_language = "Accept-Language: "    ;
						static const std::string accept_encoding = "Accept-Encoding: "    ;
						static const std::string user_agent      = "User-Agent: "         ;

						itr = lines.begin();
						itr ++;
						for ( ; itr != lines.end(); itr ++ )
						{
							line.assign((*itr));
							trim(line);
							
							if ( line.empty() == true )
								break;

							unsigned int pos_cr_lf = line.find_first_of("\x0a\x0d");
							if ( pos_cr_lf == 0 )
								break;

							line = line.substr(0, pos_cr_lf);

							if (line.substr(0, authorization.size()) == authorization)
							{
								pClientContext->req.authentication_given_ = true;
								std::string encoded = line.substr(authorization.size());
								std::string decoded = base64_decode(encoded);

								unsigned int pos_colon = decoded.find(":");

								pClientContext->req.username_ = decoded.substr(0, pos_colon);
								pClientContext->req.password_ = decoded.substr( pos_colon + 1 );
							}
							else if (line.substr(0, accept.size()) == accept)
							{
								pClientContext->req.accept_ = line.substr(accept.size());
							}
							else if (line.substr(0, accept_language.size()) == accept_language)
							{
								pClientContext->req.accept_language_ = line.substr(accept_language.size());
							}
							else if (line.substr(0, accept_encoding.size()) == accept_encoding)
							{
								pClientContext->req.accept_encoding_ = line.substr(accept_encoding.size());
							}
							else if (line.substr(0, user_agent.size()) == user_agent)
							{
								pClientContext->req.user_agent_ = line.substr(user_agent.size());
							}
						}

						static std::string dirname = ".";
						std::string filename;

						if ( pClientContext->req.path_ == "/" )
						{
							//read welcome content
							filename = dirname + pClientContext->req.path_ + "welcome-content/index.html";
						}
						else
						{
							filename = dirname + "/deployments" + pClientContext->req.path_;
						}

						if ( GetFileAttributesA(filename.c_str()) == FILE_ATTRIBUTE_DIRECTORY )
						{
							filename.append("/index.html");
						}

						std::ifstream infile (filename.c_str());
						if ( infile.is_open() )
						{
							std::string content;
							while ( std::getline (infile, content) )
								pClientContext->req.answer_ += content;

							infile.close();							
						}
						else
						{
							pClientContext->req.status_ = "404 Not Found";
							pClientContext->req.answer_  = "<html><head><title>Wrong URL</title></head><body bgcolor='#ffffff'><h1>Wrong URL</h1>Path is : &gt;"  + filename + "&lt;</body></html>";
						}

						pClientContext->SetWrite(true);
                    }

                    //Check in Write Set
                    if (FD_ISSET(pClientContext->GetSocket(), &g_WriteSet))
                    {
						std::stringstream str_str;
						str_str << pClientContext->req.answer_.size();

						time_t ltime;
						time(&ltime);
						tm* gmt= gmtime(&ltime);

						static std::string const serverName = "WebServer (Windows)";

						char* asctime_remove_nl = asctime(gmt);
						asctime_remove_nl[24] = 0;

						std::string buffer;
						buffer.assign("HTTP/1.1 ");

						if (! pClientContext->req.auth_realm_.empty() )
						{
							buffer.append("401 Unauthorized\n");
							buffer.append("WWW-Authenticate: Basic Realm=\"");
							buffer.append(pClientContext->req.auth_realm_);
							buffer.append("\"\n");
						}
						else
						{
							buffer.append(pClientContext->req.status_ + "\n");
						}
						buffer.append(std::string("Date: ") + asctime_remove_nl + " GMT" + "\n");
						buffer.append(std::string("Server: ") + serverName + "\n");
						buffer.append("Connection: close\n");
						buffer.append("Content-Type: text/html; charset=ISO-8859-1\n");
						buffer.append("Content-Length: " + str_str.str()+"\n");
						buffer.append("\n");
						buffer.append(pClientContext->req.answer_+"\n");

						if ( SendBytes(buffer, pClientContext) == false )
							continue;

						pClientContext->SetWrite(false);
                    }

                    //Check in Exception Set
                    if (FD_ISSET(pClientContext->GetSocket(), &g_ExceptSet))
                    {
						printf("Error occurred on the socket: %d\n", GetSocketSpecificError(pClientContext->GetSocket()));

                        pClientContext = DeleteClientContext(pClientContext);
                        continue;
                    }

                    //Move to next node on the list
                    pClientContext = pClientContext->GetNext();

			   }//while
          }
          else //select
          {
			  printf("Error occurred while executing select(): %d\n", WSAGetLastError());
			  return; //Get out of this function
          }
     }
}

//When using select() multiple sockets may have errors
//This function will give us the socket specific error
//WSAGetLastError() can't be relied upon
int GetSocketSpecificError(SOCKET Socket)
{
     int nOptionValue;
     int nOptionValueLength = sizeof(nOptionValue);

     //Get error code specific to this socket
     getsockopt(Socket, SOL_SOCKET, SO_ERROR, (char*)&nOptionValue, &nOptionValueLength);

     return nOptionValue;
}

//Get the head node pointer
CClientContext* GetClientContextHead()
{
     return g_pClientContextHead;
}

//Add a new client context to the list
void AddClientContextToList(CClientContext *pClientContext)
{
     //Add the new client context right at the head
     pClientContext->SetNext(g_pClientContextHead);
     g_pClientContextHead = pClientContext;
}

//This function will delete the node and will return the next node of the list
CClientContext * DeleteClientContext(CClientContext *pClientContext)
{
     //See if we have to delete the head node
     if (pClientContext == g_pClientContextHead) 
     {
          CClientContext *pTemp = g_pClientContextHead;
          g_pClientContextHead = g_pClientContextHead->GetNext();
          delete pTemp;
		  pTemp = NULL;
          return g_pClientContextHead;
     }

     //Iterate the list and delete the appropriate node
     CClientContext *pPrev = g_pClientContextHead;
     CClientContext *pCurr = g_pClientContextHead->GetNext();

     while (pCurr)
     {
          if (pCurr == pClientContext)
          {
               CClientContext *pTemp = pCurr->GetNext();
               pPrev->SetNext(pTemp);
               delete pCurr;
			   pCurr = NULL;
               return pTemp;
          }

          pPrev = pCurr;
          pCurr = pCurr->GetNext();
     }

     return NULL;
}

bool ReceiveBytes(std::string &content, CClientContext *pClientContext)
{
	while ( true )
	{
		char buff;
		int nRet = 0;
		nRet = recv(pClientContext->GetSocket(), &buff, 1, 0);

		if ( SOCKET_ERROR == nRet )
		{
			printf("Error occurred while recieving on the socket: %d. Return value: %d\n", GetSocketSpecificError(pClientContext->GetSocket()), nRet);

			//In either case remove the client from list
			pClientContext = DeleteClientContext(pClientContext);
			return false;
		}

		printf("%c", buff);
		if ( nRet == 0 )
			break;

		content += buff;
	}
	return true;
}

bool SendBytes(const std::string &line, CClientContext *pClientContext)
{
	int nRet = 0;
	nRet = send(pClientContext->GetSocket(), line.c_str(), line.length(), 0);

	if ( SOCKET_ERROR == nRet )
	{
		printf("Error occurred while sending on the socket: %d\n", GetSocketSpecificError(pClientContext->GetSocket()));

		pClientContext = DeleteClientContext(pClientContext);
		return false;
	}
	return true;
}

void SplitGetReq(std::string get_req, std::string& path, std::map<std::string, std::string>& params)
{
	// Remove trailing newlines
	if ( get_req[get_req.size() - 1] == '\x0d' || get_req[get_req.size() - 1] == '\x0a' )
		get_req = get_req.substr(0, get_req.size() - 1);

	if ( get_req[get_req.size() - 1] == '\x0d' || get_req[get_req.size() - 1] == '\x0a' )
		get_req = get_req.substr(0, get_req.size() - 1);

	// Remove potential Trailing HTTP/1.x
	if ( get_req.size() > 7 )
	{
		if ( get_req.substr(get_req.size() - 8, 7) == "HTTP/1." )
		{
			get_req = get_req.substr(0, get_req.size() - 9);
		}
	}

	std::string::size_type qm = get_req.find("?");
	if (qm != std::string::npos)
	{
		std::string url_params = get_req.substr(qm + 1);

		path = get_req.substr(0, qm);

		// Appending a '&' so that there are as many '&' as name-value pairs.
		// It makes it easier to split the url for name value pairs, he he he
		url_params += "&";

		std::string::size_type next_amp = url_params.find("&");

		while (next_amp != std::string::npos)
		{
			std::string name_value = url_params.substr(0,next_amp);
			url_params             = url_params.substr(next_amp+1);
			next_amp               = url_params.find("&");

			std::string::size_type pos_equal = name_value.find("=");

			std::string nam = name_value.substr(0,pos_equal);
			std::string val = name_value.substr(pos_equal+1);

			std::string::size_type pos_plus;
			while ( (pos_plus = val.find("+")) != std::string::npos )
			{
				val.replace(pos_plus, 1, " ");
			}

			// Replacing %xy notation
			std::string::size_type pos_hex = 0;
			while ( (pos_hex = val.find("%", pos_hex)) != std::string::npos )
			{
				std::stringstream h;
				h << val.substr(pos_hex + 1, 2);
				h << std::hex;

				int i;
				h>>i;

				std::stringstream f;
				f << static_cast<char>(i);
				std::string s;
				f >> s;

				val.replace(pos_hex, 3, s);
				pos_hex ++;
			}

			params.insert(std::map<std::string,std::string>::value_type(nam, val));
		}
	}
	else
	{
		path = get_req;
	}
}

static inline bool is_base64(unsigned char c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(std::string const& encoded_string)
{
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
	{
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4)
		{
			for (i = 0; i <4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j <4; j++)
			char_array_4[j] = 0;

		for (j = 0; j <4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret += char_array_3[j];
	}

	return ret;
}

std::vector<std::string> Tokenize(const std::string& str,const std::string& delimiters)
{

	std::vector<std::string> tokens;

	/**
	 * skip delimiters at beginning.
	 */

	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

	/**
	 * find first "non-delimiter".
	 */

	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos)
	{

		/**
		 * found a token, add it to the vector.
		 */

		tokens.push_back(str.substr(lastPos, pos - lastPos));

		/**
		 * skip delimiters.  Note the "not_of"
		 */

		lastPos = str.find_first_not_of(delimiters, pos);

		/**
		 * find next "non-delimiter"
		 */

		pos = str.find_first_of(delimiters, lastPos);
	}

	return tokens;
}

std::string trim_right( const std::string & source,const std::string & t)
{
	std::string str = source;
	return str.erase ( str.find_last_not_of ( t ) + 1 ) ;
}


std::string trim_left ( const std::string & source, const std::string & t  )
{
	std::string str = source;
	return str.erase ( 0 , source.find_first_not_of ( t ) ) ;
}


std::string trim( const std::string & source, const std::string & t )
{
	std::string str = source;
	return trim_left ( trim_right ( str , t ) , t ) ;
}