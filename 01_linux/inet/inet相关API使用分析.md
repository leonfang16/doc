# inet相关API使用分析
## IP地址的字符串转换为整型
总共有以下3个函数  
1、inet_aton  
函数原型如下：  
	`int inet_aton(const char *cp, struct in_addr *inp);`	  
该函数第一个参数为ip地址的字符串形式，第二个参数为in_addr类型的结构体指针，in_addr的定义在头文件<in
.h>中，定义如下：
	
	typedef uint32_t in_addr_t;  
	struct in_addr  
	  {  
	    in_addr_t s_addr;  
	  };  
如果转换成功返回非0整数，失败则返回0。inet_aton的作用是将字符串类型的ip地址转换成网络字节序的整型，字符串转换整型是用的最多。

2、inet_address  
函数原型如下：  
`in_addr_t inet_addr(const char *cp);`
inet_address是把字符串类型的ip地址转换成网络字节序。

3、inet_network  
函数原型如下：  
`in_addr_t inet_network(const char *cp);`
inet_address是把字符串类型的ip地址转换成本地字节序，注意和inet_address的区别。

## 网络字节序和本地字节序转换函数 
1、inet_htonl  
2、inet_htons  
3、inet_ntohl  
4、inet_ntohs  
函数原型分别如下：  

       uint32_t htonl(uint32_t hostlong);

       uint16_t htons(uint16_t hostshort);

       uint32_t ntohl(uint32_t netlong);

       uint16_t ntohs(uint16_t netshort);
htonl和htons分别是将long和short类型的本地字节序转换成网络字节序，ntohl和ntohs则刚好相反。
inet_aton和ntohl可以合并起来使用，把字符换类型的ip地址转换成本地字节序的整型类型的ip地址，如下图代码所示。

## 示例代码  
代码如下：

	#include <iostream>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	
	int main(int argc, char const *argv[])
	{
		int ret;
		uint32_t ip_int;
		char *ip = "10.10.0.7";
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

运行结果：  
10.10.0.7, using inet_aton, network endian:117443082  
10.10.0.7, after ntohl, host endian:168427527  
168427527, using inet_ntoa:7.0.10.10  
10.10.0.7, using inet_addr, addr_int:117443082  
10.10.0.7, using inet_network, network_int:168427527  
