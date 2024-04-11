#include "pdf_print_dialog.h"
#include <QPainter>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QGridLayout>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QProcess>
#include <QtConcurrent>
#include <QDoubleSpinBox>
#include "../pageengine/pageengine.h"
#include "../pageengine/DataCenter.h"
#include "../pageengine/pdfcore/PDFDocument.h"
#include "../pageengine/pdfcore/PDFPage.h"
#include "../pageengine/pdfcore/PDFPageManager.h"
#include "spdf_transformpage.h"
#include "pdf_print.h"
#include "private/qprinter_p.h"
#include <QMouseEvent>
#include "./src/util/guiutil.h"
#include "./src/util/configutil.h"
#include "./src/style/stylesheethelper.h"
#include "./src/hometoolsview/batch/batchloadingwin.h"
#include "./utils/gautil.h"
#include "./src/util/gamanager.h"
#include "./src/basicwidget/messagewidget.h"
#include "ucombobox.h"
#include "umultiselectcombobox.h"

#define MaxWidth 40
#define MinWidth 20

#undef min
#undef max

bool ModifyDevMode(QPrinter& printer, std::function<bool(DEVMODE*)> proc) {
  auto private_data = reinterpret_cast<QWin32PrintEnginePrivate*>(
    QWin32PrintEnginePrivate::get(printer.paintEngine()));
  if (proc(private_data->devMode)) {
    if (DM_ORIENTATION & private_data->devMode->dmFields) {
      if (DMORIENT_LANDSCAPE == private_data->devMode->dmOrientation) {
        printer.setPageOrientation(QPageLayout::Orientation::Landscape);
      } else if (DMORIENT_PORTRAIT == private_data->devMode->dmOrientation) {
        printer.setPageOrientation(QPageLayout::Orientation::Portrait);
      }
    }
    return true;
  }
  return false;
}

void update_custom_edit_text(QLineEdit* edit,int start,int end) {
    edit->setText(QString("%1-%2").arg(start).arg(end));
}

void fapiao_print_ga(const QString& action,const QString& result) {
    QMap<QString,QVariant> gaMap;
    gaMap["action"] = action;
    gaMap["result"] = result;
    auto baseData = GaMgr->getBaseData();
    gaMap.insert(baseData);
    GATask::addTask("fapiao_print_done",gaMap);
}

QPair<int,int> getBookletStatus(int page_count){
    QPair<int,int> booklet_status;
    int white_count = ((page_count-1) / 4 + 1) * 4 - page_count;
    int all_count = (page_count + white_count)/2;
    booklet_status.first = white_count;
    booklet_status.second = all_count;
    return booklet_status;
}

bool Print(QPrinter& printer, PageEngine* page_engine,
  const std::tuple<int, QPointF, double>& current_page_rect,
  const std::vector<int>& pages, const print::PdfPrintSettings& settings) {
  auto private_data = reinterpret_cast<QWin32PrintEnginePrivate*>(
    QWin32PrintEnginePrivate::get(printer.paintEngine()));

  if (!private_data->devMode) {
    return false;
  }
  PdfPrint pdf_print;
  pdf_print.set_pages(pages);
  pdf_print.set_settings(settings); 

  {
      qreal l = 0.0, t = 0.0, r = 0.0, b = 0.0;
      printer.getPageMargins(&l, &t, &r, &b, QPrinter::Unit::Point);
      auto size = printer.paperSize(QPrinter::Unit::Point)  ;
      size.setWidth(size.width() - l - r);
      size.setHeight(size.height() - t - b);
      pdf_print.set_papersize(size);

//      printer.setResolution(96);
      qreal dpi_factor = /*printer.resolution()*/ 96 / 72.f;
      pdf_print.set_devicepixel(size * dpi_factor);
  }

  auto printer_name = printer.printerName().toStdWString();

  if (settings.printer_settings.duplex == print::kDuplexModeSingleSide) {
    private_data->devMode->dmDuplex = DMDUP_SIMPLEX;
  } else {
    if(settings.page_mode.type == print::kPageModeKindBooklet){
        private_data->devMode->dmDuplex = DMDUP_HORIZONTAL;
    }
    if(printer.duplex() == QPrinter::DuplexLongSide){
        private_data->devMode->dmDuplex = DMDUP_VERTICAL;
    } else if(printer.duplex() == QPrinter::DuplexShortSide){
        private_data->devMode->dmDuplex = DMDUP_HORIZONTAL;
    }
  }
  if(settings.page_mode.type == print::kPageModeKindPosters){
      private_data->devMode->dmDuplex = DMDUP_SIMPLEX;
  }

  qInfo() << "printer_engine_data = " << settings.printer_settings.duplex << printer.duplex() <<  private_data->devMode->dmDuplex <<
                                         settings.gray_print << printer.colorMode() << private_data->devMode->dmColor;

  if (!pdf_print.Load(printer_name, private_data->devMode)) {
    return false;
  }
  int ret = (settings.page_mode.type == print::kPageModeKindPosters) ? pdf_print.PrintPoster(page_engine) :
                                                                       pdf_print.PrintPdfDoc(page_engine,current_page_rect);
  if(ret && settings.page_mode.type == print::kPageModeKindPages &&
            settings.page_mode.pages->invoice)
  {
    fapiao_print_ga("pages_per_sheets",QString("%1x%2").arg(settings.page_mode.pages->horiz_count).
                                                        arg(settings.page_mode.pages->vert_count));
    fapiao_print_ga("orientation",settings.page_orientation == print::kPageOrientaionPortrait ? "portrait" : "landscape" );
    fapiao_print_ga("crop_line",settings.page_mode.pages->crop_line ? "yes" : "no");
  }
  return ret;
}

struct RenderOutParam {
  float x = 0.f;
  float y = 0.f;
  float width = 0.f;
  float height = 0.f;
};

bool RenderPage(PDFDocument* doc, int page_index, const PreviewOption& opt,
  QPixmap& pixmap, RenderOutParam& out_param) {
  auto page = doc->pageManager()->pageAtIndex(page_index)->pdfPointer();
  auto width = PDF_GetPageWidth(page);
  auto height = PDF_GetPageHeight(page); 
  float scale_portrait = std::min(float(opt.width_pt) / width, float(opt.height_pt) / height);
  float scale_landscape = std::min(float(opt.width_pt) / height, float(opt.height_pt) / width);
  float scale = scale_portrait;
  int rotate = 0;

  if (opt.auto_rotate && scale_landscape > scale_portrait) {
    rotate = 3;
    std::swap(width, height);
    scale = scale_landscape;
  } 
  float x = 0.f;
  float y = 0.f;
  float unit_x = float(opt.width) / float(opt.width_pt);
  float unit_y = float(opt.height) / float(opt.height_pt);
  float unit = std::min(unit_x, unit_y);

  if (opt.auto_fit) {
    if(opt.auto_shrink){ //缩小过大的页面
        auto old_width = width * opt.preview_vs_actual_scale;
        auto old_height = height * opt.preview_vs_actual_scale;
        if(old_width > opt.width_pt || old_height > opt.height_pt) { //过大缩小
            width *= scale;
            height *= scale;
        } else {  //原始
            width = old_width;
            height = old_height;
        }
    } else {  //适合
        width *= scale;
        height *= scale;
    }
  } else { //自定义比列、实际大小
    width *= opt.scale * opt.preview_vs_actual_scale;
    height *= opt.scale * opt.preview_vs_actual_scale;
  }
  width *= unit;
  height *= unit;

  if (opt.auto_center) {
    x = (opt.width - width) / 2.f;
    y = (opt.height - height) / 2.f;

    if (x > 0) {
      out_param.x = x;
      x = 0.f;
    }

    if (y > 0) {
      out_param.y = y;
      y = 0.f;
    }
  }
  out_param.width = width;
  out_param.height = height;
  x *= opt.zoom;
  y *= opt.zoom;
  width *= opt.zoom;
  height *= opt.zoom;
  PDF_BITMAP bitmap = PDFBitmap_Create(width, height, 1);
  PDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
  int flag = PDF_PRINTING;

  if (opt.gray_scale) {
    flag |= PDF_GRAYSCALE;
  }

  if (!opt.show_content) {
    flag |= PDF_HIDE_CONTENT;
  }

  if (opt.show_annot) {
    flag |= PDF_ANNOT;
  }

  if (opt.show_widget) {
    flag |= PDF_SHOW_WIDGET;
  }
  PDF_RenderPageBitmap(bitmap, page, nullptr, x, y, width, height, rotate, flag);
  void *data = PDFBitmap_GetBuffer(bitmap);
  QImage image((uchar*)data, width, height, QImage::Format_ARGB32);
  pixmap.convertFromImage(image);
  PDFBitmap_Destroy(bitmap);
  return true;
}

PdfPrintDialog::PdfPrintDialog(PageEngine* page_engine,
  QWidget* parent):CFramelessWidget(parent) {
  SetPageEngine(page_engine);
  setObjectName("PdfPrintDialog");  
  InitView();
  SyncHistoryInfo();
  setMouseTracking(true);
}

// 只同步printer_以及持久化
//    page_size_combo_;
//    orientationCombo;
//    auto_rotate_chk_;
//    auto_center_chk_;
//    print_as_image_chk_;
//    gray_print_chk_;
//    reverse_pages_chk_;
//    page_sets_combo
//    print_both_sides_chk
//    collate_check_box;
void PdfPrintDialog::OnPrinterChanged()
{
    auto order_text = orientationCombo->currentText();
    if( tr("Portrait") == order_text ){
        printer_.setPageOrientation(QPageLayout::Portrait);
    } else if ( tr("Landscape") == order_text ){
        printer_.setPageOrientation(QPageLayout::Landscape);
    }

    bool gray_print = gray_print_chk_->isChecked();
    printer_.setColorMode(gray_print ? QPrinter::GrayScale : QPrinter::Color);

    auto collate_state = collate_check_box->checkState();
    printer_.setCollateCopies(collate_state);

    SyncPageScaleStatus();
    SyncPosterPageStatus();
    SyncPageModeStatus(false);
    SyncOrientationStatus(true);
    SyncPageSetsStatus();

    print_config::insertSubConfig(HisPaperOrient,QString::number(orientationCombo->currentIndex()));
    print_config::insertSubConfig(HisAutoRotate,QString::number(auto_rotate_chk_->checkState()));
    print_config::insertSubConfig(HisAutoCenter,QString::number(auto_center_chk_->checkState()));
    print_config::insertSubConfig(HisAsImage,QString::number(print_as_image_chk_->checkState()));
    print_config::insertSubConfig(HisReverseprint,QString::number(reverse_pages_chk_->checkState()));
    print_config::insertSubConfig(HisColorgray,QString::number(gray_print_chk_->checkState()));
    print_config::insertSubConfig(HisCollate,QString::number(collate_state));

    printer_combo_->update();
    page_size_combo_->update();
}

void PdfPrintDialog::SyncHistoryInfo() {
    auto infoMap = print_config::readConfig(printer_.printerName());
    if(!infoMap.isEmpty()){
//        if(infoMap.contains(HisPaperSize)) {
//            auto paper_size = infoMap[HisPaperSize];
//            int index = 0;
//            int tmp = 0;
//            for(auto papersize : page_size_pair_.first){
//                if(papersize.name() == paper_size){
//                    index = tmp;
//                    page_size_combo_->setCurrentText(paper_size);
//                    break;
//                }
//                tmp++;
//            }
//            page_size_combo_->currentIndexChanged(index);
//        }
        if(infoMap.contains(HisPaperOrient)){
            auto oritenIndex = infoMap[HisPaperOrient].toInt();
            if(oritenIndex <orientationCombo->count()){
                orientationCombo->setCurrentIndex(qMax(0,oritenIndex));
            }
        }
        if(infoMap.contains(HisAutoRotate)){
            auto_rotate_chk_->setChecked(infoMap[HisAutoRotate].toInt());
        }
        if(infoMap.contains(HisAutoCenter)){
            auto_center_chk_->setChecked(infoMap[HisAutoCenter].toInt());
        }
        if(infoMap.contains(HisAsImage)){
            print_as_image_chk_->setChecked(infoMap[HisAsImage].toInt());
        }
        if(infoMap.contains(HisColorgray)){
            gray_print_chk_->setChecked(infoMap[HisColorgray].toInt());
        }
        if(infoMap.contains(HisReverseprint)){
            reverse_pages_chk_->setChecked(infoMap[HisReverseprint].toInt());
        }
        if(infoMap.contains(HisBothSide)){
            auto duplex = infoMap[HisBothSide].toInt();
            page_sets_combo->blockSignals(true);
            if(duplex == 2){
                page_sets_combo->setCurrentIndex(0);
            } else if(duplex == 3){
                page_sets_combo->setCurrentIndex(1);
            }
            page_sets_combo->blockSignals(false);
            print_both_sides_chk->setChecked(duplex == 2 || duplex == 3);
        }
//        if(infoMap.contains(HisCopyCount)) {
//            copies_spin_box_->setValue(infoMap[HisCopyCount].toInt());
//        }
        if(infoMap.contains(HisCollate)){
            collate_check_box->setChecked(infoMap[HisCollate].toInt());
        }
    }
    printer_combo_->update();
    page_size_combo_->update();
}

void PdfPrintDialog::InitView() {
  auto mainLayout = new QHBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->addWidget(CreateBkFrame());
  setLayout(mainLayout);
}

void PdfPrintDialog::SetPageEngine(PageEngine * page_engine) {
  page_engine_ = page_engine;
  page_count_ = page_engine_->pageCount();
}

void PdfPrintDialog::SetCurrentPageRect(const std::tuple<int, QPointF, double>& rect) {
  current_page_rect_ = rect;
}

////////
/// \brief CPreLabel::updateImage
/// \param image
///
void CPreLabel::addImage(const QImage& image,const QRectF& rect)
{
    mImageList.push_back({rect,image});
}
void CPreLabel::updateOpt(const PreviewOption& opt)
{
    mOpt = opt;
}
void CPreLabel::clearData()
{
    mImageList.clear();
}
void CPreLabel::paintEvent(QPaintEvent* parent)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform,true);
    if(mOpt.pre_poster){
        auto rect = mImageList.first().first;
        auto image = mImageList.first().second;
        painter.drawImage(rect,image,QRectF(0.0,0.0,image.width(),image.height()));
        QPen pen(QColor(0,0,0,100));
        pen.setWidthF(1);
        if(mOpt.posters_row >1 || mOpt.posters_col > 1){
          pen.setStyle(Qt::CustomDashLine);
          QVector<qreal> pattern = {4,4};
          pen.setDashPattern(pattern);
          pen.setWidthF(1);
          painter.setPen(pen);
          for(auto row = 1;row<mOpt.posters_row;row++){
              auto average_width = width()*1.0 / mOpt.posters_row*1.0;
              painter.drawLine(QLineF(QPointF(row*average_width,0),QPointF(row*average_width,height())));
          }
          for(auto col = 1;col<mOpt.posters_col;col++){
              auto average_height = height()*1.0 / mOpt.posters_col*1.0;
              painter.drawLine(QLineF(QPointF(0,col*average_height),QPointF(width(),col*average_height)));
          }
        }
    } else {
        for(auto pair : mImageList){
            painter.drawImage(pair.first,pair.second,QRectF(0.0,0.0, pair.second.width(),pair.second.height()));
            if(mOpt.show_border){
                QPen pen(QColor(0,0,0));
                pen.setWidthF(0.5);
                painter.setPen(pen);
                painter.drawRect(pair.first.x()+1, pair.first.y()+1, pair.first.width()-2, pair.first.height()-2);
            }
        }
    }
    QLabel::paintEvent(parent);
}
///////////

void PdfPrintDialog::PreView(QPrinter* printer)
{
    auto document = page_engine_->dataCenter()->doc();
    auto page_size_pt = printer->paperSize(QPrinter::Unit::Point);
    auto adjustSize = getFinalSize(page_size_pt.toSize());
    poster_pre_lab->setFixedSize(adjustSize);
    outer_lab->setFixedSize(adjustSize + QSize(2,2));
    PreviewOption opt;
    opt.gray_scale = printer->colorMode() == QPrinter::GrayScale;
    opt.auto_rotate = print_settings_.page_mode.auto_rotate;
    opt.show_annot = print_settings_.contain_annot;
    opt.show_content = print_settings_.contain_content;
    opt.show_widget = print_settings_.contain_form_field;
    opt.auto_center = print_settings_.page_mode.auto_center;
    opt.zoom = StyleSheetHelper::DpiScale();

    if (print_settings_.page_mode.type == print::kPageModeKindSize) {
        if (auto mode_size = print_settings_.page_mode.size) {
          opt.auto_fit = mode_size->preset == print::kPageModeSizePresetFit;
          opt.scale = mode_size->scale;
          opt.preview_vs_actual_scale = adjustSize.width() * 1.0 / page_size_pt.width();
          opt.auto_shrink = mode_size->auto_shrink;
        }
      } else if (print_settings_.page_mode.type == print::kPageModeKindPages) {
        opt.auto_fit = true;
        opt.show_border = print_settings_.page_mode.pages->page_border;
        opt.row = print_settings_.page_mode.pages->vert_count;
        opt.col = print_settings_.page_mode.pages->horiz_count;
        opt.order = print_settings_.page_mode.pages->order;
      } else if (print_settings_.page_mode.type == print::kPageModeKindBooklet) {
        opt.auto_fit = true;
        opt.row = 1;
        opt.col = 2;

        if (print_settings_.page_mode.booklet->gutter_left_or_right) {
          opt.order = print::kPageModePagesOrderPortraitReverse;
        }
    }
    opt.width = adjustSize.width() / opt.col;
    opt.height = adjustSize.height() / opt.row;
    opt.width_pt = adjustSize.width() / opt.col;
    opt.height_pt = adjustSize.height() / opt.row;

    poster_pre_lab->clearData();
    poster_pre_lab->clear();
    poster_pre_lab->updateOpt(opt);   

    auto render_page = [&](int i, int row_index, int col_index) {
        {
           if( page_indexes_.size() != 0 && int(page_indexes_.size())<=i){
               return;
           }
        }
        QPixmap pix;      
        int page_index = page_indexes_.empty() ? i : page_indexes_[i];
        RenderOutParam out_param;
        int x = opt.width * col_index;
        int y = opt.height * (opt.row - 1 - row_index);

        if (RenderPage(document, page_index, opt, pix, out_param)) {
          x = (x + out_param.x);
          y = (y + out_param.y);
          poster_pre_lab->addImage(pix.toImage(),QRectF(x,y,out_param.width, out_param.height));
        }

    };

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

    {
        if(print_settings_.page_mode.type == print::kPageModeKindBooklet){
            bool render_front = print_settings_.page_mode.booklet->subset == print::kPageModeBookletSubsetOdd;//正面
            bool render_back = print_settings_.page_mode.booklet->subset == print::kPageModeBookletSubsetEven;
            auto offset = render_front ? preview_start_page_ :
                          render_back  ? preview_start_page_+2 : 0;
            auto page_count = GetPageCount();
            auto booklet_info = getBookletStatus(page_count);
            for(int count = 0;count<2;count++){ //right to left
               int real_index = (preview_start_page_+offset) /2;
               if(count == 1){
                  real_index = ((booklet_info.second) / 2)*4 - real_index - 1;
               }
               bool white_page = real_index + 1 > page_count;
               if(!white_page){
                 auto front = ((preview_start_page_+offset) % 4 == 0);
                 auto cur_col = front ? (1-count) : count;
                 if(!print_settings_.page_mode.booklet->gutter_left_or_right){
                     cur_col = cur_col == 0 ? 1 : 0 ;
                 }
                 render_page(real_index,0, cur_col);  //real_index 下标
               }
            }
            return;
         }
      } // booklet preview

    bool revese_print = print_settings_.reverse_print;

    if (revese_print) {
        auto cur_count_ = page_index_line_edit_->text().toInt();
        auto count_ = page_count_label_->text().toInt();
        int start =  count_ - cur_count_;
        if(preview_range_page_>1){
          start = int(page_indexes_.size()) - 1 - ((cur_count_-1) * preview_range_page_);
        }
        for (int i = start; i > start - preview_range_page_  && i >= 0; --i) {
          int row_index = 0;
          int col_index = 0;
          get_index(start - i, opt.row, opt.col,
            opt.order, row_index, col_index);
          render_page(i, row_index, col_index);
        }
    } else {
        for (int i = preview_start_page_; i <
          preview_start_page_ + preview_range_page_; ++i) {
          int row_index = 0;
          int col_index = 0;
          get_index(i - preview_start_page_, opt.row,
            opt.col, opt.order, row_index, col_index);
          render_page(i, row_index, col_index);
        }
    }
    poster_pre_lab->update();
}

void PdfPrintDialog::PostersPreview(QPrinter * printer,QRectF& ret_rect)
{   
    auto document = page_engine_->dataCenter()->doc();
    auto paper_size = page_size_pair_.first[page_size_combo_->currentIndex()].size(QPageSize::Point);
    int last_width,last_height;
    if(orientationCombo->currentIndex() == 1){
        last_height = print_settings_.page_mode.posters->cur_col * paper_size.width();
        last_width = print_settings_.page_mode.posters->cur_row * paper_size.height();
    } else {
        last_height = print_settings_.page_mode.posters->cur_col * paper_size.height();
        last_width = print_settings_.page_mode.posters->cur_row * paper_size.width();
    }

    auto adjustSize = getFinalSize(QSize(last_width,last_height));
    poster_pre_lab->setFixedSize(adjustSize);
    outer_lab->setFixedSize(adjustSize + QSize(2,2));
    PreviewOption opt;
    opt.gray_scale = printer->colorMode() == QPrinter::GrayScale;
    opt.auto_rotate = print_settings_.page_mode.auto_rotate;
    opt.show_annot = print_settings_.contain_annot;
    opt.show_content = print_settings_.contain_content;
    opt.show_widget = print_settings_.contain_form_field;
    opt.auto_center = print_settings_.page_mode.auto_center;
    opt.posters_row = print_settings_.page_mode.posters->cur_row;
    opt.posters_col = print_settings_.page_mode.posters->cur_col;
    opt.zoom = StyleSheetHelper::DpiScale();
    opt.pre_poster = true;

    poster_pre_lab->clear();
    poster_pre_lab->clearData();
    poster_pre_lab->updateOpt(opt);

    auto size = poster_pre_lab->size();
    auto render_page = [&](int i) {
      poster_pre_lab->clear();
      int page_index = page_indexes_.empty() ? i : page_indexes_[i];
      auto page = document->pageManager()->pageAtIndex(page_index)->pdfPointer();
      RenderOutParam out_param;
      QPixmap pix;
      auto page_width = PDF_GetPageWidth(page);
      auto page_height = PDF_GetPageHeight(page);
      auto render_width = page_width * size.width() * print_settings_.page_mode.posters->tile_percent / last_width;
      auto render_height = page_height * size.height() * print_settings_.page_mode.posters->tile_percent / last_height;
      opt.width = render_width;
      opt.height = render_height;
      opt.width_pt = render_width;
      opt.height_pt = render_height;

      if (RenderPage(document, page_index, opt, pix, out_param)) {
        qreal x = ((size.width() - render_width)/2);
        qreal y = ((size.height() - render_height)/2);
        ret_rect = QRectF(x,y,render_width,render_height);
        poster_pre_lab->addImage(pix.toImage(),ret_rect);
      }
    };

    auto revese_print = print_settings_.reverse_print;
    if (revese_print) {
        auto cur_count_ = page_index_line_edit_->text().toInt();
        auto count_ = page_count_label_->text().toInt();
        int start =  count_ - cur_count_;
        if(preview_range_page_>1){
            start = int(page_indexes_.size()) - 1 - ((cur_count_-1) * preview_range_page_);
        }
      for (int i = start; i > start - preview_range_page_  && i >= 0; --i) {
        render_page(i);
      }
    } else {
      for (int i = preview_start_page_; i <
        preview_start_page_ + preview_range_page_; ++i) {
        render_page(i);
      }
    }   
}

void PdfPrintDialog::OnPreviewChanged() {
}

void PdfPrintDialog::OnPaintRequested(QPrinter * printer) {
  if(print_settings_.page_mode.type == print::kPageModeKindPosters){
      QRectF ret_rect;
      PostersPreview(printer,ret_rect);
  } else {
      PreView(printer);
  }
  preview_widget_->hide();
  poster_pre_lab->show();
  return;
}

void PdfPrintDialog::OnCopiesChanged(int count) {
  printer_.setCopyCount(count);
  print_config::insertSubConfig(HisCopyCount,QString::number(count));

  if(count<=1){
      copies_spin_box_->setProperty("EnableType", "Disabledown");
  }
  else if(count>=1000){
      copies_spin_box_->setProperty("EnableType", "Disableup");
  }
  else{
      copies_spin_box_->setProperty("EnableType", "Enable");
  }
  style()->unpolish(copies_spin_box_);
  style()->polish(copies_spin_box_);
  copies_spin_box_->update();
}

void PdfPrintDialog::OnScaleGroupButtonClicked(int id) {
    int preset = 0;
    switch (id) {
    case 0:
    case 3:
        break;
    case 1:
        preset = 1;
        print_settings_.page_mode.size->scale = 1.0f;
        break;
    case 2:
        preset = 2;
        OnCustomScaleChanged(custom_scale_spinbox_->value());
        break;
    }
  auto presetEnum = print::PageModeSizePreset(preset);
  print_settings_.page_mode.size->preset = presetEnum;
  print_settings_.page_mode.size->auto_shrink = id == 3;
  SyncPageScaleStatus();
  SyncPageModeStatus(true);
}

void PdfPrintDialog::OnCustomScaleChanged(int val) {
  print_settings_.page_mode.size->scale = float(val) / 100.f;
  SyncPageModeStatus(true);
  scaleLab->setText(tr("Scale:") + QString("%0%").arg(QString::number(val)));

  if(val<=1){
      custom_scale_spinbox_->setProperty("EnableType", "Disabledown");
  }
  else if(val>=600){
      custom_scale_spinbox_->setProperty("EnableType", "Disableup");
  }
  else{
      custom_scale_spinbox_->setProperty("EnableType", "Enable");
  }
  style()->unpolish(custom_scale_spinbox_);
  style()->polish(custom_scale_spinbox_);
  custom_scale_spinbox_->update();
  custom_scale_spinbox_->setFocus();
}

void PdfPrintDialog::OnCustomPagesVerticalChanged(int val) {
  print_settings_.page_mode.pages->vert_count = val;
  SyncPageModeStatus(true);
  SyncPageIndexStatus(false);
}

void PdfPrintDialog::OnCustomPagesHorizalChanged(int val) {
  print_settings_.page_mode.pages->horiz_count = val;
  SyncPageModeStatus(true);
  SyncPageIndexStatus(false);
}

QFrame * PdfPrintDialog::CreatePreviewFrame() {
  QFrame* frame = new QFrame(this);
  frame->setObjectName("previewFrame");
  frame->setFixedHeight(360);
  QVBoxLayout* layout = new QVBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(0);

  auto hlayout = new QHBoxLayout();
  hlayout->setSpacing(12);
  scaleLab = new QLabel(tr("Scale:") + "100%");
  scaleLab->setObjectName("scaleLab");
  hlayout->addWidget(CreateLineFrame(106,1));
  hlayout->addWidget(scaleLab);
  hlayout->addWidget(CreateLineFrame(106,1));

  preview_widget_ = new QPrintPreviewWidget(&printer_, this);
  GuiUtil::setWidgetRoundStyle(preview_widget_);
  preview_widget_->setObjectName("printPreviewWidget");
  connect(preview_widget_, &QPrintPreviewWidget::paintRequested,
    this, &PdfPrintDialog::OnPaintRequested);
  connect(preview_widget_, &QPrintPreviewWidget::previewChanged,
    this, &PdfPrintDialog::OnPreviewChanged);
  auto file_path = page_engine_->dataCenter()->getFilePath();

  outer_lab = new QLabel(this);
  outer_lab->setObjectName("outerLab");
  auto pre_layout = LayoutUtil::CreateHLayout(outer_lab);
  poster_pre_lab = new CPreLabel(this);  
  pre_layout->addWidget(poster_pre_lab,0,Qt::AlignCenter);

  layout->addLayout(hlayout);
  layout->addSpacing(16);
  layout->addWidget(preview_widget_,0,Qt::AlignHCenter);
  layout->addWidget(outer_lab,0,Qt::AlignHCenter);
  frame->setLayout(layout);
  return frame;
}

QStringList CreatePageSizeOption() {
  static QStringList kPageSizeOpts;

  if (kPageSizeOpts.isEmpty()) {
    kPageSizeOpts << "A4" << "B5" <<
      "Letter" << "Legal" <<
      "Executive" << "A0" <<
      "A1" << "A2" << "A3" <<
      "A5" << "A6" << "A7" <<
      "A8" << "A9" << "B0" <<
      "B1" << "B10" << "B2" <<
      "B3" << "B4" << "B6" <<
      "B7" << "B8" << "B9" <<
      "C5E" << "Comm10E" << "DLE" <<
      "Folio" << "Ledger" << "Tabloid" <<
      "Custom" << "A10" << "A3Extra" <<
      "A4Extra" << "A4Plus" << "A4Small" <<
      "A5Extra" << "B5Extra" << "JisB0" <<
      "JisB1" << "JisB2" << "JisB3" <<
      "JisB4" << "JisB5" << "JisB6" <<
      "JisB7" << "JisB8" << "JisB9" <<
      "JisB10" << "AnsiC" << "AnsiD" <<
      "AnsiE" << "LegalExtra" << "LetterExtra" <<
      "LetterPlus" << "LetterSmall" << "TabloidExtra" <<
      "ArchA" << "ArchB" << "ArchC" <<
      "ArchD" << "ArchE" << "Imperial7x9" <<
      "Imperial8x10" << "Imperial9x11" << "Imperial9x12" <<
      "Imperial10x11" << "Imperial10x13" << "Imperial10x14" <<
      "Imperial12x11" << "Imperial15x11" << "ExecutiveStandard" <<
      "Note" << "Quarto" << "Statement" <<
      "SuperA" << "SuperB" << "Postcard" <<
      "DoublePostcard" << "Prc16K" << "Prc32K" <<
      "Prc32KBig" << "FanFoldUS" << "FanFoldGerman" <<
      "FanFoldGermanLegal" << "EnvelopeB4" << "EnvelopeB5" <<
      "EnvelopeB6" << "EnvelopeC0" << "EnvelopeC1" <<
      "EnvelopeC2" << "EnvelopeC3" << "EnvelopeC4" <<
      "EnvelopeC6" << "EnvelopeC65" << "EnvelopeC7" <<
      "Envelope9" << "Envelope11" << "Envelope12" <<
      "Envelope14" << "EnvelopeMonarch" << "EnvelopePersonal" <<
      "EnvelopeChou3" << "EnvelopeChou4" << "EnvelopeInvite" <<
      "EnvelopeItalian" << "EnvelopeKaku2" << "EnvelopeKaku3" <<
      "EnvelopePrc1" << "EnvelopePrc2" << "EnvelopePrc3" <<
      "EnvelopePrc4" << "EnvelopePrc5" << "EnvelopePrc6" <<
      "EnvelopePrc7" << "EnvelopePrc8" << "EnvelopePrc9" <<
      "EnvelopePrc10" << "EnvelopeYou4" << "LastPageSize" <<
      "NPageSize" << "NPaperSize" << "AnsiA" <<
      "AnsiB" << "EnvelopeC5" << "EnvelopeDL" <<
      "Envelope10";
  }
  return kPageSizeOpts;
}

QFrame * PdfPrintDialog::CreateBkFrame() {
  bkFrameMain = new QFrame();
  bkFrameMain->setObjectName("bkFrame");

  auto bk_layout = new QVBoxLayout();
  bk_layout->setContentsMargins(16,10,16,24);
  bk_layout->setSpacing(0);
  bk_layout->addWidget(CreateTopFrame());
  bk_layout->addSpacing(14);
  bk_layout->addWidget(CreateLineFrame(618,1),0,Qt::AlignHCenter);
  bk_layout->addSpacing(22);
  bk_layout->addWidget(CreateBottomFrame());

  update_custom_edit_text(page_custom_edit,1,maxPageCount);
  QString text = page_custom_edit->text();
  page_range_combo_->currentTextChanged(tr("Custom"));
  SyncPageScaleStatus();
  SyncPageSizeComboStatus({false,QPageSize()});

  bkFrameMain->setLayout(bk_layout);
  return bkFrameMain;
}

QFrame* PdfPrintDialog::CreateTopFrame()
{
    QFrame* close_frame = new QFrame(this);
    close_frame->setObjectName("closeFrame");
    auto layout = new QHBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(60);
    auto file_path = page_engine_->dataCenter()->getFilePath();
    auto title = new QLabel(QFileInfo(file_path).fileName());
    title->setObjectName("titleLabel");
    QFontMetrics fontWidth(title->font());
    QString elideNote = fontWidth.elidedText(QFileInfo(file_path).fileName(), Qt::ElideRight,520);
    title->setText(elideNote);
    auto closeBtn = new QPushButton();
    closeBtn->setObjectName("closeBtn");
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
      print_config::writeConfig(printer_.printerName());
      closeWin(Rejected);
    });
    layout->addWidget(title,0,Qt::AlignLeft);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
    close_frame->setLayout(layout);
    return close_frame;
}

QFrame* PdfPrintDialog::CreateBottomFrame()
{
    auto bkFrame = new QFrame();
    bkFrame->setObjectName("bottomFrame");
    auto layout = new QHBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(CreateLeftSettingsFrame());
    layout->addSpacing(20);
    layout->addWidget(CreateRightSettingsFrame());
    bkFrame->setLayout(layout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreateLeftSettingsFrame()
{
    auto leftSetsFrame = new QFrame();
    auto layout = new QVBoxLayout();
    leftSetsFrame->setObjectName("leftSetsFrame");
    layout->setContentsMargins(20,16,19,16);
    layout->setSpacing(0);
    layout->addWidget(CreateLeftTopSetsFrame());
    layout->addSpacing(12);
    layout->addStretch();
    layout->addWidget(CreatePreviewFrame(),0,Qt::AlignHCenter);
    layout->addStretch();
    layout->addSpacing(12);
    layout->addWidget(CreateNavigateBtnFrame(), 0, Qt::AlignHCenter);
    leftSetsFrame->setLayout(layout);
    return leftSetsFrame;
}

QFrame* PdfPrintDialog::CreateLeftTopSetsFrame()
{
    auto leftTopFrame = new QFrame();
    auto vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    vlayout->setSpacing(16);

    auto hlayout = new QHBoxLayout();
    hlayout->setSpacing(0);
    hlayout->setMargin(0);

    auto layout = new QGridLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setVerticalSpacing(10);

    QStringList printContentlist;
    printContentlist << tr("Document") << tr("Comment") << tr("Form Fields");
    auto printContentCombo = new UMultiSelectComboBox(this);
    printContentCombo->setObjectName("PrintContentCombo");
    printContentCombo->addItemList(printContentlist);
    printContentCombo->setDefaultItems(printContentlist);
    connect(qobject_cast<UMultiSelectComboBox *>(printContentCombo), &UMultiSelectComboBox::hideView, this, [=]() {
        auto list = printContentCombo->getCheckList();
        print_settings_.contain_annot = false;
        print_settings_.contain_content = false;
        print_settings_.contain_form_field = false;
        if(list.contains(printContentlist[0])){
            print_settings_.contain_content = true;
        }
        if(list.contains(printContentlist[1])){
            print_settings_.contain_annot = true;
        }
        if(list.contains(printContentlist[2])){
            print_settings_.contain_form_field = true;
        }
        SyncPageContentStatus(true);
    });
//    //printContentCombo->setCurrentIndex(2);
    SyncPageContentStatus(false);

    auto paperSizeLab = new QLabel();
    paperSizeLab->setObjectName("paperSizeLab");
    QString paper_size_text = tr("Paper Size:");
    paperSizeLab->setText(DrawUtil::elidedTextWithTip(paperSizeLab,paperSizeLab->font(),paper_size_text,Qt::ElideRight,120));

    page_size_combo_ = new UComboBox();
    page_size_combo_->setObjectName("pageSizeCombo");

    auto orientationLab = new QLabel();
    orientationLab->setObjectName("orientationLab");
    QString orientation_text = tr("Orientation:");
    orientationLab->setText(DrawUtil::elidedTextWithTip(orientationLab,orientationLab->font(),orientation_text,Qt::ElideRight,120));

    QStringList orientationlist;
    orientationlist << tr("Portrait") << tr("Landscape");
    orientationCombo = new UComboBox(orientationlist);
    orientationCombo->setObjectName("orientationCombo");
    connect(orientationCombo,&QComboBox::currentTextChanged,this,[this](const QString &text){
        if( tr("Portrait") == text ){
            printer_.setPageOrientation(QPageLayout::Portrait);
            print_settings_.page_orientation = print::kPageOrientaionPortrait;
        }
        else if( tr("Landscape") == text ){
            printer_.setPageOrientation(QPageLayout::Landscape);
            print_settings_.page_orientation = print::kPageOrientationLandscape;
        }
        if(print_settings_.page_mode.type == print::kPageModeKindSize){
           print_settings_.page_mode.auto_rotate = false;
           auto_rotate_chk_->setChecked(false);
        }  //互斥
        SyncPageScaleStatus();
        SyncPosterPageStatus();
        SyncPageModeStatus(false);
        SyncOrientationStatus(true);
        print_config::insertSubConfig(HisPaperOrient,QString::number(orientationCombo->currentIndex()));
    });
    printer_.setPageOrientation(print_settings_.page_orientation == print::kPageOrientaionPortrait ? QPageLayout::Portrait : QPageLayout::Landscape);

    QString auto_rotate_text = tr("Auto Rotate");
    QString auto_center_text = tr("Auto Center");

    auto_rotate_chk_ = new QCheckBox(this);
    auto_rotate_chk_->setObjectName("autoRotateCheckBox");
    auto_rotate_chk_->setText(DrawUtil::elidedTextWithTip(auto_rotate_chk_,auto_rotate_chk_->font(),auto_rotate_text,Qt::ElideRight,140));
    auto_center_chk_ = new QCheckBox(this);
    auto_center_chk_->setObjectName("autoCenterCheckBox");
    auto_center_chk_->setText(DrawUtil::elidedTextWithTip(auto_center_chk_,auto_center_chk_->font(),auto_center_text,Qt::ElideRight,140));

    connect(auto_rotate_chk_, &QCheckBox::stateChanged, this, [this](int state) {
      print_settings_.page_mode.auto_rotate = state;
      SyncPageModeStatus(true);
      SyncPageScaleStatus();
      print_config::insertSubConfig(HisAutoRotate,QString::number(state));
    });

    connect(auto_center_chk_, &QCheckBox::stateChanged, this, [this](int state) {
      print_settings_.page_mode.auto_center = state;
      SyncPageModeStatus(true);
      print_config::insertSubConfig(HisAutoCenter,QString::number(state));
    });

    layout->addWidget(printContentCombo,0,0,1,2);
    layout->addWidget(paperSizeLab,1,0,1,1);
    layout->addWidget(page_size_combo_,1,1,1,1);
    layout->addWidget(orientationLab,2,0,1,1);
    layout->addWidget(orientationCombo,2,1,1,1);
    hlayout->addWidget(auto_rotate_chk_);
    hlayout->addStretch();
    hlayout->addWidget(auto_center_chk_);

    vlayout->addLayout(layout);
    vlayout->addLayout(hlayout);
    leftTopFrame->setLayout(vlayout);
//    leftTopFrame->setStyleSheet("background:red;");
    return leftTopFrame;
}

QFrame* PdfPrintDialog::CreateRightSettingsFrame()
{
    auto rightSetsFrame = new QFrame();
    rightSetsFrame->setObjectName("rightSetsFrame");
    auto layout = new QVBoxLayout();
    layout->setMargin(0);
    layout->setSpacing(15);
    layout->addWidget(CreatePrintModeAndCopies());
    layout->addWidget(CreateLineFrame(228,1),0,Qt::AlignHCenter);
    layout->addWidget(CreatePagestoPrint(),0,Qt::AlignHCenter);
    layout->addWidget(CreatePageSizingAndHanding(),0,Qt::AlignHCenter);
    rightSetsFrame->setLayout(layout);
    return rightSetsFrame;
}

QFrame* PdfPrintDialog::CreatePrintModeAndCopies()
{
    auto printModeCopies = new QFrame();
    printModeCopies->setObjectName("printCopiesFrame");
    auto vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    vlayout->setSpacing(8);

    /*********************/
    auto h1layout = new QHBoxLayout();
    h1layout->setMargin(0);
    auto printImgBtn = new QPushButton();
    printImgBtn->setObjectName("printImgBtn");
    QStringList printerNames = QPrinterInfo::availablePrinterNames();
    printerNames.sort();
    printer_combo_ = new UComboBox(printerNames);
    printer_combo_->setObjectName("printerCombo");
    printer_combo_->setCurrentText(printer_.printerName());
    connect(printer_combo_, &QComboBox::currentTextChanged,
      this, [this](const QString& text) {
      hide();
      auto loadPrinter = [this,text](){
          print_config::writeConfig(printer_.printerName());
          printer_.setPrinterName(text);
          SyncPageSizeComboStatus({true,page_size_pair_.first[page_size_combo_->currentIndex()]});
      };
      BatchLoadingWin loadingWidget(tr("Connecting to printer…"));
      QFuture<void> future = QtConcurrent::run(loadPrinter);
      QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
      connect(watcher, &QFutureWatcher<bool>::finished, this,[&](){
          loadingWidget.closeWin(CFramelessWidget::Accepted);
          watcher->deleteLater();
      });
      watcher->setFuture(future);
      loadingWidget.exec();
      show();
      SyncPageIndexStatus(true);
//      SyncHistoryInfo();
      OnPrinterChanged();
    });
    connect(page_size_combo_,static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
      printer_.setPageSize(page_size_pair_.first[index]);
      SyncPageSizeStatus(true);
      SyncPageScaleStatus();
      SyncPosterPageStatus();
      print_config::insertSubConfig(HisPaperSize,page_size_combo_->currentText());
    });    
    connect(printImgBtn,&QPushButton::clicked,this,[=](){
//        QProcess::startDetached(QString("rundll32.exe printui.dll,PrintUIEntry /e /n \"%1\"").arg(printer_.printerName()));
        if (auto paintEngine = printer_.paintEngine()) {
            if (auto enginePriv = QWin32PrintEnginePrivate::get(paintEngine)) {
                if (auto private_data = reinterpret_cast<QWin32PrintEnginePrivate*>(enginePriv)) {
//                    PdfPrint::PrintProperties(private_data->devMode);
                    HANDLE printer_handle;
                    auto model = LPDEVMODE(private_data->devMode);
                    auto str = printer_.printerName();
                    wchar_t* wstr = new wchar_t[str.length() + 1];
                    str.toWCharArray(wstr);
                    wstr[str.length()] = '\0';
                    printer_.printerName().toWCharArray(wstr);
                    auto ret = OpenPrinter(wstr, &printer_handle, 0);
                    ret = DocumentProperties(HWND(winId()), printer_handle, wstr,
                      model, model, DM_IN_PROMPT | DM_OUT_BUFFER | DM_IN_BUFFER);
                    ret = ClosePrinter(printer_handle);
                    if(ret){
                        SyncSystemProperties(model);
                    }
                    delete[] wstr;
                }
            }
        }

    });
    h1layout->addWidget(printer_combo_);
    h1layout->addWidget(printImgBtn);

    /*********************/
    auto hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->setSpacing(8);
    auto copiesLab = new QLabel();
    copiesLab->setObjectName("copiesLab");
    copiesLab->setText(tr("Copies:"));
    copies_spin_box_ = new QSpinBox(this);
    GuiUtil::setWidgetContextMenu(copies_spin_box_);
    copies_spin_box_->setObjectName("copiesSpinBox");
    copies_spin_box_->setRange(1, 1000);
    connect(copies_spin_box_, SIGNAL(valueChanged(int)), this, SLOT(OnCopiesChanged(int)));    
    collate_check_box = new QCheckBox(this);
    collate_check_box->setObjectName("collateCheckBox");
    collate_check_box->setText(tr("Collate"));
    connect(collate_check_box,&QCheckBox::stateChanged,this,[this](int state){
        printer_.setCollateCopies(state);
        print_config::insertSubConfig(HisCollate,QString::number(state));
    });
    SyncCopiesStatus();
    hlayout->addWidget(copiesLab,0,Qt::AlignVCenter);
    //hlayout->addSpacing(32);
    hlayout->addStretch();
    hlayout->addWidget(copies_spin_box_,0,Qt::AlignVCenter);
    OnCopiesChanged(1);
    hlayout->addWidget(collate_check_box,0,Qt::AlignVCenter);

    /*********************/
    auto v1layout = new QVBoxLayout();
    v1layout->setSpacing(8);
    v1layout->setMargin(0);
    print_as_image_chk_ = new QCheckBox(tr("Print As Image"), this);
    print_as_image_chk_->setObjectName("printAsImageCheckBox");
    v1layout->addWidget(print_as_image_chk_);
    connect(print_as_image_chk_, &QCheckBox::stateChanged, this, [this](int state) {
      print_settings_.print_as_image = state;
      SyncPageOptionStatus(false);
      print_config::insertSubConfig(HisAsImage,QString::number(state));
    });
    gray_print_chk_ = new QCheckBox(this);
    gray_print_chk_->setObjectName("grayPrintCheckBox");
    gray_print_chk_->setText(tr("Print in grayscale (black and white)"));
    printer_.setColorMode(print_settings_.gray_print ? QPrinter::GrayScale : QPrinter::Color);
    reverse_pages_chk_ = new QCheckBox(tr("Reverse Pages"), this);

    v1layout->addWidget(gray_print_chk_, Qt::AlignBottom);

    connect(gray_print_chk_, &QCheckBox::stateChanged, this, [this](int state) {
      printer_.setColorMode(state ? QPrinter::GrayScale : QPrinter::Color);
      SyncOrientationStatus(true);
      print_config::insertSubConfig(HisColorgray,QString::number(state));
    });
    gray_print_chk_->setChecked(true);
    SyncPageOptionStatus(false);

    vlayout->addLayout(h1layout);
    vlayout->addLayout(hlayout);
    vlayout->addLayout(v1layout);
    printModeCopies->setLayout(vlayout);
    return printModeCopies;
}

QFrame* PdfPrintDialog::CreatePagestoPrint()
{
    auto bkFrame = new QFrame();
    auto PagetoPrintFrame = new QFrame();
    PagetoPrintFrame->setObjectName("PagetoPrintFrame");
    auto vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    auto vclayout = new QVBoxLayout();
    vclayout->setMargin(10);
    vclayout->setSpacing(8);
    PagetoPrintFrame->setLayout(vclayout);

    //custom page
    page_custom_edit = new QLineEdit(this);
    page_custom_edit->setObjectName("pageCustomEdit");
    page_custom_edit->setTextMargins(6,0,6+16,0);
    page_custom_edit->setFixedSize(239,26);
    connect(page_custom_edit,&QLineEdit::textChanged,this,[this](const QString&){
        ApplySubset(print_settings_.page_range.subset);
        SyncPageIndexStatus(true);
        SyncPageModeStatus(false);
    });
    auto hlayout = LayoutUtil::CreateHLayout(page_custom_edit);
    hlayout->setContentsMargins(0,0,6,0);
    auto helpLab = new QLabel(page_custom_edit);
    helpLab->setObjectName("helpLab");
    helpLab->setCursor(Qt::ArrowCursor);
    helpLab->setFixedSize(16,16);
    helpLab->setToolTip(tr("Please enter the appropriate page range. (e.g. 1, 5, 8-12)"));
    hlayout->addWidget(helpLab,0,Qt::AlignRight);

    //Pages to Print selection
    auto titleLab = new QLabel(tr("Pages to Print"));
    titleLab->setObjectName("titleLabel");
    QStringList page_range_list;
    page_range_list << tr("All Pages") << tr("Current Page") <<
        tr("Custom") ;
    page_range_combo_ = new UComboBox(page_range_list);
    page_range_combo_->setObjectName("pageRangeCombo");
    connect(page_range_combo_, &QComboBox::currentTextChanged,
            this, [this, page_range_list](const QString& text) {
      print_settings_.page_range.preset = print::PageRangePreset(page_range_list.indexOf(text));
      page_indexes_.clear();
      print_settings_.page_range.pages.clear();

      if (print_settings_.page_range.preset == print::kPageRangePresetCurrentPage ||
        print_settings_.page_range.preset == print::kPageRangePresetCurrentView) {
        preview_start_page_ = 0;
        page_indexes_.push_back(std::get<0>(current_page_rect_));
        print_settings_.page_range.pages.push_back(std::get<0>(current_page_rect_));
        auto cur_page = std::get<0>(current_page_rect_);
        update_custom_edit_text(page_custom_edit,cur_page+1,cur_page+1);
      } else if(print_settings_.page_range.preset == print::kPageRangePresetAll){
          update_custom_edit_text(page_custom_edit,1,maxPageCount);
      }
      ApplySubset(print_settings_.page_range.subset);      
      SyncPageIndexStatus(false);
      SyncPageRangeStatus(true);
      SyncPageModeStatus(false);
    });

    //subset selection
    auto subsetLab = new QLabel(tr("Odd or Even Pages"));
    subsetLab->setObjectName("subsetLab");
    QStringList page_subset_list;
    page_subset_list << tr("All Pages in Range") <<
        tr("Odd Pages Only") <<
        tr("Even Pages Only");
    page_subset_combo_ = new UComboBox(page_subset_list);
    page_subset_combo_->setObjectName("pageSubsetCombo");
    connect(page_subset_combo_, &QComboBox::currentTextChanged,
      this, [this, page_subset_list](const QString& text) {
      print_settings_.page_range.subset = print::PageRangeSubset(page_subset_list.indexOf(text));
      ApplySubset(print_settings_.page_range.subset);
      SyncPageIndexStatus(false);
      SyncPageRangeStatus(true);
      first_btn_->clicked(true);
    });
    SyncPageRangeStatus(false);

    //reverse pages check
    //reverse_pages_chk_ = new QCheckBox(tr("Reverse Pages"), this);
    reverse_pages_chk_->setObjectName("reversePagesCheckBox");
    connect(reverse_pages_chk_, &QCheckBox::stateChanged, this, [this](int state) {
      print_settings_.reverse_print = state;
      SyncPageOptionStatus(true);
      print_config::insertSubConfig(HisReverseprint,QString::number(state));
    });
    SyncPageOptionStatus(false);

    vclayout->addWidget(page_range_combo_);
    vclayout->addSpacing(6);
    vclayout->addWidget(page_custom_edit);
    vclayout->addSpacing(8);
    vclayout->addWidget(subsetLab);
    vclayout->setSpacing(4);
    vclayout->addWidget(page_subset_combo_);
    vclayout->addSpacing(6);
    vclayout->addWidget(reverse_pages_chk_);

    vlayout->addWidget(titleLab);
    vlayout->addWidget(PagetoPrintFrame);

    bkFrame->setLayout(vlayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreatePageSizingAndHanding()
{
    auto bkFrame = new QFrame();
    auto vlayout = new QVBoxLayout();
    vlayout->setSpacing(8);
    vlayout->setMargin(0);

    auto PageHandingSizeFrame = CreatePageHandingSizeFrame();
    PageHandingSizeFrame->setObjectName("PageHandingSizeFrame");
    auto PageHandingPosterFrame = CreatePageHandingPosterFrame();
    PageHandingPosterFrame->setObjectName("PageHandingPosterFrame");
    auto PageHandingMultipleFrame = CreatePageHandingMultipleFrame();
    PageHandingMultipleFrame->setObjectName("PageHandingMultipleFrame");
    auto PageHandingBookletFrame = CreatePageHandingBookletFrame();
    PageHandingBookletFrame->setObjectName("PageHandingBookletFrame");

    auto PageHandingStackWin = new QStackedWidget();
    PageHandingStackWin->setFixedSize(260,118);
    PageHandingStackWin->addWidget(PageHandingSizeFrame);
    PageHandingStackWin->addWidget(PageHandingPosterFrame);
    PageHandingStackWin->addWidget(PageHandingMultipleFrame);
    PageHandingStackWin->addWidget(PageHandingBookletFrame);
    PageHandingStackWin->setCurrentWidget(PageHandingSizeFrame);

    //stackwidget selection
    auto hlayout = new QHBoxLayout();
    auto page_text = tr("Page Sizing and Handling");
    auto titleLab = new QLabel(this);
    titleLab->setObjectName("titleLabel");
    titleLab->setText(DrawUtil::elidedTextWithTip(titleLab,titleLab->font(),page_text,Qt::ElideRight,176));
    QStringList page_handing_list;
    page_handing_list << tr("Size") <<tr("Poster")<<tr("Multiple")<<tr("Booklet");
    page_handing_combo = new UComboBox(page_handing_list);
    page_handing_combo->setObjectName("PageHandingCombo");
    hlayout->addWidget(titleLab);
    hlayout->addWidget(page_handing_combo);
    connect(page_handing_combo,&QComboBox::currentTextChanged,this,[this,PageHandingStackWin,PageHandingSizeFrame,PageHandingPosterFrame,PageHandingMultipleFrame,PageHandingBookletFrame](const QString &text){
        if(tr("Size") == text){
            print_settings_.page_mode.Create(print::kPageModeKindSize);
            RestoreState();
            SyncOrientationStatus(false);
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
            PageHandingStackWin->setCurrentWidget(PageHandingSizeFrame);
        }
        else if(tr("Multiple") == text){
            print_settings_.page_mode.Create(print::kPageModeKindPages);
//            RestoreState();
            orientationCombo->currentTextChanged(orientationCombo->currentText());
            SyncOrientationStatus(false);
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
            PageHandingStackWin->setCurrentWidget(PageHandingMultipleFrame);
        }
        else if(tr("Booklet") == text){
            print_settings_.page_mode.Create(print::kPageModeKindBooklet);
//            RestoreState();
            SyncOrientationStatus(false);
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
            //SyncPrintSettingStatus();
            PageHandingStackWin->setCurrentWidget(PageHandingBookletFrame);
        }
        else if(tr("Poster") == text){
            print_settings_.page_mode.Create(print::kPageModeKindPosters);           
            orientationCombo->setCurrentIndex(1);
            SyncPageScaleStatus();
            SyncPosterPageStatus();
//            RestoreState();
            SyncOrientationStatus(false);
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
            PageHandingStackWin->setCurrentWidget(PageHandingPosterFrame);
        }
        SyncPageSetsStatus();
    });

    //Print on both sides of paper
    print_both_sides_chk = new QCheckBox(tr("Print on both sides of paper"));
    print_both_sides_chk->setObjectName("printbothsidesCheckBox");
    QStringList list;
    list << tr("Flip on long edge") << tr("Flip on short edge");
    page_sets_combo = new UComboBox(list);
    page_sets_combo->setObjectName("pageSets");

    connect(print_both_sides_chk, &QCheckBox::stateChanged,this, [=](int){
      if (print_both_sides_chk->isChecked()) {
        print_settings_.printer_settings.duplex = print::kDuplexModeDuplexAuto;
      } else {
        print_settings_.printer_settings.duplex = print::kDuplexModeSingleSide;
      }
      SyncPageSetsStatus();
      //SyncPrintSettingStatus();
    });

    connect(page_sets_combo,&QComboBox::currentTextChanged,this,[=](const QString&){
      SyncPageSetsStatus();     
    });
    SyncPageSetsStatus();

    //printBtn cancelBtn
    auto h1layout = new QHBoxLayout();
    h1layout->setMargin(0);
    h1layout->setSpacing(8);
    auto cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("cancelBtn");
    auto printBtn = new QPushButton(tr("Print"));
    printBtn->setObjectName("printBtn");
    h1layout->addWidget(printBtn);
    h1layout->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
      closeWin(Rejected);
      print_config::writeConfig(printer_.printerName());
    });
    connect(printBtn, &QPushButton::clicked, this, [this]() {
//      ConfigureUtil::writeHisToINI(PrintInfo,HisPaperSize,page_size_combo_->currentText(),ConfigUtil::UPDFHisPath());
      if(!CustomPages(page_custom_edit->text(),false)){
          MessageWidget win({tr("Please enter a valid page range.")},
                            {tr("OK"),"",""});
          win.resetIconImage(MessageWidget::IT_Delete);
          win.showBackground(false);
          win.exec();
          return;
      }
      print_config::writeConfig(printer_.printerName());
      hide();
      auto loadPrinter = [=](){
          Print(printer_, page_engine_, current_page_rect_, page_indexes_, print_settings_);
      };
      BatchLoadingWin loadingWidget(tr("Adding to the print queue…"));
      QFuture<void> future = QtConcurrent::run(loadPrinter);
      QFutureWatcher<void> *watcher = new QFutureWatcher<void>();
      connect(watcher, &QFutureWatcher<bool>::finished, this,[&](){
          loadingWidget.closeWin(CFramelessWidget::Accepted);
          watcher->deleteLater();
      });
      watcher->setFuture(future);
      if(loadingWidget.exec() == CFramelessWidget::Accepted){
        closeWin();
      }
    });

    vlayout->addLayout(hlayout);
    vlayout->addWidget(PageHandingStackWin);
    vlayout->addSpacing(3);
    vlayout->addWidget(print_both_sides_chk);
    vlayout->addWidget(page_sets_combo);
    vlayout->addSpacing(1);
    vlayout->addLayout(h1layout);
    vlayout->addStretch();

    bkFrame->setLayout(vlayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreatePageHandingSizeFrame()
{
    auto bkFrame = new QFrame();
    auto vclayout = new QVBoxLayout();
    vclayout->setContentsMargins(10,10,10,10);
    vclayout->setSpacing(8);
    auto hclayout = new QHBoxLayout();
    hclayout->setSpacing(24);
    hclayout->setMargin(0);
    fit_radio_btn_ = new QRadioButton(tr("Fit"), this);
    fit_radio_btn_->setObjectName("fitRatioButton");
    actual_size_radio_btn_ = new QRadioButton(tr("Actual Size"), this);
    actual_size_radio_btn_->setObjectName("actualSizeRadioButton");
    hclayout->addWidget(fit_radio_btn_);
    hclayout->addWidget(actual_size_radio_btn_);
    hclayout->addStretch();

    auto glayout = new QGridLayout();
    //glayout->setSpacing(88);
    glayout->setMargin(0);
    custom_scale_radio_btn_ = new QRadioButton(this);
    custom_scale_radio_btn_->setText(DrawUtil::elidedTextWithTip(custom_scale_radio_btn_,custom_scale_radio_btn_->font(),tr("Custom Scale (%):"),Qt::ElideRight,180));
    custom_scale_radio_btn_->setObjectName("customScaleRadioButton");
    custom_scale_radio_btn_->setFixedHeight(24);
    custom_scale_spinbox_ = new QSpinBox(this);
    custom_scale_spinbox_->setObjectName("customScaleSpinBox");
    custom_scale_spinbox_->setRange(1, 600);
    custom_scale_spinbox_->setValue(100);
    GuiUtil::setWidgetContextMenu(custom_scale_spinbox_);
    glayout->addWidget(custom_scale_radio_btn_,0,0,1,2);
    glayout->addWidget(custom_scale_spinbox_,0,2,1,1);
    connect(custom_scale_spinbox_, SIGNAL(valueChanged(int)),this, SLOT(OnCustomScaleChanged(int)));
    shrink_page_radio_btn_ = new QRadioButton(this);
    shrink_page_radio_btn_->setText(DrawUtil::elidedTextWithTip(shrink_page_radio_btn_,shrink_page_radio_btn_->font(),tr("Shrink oversized pages"),Qt::ElideRight,250));
    shrink_page_radio_btn_->setObjectName("shrinkPagesRadioBtn");

    scale_button_group_ = new QButtonGroup(this);
    scale_button_group_->addButton(fit_radio_btn_, 0);
    scale_button_group_->addButton(actual_size_radio_btn_, 1);
    scale_button_group_->addButton(custom_scale_radio_btn_, 2);
    scale_button_group_->addButton(shrink_page_radio_btn_,3);
    connect(scale_button_group_, SIGNAL(buttonClicked(int)), this, SLOT(OnScaleGroupButtonClicked(int)));

    choose_paper_source_btn = new QRadioButton(this);
    choose_paper_source_btn->setText(DrawUtil::elidedTextWithTip(choose_paper_source_btn,choose_paper_source_btn->font(),tr("Choose paper source by PDF page size"),Qt::ElideRight,250));
    choose_paper_source_btn->setObjectName("choosePaperRadioBtn");
    connect(choose_paper_source_btn,&QRadioButton::clicked,this,[=](bool checked){
        page_size_combo_->setEnabled(!checked);
        SyncPageScaleStatus();
    });

    vclayout->addLayout(hclayout);
    vclayout->addLayout(glayout);
    vclayout->addSpacing(4);
    vclayout->addWidget(shrink_page_radio_btn_);
    vclayout->addSpacing(2);
    vclayout->addWidget(choose_paper_source_btn);
    bkFrame->setLayout(vclayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreatePageHandingPosterFrame()
{
    auto bkFrame = new QFrame();   
    auto vlayout = new QVBoxLayout();
    vlayout->setSpacing(0);
    vlayout->setMargin(10);

    auto scaleFrame = new QFrame();
    auto scalelayout = qobject_cast<QHBoxLayout*>(LayoutUtil::CreateLayout(Qt::Horizontal));
    scaleFrame->setLayout(scalelayout);
    auto label = new QLabel(this);
    label->setObjectName("percentageOfTilesLabel");
    label->setText(tr("Tile Scale (%):"));
    percentage_of_tile_spinbox_ = new QSpinBox(this);
    percentage_of_tile_spinbox_->setObjectName("percentageOfTilesSpinBox");
    percentage_of_tile_spinbox_->setRange(1, 600);
    percentage_of_tile_spinbox_->setValue(100);
    scalelayout->addWidget(label);
    scalelayout->addStretch();
    scalelayout->addWidget(percentage_of_tile_spinbox_);
    connect(percentage_of_tile_spinbox_,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,[=](int value){
        print_settings_.page_mode.posters->tile_percent = float(value / 100.0);
        SyncPageScaleStatus();
        SyncPosterPageStatus();
    });
    auto overlapFrame = new QFrame();
    auto overlaplayout = qobject_cast<QHBoxLayout*>(LayoutUtil::CreateLayout(Qt::Horizontal));
    overlapFrame->setLayout(overlaplayout);
    auto label1 = new QLabel(this);
    label1->setObjectName("overlayLabel");
    label1->setText(tr("Overlap (in):"));
    overlap_spinbox_ = new QDoubleSpinBox(this);
    overlap_spinbox_->setObjectName("overlaySpinBox");
    overlap_spinbox_->setRange(0, 5.08);
    overlap_spinbox_->setValue(0);
    overlaplayout->addWidget(label1);
    overlaplayout->addStretch();
    overlaplayout->addWidget(overlap_spinbox_);
    connect(overlap_spinbox_,static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),this,[=](double value){
        print_settings_.page_mode.posters->overlap = value;
    });

    auto checklayout = qobject_cast<QHBoxLayout*>(LayoutUtil::CreateLayout(Qt::Horizontal,0,20));
    checklayout->setContentsMargins(0,4,0,0);
    auto labelsCheckBox = new QCheckBox(tr("Labels"));
    labelsCheckBox->setObjectName("labelsCheckBox");
    auto cutmarksCheckBox = new QCheckBox(tr("Cut Marks"));
    cutmarksCheckBox->setObjectName("cutmarksCheckBox");
//    auto onlylargepagebox = new QCheckBox(tr("Tile only large pages"));
//    onlylargepagebox->setObjectName("onlylargepagebox");
    checklayout->addWidget(labelsCheckBox);
    checklayout->addWidget(cutmarksCheckBox);
    checklayout->addStretch();
    connect(labelsCheckBox,&QCheckBox::stateChanged,this,[=](int state){
        print_settings_.page_mode.posters->show_tag = state;
    });
    connect(cutmarksCheckBox,&QCheckBox::stateChanged,this,[=](int state){
        print_settings_.page_mode.posters->show_crop = state;
    });

    vlayout->addWidget(scaleFrame);
    vlayout->addWidget(overlapFrame);
    vlayout->addLayout(checklayout);
    bkFrame->setLayout(vlayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreatePageHandingMultipleFrame()
{
    auto bkFrame = new QFrame();
    auto vlayout = new QVBoxLayout();
    vlayout->setSpacing(8);
    vlayout->setContentsMargins(10,15,10,16);

    auto hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    hlayout->setSpacing(8);
    auto printPagesLab = new QLabel(tr("Pages per Sheet:"));
    printPagesLab->setObjectName("printPagesLab");
    QStringList list;
    for(int i = 1; i <5 ; i++){
    list.append(QString::number(i));
    }
    printPagesComboL = new UComboBox(list);
    printPagesComboL->setObjectName("printPagesComboL");
    printPagesComboR = new UComboBox(list);
    printPagesComboR->setObjectName("printPagesComboR");
    auto lineEditL = printPagesComboL->getLineEdit();
    auto lineEditR = printPagesComboR->getLineEdit();
    QRegExp regExp("^[1-9][0-0]$");
    lineEditL->setValidator(new QRegExpValidator(regExp));
    lineEditR->setValidator(new QRegExpValidator(regExp));
    lineEditL->setObjectName("PdfPrintDialog_lineEditL");
    lineEditL->setStyleSheet("border:hidden");
    lineEditR->setObjectName("PdfPrintDialog_lineEditR");
    lineEditR->setStyleSheet("border:hidden");
    printPagesComboL->setEditable(true);
    printPagesComboR->setEditable(true);
    printPagesComboR->setCurrentText("2");

    printPagesComboL->setContextMenuPolicy(Qt::NoContextMenu);
    printPagesComboR->setContextMenuPolicy(Qt::NoContextMenu);
    auto multiplyLab = new QLabel("x");
    hlayout->addWidget(printPagesLab);
    //hlayout->addSpacing(21);
    hlayout->addStretch();
    hlayout->addWidget(printPagesComboL);
    hlayout->addWidget(multiplyLab,0,Qt::AlignVCenter);
    hlayout->addWidget(printPagesComboR);   
    connect(printPagesComboL,&QComboBox::currentTextChanged,this,[this](const QString &text){
        auto hcount = text.toInt();
        if(hcount>0 && hcount<=10){
            print_settings_.page_mode.pages->pages = -1;
            print_settings_.page_mode.pages->horiz_count = text.toInt();
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
        }
    });
    connect(printPagesComboR,&QComboBox::currentTextChanged,this,[this](const QString &text){
        auto vcount = text.toInt();
        if(vcount>0 && vcount<=10){
            print_settings_.page_mode.pages->pages = -1;
            print_settings_.page_mode.pages->vert_count = text.toInt();
            SyncPageModeStatus(true);
            SyncPageIndexStatus(false);
        }
    });

    auto h1layout = new QHBoxLayout();
    auto pageOrderLab = new QLabel(tr("Page Order:"));
    pageOrderLab->setObjectName("pageOrderLab");
    QStringList opt;
    opt << tr("Horizontal") << tr("Horizontal Reversed")<<
        tr("Vertical") << tr("Vertical Reversed");
    page_order_combo_ = new UComboBox(opt);
    page_order_combo_->setObjectName("pageOrderCombo");
    connect(page_order_combo_, &QComboBox::currentTextChanged,
      this, [this, opt](const QString& text) {
      print_settings_.page_mode.pages->order =
        print::PageModePagesOrder(opt.indexOf(text));
      SyncPageModeStatus(true);
    });
    h1layout->addWidget(pageOrderLab);
    h1layout->addStretch();
    h1layout->addWidget(page_order_combo_);

    auto lineLayout = new QHBoxLayout();
    lineLayout->setContentsMargins(0,0,2,0);
    lineLayout->setSpacing(10);
    auto printPageBorderCheckBox = new QCheckBox(this);
    printPageBorderCheckBox->setText(DrawUtil::elidedTextWithTip(printPageBorderCheckBox,printPageBorderCheckBox->font(),tr("Print page border"),Qt::ElideRight,120));
    printPageBorderCheckBox->setObjectName("printPageBorderCheckBox");
    connect(printPageBorderCheckBox,&QCheckBox::stateChanged,this,[=](int state){
//        print_as_image_chk_->setEnabled(state);
        print_settings_.page_mode.pages->page_border = state;
        UpdatePreview();
    });
    auto printCropCheckBox = new QCheckBox(tr("Print crop line"));
    printCropCheckBox->setObjectName("printCropCheckBox");
    connect(printCropCheckBox,&QCheckBox::stateChanged,this,[this](int state){
        print_settings_.page_mode.pages->crop_line = state;
        UpdatePreview();
    });
    lineLayout->addWidget(printPageBorderCheckBox);
    lineLayout->addStretch();
    lineLayout->addWidget(printCropCheckBox);

    vlayout->addLayout(hlayout);
    vlayout->addSpacing(3);
    vlayout->addLayout(h1layout);
    vlayout->addSpacing(8);
    vlayout->addLayout(lineLayout);

    bkFrame->setLayout(vlayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreatePageHandingBookletFrame()
{
    auto bkFrame = new QFrame();
    auto glayout = new QGridLayout();
    glayout->setHorizontalSpacing(8);
    glayout->setVerticalSpacing(10);
    glayout->setContentsMargins(10,15,10,16);

    auto bookletsubsetLab = new QLabel(this);
    bookletsubsetLab->setText(DrawUtil::elidedTextWithTip(bookletsubsetLab,bookletsubsetLab->font(),tr("Booklet Subset:"),Qt::ElideRight,120));
    bookletsubsetLab->setObjectName("bookletSubsetLab");
    QStringList opt;
    opt << tr("Both Sides") << tr("Front Side Only") << tr("Back Side Only");
    booklet_subset_combo_ = new UComboBox(opt);
    booklet_subset_combo_->setObjectName("bookletSubsetComboBox");
    connect(booklet_subset_combo_, &QComboBox::currentTextChanged,
      this, [this, opt](const QString& text) {
      int index = opt.indexOf(text);
      print_settings_.page_mode.booklet->subset = print::PageModeBookletSubset(index);
      SyncPageModeStatus(true);
      SyncPageIndexStatus(false);
      first_btn_->clicked();
      //SyncPrintSettingStatus();
    });

    auto sheetsLab = new QLabel(tr("Sheets from:"));
    sheetsLab->setObjectName("sheetsLab");
    auto hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    hlayout->setSpacing(4);

    auto page_count = GetPageCount();
    booklet_page_start_spinbox_ = new QSpinBox(this);
    booklet_page_start_spinbox_->setObjectName("bookletPageFromSpinBox");
    booklet_page_start_spinbox_->setRange(1, page_count);
    booklet_page_start_spinbox_->setValue(1);
    //auto line = CreateLineFrame(6,1);
    auto lineFrame = new QFrame();
    lineFrame->setStyleSheet("background:#353535;max-width:8px;min-width:8px;max-height:1px;min-height:1px;");
    auto label1 = new QLabel("一");
    label1->setObjectName("formToLab");
    label1->setAlignment(Qt::AlignCenter);
    booklet_page_end_spinbox_ = new QSpinBox(this);
    booklet_page_end_spinbox_->setObjectName("bookletPageToSpinBox");
    booklet_page_end_spinbox_->setRange(1, page_count);
    booklet_page_end_spinbox_->setValue(page_count);
    GuiUtil::setWidgetContextMenu(booklet_page_end_spinbox_);

    hlayout->addWidget(booklet_page_start_spinbox_);
    //hlayout->addWidget(lineFrame,0);
    hlayout->addWidget(label1);
    hlayout->addWidget(booklet_page_end_spinbox_);

    auto label = new QLabel(tr("Binding:"), this);
    label->setObjectName("bookletGutterLabel");
    QStringList opt1;
    opt1 << tr("Left") << tr("Right");
    booklet_gutter_combo_ = new UComboBox(opt1);
    booklet_gutter_combo_->setObjectName("bookletGutterComboBox");
    connect(booklet_gutter_combo_, &QComboBox::currentTextChanged,
      this, [this, opt1](const QString& text) {
      print_settings_.page_mode.booklet->gutter_left_or_right = (opt1.indexOf(text) == 0);
      SyncPageModeStatus(true);
    });

    glayout->addWidget(bookletsubsetLab,0,0,1,1);
    glayout->addWidget(booklet_subset_combo_,0,1,1,1);
    glayout->addWidget(sheetsLab,1,0,1,1);
    glayout->addLayout(hlayout,1,1,1,1);
    glayout->addWidget(label,2,0,1,1);
    glayout->addWidget(booklet_gutter_combo_,2,1,1,1);
    bkFrame->setLayout(glayout);
    return bkFrame;
}

QFrame* PdfPrintDialog::CreateLineFrame(const int width,const int height)
{
    auto lineFrame = new QFrame();
    lineFrame->setObjectName("lineFrame");
    lineFrame->setFixedSize(width,height);
    return lineFrame;
}

int GetPagesPresetIndex(int count) {
  if (count == 2) {
    return 0;
  } else if (count == 4) {
    return 1;
  } else if (count == 6) {
    return 2;
  } else if (count == 9) {
    return 3;
  } else if (count == 16) {
    return 4;
  }
  return 5;
}

int GetPagesPresetFromIndex(int index) {
  if (index == 0) {
    return 2;
  } else if (index == 1) {
    return 4;
  } else if (index == 2) {
    return 6;
  } else if (index == 3) {
    return 9;
  } else if (index == 4) {
    return 16;
  }
  return -1;
}

QFrame * PdfPrintDialog::CreateNavigateBtnFrame() {
  QFrame* frame = new QFrame(this);
  frame->setObjectName("navigateFrame");
  QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(frame);
  shadow->setOffset(0, 0);
  shadow->setColor(QColor(0,0,0,60));
  shadow->setBlurRadius(6);
  frame->setGraphicsEffect(shadow);
  auto layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(6);
  frame->setLayout(layout);
  prev_btn_ = new QPushButton();
  next_btn_ = new QPushButton();
  first_btn_ = new QPushButton();
  last_btn_ = new QPushButton();
  page_index_line_edit_ = new LineEdit();
  QRegExp rx("[0-9]+");
  page_index_line_edit_->setValidator(new QRegExpValidator(rx));
  page_index_line_edit_->setObjectName("pageIndexLineEdit");
  page_count_label_ = new QLabel();
  page_count_label_->setObjectName("pageCountLab");
  page_index_line_edit_->setAlignment(Qt::AlignCenter);
  GuiUtil::setWidgetContextMenu(page_index_line_edit_);
  QLabel* lineLabel = new QLabel("/");
  lineLabel->setFixedWidth(10);
  lineLabel->setObjectName("lineLabel");
  layout->addStretch();

  // Fist
  first_btn_->setObjectName("firstBtn");
  connect(first_btn_, &QPushButton::clicked, this, [this]() {
    preview_start_page_ = 0;
    SyncPageIndexStatus(true);
    SyncPageScaleStatus();
  });
  layout->addWidget(first_btn_);
  // Prev
  prev_btn_->setObjectName("prevBtn");
  connect(prev_btn_, &QPushButton::clicked, this, [this]() {
    preview_start_page_ = (preview_start_page_ / preview_range_page_ - 1) * preview_range_page_;
    SyncPageIndexStatus(true);
    SyncPageScaleStatus();
  });

  auto lineEditLayout = new QHBoxLayout();
  lineEditLayout->setMargin(0);
  lineEditLayout->addWidget(page_index_line_edit_);

  auto countFrame = new QFrame();
  countFrame->setObjectName("countFrame");
  auto hlayout = new QHBoxLayout();
  hlayout->setContentsMargins(0,5,0,5);
  hlayout->setSpacing(0);
  //hlayout->addWidget(page_index_line_edit_);
  hlayout->addLayout(lineEditLayout);
  hlayout->addWidget(lineLabel);
  hlayout->addWidget(page_count_label_);
  countFrame->setLayout(hlayout);
  layout->addWidget(prev_btn_);
  layout->addWidget(countFrame,0,Qt::AlignHCenter);
//  layout->addWidget(page_index_line_edit_);
//  layout->addWidget(page_count_label_);
  maxPageCount =  GetPageCount();
  connect(page_index_line_edit_, &QLineEdit::returnPressed, this, [this]() {
    if(page_index_line_edit_->text().toInt()<1){
        preview_start_page_ = 0;
    }
    else if(page_index_line_edit_->text().toInt()>maxPageCount){
        preview_start_page_ = maxPageCount-1;
    }
    else{
        preview_start_page_ = (page_index_line_edit_->text().toInt() - 1) / preview_range_page_;
    }
    SyncPageIndexStatus(true);
    SyncPageScaleStatus();
  });
  connect(page_index_line_edit_,&QLineEdit::textChanged,this,[this](){
      page_index_line_edit_->setAlignment(Qt::AlignRight);
      int width = (DrawUtil::textWidth(font(),page_index_line_edit_->text())+20);
      page_index_line_edit_->setFixedWidth(width>MaxWidth?MaxWidth:width);
  });
  // Next
  next_btn_->setObjectName("nextBtn");
  connect(next_btn_, &QPushButton::clicked, this, [this]() {
    preview_start_page_ = (preview_start_page_ / preview_range_page_ + 1) * preview_range_page_;
    SyncPageIndexStatus(true);
    SyncPageScaleStatus();
  });
  layout->addWidget(next_btn_);
  // Last
  last_btn_->setObjectName("lastBtn");
  connect(last_btn_, &QPushButton::clicked, this, [this]() {
    preview_start_page_ = (GetPageCount() - 1) / preview_range_page_ * preview_range_page_;
    SyncPageIndexStatus(true);
    SyncPageScaleStatus();
  });

  layout->addWidget(last_btn_);
  layout->addStretch();
  SyncPageIndexStatus(false);

  if(StyleSheetHelper::WinDpiScale() > 1){
      lineEditLayout->setContentsMargins(0,0,0,0);
      hlayout->setContentsMargins(0,0,0,0);
      page_count_label_->setStyleSheet("margin-bottom: 1px;");
      lineLabel->setStyleSheet("margin-bottom: 2px;");
  }
  else{
      lineEditLayout->setContentsMargins(0,0,0,0);
      hlayout->setContentsMargins(0,0,0,1);
      page_count_label_->setStyleSheet("margin-bottom: 1px;");
      lineLabel->setStyleSheet("margin-bottom: 2px;");
  }

  return frame;
}

int PdfPrintDialog::GetPageCount() {
  if (page_indexes_.empty()) {
    return page_count_;
  }
  return (int)page_indexes_.size();
}

void PdfPrintDialog::ApplySubset(print::PageRangeSubset subset) {
  QString text = page_custom_edit->text();
  CustomPages(text);
  if(print_settings_.page_range.pages.size() == 1 ||
     print_settings_.page_range.pages.size() == 0)
  {
    subset = print::kPageRangeSubsetAll;
  }
  if (subset == print::kPageRangeSubsetAll) {
    page_indexes_ = print_settings_.page_range.pages;
  } else if (subset == print::kPageRangeSubsetEven) { // 偶数页
    page_indexes_.clear();
    if (print_settings_.page_range.pages.empty()) {
      for (int i = 1; i < page_count_; i += 2) {
        page_indexes_.push_back(i);
      }
    } else {
      for (auto index : print_settings_.page_range.pages) {
        if (index % 2 == 1) {
          page_indexes_.push_back(index);
        }
      }
    }
  } else if (subset == print::kPageRangeSubsetOdd) { // 奇数页
    page_indexes_.clear();
    if (print_settings_.page_range.pages.empty()) {
      for (int i = 0; i < page_count_; i += 2) {
        page_indexes_.push_back(i);
      }
    } else {
      for (auto index : print_settings_.page_range.pages) {
        if (index % 2 == 0) {
          page_indexes_.push_back(index);
        }
      }
    }
  }
  SyncPosterPageStatus();
}

bool PdfPrintDialog::CustomPages(const QString & str,bool change) {
  if(change){
    page_indexes_.clear();
    print_settings_.page_range.pages.clear();
  }
  std::vector<int> indexes;
  QString custom_str = str;
  custom_str.replace("，",",").replace("、",",");
  auto list = custom_str.split(',');
  bool ok = true;
  for (auto item : list) {
    if (item.contains('-')) {
      auto sub_list = item.split('-');

      if (sub_list.size() != 2) {
        return false;
      }
      int start = sub_list[0].trimmed().toInt(&ok);

      if (!ok) {
        return false;
      }
      int end = sub_list[1].trimmed().toInt(&ok);

      if (!ok) {
        return false;
      }
      if(std::max(start,end) > page_count_ || std::min(start,end) <= 0){
          return false;
      }
      if(start <= end) {
        for (int i = start; i <= end; ++i) {
          indexes.push_back(i- 1);
        }
      } else {
        for(int i = start;i >= end;--i){
          indexes.push_back(i-1);
        }
      }

    } else {
      auto index = item.trimmed().toInt(&ok);
      if(index < 1 || index > page_count_){
          return false;
      }
      indexes.push_back(item.trimmed().toInt(&ok) - 1);
    }

    if (!ok) {
      return false;
    }
  }

  if(change) {
    std::copy(indexes.begin(), indexes.end(), std::back_inserter(page_indexes_));
    std::copy(indexes.begin(), indexes.end(), std::back_inserter(print_settings_.page_range.pages));
  }

  return true;
}

QSize PdfPrintDialog::getFinalSize(const QSize& size)
{
    QSize targetSize;
    auto maxSize = QSize(282-2,324-2);
    bool whScale = size.width()*maxSize.height() >= size.height()*maxSize.width();//过宽 true
    if(whScale){
        targetSize = QSize(maxSize.width(),size.height()*maxSize.width()*1.0/size.width());
    } else if(!whScale){
        targetSize = QSize(size.width()*maxSize.height()*1.0/size.height(),maxSize.height());
    }
    return targetSize;
}

void PdfPrintDialog::SyncPrintSettingStatus() {
  print_setting_combo_->blockSignals(true);
  auto duplex_mode = print_settings_.printer_settings.duplex;
  print_setting_combo_->setCurrentIndex(duplex_mode);
  print_setting_combo_->blockSignals(false);

}

void PdfPrintDialog::SyncPageIndexStatus(bool preview) {
  auto page_count = GetPageCount();
  page_count = page_count % preview_range_page_ ?
    (page_count / preview_range_page_ + 1) :
    (page_count / preview_range_page_);
  auto page_number = preview_start_page_ / preview_range_page_ + 1;

  {
      if(print_settings_.page_mode.type == print::kPageModeKindBooklet){
          auto page_info = getBookletStatus(GetPageCount());
          page_count = page_info.second;
          if(print_settings_.page_mode.booklet->subset != print::kPageModeBookletSubsetAll){
              page_count = qMax(1,page_info.second / 2);
          }
          booklet_page_start_spinbox_->setValue(1);
          booklet_page_end_spinbox_->setValue(page_count);
      }
  }

  prev_btn_->setDisabled(page_number == 1);
  next_btn_->setDisabled(page_number == page_count);
  first_btn_->setDisabled(page_number == 1);
  last_btn_->setDisabled(page_number == page_count); 

  page_index_line_edit_->setText(QString("%1").arg(page_number));
  page_index_line_edit_->setAlignment(Qt::AlignRight);
  int width = (DrawUtil::textWidth(font(),page_index_line_edit_->text())+20);
  page_index_line_edit_->setFixedWidth(width>MaxWidth?MaxWidth:width);
  page_count_label_->setText(QString("%1").arg(page_count));
  auto countlabW = (DrawUtil::textWidth(font(),page_count_label_->text())+10);
  page_count_label_->setFixedWidth(countlabW>50?50:countlabW);   

  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncOrientationStatus(bool preview) {
  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncCopiesStatus() {
	if (auto paintEngine = printer_.paintEngine()) {
		if (auto enginePriv = QWin32PrintEnginePrivate::get(paintEngine)) {
			if (auto private_data = reinterpret_cast<QWin32PrintEnginePrivate*>(enginePriv)) {
				copies_spin_box_->setValue(private_data->devMode->dmCopies);
                collate_check_box->setChecked(printer_.collateCopies());
			}
		}
	}
}

void PdfPrintDialog::SyncPageSizeStatus(bool preview) {
//  page_size_combo_->setCurrentIndex(printer_.pageSize());

  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncPageModeStatus(bool preview) {   
  auto_rotate_chk_->blockSignals(true);
  auto_rotate_chk_->setChecked(
    print_settings_.page_mode.auto_rotate);
  auto_rotate_chk_->blockSignals(false);
  auto_center_chk_->blockSignals(true);
  auto_center_chk_->setChecked(
    print_settings_.page_mode.auto_center);
  auto_center_chk_->blockSignals(false);
  auto_rotate_chk_->setEnabled(true);
  auto_center_chk_->setEnabled(true);
  booklet_page_start_spinbox_->setEnabled(false);
  booklet_page_end_spinbox_->setEnabled(false);
  page_size_combo_->setEnabled(true);

  switch (print_settings_.page_mode.type) {
  case print::kPageModeKindSize:
  {
    page_size_combo_->setEnabled(!choose_paper_source_btn->isChecked());
    preview_range_page_ = 1;
    auto preset = print_settings_.page_mode.size->preset;
    custom_scale_spinbox_->setDisabled(true);
    scale_button_group_->blockSignals(true);
    switch (preset) {
    case print::kPageModeSizePresetFit:      
      if(print_settings_.page_mode.size->auto_shrink){
          shrink_page_radio_btn_->setChecked(true);
      } else {
          fit_radio_btn_->setChecked(true);
      }
      break;
    case print::kPageModeSizePresetActual:
      actual_size_radio_btn_->setChecked(true);
      break;
    case print::kPageModeSizePresetCustom:
    {
      custom_scale_radio_btn_->setChecked(true);
      custom_scale_spinbox_->setEnabled(true);
      custom_scale_spinbox_->blockSignals(true);
      custom_scale_spinbox_->setValue(
        int((print_settings_.page_mode.size->scale + 0.005f) * 100));
      custom_scale_spinbox_->blockSignals(false);
      break;
    }
    default:
      break;
    }
    scale_button_group_->blockSignals(false);
    break;
  }
  case print::kPageModeKindPosters:
  {
    preview_range_page_ = 1;
    auto_rotate_chk_->setEnabled(false);
    auto_center_chk_->setEnabled(false);
    break;
  }
  case print::kPageModeKindPages:
  {
    auto& horiz_count = print_settings_.page_mode.pages->horiz_count;
    auto& vert_count = print_settings_.page_mode.pages->vert_count;
    preview_range_page_ = horiz_count * vert_count;
    preview_start_page_ = preview_start_page_ / preview_range_page_ * preview_range_page_;
    printPagesComboL->blockSignals(true);
    printPagesComboL->setCurrentText(QString::number(horiz_count));
    printPagesComboL->blockSignals(false);
    printPagesComboR->blockSignals(true);
    printPagesComboR->setCurrentText(QString::number(vert_count));
    printPagesComboR->blockSignals(false);
    page_order_combo_->blockSignals(true);
    page_order_combo_->setCurrentIndex(
      print_settings_.page_mode.pages->order);
    page_order_combo_->blockSignals(false);
    break;
  }
  case print::kPageModeKindBooklet:
  {
    SaveState();
    if (print_settings_.page_mode.booklet->subset == print::kPageModeBookletSubsetAll) {
      print_settings_.printer_settings.duplex = print::kDuplexModeDuplexAuto;
    } else {
      print_settings_.printer_settings.duplex = print::kDuplexModeSingleSide;
    }
    printer_.setPageOrientation(QPageLayout::Landscape);
    preview_range_page_ = 2;
    preview_start_page_ = preview_start_page_ / preview_range_page_ * preview_range_page_;
    booklet_page_start_spinbox_->blockSignals(true);
    booklet_page_start_spinbox_->blockSignals(false);
    booklet_page_end_spinbox_->blockSignals(true);
    booklet_page_end_spinbox_->blockSignals(false);
    break;
  }
  default:
    break;
  }

  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncPageRangeStatus(bool preview) {
  page_custom_edit->setEnabled(print_settings_.page_range.preset == print::kPageRangePresetCustom);
  page_subset_combo_->setEnabled(
    print_settings_.page_range.preset == print::kPageRangePresetCustom ||
    print_settings_.page_range.preset == print::kPageRangePresetAll);
  page_subset_combo_->blockSignals(true);
  page_subset_combo_->setCurrentIndex(print_settings_.page_range.subset);
  page_subset_combo_->blockSignals(false);
  page_range_combo_->blockSignals(true);
  page_range_combo_->setCurrentIndex(print_settings_.page_range.preset);
  page_range_combo_->blockSignals(false);

  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncPageContentStatus(bool preview) {
  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncPageOptionStatus(bool preview) {
  reverse_pages_chk_->blockSignals(true);
  reverse_pages_chk_->setChecked(print_settings_.reverse_print);
  reverse_pages_chk_->blockSignals(false);
  print_as_image_chk_->blockSignals(true);
  print_as_image_chk_->setChecked(print_settings_.print_as_image);
  print_as_image_chk_->blockSignals(false);

  if (preview) {
    UpdatePreview();
  }
}

void PdfPrintDialog::SyncPosterPageStatus()
{
    if(print_settings_.page_mode.type == print::kPageModeKindPosters){
        print_settings_.page_mode.posters->poster_page_sets.clear();
        qreal l = 0.0, t = 0.0, r = 0.0, b = 0.0;
        printer_.getPageMargins(&l, &t, &r, &b, QPrinter::Unit::Point);
        for(int i =0;i<int(page_indexes_.size());i++){
            print::PosterPageSettings poster_set;
            auto page_index = page_indexes_[i];
            auto page = page_engine_->dataCenter()->doc()->pageManager()->pageAtIndex(page_index)->pdfPointer();
            auto width = PDF_GetPageWidth(page);
            auto height = PDF_GetPageHeight(page);
            bool vertical = orientationCombo->currentIndex() == 0;
            auto paper_size = page_size_pair_.first[page_size_combo_->currentIndex()].size(QPageSize::Point);
            auto scale_tile = print_settings_.page_mode.posters->tile_percent;
            auto with_height = scale_tile * height;
            auto with_width = scale_tile * width;
            int row_count = 1;
            int col_count = 1;
            if(!vertical){
                row_count = (with_width+0.5f)/paper_size.height() + 1;
                col_count = (with_height+0.5f)/paper_size.width() + 1;
            } else {
                row_count = (with_width)/paper_size.width() + 1;
                col_count = (with_height)/paper_size.height() + 1;
            }
            qreal last_width,last_height;
            if(orientationCombo->currentIndex() == 1){
                last_height = col_count * paper_size.width();
                last_width = row_count * paper_size.height();
            } else {
                last_height = col_count * paper_size.height();
                last_width = row_count * paper_size.width();
            }
            auto size = getFinalSize(QSize(last_width,last_height));
            poster_set.row = row_count;
            poster_set.col = col_count;
            poster_set.real_paper_width = paper_size.width() - l -r;
            poster_set.real_paper_height = paper_size.height() - t - b;
            if(vertical){ //纵
                poster_set.paper_width = size.width()*1.0 / row_count;
                poster_set.paper_height = size.height()*1.0 / col_count;
            } else {
                poster_set.paper_width = size.height()*1.0 / col_count;
                poster_set.paper_height = size.width()*1.0 / row_count;
            }
            auto render_width = width * size.width() * print_settings_.page_mode.posters->tile_percent / last_width;
            auto render_height = height * size.height() * print_settings_.page_mode.posters->tile_percent / last_height;
            poster_set.x = ((size.width() - render_width)/2);
            poster_set.y = ((size.height() - render_height)/2);
            poster_set.width = render_width;
            poster_set.height = render_height;
            print_settings_.page_mode.posters->poster_page_sets.push_back(poster_set);
        }
        UpdatePreview();
    }
}

void PdfPrintDialog::SyncSystemProperties(LPDEVMODE model)
{   
    QTimer::singleShot(10,[=](){
        auto paper_size = page_size_windowid[model->dmPaperSize];
        auto page_size_index = page_size_pair_.first.indexOf(paper_size);
        page_size_combo_->setCurrentIndex(page_size_index);
        orientationCombo->setCurrentIndex(model->dmOrientation == 2 ? 1 :0);
        copies_spin_box_->setValue(model->dmCopies);
        collate_check_box->setChecked(model->dmCollate);
        gray_print_chk_->setChecked(model->dmColor == 1);
        page_sets_combo->blockSignals(true);
        if(model->dmDuplex == 2){
            page_sets_combo->setCurrentIndex(0);
        } else if(model->dmDuplex == 3){
            page_sets_combo->setCurrentIndex(1);
        }
        page_sets_combo->blockSignals(false);
        print_both_sides_chk->setChecked(model->dmDuplex == 2 || model->dmDuplex == 3);

        scaleLab->update();
        copies_spin_box_->update();
        collate_check_box->update();
        gray_print_chk_->update();
        print_both_sides_chk->update();
    });
}

void PdfPrintDialog::SyncInvoiceStatus(std::tuple<int, print::PageOrientation, print::PageModePagesOrder,bool,bool> data)
{
    int count = std::get<0>(data);
    print::PageOrientation orientation = std::get<1>(data);
    print::PageModePagesOrder order = std::get<2>(data);
//    bool crop = std::get<3>(data);
    bool rotate = std::get<4>(data);
    int left = 1 , right = 1;
    page_handing_combo->setCurrentIndex(2);
    switch (count) {
    case 1:
        break;
    case 2:
        left = 1;
        right = 2;
        break;
    case 4:
        left = 2;
        right = 2;
        break;
    case 6:
        left = 2;
        right = 3;
        break;
    }

//    print_settings_.page_mode.pages->crop_line = crop;
    print_settings_.page_mode.pages->invoice = true;
    printPagesComboL->setCurrentText(QString::number(left));
    printPagesComboR->setCurrentText(QString::number(right));        
    if(order == 1){
        order = print::PageModePagesOrder(2);
    } else if(order == 2){
        order = print::PageModePagesOrder(1);
    }
    page_order_combo_->setCurrentIndex(order);
    orientationCombo->setCurrentIndex(orientation);
    auto_rotate_chk_->setChecked(rotate && count != 1 && count != 2);
    auto_center_chk_->setChecked(true);
}

void PdfPrintDialog::SyncInvoiceReimbStatus(bool a5)
{
    QPageSize size(a5 ? QPageSize::A5 : QPageSize::A4);
    int index = 0;
    for(auto page_size : page_size_pair_.first) {
        if(page_size.id() == size.id()) {
            page_size_combo_->setCurrentText(page_size.name());
            page_size_combo_->currentIndexChanged(index);
            break;
        }
        index++;
    }
    orientationCombo->setCurrentIndex(a5);
    print_both_sides_chk->setChecked(false);
    gray_print_chk_->setChecked(true);
}

void PdfPrintDialog::SyncPageScaleStatus()
{
    float scale = 1.0f;
    int page_index = 0;
    if(print_settings_.page_range.preset == print::kPageRangePresetCurrentPage){
      page_index = std::get<0>(current_page_rect_);
    } else {
        page_index =  page_indexes_[page_index_line_edit_->text().toInt() - 1];
    }
    bool auto_rotate = print_settings_.page_mode.auto_rotate;
    auto page = page_engine_->dataCenter()->doc()->pageManager()->pageAtIndex(page_index)->pdfPointer();
    qreal l = 0.0, t = 0.0, r = 0.0, b = 0.0;
    printer_.getPageMargins(&l, &t, &r, &b, QPrinter::Unit::Point);
    auto width = PDF_GetPageWidth(page);
    auto height = PDF_GetPageHeight(page);

    if(!page_size_pair_.first.isEmpty()  //choose paper size
        && choose_paper_source_btn->isChecked()
        && print_settings_.page_mode.type == print::kPageModeKindSize
            )
    {
        int page_size_index = 0;
        for(auto sub_size : page_size_pair_.second){
            if( (width - 0.5f) <= sub_size.size(QPageSize::Point).width() && (height - 0.5f) <= sub_size.size(QPageSize::Point).height()){
                page_size_index = page_size_pair_.first.indexOf(sub_size);
//                auto sort_index = page_size_pair_.second.indexOf(sub_size);
//                if(sort_index != 0 && ){
//                    page_size_index = page_size_pair_.first.indexOf(page_size_pair_.second[sort_index-1]);
//                }
                break;
            }
        }
        page_size_combo_->blockSignals(true);
        page_size_combo_->setCurrentIndex(page_size_index);
        printer_.setPageSize(page_size_pair_.first[page_size_index]);
        page_size_combo_->blockSignals(false);
        UpdatePreview();
    }   

    auto preset = print_settings_.page_mode.size->preset;
    if (preset == print::kPageModeSizePresetFit ) {       
        auto page_size_pt = printer_.paperSize(QPrinter::Unit::Point);
        page_size_pt.setWidth(page_size_pt.width() - l - r);
        page_size_pt.setHeight(page_size_pt.height() - t - b);
        float scale_portrait = std::min(float(page_size_pt.width()) / width, float(page_size_pt.height()) / height);
        float scale_landscape = std::min(float(page_size_pt.width()) / height, float(page_size_pt.height()) / width);
        scale = scale_portrait;
        if (auto_rotate && scale_landscape > scale_portrait){
            scale = scale_landscape;
        }
        scale += 0.005f;
        if(scale>1.0f && print_settings_.page_mode.size->auto_shrink){
            scale = 1.0f;
        }
    } else if(preset == print::kPageModeSizePresetCustom ){
        scale = print_settings_.page_mode.size->scale;
    }

    if(print_settings_.page_mode.type == print::kPageModeKindPosters){
        bool vertical = orientationCombo->currentIndex() == 0;
        auto paper_size = page_size_pair_.first[page_size_combo_->currentIndex()].size(QPageSize::Point);
        auto scale_tile = print_settings_.page_mode.posters->tile_percent;
        auto with_height = scale_tile * height;
        auto with_width = scale_tile * width;
        int row_count = 1;
        int col_count = 1;
        if(!vertical){
            row_count = (with_width+0.5f)/paper_size.height() + 1;
            col_count = (with_height+0.5f)/paper_size.width() + 1;
        } else {
            row_count = (with_width)/paper_size.width() + 1;
            col_count = (with_height)/paper_size.height() + 1;
        }
        print_settings_.page_mode.posters->cur_row = row_count;
        print_settings_.page_mode.posters->cur_col = col_count;
        scale = scale_tile;
        UpdatePreview();
    }
    scaleLab->setText(tr("Scale:") + QString("%1%").arg(QString::number(int(scale*100))));
}

void PdfPrintDialog::SyncPageSetsStatus()
{    
    auto banBothSides = print_settings_.page_mode.type == print::kPageModeKindBooklet || print_settings_.page_mode.type == print::kPageModeKindPosters;
    print_both_sides_chk->setEnabled(!banBothSides);
    page_sets_combo->setEnabled(print_both_sides_chk->isChecked() && print_both_sides_chk->isEnabled());
    if(print_both_sides_chk->isChecked() && !banBothSides){
        printer_.setDuplex(page_sets_combo->currentIndex() == 0 ? QPrinter::DuplexLongSide : QPrinter::DuplexShortSide);
    } else {
        printer_.setDuplex(QPrinter::DuplexNone);
    }
    int bothSide =  print_both_sides_chk->isChecked() ? page_sets_combo->currentIndex() == 0 ? 2 : 3 : 0;
    print_config::insertSubConfig(HisBothSide,QString::number(bothSide)); //0 2 3
    UpdatePreview();
}

void PdfPrintDialog::UpdatePreview() {
  preview_widget_->updatePreview();
  QApplication::processEvents();
}

void PdfPrintDialog::SaveState() {
  state_.orientation = printer_.pageLayout().orientation();
  state_.page_orientation = print_settings_.page_orientation;
  state_.duplex_mode = print_settings_.printer_settings.duplex;
}

void PdfPrintDialog::RestoreState() {
  printer_.setPageOrientation(state_.orientation);
  print_settings_.page_orientation = state_.page_orientation;
  print_settings_.printer_settings.duplex = state_.duplex_mode;
}

void PdfPrintDialog::SyncPageSizeComboStatus(const QPair<bool,QPageSize> old_page_size)
{
    QList<QString> page_size_name;
    auto printer_info = QPrinterInfo::printerInfo(printer_combo_->currentText());
    auto page_size_list = printer_info.supportedPageSizes();
    for(auto size : page_size_list){
        page_size_name.push_back(size.name());
        page_size_windowid.insert(size.windowsId(),size);
    }
    QList<QPageSize> sort_size;
    for(auto page_size : page_size_list){
        sort_size.push_back(page_size);
    }
    std::sort(sort_size.begin(),sort_size.end(),[=](const QPageSize& size1,const QPageSize& size2){
        return (size1.size(QPageSize::Point).width() < size2.size(QPageSize::Point).width());
    });
    std::sort(sort_size.begin(),sort_size.end(),[=](const QPageSize& size1,const QPageSize& size2){
        return (size1.size(QPageSize::Point).height() < size2.size(QPageSize::Point).height()) &&
               ( qAbs(size2.size(QPageSize::Point).height() - size1.size(QPageSize::Point).height()) > qAbs(size2.size(QPageSize::Point).width() - size1.size(QPageSize::Point).width()) );
    });
    page_size_pair_.first = page_size_list;
    page_size_pair_.second = sort_size;

    auto infoMap = print_config::readConfig(printer_.printerName());
    if( !old_page_size.first){ //持久化
        page_size_combo_->blockSignals(true);
        page_size_combo_->clear();
        page_size_combo_->insertItems(0,page_size_name);
        page_size_combo_->blockSignals(false);
        if(!infoMap.isEmpty() && infoMap.contains(HisPaperSize)) {
            auto paper_size = infoMap[HisPaperSize];
            int index = 0;
            int tmp = 0;
            for(auto papersize : page_size_pair_.first){
                if(papersize.name() == paper_size){
                    index = tmp;
                    page_size_combo_->setCurrentText(paper_size);
                    break;
                }
                tmp++;
            }
            page_size_combo_->currentIndexChanged(index);
        } else {
            page_size_combo_->currentIndexChanged(0);
        }
    } else if(old_page_size.first) { //切换打印机
        QMetaObject::invokeMethod(this, "UpdatePageSizeCombo", Qt::QueuedConnection, Q_ARG(const QList<QPageSize>&, page_size_list),
                                                                                     Q_ARG(const QPageSize&, old_page_size.second));
    }
}

void PdfPrintDialog::UpdatePageSizeCombo(const QList<QPageSize>& page_size_list,
                                         const QPageSize& old_page_size)
{
    QList<QString> page_size_name;
    for(auto size : page_size_list){
        page_size_name.push_back(size.name());
        page_size_windowid.insert(size.windowsId(),size);
    }

    page_size_combo_->blockSignals(true);
    page_size_combo_->clear();
    page_size_combo_->insertItems(0,page_size_name);
    page_size_combo_->blockSignals(false);

    auto adjust_index = page_size_list.indexOf(old_page_size);
    adjust_index = qMax(0,adjust_index);
    if(adjust_index > 0 ){
        page_size_combo_->setCurrentText(old_page_size.name());
    }
    page_size_combo_->currentIndexChanged(adjust_index);
}

void PdfPrintDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_lastPos = event->globalPos();
    mIsPressed = false;
    QWidget::mouseReleaseEvent(event);
}

void PdfPrintDialog::mousePressEvent(QMouseEvent* event)
{
    if(event->pos().y() <= 30){
        mIsPressed = true;
        m_lastPos = event->globalPos();
    }
    QWidget::mousePressEvent(event);
}

void PdfPrintDialog::mouseMoveEvent(QMouseEvent* event)
{
    if(mIsPressed && !isMaximized()){
        this->move(this->x() + (event->globalX() - m_lastPos.x()),
                   this->y() + (event->globalY() - m_lastPos.y()));
        m_lastPos = event->globalPos();
    }
    QWidget::mouseMoveEvent(event);
}
