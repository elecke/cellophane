// you aren't getting any documentation for this one. this is shit!

#include "FL/Fl.H"
#include "FL/Fl_Box.H"
#include "FL/Fl_Button.H"
#include "FL/Fl_Choice.H"
#include "FL/Fl_Input.H"
#include "FL/Fl_Value_Input.H"
#include "FL/Fl_Window.H"
#include "FL/fl_draw.H"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <regex>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <vector>

#include <gtk/gtk.h>

extern char **environ;

namespace {

static std::string openFileDialog() {
  GtkWidget *Dialog = gtk_file_chooser_dialog_new(
    "Select Wallpaper", nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel",
    GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr);

  GtkFileFilter *Filter = gtk_file_filter_new();
  gtk_file_filter_set_name(Filter, "Images");
  gtk_file_filter_add_mime_type(Filter, "image/*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(Dialog), Filter);

  std::string Result;
  if (gtk_dialog_run(GTK_DIALOG(Dialog)) == GTK_RESPONSE_ACCEPT) {
    char *Filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(Dialog));
    if (Filename) {
      Result = Filename;
      g_free(Filename);
    }
  }
  gtk_widget_destroy(Dialog);
  while (gtk_events_pending())
    gtk_main_iteration();
  return Result;
}

class ColorPreview : public Fl_Box {
  Fl_Color Color = FL_BLACK;

public:
  ColorPreview(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {}

  void draw() override {
    Fl_Box::draw();
    fl_rectf(x(), y(), w(), h(), Color);
    fl_rect(x(), y(), w(), h(), FL_BLACK);
  }

  void setColor(const char *Hex) {
    if (!Hex || !*Hex)
      return;

    unsigned RGB;
    if (sscanf(Hex, "%x", &RGB) != 1)
      return;

    if (strlen(Hex) <= 3) {
      int R = (RGB >> 8) & 0xF;
      int G = (RGB >> 4) & 0xF;
      int B = RGB & 0xF;
      RGB = (R << 20) | (R << 16) | (G << 12) | (G << 8) | (B << 4) | B;
    }

    Color = fl_rgb_color((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF);
    redraw();
  }
};

class CellophaneUI : public Fl_Window {
  std::string ImagePath;
  std::string ColourHex = "000";
  int Mode = 1;
  int OffsetX = 0;
  int OffsetY = 0;

  static constexpr const char *Modes[] = {"Center", "Fill", "Max", "Scale",
                                          "Tile"};
  static constexpr const char *Args[] = {"center", "fill", "max", "scale",
                                         "tile"};

  Fl_Input *ImageInput;
  Fl_Input *ColourInput;
  Fl_Choice *ModeChoice;
  Fl_Value_Input *XInput;
  Fl_Value_Input *YInput;
  ColorPreview *ColorPreview_;

  static std::string decodeURI(const std::string &Uri) {
    std::regex re("^file://+");
    std::string stripped = std::regex_replace(Uri, re, "/");
    char *dec = g_uri_unescape_string(stripped.c_str(), nullptr);
    std::string out = dec ? dec : "";
    if (dec)
      g_free(dec);
    return out;
  }

  static void cbBrowse(Fl_Widget *, void *Data) {
    auto *UI = static_cast<CellophaneUI *>(Data);
    std::string Selected = openFileDialog();
    if (!Selected.empty()) {
      UI->ImagePath = Selected;
      UI->ImageInput->value(UI->ImagePath.c_str());
    }
  }

  static void cbApply(Fl_Widget *, void *Data) {
    static_cast<CellophaneUI *>(Data)->applyWallpaper();
  }

  static void cbColorChanged(Fl_Widget *W, void *Data) {
    auto *UI = static_cast<CellophaneUI *>(Data);
    UI->ColorPreview_->setColor(static_cast<Fl_Input *>(W)->value());
  }

  void loadConfig() {
    const char *Home = getenv("HOME");
    if (!Home)
      return;

    std::ifstream F(std::string(Home) + "/.wprc");
    if (!F.is_open())
      return;

    std::string Line;

    if (std::getline(F, Line))
      ImagePath = Line;

    if (std::getline(F, Line)) {
      for (int I = 0; I < 5; ++I) {
        if (Line == Args[I]) {
          Mode = I;
          break;
        }
      }
    }

    if (std::getline(F, Line))
      sscanf(Line.c_str(), "x=%d,y=%d", &OffsetX, &OffsetY);

    if (std::getline(F, Line))
      ColourHex = Line;
  }

  void syncWidgets() {
    ImageInput->value(ImagePath.c_str());
    ModeChoice->value(Mode);
    XInput->value(OffsetX);
    YInput->value(OffsetY);
    ColourInput->value(ColourHex.c_str());
    ColorPreview_->setColor(ColourHex.c_str());
  }

  void applyWallpaper() {
    ImagePath = ImageInput->value();
    Mode = ModeChoice->value();
    OffsetX = XInput->value();
    OffsetY = YInput->value();
    ColourHex = ColourInput->value();

    if (ImagePath.empty())
      return;

    std::vector<const char *> Arguments = {"wall", ImagePath.c_str(),
                                           Args[Mode]};

    if (Mode < 2 && (OffsetX != 0 || OffsetY != 0)) {
      std::string OffsetStr =
        "x=" + std::to_string(OffsetX) + ",y=" + std::to_string(OffsetY);
      Arguments.push_back(OffsetStr.c_str());
    }

    Arguments.push_back("-c");
    Arguments.push_back(ColourHex.c_str());
    Arguments.push_back(nullptr);

    pid_t PID;
    if (posix_spawnp(&PID, "wall", nullptr, nullptr,
                     const_cast<char *const *>(Arguments.data()),
                     environ) == 0) {
      waitpid(PID, nullptr, 0);
    }
  }

  void centerWindowOnScreen() {
    int X, Y, W, H;
    Fl::screen_work_area(X, Y, W, H);
    int winW = w();
    int winH = h();

    position(X + (W - winW) / 2, Y + (H - winH) / 2);
  }

public:
  CellophaneUI() : Fl_Window(400, 0, "Cellophane") {
    begin();

    ImageInput = new Fl_Input(60, 10, 250, 25, "Image:");
    Fl_Button *BrowseButton = new Fl_Button(320, 10, 70, 25, "Browse");
    BrowseButton->callback(cbBrowse, this);

    ModeChoice = new Fl_Choice(60, 45, 330, 25, "Mode:");
    for (const char *ModeName : Modes)
      ModeChoice->add(ModeName);

    XInput = new Fl_Value_Input(60, 80, 90, 25, "X:");
    YInput = new Fl_Value_Input(220, 80, 90, 25, "Y:");

    ColourInput = new Fl_Input(60, 115, 150, 25, "Hex:");
    ColourInput->callback(cbColorChanged, this);
    ColourInput->when(FL_WHEN_CHANGED);

    ColorPreview_ = new ColorPreview(220, 115, 50, 25);

    Fl_Button *ApplyButton = new Fl_Button(10, 150, 380, 30, "Apply");
    ApplyButton->callback(cbApply, this);

    end();
    resizable(nullptr);

    int Height = ApplyButton->y() + ApplyButton->h() + 10;
    size(400, Height);
    size_range(400, Height, 400, Height);

    loadConfig();
    syncWidgets();

    centerWindowOnScreen();
  }

  int handle(int ev) override {
    if (ev == FL_DND_ENTER || ev == FL_DND_DRAG || ev == FL_DND_RELEASE)
      return 1;
    if (ev == FL_PASTE) {
      std::string text = Fl::event_text();
      while (!text.empty() && std::isspace(text.back()))
        text.pop_back();
      std::string path = decodeURI(text);
      if (!path.empty()) {
        ImagePath = path;
        ImageInput->value(ImagePath.c_str());
      }
      return 1;
    }
    return Fl_Window::handle(ev);
  }
};

} // namespace

int main(int ArgC, char **ArgV) {
  if (!gtk_init_check(&ArgC, &ArgV)) {
    fprintf(stderr, "Failed to init GTK\n");
    return 1;
  }

  CellophaneUI UI;
  UI.show(ArgC, ArgV);

  return Fl::run();
}
