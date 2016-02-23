#include "CDGParser.h"
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <time.h>
#include "fmod.hpp"
#include "fmod_errors.h"


/*
** Implementing CDG parsing based on spec described at:
**  http://jbum.com/cdg_revealed.html
**
** Code by Niranjan Nagar
*/

struct SubCode
{
	unsigned char command;
	unsigned char instruction;
	unsigned char parityQ[2];
	unsigned char data[16];
	unsigned char parityP[4];
};

class AudioPlayer
{
private:
	FMOD::System 	*fmod_system;
	FMOD::Sound		*karaoke;
	FMOD::Channel	*channel;
	FMOD_RESULT		result;
	unsigned int	version;
	void			*extradriverdata ;
public:
	AudioPlayer(const char *filename)
	{
		channel = 0;
		result = FMOD::System_Create(&fmod_system);
		if (result != FMOD_OK)
		{
			fprintf(stderr, "FMOD did not initialize: (%d) - %s\n", result, FMOD_ErrorString(result));
			fmod_system = NULL;
			return;
		}
		extradriverdata = NULL;
		result = fmod_system->init(32, FMOD_INIT_NORMAL, extradriverdata);
		if (result != FMOD_OK)
		{
			fprintf(stderr, "FMOD System did not initialize: (%d) - %s\n", result, FMOD_ErrorString(result));
			fmod_system = NULL;
			return;
		}
		result = fmod_system->createStream(filename, FMOD_2D, 0, &karaoke);
		if (result != FMOD_OK)
		{
			fprintf(stderr, "Cannot create audio stream: (%d) - %s\n", result, FMOD_ErrorString(result));
			fmod_system = NULL;
			karaoke = NULL;
			return;
		}
	}

	~AudioPlayer()
	{
		if (karaoke)
			karaoke->release();
		if (fmod_system)
		{
			fmod_system->close();
			fmod_system->release();
		}
	}

	void Play()
	{
		if (fmod_system && karaoke)
			fmod_system->playSound(karaoke, 0, false, &channel);
		else
			fprintf(stderr, "Nothing to play in AudioPlayer::Play\n");
	}

	unsigned int GetPlayPosition()
	{
		unsigned int ret;
		if (channel)
		{
			result = channel->getPosition(&ret, FMOD_TIMEUNIT_MS);
			if (result != FMOD_OK)
			{
				fprintf(stderr, "Error in getting play position: (%d) - %s\n", result, FMOD_ErrorString(result));
				ret = 0;
			}
		}
		return ret;
	}

	void Update()
	{
		if (fmod_system)
			fmod_system->update();
	}
};

class CDGFileIO
{
private:
	enum BufState {
		EMPTY, READY, READING
	};
	static const int max_packets = 300;
	struct Buffer {
		SubCode buf[max_packets];
		int ready_count;
		BufState state;
	};
	std::fstream *cdg_file;
	Buffer *buffers[2];
	int cur_buf;
	int read_ptr;
	pthread_t thread;
	bool thread_dead;
	sem_t ready_buffers;
	sem_t read_next_packet;
	pthread_mutex_t safety;
public:
	bool Done()
	{
		if (thread_dead)
			return true;
		if (cdg_file && (cdg_file->eof() || (cdg_file->fail())))
			return true;
		return false;
	}
	CDGFileIO(const char *filename)
	{
		thread = 0;
		read_ptr = 0;
		thread_dead = false;
		buffers[0] = buffers[1] = NULL;
		cur_buf = 0;

		cdg_file = new std::fstream(filename, std::ios_base::in | std::ios_base::binary);
		if (cdg_file == NULL)
		{
			std::cerr << "Cannot open CDG file\n";
			return;
		}
		if (sem_init(&ready_buffers, 0, 0))
		{
			cdg_file->close();
			cdg_file = NULL;
			std::cerr << "Cannot create a semaphore variable\n";
			return;
		}
		if (sem_init(&read_next_packet, 0, 1))
		{
			sem_destroy(&ready_buffers);
			cdg_file->close();
			cdg_file = NULL;
			std::cerr << "Cannot create a semaphore variable\n";
			return;			
		}

		if (pthread_mutex_init(&safety, NULL))
		{
			sem_destroy(&ready_buffers);
			sem_destroy(&read_next_packet);
			cdg_file->close();
			cdg_file = NULL;
			std::cerr << "Cannot create a mutex variable\n";
			return;						
		}

		buffers[0] = new Buffer();
		buffers[1] = new Buffer();
		if (!buffers[0] || !buffers[1])
		{
			std::cerr << "Failed memory alocation for buffers\n";
			if (buffers[0])
				delete buffers[0];
			if (buffers[1])
				delete buffers[1];
			buffers[0] = buffers[1] = NULL;
			sem_destroy(&ready_buffers);
			sem_destroy(&read_next_packet);
			pthread_mutex_destroy(&safety);
			cdg_file->close();
			cdg_file = NULL;
		}
		int cur_buffer = 0;
		buffers[0]->ready_count = 0;
		buffers[0]->state = EMPTY;
		buffers[1]->ready_count = 0;
		buffers[1]->state = EMPTY;
	}

	~CDGFileIO()
	{
		sem_destroy(&ready_buffers);
		sem_destroy(&read_next_packet);
		pthread_cancel(thread);
		if (cdg_file != NULL)
			cdg_file->close();
		delete buffers[0];
		delete buffers[1];
	}

	bool Start()
	{
		if (pthread_create(&thread, NULL, ReadCDG, (void *)this)) {
			std::cerr << "CDGFileIO::Start - Failed thread\n";
			thread_dead = true;
			return false;
		}
		return true;
	}

	const SubCode *ReadNext()
	{
		if (thread_dead)
			return NULL;
		if (buffers[cur_buf]->state == READING)
		{
			if (read_ptr < buffers[cur_buf]->ready_count)
				return &(buffers[cur_buf]->buf[read_ptr++]);
			if (pthread_mutex_lock(&safety))
				return NULL;
			read_ptr = 0;
			buffers[cur_buf]->state = EMPTY;
			cur_buf = (cur_buf + 1) % 2;
			pthread_mutex_unlock(&safety);
		}
		if (buffers[cur_buf]->state != READY)
		{
			if (thread_dead)
				return NULL;
			//std::cerr << "Waiting for read\n";
			if (sem_wait(&ready_buffers))
				return NULL;
			//std::cerr << "Done reading\n";
			if (buffers[cur_buf]->state != READY)
				return NULL;
		}
		read_ptr = 0;
		buffers[cur_buf]->state = READING;
		sem_post(&read_next_packet);
		if (read_ptr < buffers[cur_buf]->ready_count)
			return &(buffers[cur_buf]->buf[read_ptr++]);
		return NULL;
	}

	static void *ReadCDG(CDGFileIO *obj)
	{
		if ((obj == NULL) || (obj->cdg_file == NULL))
		{
			if (obj)
				obj->thread_dead = true;
			pthread_exit(NULL);
		}
		int cur_buf = 0;
		while (!obj->cdg_file->eof())
		{
			if (pthread_mutex_lock(&(obj->safety)))
			{
				obj->thread_dead = true;
				pthread_exit(NULL);
			}
			if (obj->buffers[cur_buf]->state != EMPTY)
			{
				pthread_mutex_unlock(&(obj->safety));
				if (sem_wait(&(obj->read_next_packet)))
				{
					obj->thread_dead = true;
					pthread_exit(NULL);
				}
				if (obj->buffers[cur_buf]->state != EMPTY)
					continue;
			}
			else
				pthread_mutex_unlock(&(obj->safety));

			obj->cdg_file->read((char *)&(obj->buffers[cur_buf]->buf), CDGFileIO::max_packets * sizeof(SubCode));
			unsigned long size = obj->cdg_file->gcount();
			if (obj->cdg_file->bad())
			{
				obj->buffers[cur_buf]->ready_count = 0;
				sem_post(&(obj->ready_buffers));
				obj->thread_dead = true;
				pthread_exit(NULL);
			}
			if (size % sizeof(SubCode))
				std::cerr << "CDG file has incomplete packet\n";
			obj->buffers[cur_buf]->ready_count = size / sizeof(SubCode);
			obj->buffers[cur_buf]->state = READY;
			cur_buf = (cur_buf + 1) % 2;
			sem_post(&(obj->ready_buffers));
		}
	}

};

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
	CDGFileIO *cdg_file;
	unsigned short colors[16];
	CDGScreenHandler::Screen screen;
	CDGScreenHandler *handler;
	AudioPlayer *ap;
public:
	MyCDGParser(char *filename, CDGScreenHandler *h);
	~MyCDGParser();	
	void Start();
	bool PrintNext();
	void MemoryPreset(const SubCode *s);
	void BorderPreset(const SubCode *s);
	void TileBlockNormal(const SubCode *s);
	void ScrollPreset(const SubCode *s);
	void ScrollCopy(const SubCode *s);
	void DefTransparentColor(const SubCode *s);
	void LoadColorTableLo(const SubCode *s);
	void LoadColorTableHi(const SubCode *s);
	void TileBlockXor(const SubCode *s);
	static void *DoParse(MyCDGParser *obj);
};


CDGParser *CDGParser::GetParser(char *filename, CDGScreenHandler* h)
{
	return new MyCDGParser(filename, h);
}

MyCDGParser::~MyCDGParser()
{
	if (cdg_file)
		delete cdg_file;
/*	if (ap)
		delete ap;
*/}

void MyCDGParser::Start()
{
	pthread_t thread;

	if ((cdg_file == NULL) || (handler == NULL) || (cdg_file->Done()))
		return;

	if (!cdg_file->Start())
		return;

	if (pthread_create(&thread, NULL, DoParse, (void *)this))
		return;

	//void *status;
	//pthread_join(thread, &status);
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

static void * MyCDGParser::DoParse(MyCDGParser *obj)
{ 
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

MyCDGParser::MyCDGParser(char *filename, CDGScreenHandler *h)
{
	if (filename == NULL)
	{
		cdg_file = NULL;
		return;
	}

	char *cdg_name = new char[strlen(filename) + 4];
	char *mp3_name = new char[strlen(filename) + 4];
	handler = h;
	sprintf(cdg_name, "%s.cdg", filename);
	cdg_file = new CDGFileIO(cdg_name);
	if (cdg_file == NULL)
		std::cerr << "Cannot open CDG file";
	sprintf(mp3_name, "%s.mp3", filename);
	ap = new AudioPlayer(mp3_name);
	if (ap == NULL)
		std::cerr << "Cannot play Audio\n";
}

