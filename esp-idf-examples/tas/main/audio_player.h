#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>
#include "esp_err.h"

/**
 * @brief Initialize the audio player
 * @return ESP_OK on success
 */
esp_err_t audio_player_init();

/**
 * @brief Play an audio file (MP3 or WAV)
 * @param filename Name of the file in /fatfs/
 * @return ESP_OK if playback started
 */
esp_err_t audio_player_play(const char *filename);

/**
 * @brief Stop current playback
 */
void audio_player_stop();

/**
 * @brief Check if audio is currently playing
 * @return true if playing
 */
bool audio_player_is_playing();

#endif // AUDIO_PLAYER_H
