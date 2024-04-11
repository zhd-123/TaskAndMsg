#ifndef PDF_PRINT_IMP_WITH_QPRINTER_H
#define PDF_PRINT_IMP_WITH_QPRINTER_H

#include "pdf_print.h"

class PdfPrintImpWithQPrinter : public PdfPrint {
  Q_OBJECT
public:
  explicit PdfPrintImpWithQPrinter(QObject *parent = nullptr);
  virtual ~PdfPrintImpWithQPrinter();
  virtual bool PrintPdfDoc(PageEngine* page_engine, 
    const std::tuple<int, QPointF, double>& current_page_rect);
  bool PrintInvoiceReimbDoc(PageEngine* page_engine,
                          const std::tuple<int, QPointF, double>& current_page_rect,bool a5);
  bool PrintInvoicePdfDoc(PageEngine *page_engine,std::tuple<int,print::PageOrientation,print::PageModePagesOrder,bool,bool>);
signals:

public slots:

private:
};

#endif // PDF_PRINT_IMP_WITH_QPRINTER_H
