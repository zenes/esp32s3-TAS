#pragma once
#include "utilities.h"

#ifdef ENABLE_AUDIO

#ifdef __cplusplus
extern "C" {
#endif

// I2C 초기화 및 오디오 출력 설정 시작
void audio_output_start(void);

// 오디오 출력 정지
void audio_output_stop(void);

// 하드웨어 볼륨 조절 (0~100)
void audio_output_set_volume(int volume);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_AUDIO
