#include "pdf_print.h"
#include <windows.h>
#include <gdiplus.h>
#include <gdiplusgraphics.h>
#include <gdiplusbase.h>
#include <QDebug>
#include <QFileInfo>
#include <QTime>
#include <map>
#include <qmath.h>
#include <QStandardPaths>
#include <QPainter>
#include "../pageengine/pageengine.h"
#include "../pageengine/DataCenter.h"
#include "../pageengine/pdfcore/PDFDocument.h"
#include "../pageengine/pdfcore/PDFPageManager.h"
#include "../pageengine/pdfcore/PDFPage.h"
#include "spdfview.h"
#pragma comment (lib, "GdiPlus.lib")
#undef min

using namespace Gdiplus;

PdfPrint::PdfPrint(QObject *parent): QObject(parent) {
}

PdfPrint::~PdfPrint() {
  ReleasePrintHDC();
  ReleaseGDIPlus();
}

bool PdfPrint::Load(std::wstring& print_driver_name, void* dev_mode) {
  InitGDIPrint();
  ReleasePrintHDC();
  if (print_driver_name.empty()) {
    DWORD size = 0;
    // Get the size of the default printer name.
    GetDefaultPrinter(NULL, &size);

    if (size != 0) {
      // Allocate a buffer large enough to hold the printer name.
      print_driver_name.resize(size);
      // Get the printer name.
      GetDefaultPrinter(&print_driver_name[0], &size);
    }
  }
  auto mode = (LPDEVMODE)dev_mode;
  print_hdc_ = CreateDC(NULL, print_driver_name.c_str(), NULL, mode);
  return print_hdc_ != nullptr;
}

void PdfPrint::set_pages(const std::vector<int>& pages) {
  pages_ = pages;
}

void PdfPrint::set_settings(const print::PdfPrintSettings & settings) {
  settings_ = settings;
}

void PdfPrint::set_papersize(const QSizeF & size) {
  paper_size_ = size;
}

void PdfPrint::set_devicepixel(const QSizeF &size) {
  device_pixel_ = size;
}

bool PdfPrint::PrintProperties(void * dev_model) {
  HANDLE printer_handle;
  auto model = LPDEVMODE(dev_model);
  auto ret = OpenPrinter(model->dmDeviceName, &printer_handle, 0);
  ret = DocumentProperties(NULL, printer_handle, model->dmDeviceName,
    model, model, DM_IN_PROMPT | DM_OUT_BUFFER | DM_IN_BUFFER);
  ret = ClosePrinter(printer_handle);
  return true;
}

bool PdfPrint::PrintPoster(PageEngine *page_engine,print::_Print_PROCESS*print_process)
{
    if (!page_engine) {
      return false;
    }
    auto document = page_engine->dataCenter()->doc();

    if (!document) {
      return false;
    }
    InitGDIPrint();
    auto page_count = PDF_GetPageCount(document->pdfPointer());
    if (print_hdc_ == nullptr) {
      // Create a PRINTDLG structure, and initialize the appropriate fields.
      PRINTDLG printDlg;
      ZeroMemory(&printDlg, sizeof(printDlg));
      printDlg.lStructSize = sizeof(printDlg);
      printDlg.Flags = PD_RETURNDC;
      printDlg.nMinPage = 0;
      printDlg.nMaxPage = page_count - 1;
      printDlg.nFromPage = 0;
      printDlg.nToPage = page_count - 1;

      // Display a print dialog box.
      if (!PrintDlg(&printDlg)) {
        printf("Failure\n");
        return false;
      }
      print_hdc_ = printDlg.hDC;

      if (printDlg.hDevMode) {
        GlobalFree(printDlg.hDevMode);
      }

      if (printDlg.hDevNames) {
        GlobalFree(printDlg.hDevNames);
      }
    }
    auto file_path = page_engine->dataCenter()->getFilePath();
    auto title = QFileInfo(file_path).fileName().toStdWString();

    auto print_hdc = reinterpret_cast<HDC>(print_hdc_);
    if (!document || !print_hdc) {
      return false;
    }
    DOCINFO docInfo;
    ZeroMemory(&docInfo, sizeof(docInfo));
    docInfo.cbSize = sizeof(docInfo);
    docInfo.lpszDocName = title.c_str();
    // Now that PrintDlg has returned, a device context handle
    // for the chosen printer is in printDlg->hDC.
    StartDoc(print_hdc, &docInfo);
    if (!pages_.empty()) {
      page_count = (int)pages_.size();
    }
    int flag = PDF_PRINTING;

    if (settings_.contain_annot) {
      flag |= PDF_ANNOT;
    }

    if (settings_.contain_form_field) {
      flag |= PDF_SHOW_WIDGET;
    }

    if (!settings_.contain_content) {
      flag |= PDF_HIDE_CONTENT;
    }       
    auto rotate  = 0;
    auto get_rect = [&](int i,int cur_row,int cur_col,QPointF& bitmap_point,print::ShowCrop& show_crop_stru)->QRectF{
        auto paper_size_width = settings_.page_mode.posters->poster_page_sets[i].paper_width;
        auto paper_size_height = settings_.page_mode.posters->poster_page_sets[i].paper_height;
        auto whole_sheet_rect = QRectF(settings_.page_mode.posters->poster_page_sets[i].x,
                                       settings_.page_mode.posters->poster_page_sets[i].y,
                                       settings_.page_mode.posters->poster_page_sets[i].width,
                                       settings_.page_mode.posters->poster_page_sets[i].height);
        auto row = settings_.page_mode.posters->poster_page_sets[i].row;
        auto col = settings_.page_mode.posters->poster_page_sets[i].col;
        if(settings_.page_orientation == print::kPageOrientationLandscape){
            std::swap(paper_size_width,paper_size_height);
//            rotate = 3;
        }
        QRectF rect;
        qreal first_width = paper_size_width - whole_sheet_rect.x();
        qreal first_height = paper_size_height - whole_sheet_rect.y();
        qreal lefttop_x= cur_row > 0 ? ((cur_row - 1) * paper_size_width + first_width) : 0;
        qreal lefttop_y = cur_col > 0 ? ((cur_col - 1) * paper_size_height + first_height) : 0;
        qreal cur_width = (cur_row == 0 || cur_row == row - 1) ? first_width : paper_size_width;
        qreal cur_height= (cur_col == 0 || cur_col == col - 1) ? first_height : paper_size_height;
        qreal bitmap_x = cur_row > 0 ? 0 : whole_sheet_rect.x();
        qreal bitmap_y = cur_col > 0 ? 0 : whole_sheet_rect.y();
        bitmap_point = QPointF(bitmap_x,bitmap_y);
        show_crop_stru.left_top = (cur_row == 0 && cur_col == 0) ? false : true;
        show_crop_stru.right_top = (cur_col == 0 && cur_row == row - 1) ? false : true;
        show_crop_stru.right_bottom = (cur_row == row - 1 && cur_col == col - 1) ? false : true;
        show_crop_stru.left_bottom = (cur_row == 0 && cur_col == col - 1) ? false : true;
        rect.setTopLeft(QPointF(lefttop_x,lefttop_y));
        rect.setSize(QSizeF(cur_width,cur_height));
        return rect;
    };

    auto print_page = [&](int i) ->bool{
        auto paper_size_width = settings_.page_mode.posters->poster_page_sets[i].paper_width;
        auto paper_size_height = settings_.page_mode.posters->poster_page_sets[i].paper_height;
        auto whole_sheet_rect = QRectF(settings_.page_mode.posters->poster_page_sets[i].x,
                                       settings_.page_mode.posters->poster_page_sets[i].y,
                                       settings_.page_mode.posters->poster_page_sets[i].width,
                                       settings_.page_mode.posters->poster_page_sets[i].height);
        auto row = settings_.page_mode.posters->poster_page_sets[i].row;
        auto col = settings_.page_mode.posters->poster_page_sets[i].col;
        int width = settings_.page_mode.posters->poster_page_sets[i].real_paper_width;
        int height = settings_.page_mode.posters->poster_page_sets[i].real_paper_height;
        int page_index = pages_.empty() ? i : pages_[i];
        auto pdf_page = document->pageManager()->pageAtIndex(page_index)->pdfPointer();
        float page_width = PDF_GetPageWidth(pdf_page);
        float page_height = PDF_GetPageHeight(pdf_page);
        float scale_ = page_width / whole_sheet_rect.width();
        float bitmap_scale = width / paper_size_width;
        auto draw_width  = settings_.page_mode.posters->poster_page_sets[i].real_paper_width;
        auto draw_height  = settings_.page_mode.posters->poster_page_sets[i].real_paper_height;
        auto draw_width_p = device_pixel_.width();
        auto draw_height_p = device_pixel_.height();
        if(settings_.page_orientation == print::kPageOrientationLandscape){
            std::swap(paper_size_width,paper_size_height);
            std::swap(width,height);
            std::swap(draw_width,draw_height);
        }       

        Graphics* graphics = new Graphics(print_hdc);
        graphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
        auto dpi_x = graphics->GetDpiX();
        auto dpi_y = graphics->GetDpiY();
        qDebug() << "DPI X:" << dpi_x << "DPI Y:" << dpi_y;
        float dpi_scale_x = dpi_x / 72.f;
        float dpi_scale_y = dpi_y / 72.f;
        float dpi_scale = std::min(dpi_scale_x, dpi_scale_y);
        draw_width *= dpi_scale;
        draw_height *= dpi_scale;
        width *= dpi_scale;
        height *= dpi_scale;
        float reserve = 0.25 * 72 * dpi_scale;//预留
        if (pdf_page == nullptr) {
          return false;
        }

        QList<QRectF> render_rect_list; //00
        for(int m = 0;m<row;m++){
            for(int n = 0;n<col;n++){
                StartPage(print_hdc);
                QPointF bitmap_point;
                print::ShowCrop showcrop_stru;
                auto render_rect = get_rect(i,m,n,bitmap_point,showcrop_stru);
                auto bitmap_rect = QRectF(bitmap_point*bitmap_scale*dpi_scale,QSizeF(render_rect.size())*bitmap_scale*dpi_scale);
                auto render_point = QPointF(render_rect.topLeft())*scale_;
                auto render_size = QSizeF(render_rect.size())*scale_;
                render_rect.setTopLeft(render_point);
                render_rect.setSize(render_size);
                if( m == 0 && n == 0){
                    render_rect_list.push_back(render_rect);
                }

                {
                    if(settings_.page_mode.posters->overlap > 0.0f){
                        auto point_x = render_rect.x();
                        auto point_y = render_rect.y();
                        auto size_width = render_rect.width();
                        auto size_height = render_rect.height();
                        auto overlap_width = settings_.page_mode.posters->overlap * 72 / 2.54;
                        if(showcrop_stru.left_top){
                            if(m==1 && settings_.page_orientation == print::kPageOrientationLandscape){
                                point_x -= qMin(render_rect_list[0].width(),overlap_width);
                                size_width += qMin(render_rect_list[0].width(),overlap_width);
                            } else if(m>0 && settings_.page_orientation == print::kPageOrientationLandscape){
                                point_x -= overlap_width;
                                size_width += overlap_width;
                            }
                            if(n==1 && settings_.page_orientation == print::kPageOrientaionPortrait){
                                point_y -= qMin(render_rect_list[0].height(),overlap_width);
                                size_height += qMin(render_rect_list[0].height(),overlap_width);
                            } else if(n>0 && settings_.page_orientation == print::kPageOrientaionPortrait){
                                point_y -= overlap_width;
                                size_height += overlap_width;
                            }
                        }
                        render_rect.setTopLeft(QPointF(point_x,point_y));
                        render_rect.setSize(QSizeF(size_width,size_height));

                        //adjust bitmap_rect
//                        auto fixed_width = settings_.page_orientation == print::kPageOrientaionPortrait;
//                        auto aspect_ratio = render_rect.width() / render_rect.height();
//                        if(fixed_width){
//                            auto adjust_height = bitmap_rect.width() / aspect_ratio;
//                            bitmap_rect = QRectF(QPointF(bitmap_rect.x(),(bitmap_rect.height() - adjust_height)/2),
//                                                 QSizeF(bitmap_rect.width(),adjust_height));
//                        } else {
//                            auto adjust_width = bitmap_rect.height() * aspect_ratio;
//                            bitmap_rect = QRectF(QPointF((bitmap_rect.width() - adjust_width)/2,bitmap_rect.y()),
//                                                 QSizeF(adjust_width,bitmap_rect.height()));
//                        }
                    }
                }

                if(settings_.page_mode.posters->show_tag || settings_.page_mode.posters->show_crop){ //shrink rect
                    auto shrink_scale = (height - reserve*2) / height;
                    auto shrink_offset_x = (bitmap_rect.width() - bitmap_rect.width() * shrink_scale) / 2;
                    auto shrink_offset_y = (bitmap_rect.height() - bitmap_rect.height() * shrink_scale) / 2;
                    bitmap_rect = QRectF(bitmap_rect.topLeft() + QPointF(shrink_offset_x,shrink_offset_y),bitmap_rect.size()*shrink_scale);
                } else {
                    std::memset(&showcrop_stru, 0, sizeof(showcrop_stru));
                }
                PDF_RECTF page_rect = {float(render_rect.x()),
                                       float(page_height-render_rect.y()),
                                       float(render_rect.x()+render_rect.width()),
                                       float(page_height-render_rect.height()-render_rect.y())};

                if (settings_.print_as_image) {                 
                  PDF_BITMAP bitmap = PDFBitmap_Create(width, height, 1);
                  PDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
                  float offset_x = 0;
                  float offset_y = 0;

                  PDF_RenderPageBitmap(bitmap,pdf_page,
                                       &page_rect,
                                       bitmap_rect.x(),bitmap_rect.y(),
                                       bitmap_rect.width(),bitmap_rect.height(),
                                       rotate,flag);
                  void *data = PDFBitmap_GetBuffer(bitmap);
                  auto gdi_bitmap = new Gdiplus::Bitmap( int(width), int(height), PixelFormat32bppARGB);
                  gdi_bitmap->SetResolution(dpi_x, dpi_y);
                  Gdiplus::BitmapData datas;
                  Gdiplus::Rect rc(0, 0, int(width), int(height));
                  gdi_bitmap->LockBits(&rc, Gdiplus::ImageLockMode::ImageLockModeWrite,
                    PixelFormat32bppARGB, &datas);
                  memcpy(datas.Scan0, data, width * height * 4);
                  gdi_bitmap->UnlockBits(&datas);
                  graphics->DrawImage(gdi_bitmap, offset_x, offset_y);
                  delete gdi_bitmap;
                  PDFBitmap_Destroy(bitmap);
                } else {
                    PDF_RenderPageHDC(reinterpret_cast<PDF_HDC>(print_hdc),pdf_page,&page_rect,
                                      bitmap_rect.x(),bitmap_rect.y(),
                                      bitmap_rect.width(),bitmap_rect.height(),rotate,flag);
                }

                qDebug()<<"draw_width="<<draw_width_p<<"draw_height="<<draw_height_p;
                Gdiplus::Pen pen(Color(0,0,0));
                if(showcrop_stru.left_top){
                    graphics->DrawLine(&pen,Point(0,12),Point(12,12));
                    graphics->DrawLine(&pen,Point(12,0),Point(12,12));
                }
                if(showcrop_stru.right_top){
                    graphics->DrawLine(&pen,Point(draw_width_p-12,0),Point(draw_width_p-12,12));
                    graphics->DrawLine(&pen,Point(draw_width_p,12),Point(draw_width_p-12,12));
                }
                if(showcrop_stru.right_bottom){
                    graphics->DrawLine(&pen,Point(draw_width_p,draw_height_p-12),Point(draw_width_p-12,draw_height_p-12));
                    graphics->DrawLine(&pen,Point(draw_width_p-12,draw_height_p),Point(draw_width_p-12,draw_height_p-12));
                }
                if(showcrop_stru.left_bottom){
                    graphics->DrawLine(&pen,Point(0,draw_height_p-12),Point(12,draw_height_p-12));
                    graphics->DrawLine(&pen,Point(12,draw_height_p),Point(12,draw_height_p-12));
                }
                if(settings_.page_mode.posters->show_tag){
                    auto time_str = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss");
                    QString tag_string = QString("(%1,%2)").arg(n+1).arg(m+1) + QString(" -%1- ").arg(i+1) + QString(QFileInfo(file_path).fileName()) + QString(" ")+time_str;
                    Gdiplus::Font font(L"",6);
                    Gdiplus::RectF rectF(72,0,width-(reserve+72)*2,12);
                    Gdiplus::StringFormat string_format;
                    string_format.SetAlignment(StringAlignment::StringAlignmentNear);
                    string_format.SetLineAlignment(StringAlignment::StringAlignmentNear);
                    Gdiplus::SolidBrush brush(Gdiplus::Color(0,0,0));
                    auto tag_wchar = (wchar_t*)reinterpret_cast<const wchar_t *>(tag_string.utf16());
                    graphics->DrawString(tag_wchar,-1,&font,rectF,&string_format,&brush);
                }
                EndPage(print_hdc);
            }
        }
        delete graphics;
        return true;
    };

    if (settings_.reverse_print) {
      for (int i = page_count - 1; i >= 0; --i) {
      if(print_process && print_process->Abort(print_process))
        break;
        print_page(i);
      }
    } else {
      for (int i = 0; i < page_count; ++i) {
      if(print_process && print_process->Abort(print_process))
        break;
        print_page(i);
      }
    }
    EndDoc(print_hdc);
    return true;
}

QImage create_crop_image(const QSize& size, int row_count ,int col_count){
    qreal penWidth = size.width() * 10 / 10000;
    penWidth = qMax(penWidth,2.0);
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    QPen pen(QColor(0,0,0,255));
    pen.setStyle(Qt::CustomDashLine);
    QVector<qreal> pattern = {penWidth,penWidth};
    pen.setDashPattern(pattern);
    pen.setWidthF(penWidth);
    painter.setPen(pen);

    qreal average_width = size.width()*1.0 / col_count*1.0;
    qreal average_height = size.height()*1.0 / row_count*1.0;

    for (int row = 1; row < row_count; row++) {
        painter.drawLine(QPointF(0, average_height*row), QPointF(size.width(), average_height*row));
    }
    for (int col = 1; col < col_count; col++) {
        painter.drawLine(QPointF(average_width*col,0), QPointF(average_width*col,size.height()));
    }
    painter.end();
    return pixmap.toImage();
}

bool PdfPrint::PrintPdfDocWithHDC(void * hdc, PDFDocument* document, const std::wstring& name,
                                  print::_Print_PROCESS* print_process) {
  auto print_hdc = reinterpret_cast<HDC>(hdc);
  if (!document || !print_hdc) {
    return false;
  }
  float paper_width = paper_size_.width();
  float paper_height = paper_size_.height();
  auto page_count = PDF_GetPageCount(document->pdfPointer());
  DOCINFO docInfo;
  ZeroMemory(&docInfo, sizeof(docInfo));
  docInfo.cbSize = sizeof(docInfo);
  docInfo.lpszDocName = name.c_str();
  // Now that PrintDlg has returned, a device context handle
  // for the chosen printer is in printDlg->hDC.
  StartDoc(print_hdc, &docInfo);

  if (!pages_.empty()) {
    page_count = (int)pages_.size();
  }
  int flag = PDF_PRINTING;

  if (settings_.contain_annot) {
    flag |= PDF_ANNOT;
  }

  if (settings_.contain_form_field) {
    flag |= PDF_SHOW_WIDGET;
  }

  if (!settings_.contain_content) {
    flag |= PDF_HIDE_CONTENT;
  }
  if (settings_.page_mode.type == print::kPageModeKindPages &&
      settings_.page_mode.pages->page_border    ) {
    flag |= PDF_DRAW_PAGE_BOX;
  }
  bool auto_rotate = settings_.page_mode.auto_rotate;
  auto mode_type = settings_.page_mode.type;
  bool auto_fit = false;
  bool custom_scale = false;
  int row = 1;
  int col = 1;
  print::PageModePagesOrder order = print::kPageModePagesOrderPortrait;

  if (mode_type == print::kPageModeKindSize) {
    auto_fit = settings_.page_mode.size->preset == print::kPageModeSizePresetFit;
    custom_scale = !auto_fit;
  } else if (mode_type == print::kPageModeKindPages) {
    row = settings_.page_mode.pages->vert_count;
    col = settings_.page_mode.pages->horiz_count;
    order = settings_.page_mode.pages->order;
    auto_fit = true;
  } else if (mode_type == print::kPageModeKindBooklet) {
    row = 1;
    col = 2;
//    auto_rotate = false;
    page_rotate_ = 0;

    if (settings_.page_mode.booklet->gutter_left_or_right) {
      order = print::kPageModePagesOrderPortraitReverse;
    }
    auto_fit = true;
  }
  int range = row * col;
  paper_width /= col;
  paper_height /= row;

  auto get_index = [](int i, int row, int col,
    print::PageModePagesOrder order, int& row_index, int& col_index) {
    switch (order) {
    case print::kPageModePagesOrderPortrait:
    {
      row_index = row - 1 - i / col;
      col_index = i % col;
      break;
    }
    case print::kPageModePagesOrderPortraitReverse:
    {
      row_index = row - 1 - i / col;
      col_index = col - 1 - i % col;
      break;
    }
    case print::kPageModePagesOrderLandscape:
    {
      row_index = row - 1 - i % row;
      col_index = i / row;
      break;
    }
    case print::kPageModePagesOrderLandscapeReverse:
    {
      row_index = row - 1 - i % row;
      col_index = col - 1 - i / row;
      break;
    }
    default:
      break;
    }
  };

  auto print_crop_line = [&](){
      {
        if(mode_type == print::kPageModeKindPages && settings_.page_mode.pages->crop_line){
            int dpi_x = GetDeviceCaps(print_hdc, LOGPIXELSX);
            int dpi_y = GetDeviceCaps(print_hdc, LOGPIXELSY);
            float dpi_scale_x = dpi_x / 72.f;
            float dpi_scale_y = dpi_y / 72.f;
            float dpi_scale = std::min(dpi_scale_x, dpi_scale_y);
            float paper_width = paper_size_.width();
            float paper_height = paper_size_.height();
            int width = paper_width * dpi_scale;
            int height = paper_height * dpi_scale;
            auto row_count = settings_.page_mode.pages->vert_count;
            auto col_count = settings_.page_mode.pages->horiz_count;
            qreal average_width = width*1.0 / col_count*1.0;
            qreal average_height = height*1.0 / row_count*1.0;
            HPEN pen = CreatePen(PS_DASH, 1, RGB(0,0,0));
            SelectObject(print_hdc, pen);  // 选择画笔

            int x1, y1, x2, y2;
            for (int row = 1; row < row_count; row++) {
                y1 = row * average_height;
                y2 = y1;
                x1 = 0;
                x2 = width;
                MoveToEx(print_hdc, x1, y1, nullptr);
                LineTo(print_hdc, x2, y2);
            }

            for (int col = 1; col < col_count; col++) {
                x1 = col * average_width;
                x2 = x1;
                y1 = 0;
                y2 = height;
                MoveToEx(print_hdc, x1, y1, nullptr);
                LineTo(print_hdc, x2, y2);
            }
            DeleteObject(pen);
        }
    }//crop_line
  };

  auto print_page = [&](int i, int order_id) -> bool {
    if (order_id % range == 0) {
      if (order_id != 0) {
        print_crop_line();
        EndPage(print_hdc);
      }
      StartPage(print_hdc);
    }
    Graphics* graphics = new Graphics(print_hdc);
    auto dpi_x = graphics->GetDpiX();
    auto dpi_y = graphics->GetDpiY();
    qDebug() << "DPI X:" << dpi_x << "DPI Y:" << dpi_y;
    float dpi_scale_x = dpi_x / 72.f;
    float dpi_scale_y = dpi_y / 72.f;
    float dpi_scale = std::min(dpi_scale_x, dpi_scale_y);
    int page_index = pages_.empty() ? i : pages_[i];
    auto pdf_page = document->pageManager()->pageAtIndex(page_index)->pdfPointer();

    if (pdf_page == nullptr) {
      return false;
    }
    float page_width = PDF_GetPageWidth(pdf_page);
    float page_height = PDF_GetPageHeight(pdf_page);
    float scale_portrait = std::min(paper_width / page_width, paper_height / page_height);
    float scale_landscape = std::min(paper_width / page_height, paper_height / page_width);
    page_rotate_ = 0;
    if (auto_rotate && scale_landscape > scale_portrait) {
      page_rotate_ = 3;
      std::swap(page_width, page_height);
    }
    float scale = 1.0f;

    if (auto_fit) {
      scale = page_rotate_ == 0 ? scale_portrait : scale_landscape;
    } else if (custom_scale) {
      scale = settings_.page_mode.size->scale;
    }
    if(scale>1.0f
        && settings_.page_mode.type == print::kPageModeKindSize
        && settings_.page_mode.size->auto_shrink
        ){
        scale = 1.0f;
    }
    int width = page_width * dpi_scale * scale;
    int height = page_height * dpi_scale * scale;
    int row_index = 0;
    int col_index = 0;
    get_index(order_id % range, row, col, order, row_index, col_index);
    float x = paper_width * col_index;
    float y = paper_height * (row - 1 - row_index);

    settings_.print_as_image = false;
    if (settings_.print_as_image) {
      PDF_BITMAP bitmap = PDFBitmap_Create(width, height, 1);
      PDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
      float offset_x = 0;
      float offset_y = 0;

      std::swap(offset_x, x);
      std::swap(offset_y, y);

      if (settings_.page_mode.auto_center) {
        x = x + (paper_width - page_width * scale) / 2.0f;

        if (x > 0) {
          offset_x = x;
          x = 0;
        }
        y = y + (paper_height - page_height * scale) / 2.0f;

        if (y > 0) {
          offset_y = y;
          y = 0;
        }
      } else {
        if (offset_x > 0) {
          offset_x += (paper_width / col * col_index);
        }

        if (offset_y > 0) {
          offset_y += (paper_height / row * (row - 1 - row_index));
        }
      }
      PDF_RenderPageBitmap(bitmap, pdf_page, nullptr, x * dpi_scale, y * dpi_scale,
        width, height, page_rotate_, flag);
      void *data = PDFBitmap_GetBuffer(bitmap);
      auto gdi_bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
      gdi_bitmap->SetResolution(dpi_x, dpi_y);
      Gdiplus::BitmapData datas;
      Gdiplus::Rect rc(0, 0, width, height);
      gdi_bitmap->LockBits(&rc, Gdiplus::ImageLockMode::ImageLockModeWrite,
        PixelFormat32bppARGB, &datas);
      memcpy(datas.Scan0, data, width * height * 4);
      gdi_bitmap->UnlockBits(&datas);
      graphics->DrawImage(gdi_bitmap, offset_x, offset_y);
      delete gdi_bitmap;
      PDFBitmap_Destroy(bitmap);
    } else {
      if (settings_.page_mode.auto_center) {
        x = x + (paper_width - page_width * scale) / 2.0f;
        y = y + (paper_height - page_height * scale) / 2.0f;
      }
      PDF_RenderPageHDC(reinterpret_cast<PDF_HDC>(print_hdc),
        pdf_page, nullptr, x * dpi_scale, y * dpi_scale,
        width, height, page_rotate_, flag);
    }
    delete graphics;

    if (order_id == page_count - 1) {
      print_crop_line();
      EndPage(print_hdc);
    }
    return true;
  };

  {
    auto getBookletStatus = [=](int page_count)->QPair<int,int>{
      QPair<int,int> booklet_status;
      int white_count = ((page_count-1) / 4 + 1) * 4 - page_count;
      int all_count = (page_count + white_count)/2;
      booklet_status.first = white_count;
      booklet_status.second = all_count;
      return booklet_status;
    };

    auto print_booklet = [=](int real_index,int cur_row,int cur_col){
        Graphics* graphics = new Graphics(print_hdc);
        auto dpi_x = graphics->GetDpiX();
        auto dpi_y = graphics->GetDpiY();
        qDebug() << "DPI X:" << dpi_x << "DPI Y:" << dpi_y;
        float dpi_scale_x = dpi_x / 72.f;
        float dpi_scale_y = dpi_y / 72.f;
        float dpi_scale = std::min(dpi_scale_x, dpi_scale_y);
        int page_index = pages_.empty() ? real_index : pages_[real_index];
        auto pdf_page = document->pageManager()->pageAtIndex(page_index)->pdfPointer();
        if (pdf_page == nullptr) {
          return false;
        }
        float page_width = PDF_GetPageWidth(pdf_page);
        float page_height = PDF_GetPageHeight(pdf_page);
        float scale_portrait = std::min(paper_width / page_width, paper_height / page_height);
        float scale_landscape = std::min(paper_width / page_height, paper_height / page_width);
        page_rotate_ = 0;
        if (auto_rotate && scale_landscape > scale_portrait) {
          page_rotate_ = 3;
          std::swap(page_width, page_height);
        }
        float scale = 1.0f;
        scale = page_rotate_ == 0 ? scale_portrait : scale_landscape;

        int width = page_width * dpi_scale * scale;
        int height = page_height * dpi_scale * scale;
        int row_index = cur_row;
        int col_index = cur_col;
        float x = paper_width * col_index;
        float y = paper_height * (row - 1 - row_index);

        if (settings_.page_mode.auto_center) {
          x = x + (paper_width - page_width * scale) / 2.0f;
          y = y + (paper_height - page_height * scale) / 2.0f;
        }
        PDF_RenderPageHDC(reinterpret_cast<PDF_HDC>(print_hdc),pdf_page,nullptr,
                          x * dpi_scale, y * dpi_scale,width, height,
                          page_rotate_,flag);
        delete graphics;
        return true;
    };

    if(settings_.page_mode.type == print::kPageModeKindBooklet){
       bool render_front = settings_.page_mode.booklet->subset == print::kPageModeBookletSubsetOdd;//正面
       bool render_back = settings_.page_mode.booklet->subset == print::kPageModeBookletSubsetEven;
       auto preview_start_page_ = 0;
       auto booklet_info = getBookletStatus(page_count);
       auto real_page_count = booklet_info.second;
       if(settings_.page_mode.booklet->subset != print::kPageModeBookletSubsetAll){
           real_page_count = qMax(1,booklet_info.second / 2);
       }
       for(int index = 0;index<real_page_count; preview_start_page_+=2,++index){
         StartPage(print_hdc);
          auto offset = render_front ? preview_start_page_ :
                        render_back  ? preview_start_page_+2 : 0;
          for(int count = 0;count<2;count++){ //right to left
            int real_index = (preview_start_page_+offset) /2;
            if(count == 1){
               real_index = ((booklet_info.second) / 2)*4 - real_index - 1;
            }
            bool white_page = real_index + 1 > page_count;
            if(!white_page){
              auto cur_col = ((preview_start_page_+offset) % 4 == 0) ? (1-count) : count;
              if(!settings_.page_mode.booklet->gutter_left_or_right){
                 cur_col = cur_col == 0 ? 1 : 0 ;
              }
              print_booklet(real_index, 0, cur_col); //real_index 下标
            }
         }
        EndPage(print_hdc);
      }
      EndDoc(print_hdc);
      return true;
    }
  }

  if (settings_.reverse_print) {
    for (int i = page_count - 1; i >= 0; --i) {
      if(print_process && print_process->Abort(print_process))
        break;
      print_page(i, page_count - 1 - i);
    }
  } else {
    for (int i = 0; i < page_count; ++i) {
      if(print_process && print_process->Abort(print_process))
        break;
      print_page(i, i);
    }
  }
  EndDoc(print_hdc);
  return true;
}

bool PdfPrint::PrintPdfDoc(PageEngine * page_engine, 
  const std::tuple<int, QPointF, double>& current_page_rect,
                           print::_Print_PROCESS* print_process) {
  if (!page_engine) {
    return false;
  }
  auto document = page_engine->dataCenter()->doc();

  if (!document) {
    return false;
  }
  InitGDIPrint();
  auto page_count = PDF_GetPageCount(document->pdfPointer());

  if (print_hdc_ == nullptr) {
    // Create a PRINTDLG structure, and initialize the appropriate fields.
    PRINTDLG printDlg;
    ZeroMemory(&printDlg, sizeof(printDlg));
    printDlg.lStructSize = sizeof(printDlg);
    printDlg.Flags = PD_RETURNDC;
    printDlg.nMinPage = 0;
    printDlg.nMaxPage = page_count - 1;
    printDlg.nFromPage = 0;
    printDlg.nToPage = page_count - 1;

    // Display a print dialog box.
    if (!PrintDlg(&printDlg)) {
      printf("Failure\n");
      return false;
    }
    print_hdc_ = printDlg.hDC;

    if (printDlg.hDevMode) {
      GlobalFree(printDlg.hDevMode);
    }

    if (printDlg.hDevNames) {
      GlobalFree(printDlg.hDevNames);
    }
  }
  auto file_path = page_engine->dataCenter()->getFilePath();
  auto title = QFileInfo(file_path).fileName().toStdWString();
  return PrintPdfDocWithHDC(print_hdc_, document, title,print_process);
}

void PdfPrint::InitGDIPrint() {
  if (init_gdi_) {
    return;
  }
  // Initialize GDI+.
  GdiplusStartupInput gdiplusStartupInput;
  GdiplusStartup((ULONG_PTR *)&gdiplus_token_, &gdiplusStartupInput, NULL);
  init_gdi_ = true;
}

void PdfPrint::ReleasePrintHDC() {
  if (print_hdc_) {
    DeleteDC((HDC)print_hdc_);
    print_hdc_ = nullptr;
  }
}

void PdfPrint::ReleaseGDIPlus() {
  if (gdiplus_token_ != 0) {
    GdiplusShutdown(gdiplus_token_);
    gdiplus_token_ = 0;
  }
}
