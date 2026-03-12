#include <VkEngine.h>
#include <iostream>

int main(int argc, char* argv[])
{
#ifdef PERFORMANCE_TEST
	for (int i = 0; i < 100; i++)
	{
#endif // PERFORMANCE_TEST
		
		ExitInstructions instructions{};

		do
		{
			VulkanEngine engine;

			engine.init(instructions);

			engine.run();

			engine.cleanup();

			instructions = engine.exitInstructions;
		} while (instructions.relaunch);
#ifdef PERFORMANCE_TEST
	}
#endif // PERFORMANCE_TEST
	

	return 0;
}
