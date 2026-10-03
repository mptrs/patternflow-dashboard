// Renders the dashboard screens on a desktop, with the real screen code and
// Patternflow's real fonts. Used by tools/render_screens.py.
//   render <open-meteo.json> <out dir> [screen...]
#include <string>
#include "features/dashboard/preset_dashboard.h"

static void save(const std::string& path) {
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", PFCanvas::W, PFCanvas::H);
  fwrite(PFCanvas::rgb, 1, PFCanvas::W * PFCanvas::H * 3, f);
  fclose(f);
}

int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "rb");
  std::string body;
  char buf[4096];
  for (size_t n; (n = fread(buf, 1, sizeof buf, f));) body.append(buf, n);
  fclose(f);
  PatternflowClock::fixedNow = time(nullptr);
  DashWeather::lat = 52.37f;
  DashWeather::lon = 4.89f;
  if (!DashWeather::parse(body.c_str(), DashWeather::current)) return printf("could not parse the weather\n"), 1;
  DashClocks::load();
  const char* names[] = {"clock", "weather", "hourly", "forecast", "world"};
  for (int o : {1, 0}) {
    DashState::orientation = o;
    for (int s = 0; s < 5; s++) {
      if (argc > 3) {
        bool want = false;
        for (int a = 3; a < argc; a++) want |= strcmp(argv[a], names[s]) == 0;
        if (!want) continue;
      }
      Dashboard::screen = s;
      const std::string tail = o ? "-portrait.ppm" : "-landscape.ppm";
      Dashboard::draw();
      save(std::string(argv[2]) + "/" + names[s] + tail);
    }
  }
  return 0;
}
