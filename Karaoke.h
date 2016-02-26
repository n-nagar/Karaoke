
/*
** Karaoke Player using CD+G format files
**  with Extensible architecture for Display, Audio & IO
**  Default implementation uses OpenGL, GLFW3 and FMOD with local files
**
** (c) Niranjan Nagar
**  uses CD+G spec from http://jbum.com/cdg_revealed.html
*/
struct SubCode
{
	unsigned char command;
	unsigned char instruction;
	unsigned char parityQ[2];
	unsigned char data[16];
	unsigned char parityP[4];
};

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

class KaraokeAudio
{
public:
	virtual bool Play() = 0;
	virtual unsigned int GetPlayPosition() = 0;
	virtual void Update() = 0;
	static KaraokeAudio *GetPlayer(const char *filename);
};

class CDGReader
{
public:
	virtual bool Done() = 0;
	virtual bool Start() = 0;
	virtual const SubCode *ReadNext() = 0;
	static CDGReader *GetReader(const char *filename);
};

class CDGParser
{
public:
	virtual bool Start() = 0;
	virtual bool WaitUntilDone() = 0;
	static CDGParser *GetParser(CDGScreenHandler *h, KaraokeAudio *p, CDGReader *r);
};

