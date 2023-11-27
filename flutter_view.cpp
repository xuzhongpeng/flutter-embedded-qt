//
// Created by xzp on 2023/11/23.
//

#include "flutter_view.h"


FlutterView::FlutterView(QWindow *parent)
        : QWindow(parent) {
    // 将此窗口的渲染类型设置为OpenGL
    setSurfaceType(QSurface::OpenGLSurface);
    // 设置窗口宽高
    resize(1280, 720);
    // 创建窗口
    create();

    context = new QOpenGLContext;
    context->create();

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