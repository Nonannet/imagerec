int setupCmdTcpServer(int port);
int waitForTcpData(char* cmdData, int lenght);
void sendTcpData(char* data, int lenght);
void closeTcpConnection();