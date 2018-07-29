#include <iostream>
#include <boost/thread/thread.hpp>

void RunTask()
{
	std::cout << "hell RunTask1" << std::endl;
	return;
}

int main(int argc, char const *argv[])
{
	boost::thread my_thread(RunTask);

	my_thread.join();
	return 0;
}