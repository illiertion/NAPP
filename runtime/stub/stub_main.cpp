// Stub runtime host: built only when NAPP_ENABLE_CEF=OFF.
// It never pretends to run an app; it reports the missing engine honestly so
// the launcher and the user see why nothing started.
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTextStream(stderr) << "NAPP runtime host: CEF 지원 없이 빌드되었습니다.\n"
                        << "Chromium(CEF) 런타임으로 다시 빌드하세요: "
                        << "cmake -DNAPP_ENABLE_CEF=ON (third_party/cef 필요)\n";
    return 3;
}
