#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char const *argv[])
{
	int ret;
	uint32_t ip_int;
	char *ip = (char*)"10.10.0.7";
	struct in_addr inp, outp;

	ret = inet_aton(ip, &inp);
	if (ret <= 0)
	{
		std::cout << "inet_aton failed" << std::endl;
		return -1;
	}
	std::cout << ip << ", using inet_aton, network endian:" << inp.s_addr << std::endl;

	ip_int = ntohl(inp.s_addr);
	std::cout << ip << ", after ntohl, host endian:" << ip_int << std::endl;

	// 把整型的ip地址转换成字符串
	outp.s_addr = ip_int;
	char *r = inet_ntoa(outp);
	std::cout << ip_int << ", using inet_ntoa:" << r << std::endl;

	in_addr_t addr_int;
	addr_int = inet_addr(ip);
	std::cout << ip << ", using inet_addr, addr_int:" << addr_int << std::endl;

	in_addr_t network_int;
	network_int = inet_network(ip);
	std::cout << ip << ", using inet_network, network_int:" << network_int << std::endl;

	return 0;
}