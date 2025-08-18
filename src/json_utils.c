#include "../include/json_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

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
static bool icontains(const char *s, const char *sub) {
    if (!s || !sub) return false;
#ifdef _GNU_SOURCE
    return strcasestr(s, sub) != NULL;     // glibc/gnu 확장
#else
    // 소형 대체 구현 (대소문자 무시 부분 일치)
    for (const char *p = s; *p; ++p) {
        const char *a = p, *b = sub;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
#endif
}

static bool extract_first_int(const char *s, int *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && !isdigit((unsigned char)*p) && *p!='-' ) p++;
    if (!*p) return false;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (p == end) return false;
    if (out) *out = (int)v;
    return true;
}
// 대소문자 무시 문자열 동등 비교
static bool istr_eq(const char* a, const char* b) {
    return a && b && strcasecmp(a, b) == 0;
}

// "off/low/mid/medium/high/max/min/0~3" → 0~3 매핑
static bool parse_ac_level(const char* s, int* out) {
    if (!s || !*s) return false;
    if (istr_eq(s, "off")) { *out = 0; return true; }
    if (istr_eq(s, "low") || istr_eq(s, "min")) { *out = 1; return true; }
    if (istr_eq(s, "mid") || istr_eq(s, "medium")) { *out = 2; return true; }
    if (istr_eq(s, "high") || istr_eq(s, "max")) { *out = 3; return true; }
    // 숫자 문자열도 허용: "0","1","2","3"
    if (strlen(s) == 1 && s[0] >= '0' && s[0] <= '3') { *out = s[0]-'0'; return true; }
    return false;
}

// cJSON value -> 문자열로 추출 (문자/숫자 모두 허용)
static bool get_value_str_from_json(const cJSON *jv, char *out, size_t n) {
    if (!jv) return false;
    if (cJSON_IsString(jv) && jv->valuestring) {
        snprintf(out, n, "%s", jv->valuestring);
        return true;
    }
    if (cJSON_IsNumber(jv)) {
        // 정수/실수 모두 "숫자" 문자열로 변환 (레벨 매핑용이면 정수 변환 충분)
        snprintf(out, n, "%d", (int)jv->valuedouble);
        return true;
    }
    return false;
}


// ==== fifo 명령 파싱 ==== // 
int parse_gui_command_json(const char *json_str, command_t *cmd)
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

        if (strcasecmp(device->valuestring, "navi") == 0) {
            if (cJSON_IsString(value) && value->valuestring && value->valuestring[0] != '\0'){
                run_piper(value->valuestring);         
            }
            cJSON_Delete(root);
            return 0;
        }

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

static int decide_ac_level_from_target(int current_temp, int target_temp) {
    int delta = current_temp - target_temp;
    if (delta < 0) delta = -delta;

    if (delta <= 1) return 1; // low
    if (delta <= 3) return 2; // mid
    return 3;                               // high
}

// === 음성 명령 파싱 ====  
// dispatcher / 명령LLM / 대화LLM에서 오는 내용 해석해서 수행
void dispatch_voice_command_json(const char* raw_json) {
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
		
		    const char *co2_status;

		// 음성 안내
		switch (shm_ptr->sensor.CO2_flag) {
			case 0:  co2_status = "normal";   break;
			case 1:  co2_status = "warning";  break;
			case 2:  co2_status = "critical"; break;
			default: co2_status = "unknown";  break;
		}

		char tts_msg[128];
		snprintf(tts_msg, sizeof(tts_msg),
				"Current cabin temperature is %d degrees, humidity is %d percent, and CO2 level is %s.",
				shm_ptr->sensor.temperature, shm_ptr->sensor.humidity, co2_status);
		run_piper(tts_msg);
		
        cJSON_Delete(root);
        return;
    }
	
    // ==== AIRCON ====
    if (str_has(dev, "air")) {
        // 1) 끄기: "off" 또는 "turn off"가 들어가면 무조건 off
        if (icontains(cmd, "turn off") || icontains(cmd, "off")) {
            printf("[AIRCON] off \n"); fflush(stdout);
            aircon_control(0, shm_ptr, 1);
        }

        // 2) 그 외 on/set/target/temp/degree/turn on 등은 value 우선 해석
        else if (icontains(cmd, "on") || icontains(cmd, "turn on") ||
                icontains(cmd, "enable") || icontains(cmd, "start") ||
                icontains(cmd, "power on") ||
                icontains(cmd, "set") || icontains(cmd, "target") ||
                icontains(cmd, "temp") || icontains(cmd, "degree") ||  icontains(cmd, "level") ||
                extract_first_int(cmd, NULL)) {

            int level = -1;
            int num = 0;
            char vstr[32] = {0};

            // value 문자열/숫자 → 우선 해석
            bool has_vstr   = get_value_str_from_json(value, vstr, sizeof(vstr));
            bool has_level  = has_vstr && parse_ac_level(vstr, &level);   // "low/mid/high/min/max/0~3"
            bool has_num    = get_value_int(value, &num) || extract_first_int(cmd, &num); // 숫자 우선

            if (has_level) {
                // 예: {"command":"on","value":"high"} / {"command":"set","value":"mid"}
                printf("[AIRCON] on/set -> level %d (%s)\n", level, vstr);
                aircon_control(level, shm_ptr, 1);
            }
           else if (has_num) {
                if (num >= 0 && num <= 3) {
                    // 0~3이면 "레벨"로 간주
                    printf("[AIRCON] on/set -> level %d\n", num);
                    aircon_control(num, shm_ptr, 1);
                } else {
                    // 그 외 숫자는 "목표온도"로 간주 → 현재온도와 비교해 레벨 결정
                    int cur = shm_ptr ? shm_ptr->sensor.temperature : 25; // fallback 25℃
                    int lev = decide_ac_level_from_target(cur, num);
                    int delta = cur - num; if (delta < 0) delta = -delta;

                    printf("[AIRCON] set target_temp = %d (current=%d, Δ=%d) -> level %d\n",
                        num, cur, delta, lev);

                    // 필요 시 목표온도 상태 저장:
                    // shm_ptr->aircon.target_temp = num;

                    aircon_control(lev, shm_ptr, 1);
                }
            }
            else {
                // value가 없고 숫자/레벨도 없으면: 이전 레벨로 켬(없으면 기본 1)
                int prev = shm_ptr ? shm_ptr->aircon.level : 1;
                if (prev < 1 || prev > 3) prev = 1;
                printf("[AIRCON] on (no value) -> restore level %d\n", prev);
                aircon_control(prev, shm_ptr, 1);
            }
        }

        else {
            printf("[AIRCON] unknown command: %s\n", cmd);
        }
        shm_ptr->user.aircon_autoflag = 0; // 자동 제어 해제 (사용자 수동 개입)
    }




    // ==== AMBIENT ====
   else if (str_eq(dev, "ambient")) {
		if (str_eq(cmd, "on") || str_eq(cmd, "turn on") || str_eq(cmd, "set")) {
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
			ambient_control(0, color, shm_ptr, 1);
		}
		else if (str_eq(cmd, "brightness")) {
			const char* b = "mid";
			if (cJSON_IsString(value) && value->valuestring && *value->valuestring) {
				const char* v = value->valuestring;
				if      (str_has(v, "low"))  b = "low";
				else if (str_has(v, "mid"))  b = "mid";
				else if (str_has(v, "high")) b = "high";
			}
			ambient_control(1, b, shm_ptr, 1);
		}
		else if (str_eq(cmd, "off") || str_eq(cmd, "turn off")) {
			ambient_control(0, "off", shm_ptr, 1);
		}
		else {
			printf("[AMBIENT] unknown command: %s\n", cmd);
		}
        shm_ptr->user.ambient_autoflag = 0;
	}


    // ==== WINDOW ====
    else if (str_eq(dev, "window")) {
        if (str_eq(cmd, "open")) {
            window_control(1, shm_ptr, 1);
        } else if (str_eq(cmd, "close")) {
            window_control(2, shm_ptr, 1);
        } else if (str_eq(cmd, "stop")) {
            window_control(0, shm_ptr, 1);
        } else if (str_eq(cmd, "set")) {
            int pos = -1;
            if (get_value_int(value, &pos)) {
                window_control(pos, shm_ptr, 1);
            } else {
                printf("[WINDOW] set without numeric value -> ignored\n");
            }
        } else {
            printf("[WINDOW] unknown command: %s\n", cmd);
        }
        shm_ptr->user.window_autoflag = 0;
    }

    // ==== WIPER ====
    else if (str_eq(dev, "wiper")) {
        int mode = -1;
        if      (str_eq(cmd, "off"))  mode = 0;
        else if (str_eq(cmd, "on"))   mode = 1;
        else if (str_eq(cmd, "slow")) mode = 1;
        else if (str_eq(cmd, "fast")) mode = 2;
        else if (str_eq(cmd, "set")) mode = 1;

        if (mode != -1) {
            wiper_control(mode, shm_ptr, 1);
        } else {
            printf("[WIPER] unknown command: %s\n", cmd);
        }
        shm_ptr->user.wiper_autoflag = 0;
    }

    // ==== HEADLAMP ====
    else if (str_eq(dev, "headlamp")) {
        if (str_eq(cmd, "on")) {
            headlamp_control(1, shm_ptr, 1);
        } else if (str_eq(cmd, "off")) {
            headlamp_control(0, shm_ptr, 1);
        } else if (str_eq(cmd, "set")) {
            int lv = -1;
            if (get_value_int(value, &lv)) {
                headlamp_control(lv, shm_ptr, 1);
            } else {
                printf("[HEADLAMP] set without numeric value -> ignored\n");
            }
        } else {
            printf("[HEADLAMP] unknown command: %s\n", cmd);
        }
    }

    // ==== (옵션) MUSIC ====
    else if (str_eq(dev, "music")) {
        if (icontains(cmd, "play") || icontains(cmd, "turn on") || icontains(cmd, "on")) {
            printf("[VOICE][MUSIC] forward: play\n");
            shm_ptr->media.cmd_in = 1;
        } else if (icontains(cmd, "stop") || icontains(cmd, "turn off") || icontains(cmd, "off")) {
            printf("[VOICE][MUSIC] forward: stop\n");
            shm_ptr->media.cmd_in = 2;
        } else {
            printf("[VOICE][MUSIC] unknown command: %s\n", cmd);
            run_piper("Unknown music command. Say play or stop.");
        }
        // music은 자동플래그 해제 등 없음
    }

    else {
		run_piper("Unknown command. Please say it again.");
        printf("[WARN] Unknown device: %s\n", dev); fflush(stdout);
    }

    cJSON_Delete(root);
}


