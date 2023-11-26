//
// Created by xzp on 2023/11/16.
//

#ifndef QT_FLUTTER_SAMPLE_FLUTTER_EMBEDDER_UTILS_H
#define QT_FLUTTER_SAMPLE_FLUTTER_EMBEDDER_UTILS_H

#include "FlutterEmbedder.h"
#include <QWindow>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOffscreenSurface>
#include <QScreen>

class FlutterEmbedderUtils : public QObject {
Q_OBJECT
private:
    QOpenGLContext *mRender;
    QWindow *mQwindow;
    FlutterEngine mEngine;
    bool mouseDown = false;
    bool hasRemove = true;
    bool mIsRunning = false;

    double devicePixelRatio();

    void postTask(FlutterTask task);

    bool runsTasksOnSelfThread();

    int translateModifiers(Qt::KeyboardModifiers rawMods);

public:
    explicit FlutterEmbedderUtils(QOpenGLContext *glWidget, QWindow *qWindow);

    bool isRunning() {
        return mEngine != nullptr && mIsRunning;
    }

    bool handleWindowResize();

    void initByWindow();

    void run();

    bool flutterEvent(QEvent *event);

public slots:

    void handleTask(FlutterTask task);

signals:

    void onNewTask(FlutterTask task);
};


#endif //QT_FLUTTER_SAMPLE_FLUTTER_EMBEDDER_UTILS_H
