#ifndef PDF_PRINT_DIALOG_H
#define PDF_PRINT_DIALOG_H

#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QFrame>
#include <QPushButton>
#include "./src/basicwidget/lineedit.h"
#include <QLabel>
#include <QCheckBox>
#include <QPrintEngine>
#include <QSpinBox>
#include <QRadioButton>
#include <QStackedWidget>
#include <QButtonGroup>
#include "private/qprintengine_win_p.h"
#include "src/basicwidget/cframelesswidget.h"
#include "pdf_print_settings.h"

struct PreviewOption {
  int width = 0;
  int height = 0;
  int width_pt = 0;
  int height_pt = 0;
  bool gray_scale = false;
  bool auto_rotate = true;
  bool show_content = true;
  bool show_annot = true;
  bool show_widget = true;
  bool show_border = false;
  bool auto_fit = true;
  bool auto_shrink = false;
  bool auto_center = false;
  int row = 1;
  int col = 1;
  print::PageModePagesOrder order = print::kPageModePagesOrderPortrait;
  float scale = 1.0f;
  float preview_vs_actual_scale = 1.0f;
  float zoom = 1.0f;
  int posters_row = 1;
  int posters_col = 1;
  bool pre_poster = false;
};

class UComboBox;

class CPreLabel : public QLabel
{
    Q_OBJECT
public:
    CPreLabel(QWidget* parent = nullptr) : QLabel(parent){}
    void clearData();
    void addImage(const QImage&,const QRectF&);
    void updateOpt(const PreviewOption& opt);
    void paintEvent(QPaintEvent *) override;
private:
    PreviewOption mOpt;
    QList<QPair<QRectF,QImage>> mImageList;
};

class PageEngine;
class PdfPrint;
class QDoubleSpinBox;
class PdfPrintDialog : public CFramelessWidget {
  Q_OBJECT
public:
  PdfPrintDialog(PageEngine* page_engine, QWidget* parent = nullptr);
  void SyncHistoryInfo(); 
  void InitView();
  void SetPageEngine(PageEngine* page_engine);
  void SetCurrentPageRect(const std::tuple<int, QPointF, double>& rect);
  void PreView(QPrinter*);
  void PostersPreview(QPrinter * printer,QRectF&);
  void SyncInvoiceStatus(std::tuple<int, print::PageOrientation, print::PageModePagesOrder, bool, bool>);
  void SyncInvoiceReimbStatus(bool a5);
private slots:
  void OnPreviewChanged();
  void OnPaintRequested(QPrinter *printer);
  void OnCopiesChanged(int);
  void OnScaleGroupButtonClicked(int);
  void OnCustomScaleChanged(int);
  void OnCustomPagesVerticalChanged(int);
  void OnCustomPagesHorizalChanged(int); 
  void OnPrinterChanged(); //打印机切换 同步设置
private:
  struct PrinterState {
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    print::PageOrientation page_orientation = print::kPageOrientaionPortrait;
    print::DuplexMode duplex_mode = print::kDuplexModeSingleSide;
  };

  QFrame* CreateBkFrame();
  QFrame* CreateLineFrame(const int width,const int height);
  // Right
  QFrame* CreatePreviewFrame();
  QFrame* CreateNavigateBtnFrame();

  // new ui
  QFrame* CreateTopFrame();

  QFrame* CreateBottomFrame();
  QFrame* CreateLeftSettingsFrame(); 
  QFrame* CreateLeftTopSetsFrame();

  QFrame* CreateRightSettingsFrame();
  QFrame* CreatePrintModeAndCopies();
  QFrame* CreatePagestoPrint();
  QFrame* CreatePageSizingAndHanding();

  QFrame* CreatePageHandingSizeFrame();
  QFrame* CreatePageHandingPosterFrame();
  QFrame* CreatePageHandingMultipleFrame();
  QFrame* CreatePageHandingBookletFrame();

  int GetPageCount();
  void ApplySubset(print::PageRangeSubset subset);
  bool CustomPages(const QString& str,bool change = true);
  QSize getFinalSize(const QSize&);//max 280*336

  void SyncPrintSettingStatus();
  void SyncPageIndexStatus(bool preview);
  void SyncOrientationStatus(bool preview);
  void SyncCopiesStatus();
  void SyncPageSizeStatus(bool preview);
  void SyncPageModeStatus(bool preview);
  void SyncPageRangeStatus(bool preview);
  void SyncPageContentStatus(bool preview);
  void SyncPageOptionStatus(bool preview);
  void SyncPageScaleStatus();
  void SyncPageSetsStatus();
  void SyncPageSizeComboStatus(const QPair<bool,QPageSize> old_page_size); //持久化或切换打印机
  Q_INVOKABLE void UpdatePageSizeCombo(const QList<QPageSize>&,
                                       const QPageSize&);
  void SyncPosterPageStatus();
  void SyncSystemProperties(LPDEVMODE);  
  void UpdatePreview();  

  void SaveState();
  void RestoreState();

  QPrintPreviewWidget* preview_widget_ = nullptr;
  CPreLabel* poster_pre_lab = nullptr;
  QLabel* outer_lab = nullptr;
  QPushButton* prev_btn_ = nullptr;
  QPushButton* next_btn_ = nullptr;
  QPushButton* first_btn_ = nullptr;
  QPushButton* last_btn_ = nullptr;
  QLineEdit* page_index_line_edit_ = nullptr;
  QLabel* page_count_label_ = nullptr;
  UComboBox* printer_combo_ = nullptr;
  QPushButton* portrait_btn_ = nullptr;
  QPushButton* landscape_btn_ = nullptr;
  QCheckBox* gray_print_chk_ = nullptr;
  UComboBox* print_setting_combo_ = nullptr;
  QSpinBox* copies_spin_box_ = nullptr;
  QCheckBox* collate_check_box = nullptr;
  UComboBox* page_size_combo_ = nullptr;
  UComboBox* orientationCombo = nullptr;
  UComboBox* page_range_combo_ = nullptr;
  UComboBox* page_subset_combo_ = nullptr;
  QCheckBox* print_content_chk_ = nullptr;
  QCheckBox* print_annot_chk_ = nullptr;
  QCheckBox* print_field_chk_ = nullptr;
  QCheckBox* reverse_pages_chk_ = nullptr;
  QCheckBox* print_as_image_chk_ = nullptr;
  QPushButton* print_mode_size_btn_ = nullptr;
  QPushButton* print_mode_poster_btn_ = nullptr;
  QPushButton* print_mode_multiple_btn_ = nullptr;
  QPushButton* print_mode_booklet_btn_ = nullptr;
  QStackedWidget* print_mode_stacked_widget_ = nullptr;
  QFrame* print_mode_size_frame_ = nullptr;
  QFrame* print_mode_poster_frame_ = nullptr;
  QFrame* print_mode_multiple_frame_ = nullptr;
  QFrame* print_mode_booklet_frame_ = nullptr;
  QCheckBox* auto_rotate_chk_ = nullptr;
  QCheckBox* auto_center_chk_ = nullptr;
  QRadioButton* fit_radio_btn_ = nullptr;
  QRadioButton* actual_size_radio_btn_ = nullptr;
  QRadioButton* custom_scale_radio_btn_ = nullptr;
  QRadioButton* shrink_page_radio_btn_ = nullptr;
  QRadioButton* choose_paper_source_btn = nullptr;
  QButtonGroup* scale_button_group_ = nullptr;
  QSpinBox* custom_scale_spinbox_ = nullptr;
  QSpinBox* percentage_of_tile_spinbox_ = nullptr;
  QDoubleSpinBox* overlap_spinbox_ = nullptr;
  QCheckBox* crop_mark_chk_ = nullptr;
  QCheckBox* label_chk_ = nullptr;
  UComboBox* custom_pages_per_paper_combo_ = nullptr;
  QSpinBox* horiz_page_per_pager_spinbox_ = nullptr;
  QSpinBox* vert_page_per_pager_spinbox_ = nullptr;
  UComboBox* page_order_combo_ = nullptr;
  UComboBox* booklet_subset_combo_ = nullptr;
  QSpinBox* booklet_page_start_spinbox_ = nullptr;
  QSpinBox* booklet_page_end_spinbox_ = nullptr;
  UComboBox* booklet_gutter_combo_ = nullptr;
  UComboBox* page_sets_combo = nullptr;
  QCheckBox* print_both_sides_chk = nullptr;

  QLineEdit* page_custom_edit = nullptr;
  UComboBox* printPagesComboL = nullptr;
  UComboBox* printPagesComboR = nullptr;
  QFrame* bkFrameMain = nullptr;

  QLabel* scaleLab = nullptr;

  UComboBox* page_handing_combo = nullptr;

  PageEngine* page_engine_ = nullptr;
  int preview_start_page_ = 0;
  int preview_range_page_ = 1;
  int page_count_ = 0;
  print::PdfPrintSettings print_settings_;
  QPrinter printer_;
  std::vector<int> page_indexes_;
  PrinterState state_;
  std::tuple<int, QPointF, double> current_page_rect_;

private:
  bool mIsPressed = false;
  QPoint m_lastPos = QPoint(0,0);
  QPair<QList<QPageSize>,QList<QPageSize>> page_size_pair_;
  QMap<int,QPageSize> page_size_windowid;
  int maxPageCount = 0;

protected:
  void mouseReleaseEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);

};

#endif // PDF_PRINT_DIALOG_H
