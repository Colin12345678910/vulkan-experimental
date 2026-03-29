#include <gtest/gtest.h>
#include <VkEngine.h>

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}

TEST(HelloTest, CameraExample)
{
	Camera c;
	EXPECT_STRNE("fail", "fail");
	//c.velocity = glm::vec3(1 * 0.15, 0, 0);
	//c.Update();

	//EXPECT_EQ(c.position, glm::vec3(1, 0, 0));
}

/*
* Honestly, I'm not sure if testing engine init is paticularly "Good practice"
* but it does verify that this entire testing framework can link against the engine properly.
* If the engine fails to initialize, well, that's a problem.
*/
TEST(HelloTest, EngineInvoke)
{
	VulkanEngine engine;
	engine.init();

	engine.cleanup();
}

TEST(HelloTest, CVAR)
{

	//CVar::Get()->SetFloatCVar(std::hash<std::string>{}("test.float"), 2.0f);
	//EXPECT_EQ(CVar::Get()->CVar::GetFloatCVar(BY_NAME("test.float")), 2.0f);
}