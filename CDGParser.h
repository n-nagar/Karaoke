
class CDGScreenHandler
{
public:
	static const int WIDTH = 300;
	static const int HEIGHT = 216;
	static const int CHAR_HEIGHT = 12;
	static const int CHAR_WIDTH = 6;
	static const int MAX_COLORS = 16;
	static const int RED_MASK = 0x0F00;
	static const int GREEN_MASK = 0x00F0;
	static const int BLUE_MASK = 0x000F;
	typedef unsigned char Screen[HEIGHT][WIDTH];
	virtual void InitColors(const unsigned short colors[]) = 0;
	virtual void Display(const Screen *Hs) = 0;
};

class CDGParser
{
public:
	virtual bool Start() = 0;
	virtual bool WaitUntilDone() = 0;
	static CDGParser *GetParser(char *filename, CDGScreenHandler* handler);
};