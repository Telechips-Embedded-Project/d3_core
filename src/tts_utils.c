#include "../include/tts_utils.h"
#include "../include/main.h"

#define WAV_HEADER_SIZE 44
#define BUFFER_SIZE 4096

// JSON 파싱 안하고 바로 문자열로 받아서 스피커로 출력하는 코드로 수정[전지현]
void run_piper(const char* text) {
    char cmd[2048];
    const char* temp_input_file = "/tmp/tts_input.txt";

    // JSON 파싱
    //const char* cleaned_json = extract_json(text);
    //if (!cleaned_json) { printf("[ERROR] Invalid JSON format\n"); return; }
    //cJSON* root = cJSON_Parse(cleaned_json);
    //if (!root) { printf("[ERROR] JSON parsing failed\n"); return; }
    //cJSON* comm = cJSON_GetObjectItemCaseSensitive(root, "Comment");
    //const char* com_text = cJSON_IsString(comm) ? comm->valuestring : "";
    printf("[TTS text to] : %s\n", text);

    FILE* fp = fopen(temp_input_file, "w");
    if (fp == NULL) {
        perror("Failed to open temp input file");
        //cJSON_Delete(root); // 에러 발생 시에도 메모리 해제
        return;
    }
    fprintf(fp, "%s", text);
    fflush(fp);
    int fd = fileno(fp);
    fsync(fd);
    fclose(fp);

    // // --- 메모리 해제합니다. ---
    // cJSON_Delete(root);
    // -----------------------------------------------------------------

    snprintf(cmd, sizeof(cmd), "/home/root/TTS/run_tts.sh %s", temp_input_file);

    printf("Executing command: %s\n", cmd);
    system(cmd);
    printf("Shell script finished.\n");

    remove(temp_input_file);
}



/*
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
*/


/*
// JSON 추출 함수
char* extract_json(const char* input) {
    static char buf[1024];
        memset(buf, 0, sizeof(buf));
        char temp[1024];
        int j = 0;
        for (int i = 0; input[i] && j < sizeof(temp) - 1; ++i) {
                if (input[i] == '`' || input[i] == '\n' || input[i] == '\r' || input[i] == '\t') continue;
                temp[j++] = input[i];
        }
        temp[j] = '\0';
        const char* start = strchr(temp, '{');
        const char* end = strrchr(temp, '}');
        if (!start || !end || end <= start) return NULL;
        size_t len = end - start + 1;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        strncpy(buf, start, len);
        buf[len] = '\0';
        return buf;
}
*/



/*
int main(void) {
    const char* json_input1 = "```json\n"
        "{\n"
        "  \"Command\": \"Speak\",\n"
        "  \"Comment\": \"Success, This is the final working version.\"\n"
        "}\n"
        "```";

    printf("Starting TTS test program with direct ALSA playback.\n\n");
    run_piper(json_input1);
    printf("\nTTS test finished.\n");
    return 0;
}
*/
