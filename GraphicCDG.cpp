#include "Karaoke.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>
#include <cstdio>

GLushort *screen_buffer;
GLuint k_tex;
int cur_height = CDGScreenHandler::HEIGHT, cur_width = CDGScreenHandler::WIDTH;

void *RefreshScreen(GLFWwindow *win)
{
  	glViewport(0, 0, cur_width, cur_height);
  	glMatrixMode(GL_PROJECTION);
  	glLoadIdentity();
  	glOrtho(0.0f, cur_width, 0.0f, cur_height, 0.0f, 1.0f);
	glEnable(GL_TEXTURE_RECTANGLE);
	glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA4, CDGScreenHandler::WIDTH, CDGScreenHandler::HEIGHT, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, screen_buffer);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0,0);
	glTexCoord2f(CDGScreenHandler::WIDTH, 0);
	glVertex2f(cur_width, 0);
	glTexCoord2f(CDGScreenHandler::WIDTH,CDGScreenHandler::HEIGHT);
	glVertex2f(cur_width, cur_height);
	glTexCoord2f(0, CDGScreenHandler::HEIGHT);
	glVertex2f(0, cur_height);
	glEnd();
	glFlush();
	glfwSwapBuffers(win);
  	glMatrixMode(GL_MODELVIEW);
}

void ResizeScreen(GLFWwindow *window, int width, int height)
{
	if ((width < CDGScreenHandler::WIDTH) || (height < CDGScreenHandler::HEIGHT))
	{
		glfwSetWindowSize(window, CDGScreenHandler::WIDTH, CDGScreenHandler::HEIGHT);
		return;
	}
	cur_width = width;
	cur_height = height;

	GLfloat aspect = (GLfloat)width/(GLfloat)height;
  	glViewport(0, 0, width, height);
  	glMatrixMode(GL_PROJECTION);
  	glLoadIdentity();
  	glOrtho(0.0f, width, 0.0f, height, 0.0f, 1.0f);
  	glMatrixMode(GL_MODELVIEW);
}

class GraphicsDisplay : public CDGScreenHandler
{
private:
	char *song_path;
	GLFWwindow *window;
	unsigned short screen_colors[MAX_COLORS];

public:
	GraphicsDisplay(char *filename)
	{
		window = NULL;
		screen_buffer = NULL;
		if (!glfwInit())
			return;
		if ((window = glfwCreateWindow(WIDTH, HEIGHT, filename, NULL, NULL)) == NULL)
			return;
		glfwSetWindowRefreshCallback(window, (GLFWwindowrefreshfun)RefreshScreen);
		glfwSetWindowSizeCallback(window, (GLFWwindowsizefun)ResizeScreen);
		screen_buffer = new GLushort[HEIGHT * WIDTH];
		if (screen_buffer == NULL)
			return;
		memset(screen_buffer, 0, sizeof(GLushort) * HEIGHT * WIDTH);
		glfwMakeContextCurrent(window);

		glDepthMask(false);
		glGenTextures(1, &k_tex);
		glBindTexture(GL_TEXTURE_RECTANGLE, k_tex);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
		if (!window)
			return;
		while (!glfwWindowShouldClose(window))
		{
			glfwWaitEvents();
			RefreshScreen(window);
		}
	}

};

int main(int argc, char *argv[])	
{

	if (argc == 2)
	{
		char *cdg_name = new char[strlen(argv[1]) + 4];
		char *mp3_name = new char[strlen(argv[1]) + 4];
		CDGParser *c;



		sprintf(cdg_name, "%s.cdg", argv[1]);
		sprintf(mp3_name, "%s.mp3", argv[1]);

		CDGReader *rdr = CDGReader::GetReader(cdg_name);
		if (rdr == NULL)
		{
			std::cerr << "Cannot create File Reader\n";
			return -1;			
		}

		KaraokeAudio *player = KaraokeAudio::GetPlayer(mp3_name);
		if (player == NULL)
		{
			std::cerr << "Cannot create FMOD Audio\n";
			delete rdr;
			return -1;						
		}
		GraphicsDisplay *gd = new GraphicsDisplay(argv[1]);
		if (gd == NULL)
		{
			std::cerr << "Cannot create GraphicsDisplay\n";
			delete rdr;
			delete player;
			return -1;
		}

		CDGParser *parser = CDGParser::GetParser(gd, player, rdr);
		if (parser == NULL)
		{
			delete gd;
			delete player;
			delete rdr;
			std::cerr << "Cannot create CDG Parser\n";
			return -1;
		}

		parser->Start();
		gd->MainLoop();

		delete parser;
		delete player;
		delete rdr;
		delete gd;
	}
	else
		std::cerr << "Usage: " << argv[0]  << " <base file>\n";
	return 0;
}