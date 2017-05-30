
int prepare_connection(char *ip, char *port);
int get_socket_fd();
int send_to_server(char *buf, int len);
void clean_transmission();
