#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialize the voice transcription service.
 *
 * @return ESP_OK on success
 */
esp_err_t voice_transcription_init(void);

/**
 * Transcribe an audio file to text using Whisper API.
 *
 * @param audio_path    Path to the audio file (supports WAV, MP3, etc.)
 * @param transcript_buf Output buffer for the transcribed text
 * @param buf_size      Size of the output buffer
 * @return ESP_OK on success
 */
esp_err_t voice_transcribe_file(const char *audio_path, char *transcript_buf, size_t buf_size);

/**
 * Transcribe audio data directly from memory.
 *
 * @param audio_data    Pointer to audio data
 * @param audio_len     Length of audio data in bytes
 * @param transcript_buf Output buffer for the transcribed text
 * @param buf_size      Size of the output buffer
 * @return ESP_OK on success
 */
esp_err_t voice_transcribe_data(const uint8_t *audio_data, size_t audio_len, 
                              char *transcript_buf, size_t buf_size);

/**
 * Set the Whisper API key.
 *
 * @param api_key    The API key to use
 * @return ESP_OK on success
 */
esp_err_t voice_set_api_key(const char *api_key);

/**
 * Set the Whisper API base URL (for custom endpoints).
 *
 * @param base_url   The base URL to use
 * @return ESP_OK on success
 */
esp_err_t voice_set_base_url(const char *base_url);
