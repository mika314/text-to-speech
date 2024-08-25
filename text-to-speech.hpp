#pragma once
#include <RHVoice.h>
#include <deque>
#include <sdlpp/sdlpp.hpp>
#include <string>
#include <vector>

class TextToSpeech
{
public:
  static auto inst() -> TextToSpeech &;
  auto operator()(std::string msg, bool blocking = true) const -> void;
  auto join() const -> void;

private:
  TextToSpeech();
  ~TextToSpeech();

  RHVoice_tts_engine engine;
  sdl::Init sdl;
  SDL_AudioSpec want;
  SDL_AudioSpec have;
  mutable sdl::Audio audio;
  mutable std::deque<int16_t> wav;
  static auto setSampleRate(int sample_rate, void *user_data) -> int;
  static auto playSpeech(const short *samples, unsigned int count, void *user_data) -> int;
};
