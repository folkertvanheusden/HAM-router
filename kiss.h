bool recv_mkiss(int fd, unsigned char **p, int *len, bool verbose);
void send_mkiss(int fd, int channel, const unsigned char *p, const int len);
