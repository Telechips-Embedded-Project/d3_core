#include "../include/json_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <errno.h>

/* ===== 파일 전역 헬퍼들 ===== */
static inline int str_has(const char *s, const char *sub) {
    return (s && sub && strstr(s, sub) != NULL);
}

static inline int str_eq(const char *s, const char *t) {
    return (s && t && strcmp(s, t) == 0);
}

static inline int get_value_int(cJSON *value, int *out) {
    if (!out) return 0;
    if (!value) return 0;

    if (cJSON_IsNumber(value)) {
        /* valuedouble이지만 정수로 캐스팅 */
        *out = (int)(value->valuedouble);
        return 1;
    }
    if (cJSON_IsString(value) && value->valuestring) {
        char *endp = NULL;
        errno = 0;
        long v = strtol(value->valuestring, &endp, 10);
        if (errno == 0 && endp && *endp == '\0') {
            *out = (int)v;
            return 1;
        }
    }
    return 0;
}

int parse_command_json(const char *json_str, command_t *cmd)
{
	cJSON *root = cJSON_Parse(json_str);
	if (!root)
	{
		fprintf(stderr, "JSON parse error: %s\n", cJSON_GetErrorPtr());
		return -1;
	}
	memset(cmd, 0, sizeof(*cmd));  
	cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "device");
	cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");
	cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");

	if (cJSON_IsString(device) && device->valuestring != NULL)
	{
		if (strcmp(device->valuestring, "aircon") == 0)
		{
			cmd->device = DEVICE_AIRCON;
		}
		else if (strcmp(device->valuestring, "window") == 0)
		{
			cmd->device = DEVICE_WINDOW;
		}
		else if (strcmp(device->valuestring, "ambient") == 0)
		{
			cmd->device = DEVICE_AMBIENT;
		}
		else if (strcmp(device->valuestring, "headlamp") == 0)
		{
			cmd->device = DEVICE_HEADLAMP;
		}
		else if (strcmp(device->valuestring, "wiper") == 0)
		{
			cmd->device = DEVICE_WIPER;
		}
		/*
		else if (strcmp(device->valuestring, "music") == 0)
		{
			cmd->device = DEVICE_MUSIC;
		}
		*/
		else
		{
			cmd->device = DEVICE_UNKNOWN;
		}

	}

	if (cJSON_IsString(command) && command->valuestring != NULL)
	{

		// ambient 전용
		if (cmd->device == DEVICE_AMBIENT)
		{
			if (strcmp(command->valuestring, "color") == 0)
				cmd->command = 0;
			else if (strcmp(command->valuestring, "brightness") == 0)
				cmd->command = 1;
			else
				cmd->command = -1;
		}
		// ambient 아닐 경우
		else
		{
			// aircon ctrl
			if (strcmp(command->valuestring, "off") == 0) cmd->value = 0;
			else if (strcmp(command->valuestring, "low") == 0) cmd->value = 1;
			else if (strcmp(command->valuestring, "mid") == 0) cmd->value = 2;
			else if (strcmp(command->valuestring, "high") == 0) cmd->value = 3;
			// window ctrl
			else if (strcmp(command->valuestring, "stop") == 0) cmd->value = 0;
			else if (strcmp(command->valuestring, "open") == 0) cmd->value = 1;
			else if (strcmp(command->valuestring, "close") == 0) cmd->value = 2;
			// headlamp ctrl
			else if (strcmp(command->valuestring, "on") == 0) cmd->value = 1;
			else cmd->value = -1;
		}
		}
	// --- ambient 전용 value 해석 (value는 "문자열"만 사용) ---
	if (cmd->device == DEVICE_AMBIENT) {
		if (!cJSON_IsString(value) || value->valuestring == NULL) {
			cJSON_Delete(root);
			return -1; // ambient는 문자열 값 필수
		}

		// command: "color" 또는 "brightness" → 이전 단계에서
		// cmd->command = (0=color, 1=brightness) 로 이미 설정되어 있다고 가정
		// 여기서는 value 문자열만 검증/저장

		char vbuf[16] = {0};
		// 소문자화(간단 버전)
		for (size_t i = 0; i < sizeof(vbuf)-1 && value->valuestring[i]; ++i) {
			char ch = value->valuestring[i];
			if ('A' <= ch && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
			vbuf[i] = ch;
		}

		if (cmd->command == 0) {
			// color 경로: off 포함
			if (strcmp(vbuf, "red") == 0 ||
				strcmp(vbuf, "yellow") == 0 ||
				strcmp(vbuf, "green") == 0 ||
				strcmp(vbuf, "blue") == 0 ||
				strcmp(vbuf, "cyan") == 0 ||
				strcmp(vbuf, "magenta") == 0 ||
				strcmp(vbuf, "white") == 0 ||
				strcmp(vbuf, "rainbow") == 0 ||
				strcmp(vbuf, "off") == 0) {
				snprintf(cmd->svalue, sizeof(cmd->svalue), "%s", vbuf);
			} else {
				cJSON_Delete(root);
				return -1; // 미허용 색상
			}
		} else if (cmd->command == 1) {
			// brightness 경로: low/mid/high만 허용 (off 금지: 끄기는 color:"off")
			if (strcmp(vbuf, "low") == 0 ||
				strcmp(vbuf, "mid") == 0 ||
				strcmp(vbuf, "high") == 0) {
				snprintf(cmd->svalue, sizeof(cmd->svalue), "%s", vbuf);
			} else {
				cJSON_Delete(root);
				return -1; // 미허용 밝기
			}
		} else {
			cJSON_Delete(root);
			return -1; // 정의되지 않은 command
		}

		cJSON_Delete(root);
		return 0;
	}
	return 0;
}
// --- extract_json 함수 ---
// BitNet에서 들어온 문자열 json { } 내부 추출 

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
    const char* end   = strrchr(temp, '}');

    if (!start || !end || end <= start) return NULL;

    size_t len = end - start + 1;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    strncpy(buf, start, len);
    buf[len] = '\0';

    return buf;
}
// --- handle_device_control 함수 ---  
// dispatcher / 명령LLM / 대화LLM에서 오는 내용 해석해서 수행
// 없는 명령일 경우 tts 출력하는 로직 필요.
// 상태 물어보는 질문일 경우 shared 메모리 참고해서 출력하는 로직 필요.
void handle_device_control(const char* raw_json) {
    const char* cleaned_json = extract_json(raw_json);
    if (!cleaned_json) {
        printf("[ERROR] Invalid JSON format\n");
        return;
    }

    cJSON* root = cJSON_Parse(cleaned_json);
    if (!root) {
        printf("[ERROR] JSON parsing failed\n");
        return;
    }

    cJSON* device  = cJSON_GetObjectItemCaseSensitive(root, "device");
    cJSON* command = cJSON_GetObjectItemCaseSensitive(root, "command");
    cJSON* value   = cJSON_GetObjectItemCaseSensitive(root, "value");

    const char* dev = cJSON_IsString(device)  ? device->valuestring  : "";
    const char* cmd = cJSON_IsString(command) ? command->valuestring : "";

    // ==== QUESTION (조회 전용) ====
    if (str_eq(dev, "question")) {
        const char* what  = cmd;
        const char* scope = (cJSON_IsString(value) ? value->valuestring : NULL);
        printf("[QUESTION] command=%s%s%s\n",
               what, scope ? ", value=" : "", scope ? scope : "");
        cJSON_Delete(root);
        return;
    }

    // ==== AIRCON ====
    if (str_has(dev, "air")) {
        if (str_eq(cmd, "set")) {
            int target = 0;
            if (get_value_int(value, &target)) {
                printf("[AIRCON] set target = %d\n", target);
                // TODO: 목표온도 API가 있으면 사용
                aircon_control(1, shm_ptr);
            } else {
                printf("[AIRCON] set without numeric value -> ignored\n");
            }
        } else if (str_eq(cmd, "on")) {
            printf("[AIRCON] on\n");
            aircon_control(1, shm_ptr);
        } else if (str_eq(cmd, "off")) {
            printf("[AIRCON] off\n");
            aircon_control(0, shm_ptr);
        } else {
            printf("[AIRCON] unknown command: %s\n", cmd);
        }
    }

    // ==== AMBIENT ====
   else if (str_eq(dev, "ambient")) {
		if (str_eq(cmd, "on")) {
			const char* color = "green";
			if (cJSON_IsString(value) && value->valuestring && *value->valuestring) {
				const char* v = value->valuestring;
				if      (str_has(v, "red"))      color = "red";
				else if (str_has(v, "yellow"))   color = "yellow";
				else if (str_has(v, "green"))    color = "green";
				else if (str_has(v, "blue"))     color = "blue";
				else if (str_has(v, "cyan"))     color = "cyan";
				else if (str_has(v, "magenta"))  color = "magenta";
				else if (str_has(v, "white"))    color = "white";
				else if (str_has(v, "rainbow"))  color = "rainbow";
			}
			ambient_control(0, color, shm_ptr);
		}
		else if (str_eq(cmd, "brightness")) {
			const char* b = "mid";
			if (cJSON_IsString(value) && value->valuestring && *value->valuestring) {
				const char* v = value->valuestring;
				if      (str_has(v, "low"))  b = "low";
				else if (str_has(v, "mid"))  b = "mid";
				else if (str_has(v, "high")) b = "high";
			}
			ambient_control(1, b, shm_ptr);
		}
		else if (str_eq(cmd, "off")) {
			ambient_control(0, "off", shm_ptr);
		}
		else {
			printf("[AMBIENT] unknown command: %s\n", cmd);
		}
	}


    // ==== WINDOW ====
    else if (str_eq(dev, "window")) {
        if (str_eq(cmd, "open")) {
            window_control(1, shm_ptr);
        } else if (str_eq(cmd, "close")) {
            window_control(2, shm_ptr);
        } else if (str_eq(cmd, "stop")) {
            window_control(0, shm_ptr);
        } else if (str_eq(cmd, "set")) {
            int pos = -1;
            if (get_value_int(value, &pos)) {
                window_control(pos, shm_ptr);
            } else {
                printf("[WINDOW] set without numeric value -> ignored\n");
            }
        } else {
            printf("[WINDOW] unknown command: %s\n", cmd);
        }
    }

    // ==== WIPER ====
    else if (str_eq(dev, "wiper")) {
        int mode = -1;
        if      (str_eq(cmd, "off"))  mode = 0;
        else if (str_eq(cmd, "on"))   mode = 1;
        else if (str_eq(cmd, "fast")) mode = 1;
        else if (str_eq(cmd, "slow")) mode = 2;
        else if (str_eq(cmd, "set")) {
            if (!get_value_int(value, &mode)) {
                printf("[WIPER] set without numeric value -> ignored\n");
            }
        }

        if (mode != -1) {
            wiper_control(mode, shm_ptr);
        } else {
            printf("[WIPER] unknown command: %s\n", cmd);
        }
    }

    // ==== HEADLAMP ====
    else if (str_eq(dev, "headlamp")) {
        if (str_eq(cmd, "on")) {
            headlamp_control(1, shm_ptr);
        } else if (str_eq(cmd, "off")) {
            headlamp_control(0, shm_ptr);
        } else if (str_eq(cmd, "set")) {
            int lv = -1;
            if (get_value_int(value, &lv)) {
                headlamp_control(lv, shm_ptr);
            } else {
                printf("[HEADLAMP] set without numeric value -> ignored\n");
            }
        } else {
            printf("[HEADLAMP] unknown command: %s\n", cmd);
        }
    }

    // ==== (옵션) MUSIC ====
    else if (str_eq(dev, "music")) {
        printf("[MUSIC] command=%s (no-op here)\n", cmd);
    }

    else {
        printf("[WARN] Unknown device: %s\n", dev);
    }

    cJSON_Delete(root);
}


