### linux下boost安装和使用
## boost库安装
操作系统为Ubuntu 16.04.3 LTS，使用apt-get轻松完成安装，命令如下：  
`sudo apt-get install libboost-all-dev`

安装后头文件放在  
`/usr/include/boost`

库文件放在  
`/usr/lib/x86_64-linux-gnu`

## boost库使用
示例代码如下：

	#include <iostream>
	#include <boost/thread/thread.hpp>
	
	void RunTask()
	{
		std::cout << "hell RunTask" << std::endl;
		return;
	}
	
	int main(int argc, char const *argv[])
	{
		boost::thread my_thread(RunTask);
	
		my_thread.join();
		return 0;
	}

直接使用g++进行编译：  
`g++ my.cpp -lboost_thread -lboost_system`  
如果提示找不到boost相关定义，添加头文件和库文件的路径，如下：
`g++ thread.cpp -I /usr/include/ -L /usr/lib/x86_64-linux-gnu/ -lboost_thread -lboost_system`

## boost源码链接
`https://sourceforge.net/projects/boost/files/boost/`