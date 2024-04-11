#include "pdf_print_imp_with_qprinter.h"
#include "pdf_print_dialog.h"

#include "src/pageengine/pageengine.h"
#include "src/pageengine/DataCenter.h"

PdfPrintImpWithQPrinter::PdfPrintImpWithQPrinter(
  QObject *parent): PdfPrint(parent) {
}

PdfPrintImpWithQPrinter::~PdfPrintImpWithQPrinter() {
}

bool PdfPrintImpWithQPrinter::PrintPdfDoc(PageEngine * page_engine, 
  const std::tuple<int, QPointF, double>& current_page_rect) {
    if (!page_engine->dataCenter()->doc()) {
	    return false;
    }
    
  PdfPrintDialog dialog(page_engine);
  dialog.SetCurrentPageRect(current_page_rect);
  dialog.exec();
  return true;
}

bool PdfPrintImpWithQPrinter::PrintInvoiceReimbDoc(PageEngine *page_engine,
                                                   const std::tuple<int, QPointF, double> &current_page_rect,
                                                   bool a5)
{
  if (!page_engine->dataCenter()->doc()) {
     return false;
  }

  PdfPrintDialog dialog(page_engine);
  dialog.SetCurrentPageRect(current_page_rect);
  dialog.SyncInvoiceReimbStatus(a5);
  dialog.exec();
  return true;
}

bool PdfPrintImpWithQPrinter::PrintInvoicePdfDoc(PageEngine *page_engine,
                                          std::tuple<int, print::PageOrientation, print::PageModePagesOrder, bool, bool> data)
{
    if (!page_engine->dataCenter()->doc()) {
        return false;
    }
    PdfPrintDialog dialog(page_engine);
    dialog.SyncInvoiceStatus(data);
    dialog.exec();
    return true;
}
