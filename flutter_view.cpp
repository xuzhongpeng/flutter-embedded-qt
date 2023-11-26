//
// Created by xzp on 2023/11/23.
//

#include "flutter_view.h"


FlutterView::FlutterView(QWindow *parent)
        : QWindow(parent) {
//    setFlags(Qt::Window | Qt::FramelessWindowHint);
    printf("init\n");
    setSurfaceType(QSurface::OpenGLSurface);
    create();
    resize(1280, 720);

    QSurfaceFormat format = requestedFormat();
    setFormat(format);
    format.setProfile(QSurfaceFormat::CoreProfile);  // 使用核心配置

    context = new QOpenGLContext;
    context->setFormat(format);
    context->create();
    context->makeCurrent(this);
    initializeOpenGLFunctions();

    flutterEmbedderUtils = new FlutterEmbedderUtils(context, this);
    flutterEmbedderUtils->initByWindow();
    flutterEmbedderUtils->run();
}

bool FlutterView::event(QEvent *event) {
    if (!flutterEmbedderUtils || !flutterEmbedderUtils->isRunning()) return false;
    if (!flutterEmbedderUtils->flutterEvent(event)) {
        return QWindow::event(event);
    } else {
        return true;
    }
}