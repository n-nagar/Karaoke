#include "CDGParser.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdio>

GLushort *screen_buffer;

void *RefreshScreen(GLFWwindow *win)
{
	glDrawPixels(CDGScreenHandler::WIDTH, CDGScreenHandler::HEIGHT, GL_RGBA, 
                GL_UNSIGNED_SHORT_4_4_4_4, screen_buffer);
	//glFlush();
	glfwSwapBuffers(win);
}

void ResizeScreen(GLFWwindow *window, int width, int height)
{
	//fprintf(stderr, "Resize[%d][%d]\n", height, width );
  	glViewport(0, 0, width, height);
}

class GraphicsDisplay : public CDGScreenHandler
{
private:
	char *song_path;
	GLFWwindow *window;
	unsigned short screen_colors[MAX_COLORS];

public:
	GraphicsDisplay(int *argc, char **argv)
	{
		window = NULL;
		screen_buffer = NULL;
		if (*argc < 2)
			return;
		song_path = new char[strlen(argv[1])+1];
		strcpy(song_path, argv[1]);
		if (!glfwInit())
			return;
		if ((window = glfwCreateWindow(WIDTH, HEIGHT, argv[1], NULL, NULL)) == NULL)
			return;
		glfwSetWindowRefreshCallback(window, (GLFWwindowrefreshfun)RefreshScreen);
		glfwSetWindowSizeCallback(window, (GLFWwindowsizefun)ResizeScreen);
		screen_buffer = new GLushort[HEIGHT * WIDTH];
		glfwMakeContextCurrent(window);
	}

	~GraphicsDisplay()
	{
		if (screen_buffer)
			delete[] screen_buffer;
		if (window)
			glfwTerminate();
	}

	void InitColors(const unsigned short colors[])
	{
		for (int i = 0; i < MAX_COLORS; i++)
			screen_colors[i] = (colors[i] << 4) | 0x000F;
	}

	void Display(const Screen *s)
	{
		if (!screen_buffer)
			return;
		for (int i = 0; i < HEIGHT; i++)
		{
			int gl_y = HEIGHT - i - 1;
			for (int j = 0; j < WIDTH; j++)
				screen_buffer[gl_y * WIDTH + j] = screen_colors[(*s)[i][j]];
		}
		glfwPostEmptyEvent();
	}

	void MainLoop()
	{		
		CDGParser *c;

		if (!window)
			return;
		c = CDGParser::GetParser(song_path, this);
		c->Start();
		while (!glfwWindowShouldClose(window))
		{
			glfwWaitEvents();
			RefreshScreen(window);
		}
		delete c;
	}

};

int main(int argc, char *argv[])	
{
	if (argc > 1)
	{
		GraphicsDisplay *gd = new GraphicsDisplay(&argc, argv);
		if (gd == NULL)
		{
			std::cerr << "Cannot create GraphicsDisplay\n";
			return -1;
		}
		gd->MainLoop();
		delete gd;
	}
	return 0;
}