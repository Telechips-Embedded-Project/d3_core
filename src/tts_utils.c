#include "../include/tts_utils.h"
#include "../include/main.h"

// --- tts libasound 기반 (볼륨 동적 조절O) ---
void run_piper_with_volume(const char *text, float gain) {
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		"/home/pi03/.venv/bin/piper --model ../../piper/en_US-ryan-medium.onnx --output_file - --text \"%s\"",
		text);

	FILE *pipe = popen(cmd, "r");
	if (!pipe) {
		perror("popen failed");
		return;
	}

	// WAV header 읽기
	unsigned char header[WAV_HEADER_SIZE];
	fread(header, 1, WAV_HEADER_SIZE, pipe);

	// WAV 헤더에서 샘플링 정보 추출
	int sample_rate = *(int *)(header + 24);   // 24-27: sample rate
	short channels = *(short *)(header + 22); // 22-23: channels
	short bits_per_sample = *(short *)(header + 34); // 34-35: bits per sample

	if (bits_per_sample != 16) {
		fprintf(stderr, "Only 16bit PCM supported\n");
		pclose(pipe);
		return;
	}

	// ALSA 초기화
	snd_pcm_t *pcm;
	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

	snd_pcm_set_params(pcm,
		SND_PCM_FORMAT_S16_LE,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels,
		sample_rate,
		1,              // Soft resampling off
		100000);        // Latency 0.5sec

	// 오디오 스트림 읽어서 ALSA로 출력
	int16_t buffer[BUFFER_SIZE];
	size_t read;

	while ((read = fread(buffer, sizeof(short), BUFFER_SIZE, pipe)) > 0) {
		for (size_t i = 0; i < read; ++i) {
			int32_t sample = buffer[i];
			sample = (int32_t)(sample * gain);

			// 클리핑 방지
			if (sample > 32767) sample = 32767;
			if (sample < -32768) sample = -32768;

			buffer[i] = (int16_t)sample;
		}


		snd_pcm_writei(pcm, buffer, read / channels);
	}

	snd_pcm_drain(pcm);
	snd_pcm_close(pcm);
	pclose(pipe);

}

// --- tts libasound 기반 (볼륨 조절X) ---
void run_piper(const char *text) {
	char cmd[512];
	char json_clean[1024];
	char comment[512];
	char result[512];
	// comment만 추출.
	const char* cleaned_json = extract_json(text);
	if (!cleaned_json) {
		printf("[ERROR] Invalid JSON format\n");
		return;
	}

	cJSON* root = cJSON_Parse(cleaned_json);
	if (!root) {
		printf("[ERROR] JSON parsing failed\n");
		return;
	}

	cJSON* comm = cJSON_GetObjectItemCaseSensitive(root, "Comment");
	const char* com = cJSON_IsString(comm) ? comm->valuestring : "";

	printf("[JSON RESULT COM] : %s\n", com);


	snprintf(cmd, sizeof(cmd),
		"/home/pi03/.venv/bin/piper --model ../piper/en_US-ryan-medium.onnx --output_file - --text \"%s\"", com);
	//printf("run_piper CMD : %s\n", cmd);


	FILE *pipe = popen(cmd, "r");
	if (!pipe) {
		perror("popen failed");
		return;
	}

	// WAV header 읽기
	unsigned char header[WAV_HEADER_SIZE];
	fread(header, 1, WAV_HEADER_SIZE, pipe);
	for (int i = 0; i < WAV_HEADER_SIZE; i++) {
		printf("%02x ", header[i]);
	}
	printf("\n");

	// WAV 헤더에서 샘플링 정보 추출
	int sample_rate = *(int *)(header + 24);   // 24-27: sample rate
	short channels = *(short *)(header + 22); // 22-23: channels
	short bits_per_sample = *(short *)(header + 34); // 34-35: bits per sample

	if (bits_per_sample != 16) {
		fprintf(stderr, "Only 16bit PCM supported\n");
		pclose(pipe);
		return;
	}

	// ALSA 초기화
	snd_pcm_t *pcm;
	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

	snd_pcm_set_params(pcm,
		SND_PCM_FORMAT_S16_LE,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels,
		sample_rate,
		1,              // Soft resampling off
		100000);        // Latency 0.5sec

	// 오디오 스트림 읽어서 ALSA로 출력
	int16_t buffer[BUFFER_SIZE];
	size_t read;

	while ((read = fread(buffer, sizeof(short), BUFFER_SIZE, pipe)) > 0) {
		snd_pcm_writei(pcm, buffer, read / channels);
	}

	snd_pcm_drain(pcm);
	snd_pcm_close(pcm);
	pclose(pipe);

}
