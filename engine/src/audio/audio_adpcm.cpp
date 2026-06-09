/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "audio/audio_adpcm.hpp"
#include "debug/debug.hpp"
#include <tamtypes.h>
#include <malloc.h>
#include <kernel.h>
#include <cstdlib>
#include <cstring>
#include <audsrv.h>
#include <malloc.h>
#include <memory>
#include "thread/threading.hpp"

using std::unique_ptr;

namespace Tyra {

AudioAdpcm::AudioAdpcm() {}

AudioAdpcm::~AudioAdpcm() {}

void AudioAdpcm::init() { initAUDSRV(); }

void AudioAdpcm::initAUDSRV() {
  int ret = audsrv_adpcm_init();

  TYRA_ASSERT(ret >= 0,
              "AUDSRV returned error string:", audsrv_get_error_string());
}

void AudioAdpcm::reset() { audsrv_adpcm_init(); }

audsrv_adpcm_t* AudioAdpcm::load(const char* t_path) {
  FILE* file = fopen(t_path, "rb");
  fseek(file, 0, SEEK_END);
  u32 adpcmFileSize = ftell(file);
  u32 paddedSize = (adpcmFileSize + 15) & ~15;
  unique_ptr<u8, decltype(&std::free)> data(static_cast<u8*>(memalign(64, paddedSize)), &std::free);
  TYRA_ASSERT(data != nullptr, "Failed to allocate memory for ADPCM data");
  rewind(file);
  fread(data.get(), sizeof(u8), adpcmFileSize, file);
  memset(data.get() + adpcmFileSize, 0, paddedSize - adpcmFileSize);
  auto* result = new audsrv_adpcm_t();
  result->size = 0;
  result->buffer = 0;
  result->loop = 0;
  result->pitch = 0;
  result->channels = 0;

  SyncDCache(data.get(), data.get() + paddedSize);
  if (audsrv_load_adpcm(result, data.get(), paddedSize)) {
    TYRA_ERROR("AUDSRV returned error string: ", audsrv_get_error_string());
  }

  fclose(file);
  return result;
}

audsrv_adpcm_t* AudioAdpcm::load(const std::string& t_path) {
  return load(t_path.c_str());
}

AdpcmResult AudioAdpcm::tryPlay(audsrv_adpcm_t* t_adpcm) {
  return tryPlay(t_adpcm, -1);
}

AdpcmResult AudioAdpcm::tryPlay(audsrv_adpcm_t* t_adpcm, const s8& t_ch) {
  s8 ch = t_ch;
  return tryPlay(t_adpcm, ch);
}

AdpcmResult AudioAdpcm::tryPlay(audsrv_adpcm_t* t_adpcm, s8& t_ch) {
  int res = audsrv_ch_play_adpcm(t_ch, t_adpcm);
  if (res >= 0) {
    t_ch = res;
    return AdpcmResult::ADPCM_OK;
  } else if (res == -AUDSRV_ERR_NO_MORE_CHANNELS) {
    if (t_ch < 0) {
      return AdpcmResult::ADPCM_NO_FREE_CHANNELS;
    } else {
      return AdpcmResult::ADPCM_CHANNEL_USED;
    }
  } else {
    return AdpcmResult::ADPCM_ERROR;
  }
}

void AudioAdpcm::playWait(audsrv_adpcm_t* t_adpcm) { 
  playWait(t_adpcm, -1); 
}

void AudioAdpcm::playWait(audsrv_adpcm_t* t_adpcm, const s8& t_ch) {
  s8 ch = t_ch;
  return playWait(t_adpcm, ch);
}

void AudioAdpcm::playWait(audsrv_adpcm_t* t_adpcm, s8& t_ch) {
  int res = tryPlay(t_adpcm, t_ch);

  while (res == AdpcmResult::ADPCM_NO_FREE_CHANNELS ||
         res == AdpcmResult::ADPCM_CHANNEL_USED) {
    Threading::switchThread();
    res = tryPlay(t_adpcm, t_ch);
  }
}

}  // namespace Tyra
