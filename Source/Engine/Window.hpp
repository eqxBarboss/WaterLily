#pragma once

struct GLFWwindow;

struct Extent2D
{
	int width = 0;
	int height = 0;
};

class Window
{
public:
	Window(int width, int height, std::string title);
	~Window();

	bool ShouldClose() const;
	void PollEvents() const;

	Extent2D GetExtentInPixels() const;

	GLFWwindow* glfwWindow;
};