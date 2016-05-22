#include <stdlib.h>
#include <stdio.h>

#include "image.h"
#include "leepra.h"
#include "blend.h"
#include "config.h"
#include "bass.h"

void error(char *reason) {
	MessageBox(NULL, reason, NULL, MB_OK);
	exit(1);
}

void noise(unsigned int *bitmap, unsigned int random) {
	int i;
	for(i=((WIDTH*HEIGHT)>>3);i;i--){
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
		*bitmap++ = (*bitmap + random)^random;
	}
}

void main() {
	if (!leepra_open("Yo", WIDTH, HEIGHT, 1)) error("fuckin' looser");
	if (!BASS_Init(1, 44100, BASS_DEVICE_LATENCY, GetForegroundWindow(), NULL)) error( "could not init bass" );
	HSTREAM music_file = BASS_StreamCreateFile(0, "data/yo.mp3", 0, 0, 0);

	static unsigned int screen[ WIDTH*HEIGHT ];
	static unsigned int screen2[ WIDTH*HEIGHT ];
	static unsigned int *pics[29];
	static unsigned int *logo;

	if (!image_load( "yo.gif", &logo, 0, 0)) error("fuck off!");

	for (int i=0; i<29; i++) {
		char yo[256];
		sprintf(yo,"%02i.jpg", i);
		if (!image_load( yo, &pics[i], 0, 0)) error("fuck off!");
	}

	bool done = false;
	BASS_Start();
	BASS_ChannelPlay(music_file, false);
	while (!done) {
		QWORD pos = BASS_ChannelGetPosition(music_file, BASS_POS_BYTE);
		float time = (float)BASS_ChannelBytes2Seconds(music_file, pos);

		if (time < 15) {
			memcpy( screen, logo, WIDTH*HEIGHT*4 );
		} else if (time < 55) {
			int bilde = int(time) % 29;
			blend( screen, pics[bilde], WIDTH*HEIGHT, 10 );
			memcpy( screen2, screen, WIDTH*HEIGHT*4 );
			noise( screen2, rand() );
			blend( screen, screen2, WIDTH*HEIGHT, 20 );
		} else if (time < 68) {
			int bilde = int(time) % 29;
			blend( screen, pics[bilde], WIDTH*HEIGHT, 10 );
			memcpy( screen2, screen, WIDTH*HEIGHT*4 );
			noise( screen2, rand() );
			noise( screen, rand() );
			blend( screen, screen2, WIDTH*HEIGHT, 20 );
		} else {
			memcpy( screen, pics[rand()%29], WIDTH*HEIGHT*4 );
		}

		leepra_update( (void*)screen );
		done = leepra_handle_events();
	}

	leepra_close();
}

