#include <vk_engine.h>
#include <iostream>

int main(int argc, char* argv[])
{
#ifdef PERFORMANCE_TEST
	for (int i = 0; i < 100; i++)
	{
#endif // PERFORMANCE_TEST

		VulkanEngine engine;

		engine.init();

		engine.run();

		engine.cleanup();
#ifdef PERFORMANCE_TEST
	}
#endif // PERFORMANCE_TEST
	

	return 0;
}
