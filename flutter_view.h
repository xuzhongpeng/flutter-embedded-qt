//
// Created by xzp on 2023/11/23.
//

#ifndef QT_FLUTTER_SAMPLE_FLUTTER_VIEW_H
#define QT_FLUTTER_SAMPLE_FLUTTER_VIEW_H

#include <QWindow>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include "flutter_embedder_utils.h"

class FlutterView : public QWindow {
public:
    FlutterView(QWindow *parent);

    FlutterEmbedderUtils *flutterEmbedderUtils = nullptr;
protected:
    bool event(QEvent *event) override;

private:
    QOpenGLContext *context;
};


#endif //QT_FLUTTER_SAMPLE_FLUTTER_VIEW_H
