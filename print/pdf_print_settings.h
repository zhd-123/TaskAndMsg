#ifndef PDF_PRINT_SETTINGS_H
#define PDF_PRINT_SETTINGS_H

#include <QObject>
#include <string>
#include <memory>
#include <vector>
#pragma warning(push)
#pragma warning(disable: 4146)
#include "xml.h"
#pragma warning(pop)

#define PrinterHisInfo "HistoryPrintInfo.ini"

namespace print_config {
#define PrintInfo "HistoryPrintInfo"
#define HisPaperSize "PaperSize"
#define HisPaperOrient "Orientation"
#define HisAutoRotate "AutoRotate"
#define HisAutoCenter "AutoCenter"
#define HisAsImage "AsImage"
#define HisColorgray "Colorgray"
#define HisReverseprint "Reverseprint"
#define HisBothSide "BothSide"
#define HisCopyCount "CopyCount"
#define HisCollate "Collate"
    void writeConfig(const QString&);
    QMap<QString,QString> readConfig(const QString&);
    void insertSubConfig(const QString&,const QString&);
    QMap<QString,QString> getConfig();
}

namespace print {

typedef struct _Print_PROCESS {
  bool (*Abort)(struct _Print_PROCESS* pThis);
  void* user;
} Print_PROCESS;

enum DuplexMode {
  kDuplexModeSingleSide = 0,
  kDuplexModeDuplexAuto,
};

struct PrinterSettings {
  DuplexMode duplex = kDuplexModeSingleSide;
  int copys = 1;

  XPACK(AF(F(ATTR, ATTR), duplex, "Duplex", copys, "Copys"));
};

enum PaperSizePresets {
  kPaperSizePresetDefault = 0,
  kPaperSizePresetTablod, // 27.9*43.2cm
  kPaperSizePresetLedger, // 43.2*27.9cm
  kPaperSizePresetA3, // 29.7*42cm
  kPaperSizePresetA4, // 21*29.7cm
  kPaperSizePresetA5, // 14.8*21cm
  kPaperSizePresetA2, // 43*59.4cm
  kPaperSizePresetA6, // 10.5*14.8cm
  kPaperSizePresetLetter, // 21.6*27.9cm
  kPaperSizePresetLegal, // 21.6*35.6cm
  kPaperSizePresetANSIC, // 43.2*55.9cm
  kPaperSizePresetANSID, // 55.9*86.4cm
  kPaperSizePresetANSIE, // 86.4*111.8cm
  kPaperSizePresetANSIF, // 71.1*101.6cm
  kPaperSizePresetARCHA, // 22.9*30.5cm
  kPaperSizePresetARCHB, // 30.5*45.7cm
  kPaperSizePresetARCHC, // 45.7*61cm
  kPaperSizePresetARCHD, // 61*91.4cm
  kPaperSizePresetARCHE, // 91.4*121.9cm
  kPaperSizePresetARCHE1, // 76.2*106.7cm
  kPaperSizePresetARCHE2, // 66*96.5cm
  kPaperSizePresetARCHE3, // 68.6*99.1cm
  kPaperSizePresetA1, // 59.4*84.1cm
  kPaperSizePresetA0, // 84.1*118.9cm
  kPaperSizePresetOversizeA2, // 48*62.5cm
  kPaperSizePresetOversizeA1, // 62.5*90cm
  kPaperSizePresetOversizeA0, // 90*124.5cm
  kPaperSizePresetISOB5, // 17.6*25cm
  kPaperSizePresetISOB4, // 25*35.3cm
  kPaperSizePresetISOB2, // 50*70.7cm
  kPaperSizePresetISOB1, // 70.7*100cm
  kPaperSizePresetC5, // 16.2*22.9cm
  kPaperSizePresetJISB4, // 25.7*36.4cm
  kPaperSizePresetJISB3, // 36.4*51.5cm
  kPaperSizePresetJISB2, // 51.5*72.8cm
  kPaperSizePresetJISB1, // 72.8*103cm
  kPaperSizePresetJISB0, // 103*145.6cm
  kPaperSizePresetLetterSmall, // 29.7*42cm
  kPaperSizePresetA4Small, // 21*29.7cm
  kPaperSizePresetA7, // 7.4*10.5cm
  kPaperSizePresetA8, // 5.2*7.4cm
  kPaperSizePresetA9, // 3.7*5.2cm
  kPaperSizePresetA10, // 2.6*3.7cm
  kPaperSizePresetISOB0, // 100*141.4cm
  kPaperSizePresetISOB3, // 35.3*50cm
  kPaperSizePresetISOB6, // 12.5*17.6cm
  kPaperSizePresetJISB5, // 18.2*25.7cm
  kPaperSizePresetJISB6, // 12.8*18.2cm
  kPaperSizePresetC0, // 91.7*129.7cm
  kPaperSizePresetC1, // 64.8*91.7cm
  kPaperSizePresetC2, // 45.8*64.8cm
  kPaperSizePresetC3, // 32.4*45.8cm
  kPaperSizePresetC4, // 22.9*32.4cm
  kPaperSizePresetC6, // 11.4*16.2cm
  kPaperSizePresetFLSA, // 21.6*33cm
  kPaperSizePresetFLSE, // 21.6*33cm
  kPaperSizePresetHalfLetter, // 14*21.6cm
  kPaperSizePresetPA4, // 21*27.9cm
  kPaperSizePreset92x92, // 233.7*233.7cm
  kPaperSizePresetVGA, // 22.6*16.9cm
  kPaperSizePresetSVGA, // 28.2*21.2cm
  kPaperSizePresetXGA, // 36.1*27.1cm
  kPaperSizePresetSXGA, // 45.2*36.1cm
  kPaperSizePresetTVNTSCDV, // 25.4*16.9cm
  kPaperSizePresetTVNTSCD1, // 25.4*17.1cm
  kPaperSizePresetTVNTSCD1Squre, // 25.4*19.1cm
  kPaperSizePresetTVPALSquare, // 27.1*20.3cm
  kPaperSizePresetTVPAL, // 25.4*20.3cm
  kPaperSizePresetTVHDTV720p, // 45.2*25.4cm
  kPaperSizePresetTVHDTV1080p, // 67.7*38.1cm
  kPaperSizePresetPostScriptCustom, // 21*29.7cm
};

struct PaperSize {
  bool base_on_pdf_page_size = false;
  PaperSizePresets preset = kPaperSizePresetDefault;

  XPACK(AF(F(ATTR, ATTR), base_on_pdf_page_size,
    "BaseOnPdfPageSize", preset, "Preset"));
};

enum PageRangePreset {
  kPageRangePresetAll = 0,
  kPageRangePresetCurrentPage,
  kPageRangePresetCustom,
  kPageRangePresetCurrentView,
};

enum PageRangeSubset {
  kPageRangeSubsetAll = 0, // 所有
  kPageRangeSubsetOdd, // 奇数页
  kPageRangeSubsetEven, // 偶数页
};

struct PageRange {
  PageRangePreset preset = kPageRangePresetAll;
  PageRangeSubset subset = kPageRangeSubsetAll;
  std::vector<int> pages;

  XPACK(AF(F(ATTR, ATTR), preset,
    "Preset", subset, "Subset"), A(pages, "xml:Pages,vl@Page"));
};

enum PageOrientation {
  kPageOrientaionPortrait = 0, // 纵
  kPageOrientationLandscape, // 横
};

enum PageModeSizePreset {
  kPageModeSizePresetFit = 0,
  kPageModeSizePresetActual,
  kPageModeSizePresetCustom
};

struct PageModeSize {
  PageModeSizePreset preset = kPageModeSizePresetFit;
  float scale = 1.0f;
  bool auto_shrink = false;

  XPACK(AF(F(ATTR, ATTR, ATTR), preset,
    "Preset", scale, "Scale",auto_shrink,"AutoShrink"));
};

struct ShowCrop{
    bool left_top = true;
    bool right_top = true;
    bool right_bottom = true;
    bool left_bottom = true;
};

struct PosterPageSettings{
    int row = 1;
    int col = 1;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float paper_width = 0.0f;
    float paper_height = 0.0f;
    float real_paper_width = 0.0f;
    float real_paper_height = 0.0f;
};

struct PageModePosters {
  float tile_percent = 1.0f;
  float overlap = 0.0f;
  bool show_crop = false;
  bool show_tag = false;
  int cur_row = 1;
  int cur_col = 1;
  std::vector<PosterPageSettings> poster_page_sets;

  XPACK(AF(F(ATTR, ATTR, ATTR, ATTR), tile_percent, "Tile",
    overlap, "Overlap", show_crop, "Crop",cur_row, "Row",cur_col, "Col"));
};

enum PageModePagesOrder {
  kPageModePagesOrderPortrait = 0,
  kPageModePagesOrderPortraitReverse,
  kPageModePagesOrderLandscape,
  kPageModePagesOrderLandscapeReverse,
};

struct PageModePages {
  int pages = 2; // -1 表示自定义
  int horiz_count = 1;
  int vert_count = 2;
  bool page_border = false;
  bool crop_line = false;
  bool invoice = false;
  PageModePagesOrder order = kPageModePagesOrderPortrait;

  XPACK(AF(F(ATTR, ATTR, ATTR, ATTR, ATTR), pages, "Pages",
    horiz_count, "Horiz", vert_count, "Vert", order, "Order",page_border, "Border"));
};

enum PageModeBookletSubset {
  kPageModeBookletSubsetAll = 0,
  kPageModeBookletSubsetOdd,
  kPageModeBookletSubsetEven,
};

struct PageModeBooklet {
  PageModeBookletSubset subset = kPageModeBookletSubsetAll;
  int start_page = -1;
  int end_page = -1;
  bool gutter_left_or_right = true;

  XPACK(AF(F(ATTR, ATTR, ATTR, ATTR), subset, "Subset",
    start_page, "Start", end_page, "End",
    gutter_left_or_right, "Gutter"));
};

enum PageModeKind {
  kPageModeKindUnknown = 0,
  kPageModeKindSize,
  kPageModeKindPosters,
  kPageModeKindPages,
  kPageModeKindBooklet,
};

struct PageMode {
  PageModeKind type = kPageModeKindUnknown;
  std::shared_ptr<PageModeSize> size;
  std::shared_ptr<PageModePosters> posters;
  std::shared_ptr<PageModePages> pages;
  std::shared_ptr<PageModeBooklet> booklet;
  bool auto_rotate = true;
  bool auto_center = true;

  PageMode();
  void set_page_mode_size(const std::shared_ptr<PageModeSize>& val);
  void set_page_mode_poster(const std::shared_ptr<PageModePosters>& val);
  void set_page_mode_pages(const std::shared_ptr<PageModePages>& val);
  void set_page_mode_booklet(const std::shared_ptr<PageModeBooklet>& val);
  void Create(PageModeKind type);
  void Clear();

  XPACK(AF(F(ATTR, ATTR, ATTR), type, "ModeType",
    auto_rotate, "AutoRotate", auto_center, "AutoCenter"),
    A(size, "SizeMode", posters, "Posters", pages, "Pages",
      booklet, "Booklet"));
};

struct PdfPrintSettings {
  std::string printer_name;
  PrinterSettings printer_settings;
  PaperSize paper_size;
  PageRange page_range;
  PageOrientation page_orientation = kPageOrientaionPortrait;
  bool gray_print = false;
  bool contain_content = true;
  bool contain_annot = true;
  bool contain_form_field = true;
  bool reverse_print = false;
  bool print_as_image = false;
  PageMode page_mode;

  XPACK(AF(F(ATTR), printer_name, "PrinterName",
    page_orientation, "PageDirect", gray_print, "Gray",
    contain_content, "Content", contain_annot, "Annot",
    contain_form_field, "Field", reverse_print, "Reverse",
    print_as_image, "AsImage"),
    A(printer_settings, "PrinterSettings", paper_size, "PaperSize",
      page_range, "PageRange", page_mode, "PageMode"));
};

std::string SaveXml(const PdfPrintSettings& settings);
void LoadXml(const std::string& str, PdfPrintSettings& settings);
QString get_his_config();
}
#endif // PDF_PRINT_SETTINGS_H
