//
// Created by xzp on 2023/11/16.
//

#ifndef FLUTTER_EMBEDDER_QT_FLUTTER_EMBEDDER_UTILS_H
#define FLUTTER_EMBEDDER_QT_FLUTTER_EMBEDDER_UTILS_H

#include "FlutterEmbedder.h"
#include <QWindow>
#include <QOpenGLContext>

class FlutterEmbedderUtils : public QObject {
Q_OBJECT

private:
    QOpenGLContext *mContext;
    QWindow *mQwindow;
    FlutterEngine mEngine;
    bool mouseDown = false;
    bool hasRemove = true;
    bool mIsRunning = false;

    double devicePixelRatio();

    void postTask(FlutterTask task);

    bool runsTasksOnSelfThread();

    int translateModifiers(Qt::KeyboardModifiers rawMods);

    bool handleWindowResize();

public:
    explicit FlutterEmbedderUtils(QOpenGLContext *context, QWindow *window);

    bool isRunning() {
        return mEngine != nullptr && mIsRunning;
    }

    void run();

    bool flutterEvent(QEvent *event);

public slots:

    void handleTask(FlutterTask task);

signals:

    void handleMainTask(FlutterTask task);
};

#endif // FLUTTER_EMBEDDER_QT_FLUTTER_EMBEDDER_UTILS_H
