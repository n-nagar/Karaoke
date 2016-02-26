#include "Karaoke.h"
#include <pthread.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <time.h>



class CDGFileIO : public CDGReader
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

	static void *ReadCDG(void *ptr)
	{
		CDGFileIO *obj = static_cast<CDGFileIO *>(ptr);
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

CDGReader *CDGReader::GetReader(const char *filename)
{
	return new CDGFileIO(filename);
}