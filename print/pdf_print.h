#ifndef PDF_PRINT_H
#define PDF_PRINT_H

#include <QObject>
#include <vector>
#include <string>
#include <QSizeF>
#include "pdf_print_settings.h"
#include "spdfview.h"

class PageEngine;
class PDFDocument;

class DevModeWrapper {

public:
  DevModeWrapper(size_t size) {
    if (size == 0) {
      dev_mode_handle = GlobalAlloc(GMEM_ZEROINIT, sizeof(DEVMODE));
    } else {
      dev_mode_handle = GlobalAlloc(GMEM_ZEROINIT, size);
    }
  }

  ~DevModeWrapper() {
    GlobalFree(dev_mode_handle);
  }

  DEVMODE* GetDevMode() {
    return reinterpret_cast<DEVMODE*>(dev_mode_handle);
  }
   
private:
  HGLOBAL dev_mode_handle;
};

class PdfPrint : public QObject {
  Q_OBJECT
public:
  static bool PrintProperties(void* dev_model);

  explicit PdfPrint(QObject *parent = nullptr);
  virtual ~PdfPrint();
  virtual bool PrintPdfDoc(PageEngine* page_engine, 
    const std::tuple<int, QPointF, double>& current_page_rect,
                           print::_Print_PROCESS* = nullptr);
  bool PrintPoster(PageEngine* page_engine,print::_Print_PROCESS* = nullptr);
  bool Load(std::wstring& print_driver_name, void* dev_mode);
  void set_pages(const std::vector<int>& pages);
  void set_settings(const print::PdfPrintSettings& settings);
  void set_papersize(const QSizeF& size);
  void set_devicepixel(const QSizeF& size);

protected:
  bool PrintPdfDocWithHDC(void* hdc, PDFDocument* pdf_doc, const std::wstring& name,print::_Print_PROCESS*);
 
signals:

public slots:

protected:
  void InitGDIPrint();
  void ReleasePrintHDC();
  void ReleaseGDIPlus();

  void* print_hdc_ = nullptr;
  uint32_t gdiplus_token_ = 0;
  bool init_gdi_;
  int page_rotate_ = 0;
  std::vector<int> pages_;
  print::PdfPrintSettings settings_;
  QSizeF paper_size_;
  QSizeF device_pixel_;
};

#endif // PDF_PRINT_H
