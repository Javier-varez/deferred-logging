
#include <cstdint>
#include <cstdio>

#include "postform/rtt_logger.h"

#include "hal/systick.h"

int main() {
  Postform::RttLogger logger;
  SysTick& systick = SysTick::getInstance();

  const uint32_t systick_clk_hz = 72'000'000;
  systick.init(systick_clk_hz);

  logger.setLevel(Postform::LogLevel::DEBUG);

  uint32_t iteration = 0;
  while (true) {
    // All the logs here together act as the worst case for the decoder. Without proper framing
    // they would be confused from the host. If they all show that means that the COBS framing works
    LOG_DEBUG(&logger, "Iteration number: %u", iteration);
    LOG_DEBUG(&logger, "Is this %s or what?!", "nice");
    LOG_INFO(&logger, "I am %d years old...", 28);
    LOG_WARNING(&logger, "Third string! With multiple %s and more numbers: %d", "args", -1124);
    LOG_ERROR(&logger, "Oh boy, error %d just happened", 234556);
    char char_array[] = "123";
    LOG_ERROR(&logger, "This is my char array: %s", char_array);

    constexpr auto interned_string = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                             "Proin congue, libero vitae condimentum egestas, tortor metus "
                             "condimentum augue, in pretium dolor purus quis lectus. Aenean "
                             "nunc sapien, eleifend quis convallis ut, venenatis quis mauris. "
                             "Morbi tempor, ex a lobortis luctus, sem nunc laoreet dolor, "
                             "pellentesque gravida mauris risus nec est. Aliquam ante sapien, "
                             "vehicula vel elementum at, feugiat quis libero. Nulla in lorem eu "
                             "erat vulputate efficitur. Etiam dapibus purus sed sagittis lobortis. "
                             "Sed quis porttitor nulla. Nulla in ante ac arcu semper efficitur ut "
                             "at erat. Fusce porttitor suscipit augue. Donec vel lorem justo. "
                             "Aenean id dolor quis erat blandit cursus. Aenean varius fringilla "
                             "eros vitae vestibulum.\n"
                             "Morbi tristique tristique nulla, at posuere ex sagittis at. Aliquam "
                             "est quam, porta nec erat ac, convallis tempus augue. Nam eu quam "
                             "vulputate, luctus sapien vel, tristique arcu. Suspendisse et ultrices "
                             "odio. Pellentesque consectetur lacus sapien, ut ornare odio sagittis "
                             "vel. Cras molestie eros odio, vitae ullamcorper ante vestibulum non. "
                             "Vestibulum facilisis diam vel condimentum gravida. Donec in odio sit "
                             "amet metus aliquet pharetra ac in ante. Phasellus sit amet dui vehicula, "
                             "tristique neque et, ullamcorper est. Integer ullamcorper risus in mattis "
                             "laoreet. Nullam dignissim vel ex vel molestie. Vestibulum id eleifend "
                             "metus. Curabitur malesuada condimentum augue ut molestie. Vivamus "
                             "pellentesque purus sed velit placerat ultricies. In ut erat diam. "
                             "Suspendisse potenti."_intern;

    LOG_DEBUG(&logger, "Now if I wanted to print a really long text I can use %%k: %k",
              interned_string);
    systick.delay(SysTick::TICKS_PER_SECOND);
    iteration++;
  }
  return 0;
}
