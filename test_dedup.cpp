#include <iostream>
#include <QCoreApplication>
#include <QFile>
#include "catalog_parser.h"
using namespace arachnel::core;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QFile f1(R"(d:/Work/Arachnel/plugins/freetp/games.json)");
    QFile f2(R"(d:/PetWork/freetp-hydra-link/games-arachnel.json)");
    f1.open(QIODevice::ReadOnly);
    f2.open(QIODevice::ReadOnly);
    auto e1 = parseCatalogFeed(f1.readAll(), "freetp");
    auto e2 = parseCatalogFeed(f2.readAll(), "onlinefix");
    QVector<CatalogEntry> merged = e1;
    merged += e2;
    std::cout << "before " << merged.size() << std::endl;
    deduplicateCatalogEntries(merged);
    std::cout << "after " << merged.size() << std::endl;
    int multi = 0;
    for (const auto& e : merged) if (!e.sourceVariants.isEmpty()) multi++;
    std::cout << "multi " << multi << std::endl;
    return 0;
}
