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

class FlutterEmbedderUtils : public QObject{
    Q_OBJECT
private:
    QOpenGLContext* mRender;
    QWindow* mQwindow;
    FlutterEngine mEngine;
    bool mouseDown = false;
    bool hasRemove = true;
    bool mIsRunning = false;
    double devicePixelRatio();
    void PostTask(FlutterTask task);
    bool RunsTasksOnSelfThread();
public:
    bool isRunning() {
        return mEngine != nullptr && mIsRunning;
    }
    int HandleWindowResize();
    explicit FlutterEmbedderUtils(QOpenGLContext* glWidget, QWindow* qWindow);
    void initByWindow();
    void run();
    bool mouseEvent(QEvent *event);
public slots:
    void HandleTask(FlutterTask task);

signals:
    void OnNewTask(FlutterTask task);
};


#endif //QT_FLUTTER_SAMPLE_FLUTTER_EMBEDDER_UTILS_H
