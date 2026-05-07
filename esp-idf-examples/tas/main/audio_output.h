#pragma once
#include <stddef.h>
#include <stdint.h>
#include "utilities.h"

#ifdef ENABLE_AUDIO

#ifdef __cplusplus
extern "C" {
#endif

// 오디오 출력 설정 시작 (하드웨어 초기화)
void audio_output_start(void);

// 테스트용 사인파 재생 시작
void audio_output_play_sine(void);

// 오디오 출력 정지
void audio_output_stop(void);

// 하드웨어 볼륨 조절 (0~100)
void audio_output_set_volume(int volume);

// PCM 데이터 전송 (I2S Write)
size_t audio_output_write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_AUDIO
