#include "CDGParser.h"
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/glu.h>
#include <cstdio>

GLushort screen[CDGScreenHandler::HEIGHT][CDGScreenHandler::WIDTH];

void RefreshScreen(void)
{
	glDrawPixels(CDGScreenHandler::WIDTH, CDGScreenHandler::HEIGHT, GL_RGBA, 
                GL_UNSIGNED_SHORT_4_4_4_4, screen);
	glFlush();
}

class GraphicsDisplay : public CDGScreenHandler
{
private:
	unsigned short screen_colors[MAX_COLORS];

public:
	GraphicsDisplay(int *argc, char **argv)
	{
		glutInit(argc, argv);
		glutInitWindowSize(WIDTH, HEIGHT);
		glutDisplayFunc(RefreshScreen);
		glutIdleFunc(RefreshScreen);
		glutCreateWindow(argv[1]);
	}

	~GraphicsDisplay()
	{
	}

	void InitColors(const unsigned short colors[])
	{
		for (int i = 0; i < MAX_COLORS; i++)
			screen_colors[i] = colors[i];
	}

	void Display(const Screen *s)
	{
		for (int i = 0; i < HEIGHT; i++)
		{
			int gl_y = HEIGHT - i - 1;
			for (int j = 0; j < WIDTH; j++)
			{
				unsigned int col = (*s)[i][j];
				screen[gl_y][j] = (screen_colors[col] << 4) | 0x000F;
			}
		}
		//glutPostRedisplay();
	}

};

int main(int argc, char *argv[])	
{
	CDGParser *c;

	if (argc > 1)
	{
		c = CDGParser::GetParser(argv[1], new GraphicsDisplay(&argc, argv));
		c->Start();
		glutMainLoop();
		delete c;
	}
	return 0;
}