#include "pdf_print_settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QDebug>

namespace print_config {
using namespace print;
    static QMap<QString,QString> gPrinterHisConfig;
    void writeConfig(const QString& printName) {               
        auto gConfigPath = print::get_his_config();
        QSettings settings(gConfigPath, QSettings::IniFormat);
        settings.beginGroup(printName);
        for(auto key : gPrinterHisConfig.keys()){
            settings.setValue(key,gPrinterHisConfig[key]);
        }
        settings.endGroup();
    }
    QMap<QString,QString> readConfig(const QString& printName) {
        gPrinterHisConfig.clear();
        auto gConfigPath = print::get_his_config();
        if(QFile::exists(gConfigPath)){
            QFile file(gConfigPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Truncate)){
                QSettings settings(gConfigPath, QSettings::IniFormat);
                settings.beginGroup(printName);
                for(auto key:settings.childKeys()){
                    gPrinterHisConfig.insert(key,settings.value(key).toString());
                }
                settings.endGroup();
                file.close();
            }
        }
        return gPrinterHisConfig;
    }
    void insertSubConfig(const QString& key,const QString& value) {
        gPrinterHisConfig.insert(key,value);
    }
    QMap<QString,QString> getConfig() {
        return gPrinterHisConfig;
    }
}


std::string print::SaveXml(const PdfPrintSettings & settings) {
  std::string str;
  str = xpack::xml::encode(settings, "PrintSettings");
  return str;
}

void print::LoadXml(const std::string & str, PdfPrintSettings & settings) {
  xpack::xml::decode(str, settings);
}

QString print::get_his_config() {
    QDir dir;
    auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    path = QDir(path + "/../UPDF/printer/").absolutePath();
    path = QDir::toNativeSeparators(path) + QDir::separator();
    if(!dir.exists(path)){
        dir.mkdir(path);
    }
    path += PrinterHisInfo;
    return path;
}

print::PageMode::PageMode() {
  set_page_mode_size(std::make_shared<PageModeSize>());
}

void print::PageMode::set_page_mode_size(
  const std::shared_ptr<PageModeSize>& val) {
  type = kPageModeKindSize;
  size = val;
}

void print::PageMode::set_page_mode_poster(
  const std::shared_ptr<PageModePosters>& val) {
  type = kPageModeKindPosters;
  posters = val;
}

void print::PageMode::set_page_mode_pages(
  const std::shared_ptr<PageModePages>& val) {
  type = kPageModeKindPages;
  pages = val;
}

void print::PageMode::set_page_mode_booklet(
  const std::shared_ptr<PageModeBooklet>& val) {
  type = kPageModeKindBooklet;
  booklet = val;
}

void print::PageMode::Create(PageModeKind t) {
  type = t;

  switch (type) {
  case print::kPageModeKindUnknown:
    break;
  case print::kPageModeKindSize:
  {
    if (!size) {
      size = std::make_shared<PageModeSize>();
    }
    break;
  }
  case print::kPageModeKindPosters:
  {
    if (!posters) {
      posters = std::make_shared<PageModePosters>();
    }
    break;
  }    
  case print::kPageModeKindPages:
  {
    if (!pages) {
      pages = std::make_shared<PageModePages>();
    }
    break;
  }    
  case print::kPageModeKindBooklet:
  {
    if (!booklet) {
      booklet = std::make_shared<PageModeBooklet>();
    }
    break;
  }    
  default:
    break;
  }
}

void print::PageMode::Clear() {
  type = kPageModeKindUnknown;
  size.reset();
  posters.reset();
  pages.reset();
  booklet.reset();
}
