
#include "flutter_view.h"
#include <iostream>
#include <thread>
#include <QApplication>

void waitForDebuggerAttachment() {
    std::cout << "Waiting for the debugger to attach..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));
}

int main(int argc, char *argv[]) {
//    waitForDebuggerAttachment();
    QApplication app(argc, argv);
    FlutterView window(nullptr);
    window.show();
    return app.exec();
}
