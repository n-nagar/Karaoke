#include "Karaoke.h"
#include <cstring>
#include <time.h>
#include <cstdio>
#include "fmod.hpp"
#include "fmod_errors.h"

class FMODAudioPlayer : public KaraokeAudio
{
private:
	FMOD::System 	*fmod_system;
	FMOD::Sound		*karaoke;
	FMOD::Channel	*channel;
	FMOD_RESULT		result;
	unsigned int	version;
	void			*extradriverdata ;
public:
	FMODAudioPlayer(const char *filename)
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

	~FMODAudioPlayer()
	{
		if (karaoke)
			karaoke->release();
		if (fmod_system)
		{
			fmod_system->close();
			fmod_system->release();
		}
	}

	bool Play()
	{
		bool ret;
		if (fmod_system && karaoke)
		{
			ret = true;
			fmod_system->playSound(karaoke, 0, false, &channel);
		}
		else
		{
			ret = false;
			fprintf(stderr, "Nothing to play in FMODAudioPlayer::Play\n");
		}
		return ret;
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


KaraokeAudio *KaraokeAudio::GetPlayer(const char *filename)
{
	return new FMODAudioPlayer(filename);
}