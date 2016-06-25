/*
** Copyright © 2010 Chris Double <chris.double@double.co.nz>
**
** This program is made available under the ISC license.  See the
** accompanying file LICENSE for details.
*/
#include <iostream>
#include <fstream>
#include <cassert>
#define HAVE_STDINT_H 1
extern "C" {
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <nestegg/nestegg.h>
}
#include <SDL2/SDL.h>

using namespace std;

static unsigned int mem_get_le32(const unsigned char *mem) {
	return (mem[3] << 24)|(mem[2] << 16)|(mem[1] << 8)|(mem[0]);
}

int ifstream_read(void *buffer, size_t size, void *context) {
	ifstream *f = (ifstream*)context;
	f->read((char*)buffer, size);
	// success = 1
	// eof = 0
	// error = -1
	return f->gcount() == size ? 1 : f->eof() ? 0 : -1;
}

int ifstream_seek(int64_t n, int whence, void *context) {
	ifstream *f = (ifstream*)context;
	f->clear();
	ios_base::seekdir dir;
	switch (whence) {
	case NESTEGG_SEEK_SET:
	    dir = fstream::beg;
	    break;
	case NESTEGG_SEEK_CUR:
	    dir = fstream::cur;
	    break;
	case NESTEGG_SEEK_END:
	    dir = fstream::end;
	    break;
	}
	f->seekg(n, dir);
	if (!f->good()) {
		return -1;
	}
	return 0;
}

int64_t ifstream_tell(void *context) {
	ifstream *f = (ifstream*)context;
	return f->tellg();
}

#if 0
void logger(nestegg *ctx, unsigned int severity, char const *fmt, ...) {
	va_list ap;
	char const *sev = NULL;
	switch (severity) {
	case NESTEGG_LOG_DEBUG:
		sev = "debug:   ";
		break;
	case NESTEGG_LOG_WARNING:
		sev = "warning: ";
		break;
	case NESTEGG_LOG_CRITICAL:
		sev = "critical:";
		break;
	default:
		sev = "unknown: ";
	}
	fprintf(stderr, "%p %s ", (void *) ctx, sev);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
#endif

void play_webm(char const *name) {
	int r = 0;
	nestegg *ne;
	ifstream infile(name);
	nestegg_io ne_io;
	ne_io.read = ifstream_read;
	ne_io.seek = ifstream_seek;
	ne_io.tell = ifstream_tell;
	ne_io.userdata = (void*)&infile;
	r = nestegg_init(&ne, ne_io, NULL /* logger */, -1);
	assert(r == 0);
	uint64_t duration = 0;
	r = nestegg_duration(ne, &duration);
	assert(r == 0);
	cout << "Duration: " << duration << endl;
	uint32_t ntracks = 0;
	r = nestegg_track_count(ne, &ntracks);
	assert(r == 0);
	cout << "Tracks: " << ntracks << endl;
	nestegg_video_params vparams;
	vparams.width = 0;
	vparams.height = 0;
	vpx_codec_iface_t *interface;
	for (size_t i = 0; i < ntracks; ++i) {
		int id = nestegg_track_codec_id(ne, i);
		assert(id >= 0);
		int type = nestegg_track_type(ne, i);
		cout << "Track " << i << " codec id: " << id << " type " << type << " ";
		interface = id == NESTEGG_CODEC_VP9 ? &vpx_codec_vp9_dx_algo : &vpx_codec_vp8_dx_algo;
		if (type == NESTEGG_TRACK_VIDEO) {
			r = nestegg_track_video_params(ne, i, &vparams);
			assert(r == 0);
			cout << vparams.width << "x" << vparams.height << " (d: " << vparams.display_width << "x" << vparams.display_height << ")";
		}
		if (type == NESTEGG_TRACK_AUDIO) {
			nestegg_audio_params params;
			r = nestegg_track_audio_params(ne, i, &params);
			assert(r == 0);
			cout << params.rate << " " << params.channels << " channels " << " depth " << params.depth;
		}
		cout << endl;
	}
	vpx_codec_ctx_t codec;
	int flags = 0;
	vpx_codec_err_t res;
	cout << "Using " << vpx_codec_iface_name(interface) << endl;
	/* Initialize codec */
	if (vpx_codec_dec_init(&codec, interface, NULL, flags)) {
		cerr << "Failed to initialize decoder" << endl;
		return;
	}

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *texture = NULL;
	int video_count = 0;
	int audio_count = 0;
	nestegg_packet *packet = 0;
	// 1 = keep calling
	// 0 = eof
	// -1 = error
	while (1) {
		r = nestegg_read_packet(ne, &packet);
		if (r == 1 && packet == 0) {
			continue;
		}
		if (r <= 0){
			break;
		}
		unsigned int track = 0;
		r = nestegg_packet_track(packet, &track);
		assert(r == 0);
		// TODO: workaround bug
		if (nestegg_track_type(ne, track) == NESTEGG_TRACK_VIDEO) {
		    cout << "video frame: " << ++video_count << " ";
		    uint32_t count = 0;
		    r = nestegg_packet_count(packet, &count);
		    assert(r == 0);
		    cout << "Count: " << count << " ";
		    int nframes = 0;
			for (size_t j=0; j < count; ++j) {
				uint8_t *data;
				size_t length;
				r = nestegg_packet_data(packet, j, &data, &length);
				assert(r == 0);
				vpx_codec_stream_info_t si;
				memset(&si, 0, sizeof(si));
				si.sz = sizeof(si);
				vpx_codec_peek_stream_info(interface, data, length, &si);
				cout << "keyframe: " << (si.is_kf ? "yes" : "no") << " ";
				cout << "length: " << length << " ";
				/* Decode the frame */
				vpx_codec_err_t e = vpx_codec_decode(&codec, data, length, NULL, 0);
				if (e) {
					cerr << "Failed to decode frame. error: " << e << endl;
					return;
				}
				vpx_codec_iter_t iter = NULL;
				vpx_image_t     *img;

				/* Write decoded data to disk */
				while ((img = vpx_codec_get_frame(&codec, &iter))) {
					//uint32_t plane;
					cout << "h: " << img->d_h << " w: " << img->d_w << endl;
					if (!window) {
						// FIXME: asserts are *not* supposed to be used like this
						int r = SDL_Init(SDL_INIT_VIDEO);
						assert(r == 0);

						r = SDL_CreateWindowAndRenderer(
							vparams.display_width, vparams.display_height, 0,
							&window, &renderer
						);
						assert(r == 0 && window && renderer);

						texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
							vparams.width, vparams.height
						);
						assert(texture);
					}
					nframes++;

					SDL_Rect rect;
					rect.x = 0;
					rect.y = 0;
					rect.w = vparams.display_width;
					rect.h = vparams.display_height;

					void *pixels;
					int pitch;

					SDL_LockTexture(texture, &rect, &pixels, &pitch);
					/*
					for (size_t y=0; y < img->d_h; ++y) {
						memcpy(pixels[0]+(pitch * y), img->planes[0]+(img->stride[0]*y), pitch);
					}
					for (size_t y=0; y < img->d_h >> 1; ++y) {
						memcpy(pixels[1]+(pitch * y), img->planes[2]+(img->stride[2]*y), pitch);
					}
					for (size_t y=0; y < img->d_h >> 1; ++y) {
						memcpy(pixels[2]+(pitch * y), img->planes[1]+(img->stride[1]*y), pitch);
					}
					*/
					SDL_UpdateYUVTexture(
						texture, &rect,
						img->planes[0]+(img->stride[0]), pitch,
						img->planes[2]+(img->stride[2]), pitch,
						img->planes[1]+(img->stride[1]), pitch
					);
					SDL_UnlockTexture(texture);
				}
				cout << "nframes: " << nframes;
			}
			cout << endl;
		}
		if (nestegg_track_type(ne, track) == NESTEGG_TRACK_AUDIO) {
	    	cout << "audio frame: " << ++audio_count << endl;
		}
		SDL_Event event;
		if (SDL_PollEvent(&event) == 1) {
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_ESCAPE:
					break;
				case SDLK_SPACE:
					SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
				}
			}
		}
	}
	if (vpx_codec_destroy(&codec)) {
		cerr << "Failed to destroy codec" << endl;
		return;
	}
	nestegg_destroy(ne);
	infile.close();
	if (texture) {
		SDL_DestroyTexture(texture);
		texture = NULL;
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
	SDL_Quit();
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		cerr << "Usage: webm filename" << endl;
		return 1;
	}
	play_webm(argv[1]);
	return 0;
}
