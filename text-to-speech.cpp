#include "text-to-speech.hpp"
#include <cstring>
#include <log/log.hpp>
#include <thread>

TextToSpeech::TextToSpeech()
  : engine(RHVoice_new_tts_engine([]() {
      static RHVoice_init_params params;
      params.data_path = "/usr/share/RHVoice";
      params.config_path = "/etc/RHVoice";
      params.resource_paths = nullptr;
      params.callbacks.set_sample_rate = setSampleRate;
      params.callbacks.play_speech = playSpeech;
      params.callbacks.process_mark = nullptr;
      params.callbacks.word_starts = nullptr;
      params.callbacks.word_ends = nullptr;
      params.callbacks.sentence_starts = nullptr;
      params.callbacks.sentence_ends = nullptr;
      params.callbacks.play_audio = nullptr;
      params.callbacks.done = nullptr;
      params.options = 0;
      return &params;
    }())),
    sdl(SDL_INIT_AUDIO),
    want([]() {
      SDL_AudioSpec want;
      want.freq = 24000;
      want.format = AUDIO_S16;
      want.channels = 1;
      want.samples = 4096;
      return want;
    }()),
    audio(nullptr, false, &want, &have, 0, [this](Uint8 *stream, int len) {
      int16_t *s = (int16_t *)stream;
      for (auto i = 0u; i < len / sizeof(int16_t); ++i, ++s)
      {
        if (wav.empty())
          *s = 0;
        else
        {
          *s = wav.front();
          wav.pop_front();
        }
      }
      return len;
    })
{
  audio.pause(false);
}

TextToSpeech::~TextToSpeech()
{
  RHVoice_delete_tts_engine(engine);
}

static auto trimWhite(std::string text)
{
  text.erase(0, text.find_first_not_of(" \t\r\n"));
  text.erase(text.find_last_not_of(" \t\r\n") + 1);
  return text;
}

auto TextToSpeech::operator()(std::string text, bool blocking) const -> void
{
  text = trimWhite(text);
  auto endSentenceChar = false;
  auto tmp = std::string{};

  auto tts = [&]() {
    RHVoice_synth_params params;
    params.voice_profile = "Slt";
    /* The values must be between -1 and 1. */
    /*     They are normalized this way, because users can set different */
    /* parameters for different voices in the configuration file. */
    params.absolute_rate = .9f;
    params.absolute_pitch = 0.;
    params.absolute_volume = 1.;
    /* Relative values, in case someone needs them. */
    /* If you don't, just set each of them to 1. */
    params.relative_rate = 1.;
    params.relative_pitch = 1.;
    params.relative_volume = 1.;
    /* Set to RHVoice_punctuation_default to allow the synthesizer to decide */
    params.punctuation_mode = RHVoice_punctuation_default;
    /* Optional */
    params.punctuation_list = "";
    /* This mode only applies to reading by characters. */
    /* If your program doesn't support this setting, set to RHVoice_capitals_default. */
    params.capitals_mode = RHVoice_capitals_default;
    /* Set to 0 for defaults. */
    params.flags = 0;

    std::pair<int, std::vector<int16_t>> ret;
    auto m = RHVoice_new_message(engine, tmp.c_str(), tmp.size(), RHVoice_message_text, &params, &ret);
    RHVoice_speak(m);
    audio.lock();
    wav.insert(std::end(wav), std::begin(ret.second), std::end(ret.second));
    audio.unlock();
  };

  for (auto ch : text)
  {
    tmp.push_back(ch);
    if (tmp.size() < 100)
      continue;
    if ((ch == ' ' && endSentenceChar) || (ch == '\n'))
    {
      tts();
      tmp = "";
    }
    endSentenceChar = (ch == '.' || ch == '!' || ch == '?');
  }
  tts();

  if (!blocking)
    return;
  join();
}

auto TextToSpeech::join() const -> void
{
  for (;;)
  {
    audio.lock();
    if (wav.empty())
    {
      audio.unlock();
      break;
    }
    audio.unlock();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
  }
}

auto TextToSpeech::setSampleRate(int sample_rate, void *user_data) -> int
{
  auto &ret = *static_cast<std::pair<int, std::vector<int16_t>> *>(user_data);
  ret.first = sample_rate;
  return true;
}

auto TextToSpeech::playSpeech(const short *samples, unsigned int count, void *user_data) -> int
{
  auto &ret = *static_cast<std::pair<int, std::vector<int16_t>> *>(user_data);
  ret.second.insert(std::end(ret.second), samples, samples + count);
  return true;
}

auto TextToSpeech::inst() -> TextToSpeech &
{
  static auto inst = TextToSpeech{};
  return inst;
}
