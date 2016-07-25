#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "threadpool.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

void usage();

int isDigits(char *str);

char *get_mime_type(char *name);

void function(int connection);

char absPath[PATH_MAX];

int main(int argc, char **argv) {
	if (argc != 4) {
		usage();
		exit(-1);
	}

	// check the input and assign it
	int port = -1, pool_size = -1, max_requests = -1;
	if ((sscanf(argv[1], "%d", &port) != 1) || (isDigits(argv[1]) == -1)) {
		fprintf(stderr, "wrong input\n");
		exit(-1);
	}
	if ((sscanf(argv[2], "%d", &pool_size) != 1) || (isDigits(argv[2]) == -1)) {
		fprintf(stderr, "wrong input\n");
		exit(-1);
	}
	if ((sscanf(argv[3], "%d", &max_requests) != 1) || (isDigits(argv[3]) == -1)) {
		fprintf(stderr, "wrong input\n");
		exit(-1);
	}

	// get absolute path
	char absPathTmp[PATH_MAX];
	int retId = 0;
	if ((retId =readlink("/proc/self/exe", absPathTmp, PATH_MAX)) == -1) {
		perror("readlink");
		exit(-1);
	}
	absPathTmp[retId] = '\0';
char *p;
p = strrchr(absPathTmp, '/');
if (p)
*p = '\0';
	strcpy(absPath, absPathTmp);

	// create threadpool
	threadpool *tPool = NULL;

	tPool = create_threadpool(pool_size);
	if (!tPool) {
		fprintf(stderr, "create_threadpool\n");
		exit(-1);
	}

	// create socket
	int	 sd;
	if ((sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket\n");
		exit(-1);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((short) port);

	if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror("bind\n");
		exit(-1);
	}

	if (listen(sd, max_requests) < 0) {
		perror("listen\n");
		exit(-1);
	}
	int i = 0;
	// program loop
	while (i < max_requests) {
		struct sockaddr_in client;
		int clientLen = sizeof(client);
		intptr_t connection;
		if ((connection = accept(sd, (struct sockaddr*) &client, &clientLen)) < 0) {
			perror("accept\n");
		}
		else {
			dispatch(tPool, (dispatch_fn) function, (void*) connection);
		}

		i++;
	}

	destroy_threadpool(tPool);
	close(sd);

	return 0;
}

void usage() {
	printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
}

/*
** check if the a string holds digits only
** returns 0 for only digits and -1 in case of a failure
*/
int isDigits(char *str) {
	int i = 0;
	while (str[i]) {
		if (isdigit(str[i]) == 0) {
			return -1;
		}
		i++;
	}

	return 0;
}

char *get_mime_type(char *name) 
{ 
	char *ext = strrchr(name, '.'); 
	if (!ext) return NULL; 
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html"; 
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg"; 
	if (strcmp(ext, ".gif") == 0) return "image/gif"; 
	if (strcmp(ext, ".png") == 0) return "image/png"; 
	if (strcmp(ext, ".css") == 0) return "text/css"; 
	if (strcmp(ext, ".au") == 0) return "audio/basic"; 
	if (strcmp(ext, ".wav") == 0) return "audio/wav"; 
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo"; 
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg"; 
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg"; 
	return NULL; 
}

void function(int connection) {
	// read request from socket.
	// parse only the first line, assumeing that the first line is no longer then 4000 bytes.
	char request[4000];
	memset(request, 0, sizeof(request));
	char requestBuf[4000];
	memset(requestBuf, 0, sizeof(requestBuf));

	int isServerError = -1; // incase of a error like malloc in thread returns intranal server error

	char *p;
	int limitFlag = -1; // request limit flag
	int readRet = 0, i = 0, totalRead = 0;
	while ((readRet = read(connection, requestBuf, sizeof(requestBuf))) >= 0) {
		if (readRet > 0) {
			p = strstr(requestBuf, "\r\n"); // find end of first line.
			if (p) { // check to parse only first line
				*p = '\0';
				limitFlag = 0;
			}

			i = 0;
			while (totalRead < (totalRead + readRet)) {
				if ((limitFlag == 0) && (requestBuf[i] == '\0'))
				break;

				request[totalRead] = requestBuf[i];

				i++;
				totalRead++;

				if (totalRead == 4000) {
					limitFlag = 0;
					break;
				}
			}

			memset(requestBuf, 0, readRet);
		}

		if (limitFlag == 0)
		break;
	}

	if (readRet < 0) {
		close(connection);
		return;
	}

	// response init
	char *response = "";
	int responseLen = 0;
	int resFlag = -1;

	time_t now; 
	char timebuf[128]; 
	now = time(NULL); 
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now)); //timebuf holds the correct format of the current time.

	// check input, format: method, path and http version.
	const int requestLen = strlen(request);
	char requestTmp[requestLen +1];
	strcpy(requestTmp, request);
	char method[requestLen +1], path[requestLen +1], httpVer[requestLen +1];

	char *tok;
	tok = strtok(requestTmp, " ");
	int j = 0;
	while (tok) {
		if (j == 0)
		strcpy(method, tok);

		if (j == 1)
		strcpy(path, tok);

		if (j == 2)
		strcpy(httpVer, tok);

		j++;
		tok = strtok(NULL, " ");
	}

	// check http version and if there is 3 args in line otherwise return http 400 bad request
	int httpVerFlag = -1;
	if ((strcmp(httpVer, "HTTP/1.0") == 0) || (strcmp(httpVer, "HTTP/1.1") == 0) || (strcmp(httpVer, "HTTP/2") == 0))
	httpVerFlag = 0;

	if ((j != 3) || (httpVerFlag == -1)) {
		char *badRequest0 = "HTTP/1.0 400 Bad Request\r\nServer: webserver/1.0\r\nDate: ";
		char *badRequest1 = "\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>\r\n";
		int reqLen = strlen(badRequest0) + strlen(badRequest1) + strlen(timebuf);
		char httpBadRequest[reqLen +1];
		strcpy(httpBadRequest, badRequest0);
		strcat(httpBadRequest, timebuf);
		strcat(httpBadRequest, badRequest1);

		response = (char*) malloc( sizeof(httpBadRequest) );
		if (!response)
			isServerError = 0;
		else
			strcpy(response,httpBadRequest);
			responseLen = strlen(response);
		resFlag = 0;
	}

	// check for method - GET
	if ((strcmp(method, "GET") != 0) && (resFlag == -1)) {
		char *notSupported0 = "HTTP/1.0 501 Not supported\r\nServer: webserver/1.0\r\nDate: ";
		char *notSupported1 = "\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.\r\n</BODY></HTML>\r\n";
		char notSupported[(strlen(notSupported0) + strlen(notSupported1) + strlen(timebuf) +1)];
		strcpy(notSupported, notSupported0);
		strcat(notSupported, timebuf);
		strcat(notSupported, notSupported1);
	
		response = (char*) malloc( sizeof(notSupported) );
		if (!response)
			isServerError = 0;
		else
		strcpy(response,notSupported);
		responseLen = strlen(response);
		resFlag = 0;
	}

	char *pPathTmp = "";
	char pathTmp1[(strlen(path) +1)];
	strcpy(pathTmp1, path);
	if (strcmp(pathTmp1, "http://") > 0) {
		int k = 7;
		while (k < strlen(pathTmp1)) {
			if (pathTmp1[k] == '/')
			break;

			k++;
		}

		char pathTmp0[(strlen(pathTmp1) -k +1)];
		int n = 0;
		while (k < strlen(pathTmp1)) {
			pathTmp0[n] = pathTmp1[k];
			k++;
			n++;
		}
		pPathTmp = pathTmp0;
	}
	else {
		pPathTmp = pathTmp1;
	}
	char pathTmp[(strlen(absPath) + strlen(pPathTmp) +1)];
	strcpy(pathTmp, absPath);
	strcat(pathTmp, pPathTmp);

	struct stat sb;
	int isPathExists = 0;
	if (stat(pathTmp, &sb) == -1) {
		isPathExists = -1;
	}

	if ((isPathExists == -1) && (resFlag == -1)) {
		char *notFound0 = "HTTP/1.0 404 Not Found\r\nServer: webserver/1.0\r\nDate: ";
		char *notFound1 = "Content-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>\r\n";
		char notFound[(strlen(notFound0) + strlen(notFound1) + strlen(timebuf) +1)];
		strcpy(notFound, notFound0);
		strcat(notFound, timebuf);
		strcat(notFound, notFound1);
		response = (char*) malloc( sizeof(notFound) );
		if (!response)
			isServerError = 0;
		else
		strcpy(response,notFound);
		responseLen = strlen(response);
		resFlag = 0;
	}

	if (strlen(pathTmp) > 0) 
	{
		if ((S_ISDIR(sb.st_mode)) && (pathTmp[(strlen(pathTmp) -1)] != '/') && (resFlag == -1)) {
			char *found0 = "HTTP/1.0 302 Found\r\nServer: webserver/1.0\r\nDate: ";
			char *found1 = "\r\nLocation: ";
			char *found2 = "/\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>\r\n";
			char found[(strlen(found0) + strlen(found1) + strlen(found2) + strlen(timebuf) + strlen(pPathTmp) +1)];
			strcpy(found, found0);
			strcat(found, timebuf);
			strcat(found, found1);
			strcat(found, pPathTmp);
			strcat(found, found2);
			response = (char*) malloc( sizeof(found) );
		if (!response)
			isServerError = 0;
		else
			strcpy(response,found);
		responseLen = strlen(response);
			resFlag = 0;
		}

		if ((S_ISDIR(sb.st_mode)) && (pathTmp[(strlen(pathTmp) -1)] == '/') && (resFlag == -1)) {
			struct stat sb0;
			int isIndexExists = 0;
			char indexPathTmp[(strlen(pathTmp) + strlen("index.html") +1)];
			strcpy(indexPathTmp, pathTmp);
			strcat(indexPathTmp, "index.html");
			if (stat(indexPathTmp, &sb0) == -1) {
				isIndexExists = -1;
			}

			if (isIndexExists == 0) { // case: index.html exists in dir, returns it.
				size_t indexFileLen = (sb0.st_size) +1;
				char source[indexFileLen];
				size_t newLen = 0;
				FILE *fp = fopen(indexPathTmp, "r");
				if (fp != NULL) {
					newLen = fread(source, sizeof(char), indexFileLen, fp);
					if (newLen == 0) {
						isServerError = 0;
					}
					else {
						source[newLen++] = '\0'; // Just to be safe.
					}
					fclose(fp);

					if (newLen != indexFileLen) // check if all the file was copyed, or an error accoured.
					isServerError = 0;

					// constract response
					if (isServerError == -1) {
						char *indexFound0 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";
						char *indexFound1 = "\r\nContent-Type: text/html\r\nContent-Length: ";
						// Content-Length
						char contentLength[33];
						sprintf(contentLength, "%zu", newLen);
						char *indexFound2 = "\r\nLast-Modified: ";
						//<last-modification-data>
						char lastModifiedcationBuf[128];
						strftime(lastModifiedcationBuf, sizeof(lastModifiedcationBuf), RFC1123FMT, gmtime(&sb0.st_mtime));
						char *indexFound3 = "\r\nConnection: close\r\n\r\n";
						char indexFound[(strlen(indexFound0) + strlen(indexFound1) + strlen(indexFound2) + strlen(indexFound3) + strlen(timebuf) + strlen(lastModifiedcationBuf) + strlen(contentLength) + strlen(source) +1)];
						strcpy(indexFound, indexFound0);
						strcat(indexFound, timebuf);
						strcat(indexFound, indexFound1);
						strcat(indexFound, contentLength);
						strcat(indexFound, indexFound2);
						strcat(indexFound, lastModifiedcationBuf);
						strcat(indexFound, indexFound3);
						strcat(indexFound, source);
				
						response = (char*) malloc( sizeof(indexFound) );
		if (!response)
			isServerError = 0;
		else
						strcpy(response,indexFound);
						responseLen = strlen(response);
						resFlag = 0;
					}
				}
				else { // error opening file
					isServerError = 0;
				}
			}
			else { // case: index.html not exists in dir, returns the contant of the dir.
				struct dirent *pDirent;
				DIR *pDir;
				char **dirFiles = NULL;
				pDir = opendir (pathTmp);
				if (pDir == NULL) { // cannot open dir
					isServerError = 0;
				}
				else {
					int fileCounter = 0;
					while ((pDirent = readdir(pDir)) != NULL) {
						dirFiles = (char**) realloc(dirFiles, (fileCounter +1) * sizeof(char*));

						if (!dirFiles) {
							isServerError = 0;
							break;
						}

						dirFiles[fileCounter] = (char*) malloc((strlen(pDirent->d_name) +1) * sizeof(char));
						if(!dirFiles[fileCounter]) {
							isServerError = 0;
							break;
						}

						strcpy(dirFiles[fileCounter], pDirent->d_name);
						fileCounter++;
					}
					closedir (pDir);

					if (isServerError == -1) {
						char entityLines[fileCounter][500];
						struct stat sb1;
						int fileCounterTmp = 0;
						while (fileCounterTmp < fileCounter) {
						char fullPath[PATH_MAX];
							strcpy(fullPath, pathTmp);
							strcat(fullPath, dirFiles[fileCounterTmp]);
							if (stat(fullPath, &sb1) == -1) {
								isServerError = 0;
								break;
							}

							// preparing vars
							//<last-modification-data>
							char lastModifiedcationBuf[128];
							strftime(lastModifiedcationBuf, sizeof(lastModifiedcationBuf), RFC1123FMT, gmtime(&sb1.st_mtime));

							// constract entity line
							strcpy(entityLines[fileCounterTmp], "<tr><td><A HREF=\"");
							strcat(entityLines[fileCounterTmp], pPathTmp);
							strcat(entityLines[fileCounterTmp], dirFiles[fileCounterTmp]);
							strcat(entityLines[fileCounterTmp], "\">");
							strcat(entityLines[fileCounterTmp], dirFiles[fileCounterTmp]);
							strcat(entityLines[fileCounterTmp], "</A></td><td>");
							strcat(entityLines[fileCounterTmp], lastModifiedcationBuf);
							strcat(entityLines[fileCounterTmp], "</td><td>");
							if (S_ISREG(sb1.st_mode)) {
								size_t fileLen = (sb1.st_size);
								char contentLength[33];
								sprintf(contentLength, "%zu", fileLen);
								strcat(entityLines[fileCounterTmp], contentLength);
							}
							strcat(entityLines[fileCounterTmp], "</td></tr>\r\n");

							fileCounterTmp++;
						}

						// free dirFiles
						fileCounterTmp = 0;
						while (fileCounterTmp < fileCounter) {
							free(dirFiles[fileCounterTmp]);
							fileCounterTmp++;
						}
						free(dirFiles);

						//constract response
						if (isServerError == -1) { // incase there was no internal server error
							char *dir_content_body0 = "<HTML>\r\n<HEAD><TITLE>Index of <path-of-directory></TITLE></HEAD>\r\n<BODY>\r\n<H4>Index of ";
							//<path-of-directory>
							char *dir_content_body1 = "</H4>\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n";
							// entity lines
							char *dir_content_body2 = "</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>\r\n";

							// constract body
							int bodyLen = 0;
							fileCounterTmp = 0;
							while (fileCounterTmp < fileCounter) {
								bodyLen += strlen(entityLines[fileCounterTmp]);
								fileCounterTmp++;
							}
							bodyLen += strlen(dir_content_body0) + strlen(dir_content_body1) + strlen(dir_content_body2) + strlen(pathTmp);
							char dir_content_body[bodyLen +1];
							strcpy(dir_content_body, dir_content_body0);
							strcat(dir_content_body, pPathTmp);
							strcat(dir_content_body, dir_content_body1);

							// add entity lines
							fileCounterTmp = 0;
							while (fileCounterTmp < fileCounter) {
								strcat(dir_content_body, entityLines[fileCounterTmp]);
								fileCounterTmp++;
							}
							strcat(dir_content_body, dir_content_body2);

							// constract header
							char *dir_content0 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";
							//date
							char *dir_content1 = "\r\nContent-Type: text/html\r\nContent-Length: ";
							//<content-length>
							char contentLength[33];
							sprintf(contentLength, "%d", bodyLen);
							char *dir_content2 = "\r\nLast-Modified: ";
							// last modification
							char lastModifiedcationBuf[128];
							strftime(lastModifiedcationBuf, sizeof(lastModifiedcationBuf), RFC1123FMT, gmtime(&sb.st_mtime));
							char *dir_content3 = "\r\nConnection: close\r\n\r\n";

							//constract response
							char dir_content[(strlen(dir_content0) + strlen(dir_content1) + strlen(dir_content2) + strlen(dir_content3) + strlen(timebuf) + strlen(contentLength) + strlen(lastModifiedcationBuf) + strlen(dir_content_body) +1)];
							strcpy(dir_content, dir_content0);
							strcat(dir_content, timebuf);
							strcat(dir_content, dir_content1);
							strcat(dir_content, contentLength);
							strcat(dir_content, dir_content2);
							strcat(dir_content, lastModifiedcationBuf);
							strcat(dir_content, dir_content3);
							strcat(dir_content, dir_content_body);
							
							response = (char*) malloc( sizeof(dir_content) );
		if (!response)
			isServerError = 0;
		else
							strcpy(response,dir_content);
							responseLen = strlen(response);
							resFlag = 0;
						}
					}
				}			
			}
		}
		// if the path is a file
		if ((resFlag == -1) && (isServerError == -1)) {
			int isForbidden = -1;
			if (S_ISREG(sb.st_mode)) {
				
				size_t fileLen = (sb.st_size) +1;
				char source[fileLen];
				size_t newLen;
				
				FILE *fp = fopen(pathTmp, "r");
				if (fp != NULL) {
					newLen = fread(source, sizeof(char), fileLen, fp);
					if (newLen == 0) {
						isServerError = 0;
					}
					else {
						source[newLen++] = '\0'; // Just to be safe.
					}
					fclose(fp);
					
					if (newLen != fileLen) // check if all the file was copyed, or an error accoured.
					isServerError = 0;

					// constract response
					if (isServerError == -1) {
						// preparing data
						size_t fileLen0 = 0;
						char *file0 = "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: ";
						fileLen0 += strlen(file0);
						// date
						fileLen0 += strlen(timebuf);

						char *contentType = get_mime_type(pathTmp);
						char *file1 = "";
						if (contentType) { ////optional
							file1 = "\r\nContent-Type: ";
							fileLen0 += strlen(file1) + strlen(contentType);
						}
						char *file2 = "\r\nContent-Length: ";
						fileLen0 += strlen(file2);
						// Content-Length
						char contentLength[33];
						sprintf(contentLength, "%zu", newLen);
						fileLen0 += strlen(contentLength);
						char *file3 = "\r\nLast-Modified: ";
						fileLen0 += strlen(file3);

						//<last-modification-data>
						char lastModifiedcationBuf[128];
						strftime(lastModifiedcationBuf, sizeof(lastModifiedcationBuf), RFC1123FMT, gmtime(&sb.st_mtime));
						fileLen0 += strlen(lastModifiedcationBuf);
						char *file4 = "\r\nConnection: close\r\n\r\n";
						fileLen0 += strlen(file4);
						//<file-data>
//						fileLen0 += strlen(source);
						fileLen0 += fileLen;
						//fileLen0 += 1; // for end string
						char fileStr[(fileLen0 +1)];
						strcpy(fileStr, file0);
						strcat(fileStr, timebuf);
						if (contentType != NULL) {
							strcat(fileStr, file1);
							strcat(fileStr, contentType);
						}
						strcat(fileStr, file2);
						strcat(fileStr, contentLength);
						strcat(fileStr, file3);
						strcat(fileStr, lastModifiedcationBuf);

						strcat(fileStr, file4);
						strcat(fileStr, source);
memcpy(fileStr + (fileLen0 - fileLen), source, fileLen);

						
						response = (char*) malloc( sizeof(char) * fileLen0);
		if (!response)
			isServerError = 0;
		else
						memcpy(response,fileStr, fileLen0);
						responseLen = fileLen0;
						resFlag = 0;

					}
				}
				else { // cannot open file
					isForbidden = 0;
				}
			}
			else { // file is not regular
				isForbidden = 0;
			}
			// constract is forbidden
			if (isForbidden == 0) 
			{
				char *forbidden0 = "HTTP/1.0 403 Forbidden\r\nServer: webserver/1.0\r\nDate: ";
				// date
				char *forbidden1 = "\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>\r\n";
				char forbidden[(strlen(forbidden0) + strlen(forbidden1) + strlen(timebuf) +1)];
				strcpy(forbidden, forbidden0);
				strcat(forbidden, timebuf);
				strcat(forbidden, forbidden1);
				
				response = (char*) malloc( sizeof(forbidden) );
		if (!response)
			isServerError = 0;
		else
				strcpy(response,forbidden);
				responseLen = strlen(response);
				resFlag = 0;
			}
		}
	}
	if (isServerError == 0) 
	{//incase that an error accoured in thread
		const char *internalServerError0 = "HTTP/1.0 500 Internal Server Error\r\nServer: webserver/1.0\r\nDate: ";
		// date
		const char *internalServerError1 = "\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n";
		char internalServerError[(strlen(internalServerError0) + strlen(internalServerError1) + strlen(timebuf) +1)];
		strcpy(internalServerError, internalServerError0);
		strcat(internalServerError, timebuf);
		strcat(internalServerError, internalServerError1);

		response = internalServerError;
		responseLen = strlen(response);
		resFlag = 0;
	}


	// send response
	int sent = 0, sentret = 0;
	while (sent < responseLen) {
		sentret = write(connection, response +sent, responseLen - sent);
		if (sentret <= 0) {
			break;
		}
		sent += sentret;
	}

	close(connection);
	if (isServerError != 0)
		free(response);
}
