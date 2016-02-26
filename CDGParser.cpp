#include "Karaoke.h"
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <time.h>


/*
** Implementing CDG parsing based on spec described at:
**  http://jbum.com/cdg_revealed.html
**
** Code by Niranjan Nagar
*/




enum CDG_INSTRUCTIONS 
{  
	MEMORY_PRESET = 1,
	BORDER_PRESET = 2,
	TILE_BLOCK_NORMAL = 6,
	SCROLL_PRESET = 20,
	SCROLL_COPY = 24,
	DEF_TRANSPARENT_COLOR = 28,
	LOAD_COLOR_TABLE_LO = 30,
	LOAD_COLOR_TABLE_HI = 31,
	TILE_BLOCK_XOR = 38
};


class MyCDGParser : public CDGParser
{
private:
	CDGReader *cdg_file;
	unsigned short colors[16];
	CDGScreenHandler::Screen screen;
	CDGScreenHandler *handler;
	KaraokeAudio *ap;
	pthread_t thread;
	bool worker_thread_valid;

public:
	MyCDGParser(CDGScreenHandler *h, KaraokeAudio *player, CDGReader *rdr);
	~MyCDGParser();	
	bool Start();
	bool WaitUntilDone();
	void MemoryPreset(const SubCode *s);
	void BorderPreset(const SubCode *s);
	void TileBlockNormal(const SubCode *s);
	void ScrollPreset(const SubCode *s);
	void ScrollCopy(const SubCode *s);
	void DefTransparentColor(const SubCode *s);
	void LoadColorTableLo(const SubCode *s);
	void LoadColorTableHi(const SubCode *s);
	void TileBlockXor(const SubCode *s);
	static void *DoParse(void *obj);
};


MyCDGParser::~MyCDGParser()
{

}

bool MyCDGParser::Start()
{
	if ((cdg_file == NULL) || (handler == NULL) || (cdg_file->Done()))
		return false;

	if (!cdg_file->Start())
		return false;

	if (pthread_create(&thread, NULL, DoParse, (void *)this))
		return false;

	worker_thread_valid = true;
	return true;
	//void *status;
	//pthread_join(thread, &status);
}

bool MyCDGParser::WaitUntilDone()
{
	void *status;
	if (worker_thread_valid)
	{
		pthread_join(thread, &status);
		worker_thread_valid = false;
	}
	return true;
}

/*
** Returns time difference in Microseconds
*/
unsigned long time_diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp.tv_sec * 1000000 + (temp.tv_nsec/1000);
}

void * MyCDGParser::DoParse(void *ptr)
{ 
	MyCDGParser *obj = static_cast<MyCDGParser *>(ptr);
	const SubCode *s;
	const int USEC_IN_MS = 1000;
	struct timespec begin, start, end;
	unsigned long packet_num = 0;
	if (obj->ap)
		obj->ap->Play();
	clock_gettime(CLOCK_REALTIME, &begin);
	while (!obj->cdg_file->Done())
	{
		if (packet_num > 0)
		{
			unsigned long diff_time;
			if (obj->ap)
				diff_time = USEC_IN_MS * obj->ap->GetPlayPosition();
			else
			{
				clock_gettime(CLOCK_REALTIME, &start);
				diff_time = time_diff(begin, start);
			}
			// Each CDG packet paces at 1/300th of a second
			//  which is ~3333 microseconds
			unsigned long packet_time = packet_num * 3333;
			//fprintf(stderr, "%ld - %ld\n", diff_time, packet_time );
			if (packet_time > diff_time)
				usleep(packet_time - diff_time);

		}
		s = obj->cdg_file->ReadNext();
		if (s == NULL)
			pthread_exit(NULL);
		packet_num++;

		//fprintf(stderr, "CMD = %02X\n", s->command);
		const char *cmd;

		if ((s->command & 0x3F) == 9) 
		{
			switch (s->instruction & 0x3F)
			{
				case MEMORY_PRESET:
					cmd = "MEMORY_PRESET";
					obj->MemoryPreset(s);
					break;
				case BORDER_PRESET:
					cmd = "BORDER_PRESET";
					obj->BorderPreset(s);
					break;
				case TILE_BLOCK_NORMAL:
					cmd = "TILE_BLOCK_NORMAL";
					obj->TileBlockNormal(s);
					break;
				case SCROLL_PRESET:
					cmd = "SCROLL_PRESET";
					obj->ScrollPreset(s);
					break;
				case SCROLL_COPY:
					cmd = "SCROLL_COPY";
					obj->ScrollCopy(s);
					break;
				case DEF_TRANSPARENT_COLOR:
					cmd = "DEF_TRANSPARENT_COLOR";
					obj->DefTransparentColor(s);
					break;
				case LOAD_COLOR_TABLE_LO:
					cmd = "LOAD_COLOR_TABLE_LO";
					obj->LoadColorTableLo(s);
					obj->handler->InitColors(obj->colors);
					break;
				case LOAD_COLOR_TABLE_HI:
					cmd = "LOAD_COLOR_TABLE_HI";
					obj->LoadColorTableHi(s);
					obj->handler->InitColors(obj->colors);
					break;
				case TILE_BLOCK_XOR:
					cmd = "TILE_BLOCK_XOR";
					obj->TileBlockXor(s);
					break;
				default:
					cmd = "Undefined";
			}
			obj->handler->Display(&obj->screen);
			//std::cerr << cmd << "\n";
		}
	}
	pthread_exit(NULL);
}

void MyCDGParser::MemoryPreset(const SubCode *s)
{
	if (s == NULL)
		return;

	if ((s->data[1] & 0x0F) == 0)
	{
		for (int i = 0; i < CDGScreenHandler::HEIGHT; i++)
			for (int j = 0; j < CDGScreenHandler::WIDTH; j++)
				screen[i][j] = (s->data[0] & 0x0F);
	}
}

void MyCDGParser::BorderPreset(const SubCode *s)
{
	if (s == NULL)
		return;

	unsigned char col = s->data[0] & 0x0F;
	for (int i = 0; i < 12; i++)
		for (int j = 0; j < 300; j++)
		{
			screen[i][j] = col;
			screen[i+204][j] = col;
		}

	for (int i = 13; i < 204; i++)
		for (int j = 0; j < 6; j++)
		{
			screen[i][j] = col;
			screen[i][j+294] = col;
		}
}

void MyCDGParser::TileBlockNormal(const SubCode *s)
{
	if (s == NULL)
		return;

	// Operating on data[16]
	unsigned char color0 = s->data[0] & 0x0F;
	unsigned char color1 = s->data[1] & 0x0F;
	int row = (s->data[2] & 0x1F) * 12;
	int col = (s->data[3] & 0x3F) * 6;
	int pix_idx = 4;

	for (int i = row; i < row + 12; i++)
	{
		int pixels = s->data[pix_idx++] & 0x3F;
		int mask = 0x20;
		for (int j = col; j < col + 6; j++)
		{
			if (pixels & mask)
				screen[i][j] = color1;
			else
				screen[i][j] = color0;
			mask >>= 1;
		}
	}
}

void MyCDGParser::TileBlockXor(const SubCode *s)
{
	if (s == NULL)
		return;

	// Operating on data[16]
	unsigned char color0 = s->data[0] & 0x0F;
	unsigned char color1 = s->data[1] & 0x0F;
	int row = (s->data[2] & 0x1F) * 12;
	int col = (s->data[3] & 0x3F) * 6;
	int pix_idx = 4;

	for (int i = row; i < row + 12; i++)
	{
		int pixels = s->data[pix_idx++] & 0x3F;
		int mask = 0x20;
		for (int j = col; j < col + 6; j++)
		{
			int col;
			if (pixels & mask)
				screen[i][j] ^= color1;
			else
				screen[i][j] ^= color0;
			mask >>= 1;
		}
	}
}

void MyCDGParser::ScrollPreset(const SubCode *s)
{
	if (s == NULL)
		return;

	char color = s->data[0] & 0x0F;
	char hScroll = s->data[1] & 0x3F;
	char vScroll = s->data[2] & 0x3F;
	char hCmd = (hScroll & 0x30) >> 4;
	char hOffset = (hScroll & 0x07);
	char vCmd = (vScroll & 0x30) >> 4;
	char vOffset = (vScroll & 0x0F);

	if (hCmd == 0)
	{

	}
	else if (hCmd == 1)
	{

	}
	else if(hCmd == 2)
	{

	}
}

void MyCDGParser::ScrollCopy(const SubCode *s)
{
	return;
}

void MyCDGParser::LoadColorTableLo(const SubCode *s)
{
	if (s == NULL)
		return;

	for (int i = 0; i < 8; i++)
		colors[i] = ((s->data[i*2] & 0x3f) << 6) | (s->data[(i*2)+1] & 0x3F);
}

void MyCDGParser::LoadColorTableHi(const SubCode *s)
{
	if (s == NULL)
		return;

	for (int i = 0; i < 8; i++)
		colors[i+8] = ((s->data[i*2] & 0x3f) << 6) | (s->data[(i*2)+1] & 0x3F);

}

void MyCDGParser::DefTransparentColor(const SubCode *s)
{
	return;
}

MyCDGParser::MyCDGParser(CDGScreenHandler *h, KaraokeAudio *player, CDGReader *rdr)
{
	worker_thread_valid = false;
	handler = h;
	cdg_file = rdr;
	if (cdg_file == NULL)
		std::cerr << "Cannot open CDG file";
	ap = player;
	if (ap == NULL)
		std::cerr << "Cannot play Audio\n";
}

CDGParser *CDGParser::GetParser(CDGScreenHandler *h, KaraokeAudio *player, CDGReader *rdr)
{
	return new MyCDGParser(h, player, rdr);
}